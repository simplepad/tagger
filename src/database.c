#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "../include/database.h"

#define LISTINGS_TABLE_NAME "listings"
#define TAGS_TABLE_NAME "tags"
#define ITEMS_TABLE_NAME "items"
#define ITEM_TAGS_TABLE_NAME "itemtags"
#define DATABASE_DEFAULT_LOCATION "test.tdb"

int execute_sql_string(sqlite3 *db, char *sql);

/**
 * @brief Expand a parameter in an sql query into an array
 *
 * @param sql string with the sql query
 * @param param_num the number of parameter to expand into an array, index starts with 1 (like in `sqlite3_bind_*` functions)
 * @param array_size the desired size of the array
 * @return `NULL` on error or a pointer to the new sql string, the caller is responsible for freeing allocated memory!
 */
char * sql_expand_param_into_array(char *sql, size_t param_num, size_t array_size) {
	if (param_num == 0) {
		fputs("Wrong param number, index starts with 1!\n", stderr);
		return NULL;
	}
	if (param_num == 1) {
		return (char*) malloc(sizeof(char) * (strlen(sql) + 1));
	}
	param_num--;

	size_t array_start = 0;
	size_t sql_initial_length = strlen(sql);
	size_t to_find = param_num;

	// search for the `?` that should become an array
	for (size_t i = 0; i < sql_initial_length; i++) {
		if (sql[i] == '?') {
			if (!to_find) {
				array_start = i;
				break;
			} else {
				to_find--;
			}
		}
	}

	if (array_start == 0) {
		fprintf(stderr, "Could not find the %luth parameter\n", param_num+1);
		return NULL;
	}

	size_t new_sql_length = sql_initial_length + ((array_size-1) * 2);
	char * new_sql = malloc(sizeof(char) * (new_sql_length + 1)); // 1 for \0
	if (new_sql == NULL) {
		fputs("Could not allocate memory for new sql string\n", stderr);
		return NULL;
	}

	size_t bytes_written = 0;

	// copying the start of the original query
	memcpy(new_sql, sql, sizeof(char) * array_start);
	bytes_written += sizeof(char) * array_start;

	// adding the array of parameters (-1 because of the original `?` that will be copied later)
	for (size_t i = 0; i<=param_num; i++) {
		new_sql[bytes_written] = '?';
		new_sql[bytes_written+1] = ',';
		bytes_written += 2;
	}

	// copying the rest of the original query, including the original ? as the last parameter of the array
	memcpy(new_sql+bytes_written, sql+array_start, sql_initial_length-array_start);
	bytes_written += sql_initial_length-array_start;

	// ending the string
	new_sql[bytes_written] = '\0';
	
	return new_sql;
}

/**
 * Check if tag already exists in the database
 *
 * @param db SQLite3 database to search the tag in, must be initialized
 * @param tagName string with the tag name
 * @return `1` if the tag was found, `0` if not, `-1` on error
 */
int tag_exists(sqlite3 *db, char *tagName) {
	sqlite3_stmt *stmt;

	int rc = sqlite3_prepare_v2(db,
		"SELECT EXISTS(SELECT 1 FROM " TAGS_TABLE_NAME " WHERE tag_name=? LIMIT 1);", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_text(stmt, 1, tagName, -1, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	} else if (rc == SQLITE_ROW) {
		// Tag found
		if (sqlite3_column_int(stmt, 0) == 1) {
			sqlite3_finalize(stmt);
			return 1;
		} else {
			sqlite3_finalize(stmt);
			return 0;
		}
	} else {
		// Table not found
		sqlite3_finalize(stmt);
		return 0;
	}
}

/**
 * Add new tag to the database
 *
 * @param db SQLite3 database to add the tag to, must be initialized
 * @param tagName new tag's name
 * @return `tag_id` if the tag was added, `0` if tag already exists, `-1` on error
 */
int64_t add_new_tag(sqlite3 *db, char *tagName) {
	if (db == NULL || tagName == NULL) {
		return -1;
	}

	sqlite3_stmt *stmt;

	int rc = sqlite3_prepare_v2(db, "INSERT INTO " TAGS_TABLE_NAME " (tag_name) VALUES(?);", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_text(stmt, 1, tagName, -1, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return sqlite3_last_insert_rowid(db);
	} else if (rc == SQLITE_CONSTRAINT) { // Tag already exists
		sqlite3_finalize(stmt);
		return 0;
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

/**
 * @brief Get tag id by name
 *
 * @param db sqlite3 database
 * @param tag_name a null-terminated string with the exact tag name
 * @return `tag_id` if the tag was found or `0` if the tag doesn't exist, or `-1` on error
 */
int64_t get_tag_id(sqlite3 *db, char *tag_name) {
	sqlite3_stmt *stmt;
	int64_t tag_id = 0;

	int rc = sqlite3_prepare_v2(db, "SELECT tag_id FROM " TAGS_TABLE_NAME " WHERE tag_name=? LIMIT 1;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_text(stmt, 1, tag_name, -1, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		tag_id = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		return tag_id;
	} else if (rc == SQLITE_DONE) {
		// tag not found
		sqlite3_finalize(stmt);
		return 0;
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

/**
 * @brief Get the total number of tags in the database
 *
 * @param db sqlite3 database
 * @return number of tags or `-1` on error
 */
int get_total_tags_count(sqlite3 *db) {
	sqlite3_stmt *stmt;

	int tag_count = -1;
	int rc = sqlite3_prepare_v2(db, "SELECT count() FROM " TAGS_TABLE_NAME ";", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		tag_count = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		return tag_count;
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

/**
 * @brief Get the number of tags of an item
 *
 * @param db sqlite3 database
 * @param item_id id of an item to get the tags count of
 * @return number of tags of an item or `-1` on error
 */
int get_item_tags_count(sqlite3 *db, int64_t item_id) {
	sqlite3_stmt *stmt;

	int tag_count = -1;
	int rc = sqlite3_prepare_v2(db, "SELECT count() FROM " ITEM_TAGS_TABLE_NAME " WHERE item_id=?;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_int64(stmt, 1, item_id);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		tag_count = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		return tag_count;
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
}

/**
 * @brief Get an item's tag ids
 *
 * Caller must free the allocated memory. If the return code is `-1`
 * values in `tags_array_size` and `tags_array` are undefined
 * and should not be used (caller should not try to free `tags_array`)!
 *
 * @param db sqlite3 database
 * @param item_id id of an item to get the tags of
 * @param tags_array_size pointer where to store resulting array size
 * @param tags_array pointer where to store the array of tag ids
 * @return `0` on success, otherwise `-1` on error
 */
int get_item_tag_ids(sqlite3 *db, int64_t item_id, int *tags_array_size, int **tags_array) {
	if (item_id < 1 || tags_array_size == NULL || tags_array == NULL) { return -1; }

	sqlite3_stmt *stmt;

	// get tags count
	int tag_count = get_item_tags_count(db, item_id);
	if (tag_count < 1) {
		fprintf(stderr, "Error when getting item's tags count for item_id %ld\n", item_id);
		return -1;
	}

	// if item has no tags
	if (tag_count == 0) {
		*tags_array_size = 0;
		*tags_array = NULL;
		return 0;
	}

	// allocate memory to store tags
	*tags_array = malloc(tag_count * sizeof(int));
	if (*tags_array == NULL) {
		fprintf(stderr, "Error when getting item's tags count for item_id %ld\n", item_id);
		return -1;
	}

	// get tags from the database
	int rc = sqlite3_prepare_v2(db, "SELECT tag_id FROM " ITEM_TAGS_TABLE_NAME " WHERE item_id=?;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_int64(stmt, 1, item_id);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	// fill tags_array
	for (int i = 0; i < tag_count; i++) {
		rc = sqlite3_step(stmt);
		switch (rc) {
			case SQLITE_ROW:
				(*tags_array)[i] = sqlite3_column_int64(stmt, 0);
				break;
			case SQLITE_DONE:
				// less tags than expected!
				sqlite3_finalize(stmt);
				fprintf(stderr, "Error while getting item tags for item_id %lu, got less tags than expected!\n", item_id);
				free(*tags_array);
				return -1;
			default:
				// error
				sqlite3_finalize(stmt);
				fprintf(stderr, "Error while getting item tags for item_id %lu, %s\n", item_id, sqlite3_errmsg(db));
				free(*tags_array);
				return -1;
		}
	}

	sqlite3_finalize(stmt);

	*tags_array_size = tag_count;
	return 0;
}

/**
 * @brief Update item's tags to include those supplied in the provided tag array
 *
 * @param db sqlite3 database
 * @param item_id id of the item to update
 * @param tags array with tag names of char* ending with NULL
 * @param on_new_tags flag whether or not to auto-add non-existing tags
 * @return `1` if the item was updated, `0` if the item wasn't updated and `-1` on error
 */
int update_tags(sqlite3 *db, int64_t item_id, char **tags, ON_NEW_TAGS on_new_tags) {
	if (item_id <= 0) return -1; // bad item_id
	if (tags == NULL) return 0; // no updates needed
	int64_t tag_id;
	
	// begin a transaction
	if (execute_sql_string(db, "BEGIN TRANSACTION;")) {
		fprintf(stderr, "Error when trying to begin a transaction: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	int item_tags_added = 0;
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(db, "INSERT INTO " ITEM_TAGS_TABLE_NAME " (item_id,tag_id) VALUES (?,?);", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	char *tag_name = *tags;
	while (tag_name != NULL) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);

		// 1 here means leftmost SQL parameter index
		rc = sqlite3_bind_int64(stmt, 1, item_id);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			// rollback the transaction
			if (execute_sql_string(db, "ROLLBACK;")) {
				fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
				return -1;
			}
			return -1;
		}

		tag_id = get_tag_id(db, tag_name);
		if (tag_id == -1) {
			fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			// rollback the transaction
			if (execute_sql_string(db, "ROLLBACK;")) {
				fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
				return -1;
			}
			return -1;
		}
		if (tag_id == 0) { // tag not found
			if (on_new_tags == AUTO_ADD_TAGS) {
				if ((tag_id = add_new_tag(db, tag_name)) <= 0) {
					fprintf(stderr, "Error when auto-adding a tag with name %s, return code was %ld\n", tag_name, tag_id);
					sqlite3_finalize(stmt);
					// rollback the transaction
					if (execute_sql_string(db, "ROLLBACK;")) {
						fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
						return -1;
					}
					return -1;
				}
			} else {
				sqlite3_finalize(stmt);
				fprintf(stderr, "Error tag with name %s doesn't exist and cannot be auto-added\n", tag_name);
				// rollback the transaction
				if (execute_sql_string(db, "ROLLBACK;")) {
					fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
					return -1;
				}
				return -1;
			}
		}

		rc = sqlite3_bind_int64(stmt, 2, tag_id);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			// rollback the transaction
			if (execute_sql_string(db, "ROLLBACK;")) {
				fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
				return -1;
			}
			return -1;
		}

		rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE) {
			item_tags_added++;
		} else if (rc == SQLITE_CONSTRAINT) { // item already has this tag
			// do nothing
		} else {
			fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			// rollback the transaction
			if (execute_sql_string(db, "ROLLBACK;")) {
				fprintf(stderr, "Error when trying to rollback a transaction: %s\n", sqlite3_errmsg(db));
				return -1;
			}
			return -1;
		}

		tags++;
		tag_name = *tags;
	}

	sqlite3_finalize(stmt);

	// end a transaction
	if (execute_sql_string(db, "END TRANSACTION;")) {
		fprintf(stderr, "Error when trying to end a transaction: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	return item_tags_added > 0 ? 1 : 0;
}

/**
 * Execute sql string
 * @param db SQLite3 database to execute upon
 * @param sql SQL string to execute
 * @return `0` if the string was executed successfully, otherwise `-1` on error
 */
int execute_sql_string(sqlite3 *db, char *sql) {
	char *errmsg;

	if (sqlite3_exec(db, sql, NULL, 0, &errmsg) != SQLITE_OK) {
		fprintf(stderr, "SQL error when executing statement \"%s\": %s\n", sql, errmsg);
		sqlite3_free(errmsg);
		return -1;
	} else {
		return 0;
	}
}

/**
 * Add new listing to the database
 *
 * @param db SQLite3 database to add the listing to, must be initialized
 * @param listingName listing display name, must be unique
 * @param type listing type
 * @param listingPath listing path, must be unique
 * @return `1` if the listing was added, `0` if listing already exists, `-1` on error
 */
int add_new_listing(sqlite3 *db, char *listingName, LISTING_TYPE type, char *listingPath) {
	// check if the path exists and points to a directory
	char *absolutePath = realpath(listingPath, NULL);
	if (absolutePath == NULL) {
		fprintf(stderr, "Path %s does not exist\n", listingPath);
		return -1;
	}

	struct stat s;
	if (stat(absolutePath, &s) == 0) {
		// path exists
		if (!(s.st_mode & S_IFDIR)) {
			fprintf(stderr, "Listing should be a directory!\n");
			return -1;
		}
	} else {
		fprintf(stderr, "Path %s does not exist\n", absolutePath);
		return -1;
	}

	// check that listingName consists only of a-z, A-Z and 0-9 symbols TODO: not required anymore
	size_t i;
	for (i = 0; i < strlen(listingName); i++) {
		 if (!(listingName[i] >= 'a' && listingName[i] <= 'z') &&
			!(listingName[i] >= 'A' && listingName[i] <= 'Z') &&
			!(listingName[i] >= '0' && listingName[i] <= '9')) {
			 fputs("Listing name can only contain letters and numbers\n", stderr);
			 return -1;
		 }
	}

	sqlite3_stmt *stmt;

	// ADDING LISTING TO THE LISTINGS TABLE

	int rc = sqlite3_prepare_v2(db,
		"INSERT INTO " LISTINGS_TABLE_NAME " (listing_name,listing_type,listing_path) VALUES(?,?,?);", -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// NAME
	rc = sqlite3_bind_text(stmt, 1, listingName, -1, NULL); // 1 here means leftmost SQL parameter index
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	//TYPE
	rc = sqlite3_bind_int(stmt, 2, (int)type);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	//PATH
	rc = sqlite3_bind_text(stmt, 3, absolutePath, -1, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		free(absolutePath);
		return 1;
	} else if (rc == SQLITE_CONSTRAINT) { // Listing already exists
		sqlite3_finalize(stmt);
		free(absolutePath);
		return 0;
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		free(absolutePath);
		return -1;
	}
}

//TODO rewrite without using recursion and with an ability to report progress
/**
 * Helper function to recursively refresh a listing
 * @param db SQLite database
 * @param listing_id listing id
 * @param type listing type
 * @param listing_root_path_nbytes length of the listing's root filepath in bytes
 * @param path path to scan
 * @return `0` if the path was scanned successfully, otherwise `-1` on error
 */
int refresh_listing_recursive(sqlite3 *db, int64_t listing_id, LISTING_TYPE type,
							  size_t listing_root_path_nbytes, const char *path) {
	sqlite3_stmt *stmt;
	int rc;
	char name[256], *relpath, *dot, *subdir_path;
	size_t subdir_bytes;
	struct dirent *de;
	DIR *dr = opendir(path);

	if (dr == NULL) {
		fprintf(stderr, "Could not open directory: '%s'\n", path);
		return -1;
	}

	rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO " ITEMS_TABLE_NAME " (item_name, item_relpath, listing_id) VALUES (?,?,?);", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: '%s'\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		closedir(dr);
		return -1;
	}

	while ((de = readdir(dr)) != NULL) {
		if (de->d_type == DT_DIR && (strcmp(de->d_name, "..") == 0 || strcmp(de->d_name, ".") == 0)) continue; // skip .. and . dirs
		// fprintf(stderr, "Checking entry: %s\n", de->d_name);
		if (de->d_type == DT_DIR && type == FILE_AS_ITEM) {
			// scan all subdirs for files
			subdir_bytes = snprintf(NULL, 0, "%s/%s", path, de->d_name) + 1;
			subdir_path = malloc(subdir_bytes);
			if (subdir_path == NULL) {
				sqlite3_finalize(stmt);
				closedir(dr);
				return -1;
			}
			snprintf(subdir_path, subdir_bytes, "%s/%s", path, de->d_name);
			if (refresh_listing_recursive(db, listing_id, type, listing_root_path_nbytes, subdir_path)) {
				free(subdir_path);
				sqlite3_finalize(stmt);
				closedir(dr);
				return -1;
			}
			free(subdir_path);
			continue;
		} else if (de->d_type == DT_REG && type == DIR_AS_ITEM) {
			// skip files when type is DIR_AS_ITEM
			continue;
		}

		// INSERTING

		// get name
		if (de->d_type == DT_DIR) {
			memcpy(name, de->d_name, sizeof(name));
		} else {
			dot = strrchr(de->d_name, '.');
			if (dot == NULL) memcpy(name, de->d_name, sizeof(name));
			else {
				memcpy(name, de->d_name, dot - de->d_name);
				name[dot - de->d_name] = '\0';
			}
		}
		// fprintf(stderr, "Name will be: %s, ", name);

		// get relpath
		relpath = malloc(strlen(path) + strlen(de->d_name) - listing_root_path_nbytes + 2);
		if (relpath == NULL) {
			sqlite3_finalize(stmt);
			closedir(dr);
			return -1;
		}
		memcpy(relpath, path+listing_root_path_nbytes, strlen(path)-listing_root_path_nbytes);
		relpath[strlen(path)-listing_root_path_nbytes] = '/';
		strcpy(relpath+strlen(path)-listing_root_path_nbytes+1, de->d_name);
		// fprintf(stderr, "Relpath will be: %s\n", relpath);

		// reset sql statement
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);

		rc = sqlite3_bind_text(stmt, 1, name, -1, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			free(relpath);
			closedir(dr);
			return -1;
		}

		rc = sqlite3_bind_text(stmt, 2, relpath, -1, NULL);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			free(relpath);
			closedir(dr);
			return -1;
		}

		rc = sqlite3_bind_int64(stmt, 3, listing_id);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			free(relpath);
			closedir(dr);
			return -1;
		}

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			free(relpath);
			closedir(dr);
			return -1;
		}
		free(relpath);
	}

	sqlite3_finalize(stmt);
	closedir(dr);
	return 0;
}

/**
 * Refresh a listing and add new items
 * @param db SQLite database
 * @param listing_id id of the listing to refresh
 * @return `0` if the listing was refreshed successfully, otherwise `-1` on error
 */
int refresh_listing(sqlite3 *db, int64_t listing_id) {
	sqlite3_stmt *stmt;
	char *path;
	LISTING_TYPE type;
	size_t malloc_bytes;

	int rc = sqlite3_prepare_v2(db,
		"SELECT listing_type,listing_path FROM " LISTINGS_TABLE_NAME " WHERE listing_id=? LIMIT 1;", -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_int64(stmt, 1, listing_id);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		type = (LISTING_TYPE) sqlite3_column_int(stmt, 0);

		malloc_bytes = sqlite3_column_bytes(stmt, 1);
		path = malloc(malloc_bytes + 1);
		if (path == NULL) {
			sqlite3_finalize(stmt);
			return -1;
		}
		memcpy(path, sqlite3_column_text(stmt, 1), malloc_bytes);
		path[malloc_bytes] = '\0';
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_finalize(stmt);
	// fprintf(stderr, "type: %d, path: %s\n", type, path);

	if (refresh_listing_recursive(db, listing_id, type, strlen((const char*)path), (const char*)path)) {
		free(path);
		return -1;
	}

	free(path);
	return 0;
}

/**
 * @brief Get the number of items in a listing
 *
 * @param db sqlite database
 * @param listing_id id of the listing
 * @return the number of items in the listing, or `-1` on error
 */
int get_listing_size(sqlite3 *db, int64_t listing_id) {
	sqlite3_stmt *stmt;
	int count = 0;

	int rc = sqlite3_prepare_v2(db,
		"SELECT COUNT(*) FROM " ITEMS_TABLE_NAME " WHERE listing_id=?;", -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	rc = sqlite3_bind_int64(stmt, 1, listing_id);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when binding value with SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	} else {
		fprintf(stderr, "Error when executing SQL query: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_finalize(stmt);
	
	return count;
}

/**
 * Initialize all required tables in the provided database, creates them if they don't exist
 *
 * @param db pointer to SQLite3 database
 * @return `0` if all required tables are initialized,  otherwise `-1` on error
 */
int init_tables(sqlite3 *db) {
	// Creating LISTINGS table
	static const char listings_table_sql[] = "CREATE TABLE IF NOT EXISTS " LISTINGS_TABLE_NAME " ("
							   "listing_id INTEGER PRIMARY KEY NOT NULL,"
							   "listing_name TEXT NOT NULL UNIQUE,"
							   "listing_type INT NOT NULL,"
							   "listing_path TEXT NOT NULL UNIQUE"
							   ")";

	if (!execute_sql_string(db, (char*) listings_table_sql)) {
		fputs("Main Listings table created successfully\n", stderr);
	} else {
		fputs("Main Listings table could not be created\n", stderr);
		return -1;
	}

	// Creating TAGS table
	static const char tags_table_sql[] = "CREATE TABLE IF NOT EXISTS " TAGS_TABLE_NAME " ("
							   "tag_id INTEGER PRIMARY KEY NOT NULL,"
							   "tag_name TEXT NOT NULL UNIQUE"
							   ")";

	if (!execute_sql_string(db, (char*) tags_table_sql)) {
		fputs("Tags table created successfully\n", stderr);
	} else {
		fputs("Tags table could not be created\n", stderr);
		return -1;
	}

	// Creating ITEMS table
	static const char items_table_sql[] = "CREATE TABLE IF NOT EXISTS " ITEMS_TABLE_NAME " ("
							   "item_id INTEGER PRIMARY KEY NOT NULL,"
							   "item_name TEXT NOT NULL UNIQUE,"
							   "item_relpath TEXT NOT NULL UNIQUE,"
							   "listing_id INTEGER NOT NULL,"
							   "FOREIGN KEY (listing_id) REFERENCES " LISTINGS_TABLE_NAME "(listing_id) ON UPDATE CASCADE ON DELETE CASCADE"
							   ")";

	if (!execute_sql_string(db, (char*) items_table_sql)) {
		fputs("Items table created successfully\n", stderr);
	} else {
		fputs("Items table could not be created\n", stderr);
		return -1;
	}
	
	// Creating ITEM_TAGS table
	static const char item_tags_table_sql[] = "CREATE TABLE IF NOT EXISTS " ITEM_TAGS_TABLE_NAME " ("
							   "item_id INTEGER NOT NULL,"
							   "tag_id INTEGER NOT NULL,"
							   "FOREIGN KEY (item_id) REFERENCES " ITEMS_TABLE_NAME "(item_id) ON UPDATE CASCADE ON DELETE CASCADE,"
							   "FOREIGN KEY (tag_id) REFERENCES " TAGS_TABLE_NAME "(tag_id) ON UPDATE CASCADE ON DELETE CASCADE,"
							   "PRIMARY KEY (item_id, tag_id)"
							   ")";

	if (!execute_sql_string(db, (char*) item_tags_table_sql)) {
		fputs("Item_tags table created successfully\n", stderr);
	} else {
		fputs("Item_tags table could not be created\n", stderr);
		return -1;
	}

	return 0;
}

/**
 * Opens SQLite3 database in the specified location and returns the pointer to it
 *
 * @param database_location location of the database to open,
 * if specified as `NULL`, `DATABASE_DEFAULT_LOCATION` is used instead
 * @return pointer to the opened database, otherwise `NULL` on error
 */
sqlite3* open_database(char *database_location) {
	sqlite3 *db;
	char *location = DATABASE_DEFAULT_LOCATION;

	if (database_location != NULL) {
		location = database_location;
	}

	if (sqlite3_open(location, &db)) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

/**
 * Closes the SQLite3 database
 *
 * @param db pointer to the database to close
 */
void close_database(sqlite3 *db) {
	if (db != NULL) {
		sqlite3_close(db);
	}
}
