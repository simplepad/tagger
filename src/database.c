#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/database.h"

#define LISTINGS_TABLE_NAME "listings"
#define TAGS_TABLE_NAME "tags"
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

	sqlite3_finalize(stmt);
	return -1;
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

    int rc = sqlite3_prepare_v2(db, "SELECT 1 FROM " TAGS_TABLE_NAME " WHERE type='table' AND name=?;", -1, &stmt, NULL);
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
        sqlite3_finalize(stmt);
        return 1;
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

    int rc = sqlite3_prepare_v2(db, "INSERT INTO " TAGS_TABLE_NAME " (name) VALUES(?);", -1, &stmt, NULL);
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
 * @param listingPath listing path (can be relative), must be unique
 * @return `1` if the listing was added, `0` if listing already exists, `-1` on error
 */
int add_new_listing(sqlite3 *db, char *listingName, enum LISTING_TYPE type, char *listingPath) {
    // check if the path exists and points to a directory
    struct stat s;
    if (stat(listingPath, &s) == 0) {
        // path exists
        if (!(s.st_mode & S_IFDIR)) {
            fprintf(stderr, "Listing should be a directory!\n");
            return -1;
        }
    } else {
        fprintf(stderr, "Path %s does not exist\n", listingPath);
        return -1;
    }

    // check that listingName consists only of a-z, A-Z and 0-9 symbols
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
        "INSERT INTO " LISTINGS_TABLE_NAME " (name,type,path) VALUES(?,?,?);", -1, &stmt, NULL);

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
    char *absolutePath = realpath(listingPath, NULL);
    if (absolutePath == NULL) {
        fprintf(stderr, "Error when generating absolute path of %s\n", listingPath);
        sqlite3_finalize(stmt);
        return -1;
    }

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

    // CREATING A NEW TABLE FOR THE LISTING
    char * table_sql =  "CREATE TABLE %s ("
                        "id INTEGER PRIMARY KEY NOT NULL,"
                        "name TEXT NOT NULL,"
                        "relpath TEXT NOT NULL UNIQUE,"
                        "tags TEXT"
                        ")";
    size_t bytes_to_alloc = snprintf(NULL, 0, table_sql, listingName) + 1;
    char * sql = malloc(bytes_to_alloc);
    snprintf(sql, bytes_to_alloc, table_sql, listingName);

    if (execute_sql_string(db, sql)) {
        free(sql);
        return -1;
    }

    free(sql);
    return 1;
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
        char * listings_table_sql = "CREATE TABLE " LISTINGS_TABLE_NAME " ("
                                   "id INTEGER PRIMARY KEY NOT NULL,"
                                   "name TEXT NOT NULL UNIQUE,"
                                   "type INT NOT NULL,"
                                   "path TEXT NOT NULL UNIQUE"
                                   ")";

        if (!execute_sql_string(db, listings_table_sql)) {
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
        char * tags_table_sql = "CREATE TABLE " TAGS_TABLE_NAME " ("
                                   "id INTEGER PRIMARY KEY NOT NULL,"
                                   "name TEXT NOT NULL UNIQUE"
                                   ")";

        if (!execute_sql_string(db, tags_table_sql)) {
            fputs("Tags table created successfully\n", stderr);
        } else {
            fputs("Tags table could not be created\n", stderr);
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
