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

/**
 * Check if the table already exists in the database
 *
 * @param db pointer to SQLite3 database
 * @param tableName table name to search for
 * @return `1` if the table exists, `0` if the table does not exist and `-1` on error
 */
int table_exists(sqlite3 *db, char *tableName) {

	if (!db || !tableName) {
		return -1;
	}

	sqlite3_stmt *stmt;

	int rc = sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	// 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_text(stmt, 1, tableName, -1, NULL);
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
		// Table found
		sqlite3_finalize(stmt);
		return 1;
	} else {
		// Table not found
		sqlite3_finalize(stmt);
		return 0;
	}
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
 * @return `1` if the tag was added, `0` if tag already exists, `-1` on error
 */
int add_new_tag(sqlite3 *db, char *tagName) {
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
		return 1;
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
int add_new_listing(sqlite3 *db, char *listingName, enum LISTING_TYPE type, char *listingPath) {
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
int refresh_listing_recursive(sqlite3 *db, int64_t listing_id, enum LISTING_TYPE type,
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
	enum LISTING_TYPE type;
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
		type = (enum LISTING_TYPE) sqlite3_column_int(stmt, 0);

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
 * Initialize all required tables in the provided database, creates them if they don't exist
 *
 * @param db pointer to SQLite3 database
 * @return `0` if all required tables are initialized,  otherwise `-1` on error
 */
int init_tables(sqlite3 *db) {
	// Table with Listings
	if (!table_exists(db, LISTINGS_TABLE_NAME)) {
		fputs("Main Listings table not found, creating new one...\n", stderr);

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
	}

	// Table with tags
	if (!table_exists(db, TAGS_TABLE_NAME)) {
		fputs("Tags table not found, creating new one...\n", stderr);

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
	}

	// Table with items
	if (!table_exists(db, ITEMS_TABLE_NAME)) {
		fputs("Items table not found, creating new one...\n", stderr);

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
	}
	
	// Table with items to tags mapping
	if (!table_exists(db, ITEM_TAGS_TABLE_NAME)) {
		fputs("Item_tags table not found, creating new one...\n", stderr);

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
