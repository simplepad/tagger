#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Platform dependent includes
#if defined __linux__
	#include "../include/database.h"
#elif defined __CYGWIN__ || defined _WIN32
	#include "..\include\database.h"
#endif

#define LISTINGS_TABLE_NAME "LISTINGS"
#define TAGS_TABLE_NAME "TAGS"
#define DATABASE_DEFAULT_LOCATION "test.tdb"

enum COLUMN_NULL_FLAG {CAN_BE_NULL, CANNOT_BE_NULL};

struct table_template {
	char *table_name;
	size_t number_of_columns;
	char **column_names;
	char **column_types;
	enum COLUMN_NULL_FLAG *column_null_flag;
};

/**
 * Frees allocated memory of `table_template' struct, even if not fully allocated
 *
 * @param table pointer to `table_template` to free memory from
 */
void free_table_template(struct table_template *table) {
    if (table == NULL){
        return;
    }

    size_t i;

    if (table->column_types != NULL) {
        for (i=0; i<table->number_of_columns; i++) {
            free(table->column_types[i]);
        }

        free(table->column_types);
    }

    if (table->column_names != NULL) {
        for (i=0; i<table->number_of_columns; i++) {
            free(table->column_names[i]);
        }

        free(table->column_names);
    }

    if (table->column_null_flag != NULL) {
        free(table->column_null_flag);
    }

    if (table->table_name != NULL) {
        free(table->table_name);
    }

    free(table);
    table = NULL; // prevent use after free?
}

/**
 * Creates table template with the name `name` and number of columns `number_of_columns`
 *
 * @param name table name
 * @param number_of_columns number of user-defined columns in the table, not counting PRIMARY KEY column
 * @return either fully allocated `table_template` or `NULL` on error
 */
struct table_template* create_table_template(char *name, size_t number_of_columns) {

    if (name == NULL) {
        fputs("Table name is NULL\n", stderr);
        return NULL;
    }

	struct table_template *table = NULL;
	table = malloc(sizeof(struct table_template));

    if (table == NULL) {
        fputs("Could not allocate memory for table template\n", stderr);
        return NULL;
    }

    table->table_name = NULL;
    table->number_of_columns = number_of_columns;
    table->column_names = NULL;
    table->column_types = NULL;
    table->column_null_flag = NULL;

	table->table_name = malloc(strlen(name) * sizeof(char) + 1);
    if (table->table_name == NULL) {
        free_table_template(table);
        return NULL;
    }
	memcpy(table->table_name, name, strlen(name) * sizeof(char) + 1);

	table->number_of_columns = number_of_columns;
	table->column_names = malloc(number_of_columns * sizeof(char*));
    if (table->column_names == NULL) {
        free_table_template(table);
        return NULL;
    }
	table->column_types = malloc(number_of_columns * sizeof(char*));
    if (table->column_types == NULL) {
        free_table_template(table);
        return NULL;
    }
	table->column_null_flag = malloc(number_of_columns * sizeof(enum COLUMN_NULL_FLAG));
    if (table->column_null_flag == NULL) {
        free_table_template(table);
        return NULL;
    }

	return table;
}

/**
 * Tries to add a column to the table template
 *
 * @param table pointer to the `table_template` to add a column to
 * @param column_number number of the column to add, starting from 0
 * @param column_type SQL type of column to add
 * @param column_name name of the column to add
 * @param column_null_flag whether or not to set column as `NOT NULL`
 */
void add_column_to_table_template(struct table_template *table, size_t column_number, char *column_type,
        char *column_name, enum COLUMN_NULL_FLAG column_null_flag) {
	if (table == NULL || column_type == NULL || column_name == NULL) {
        return;
    }

    if (column_number >= table->number_of_columns) {
		fputs("Bad column number\n", stderr);
		return;
	}

	table->column_types[column_number] = malloc(sizeof(char) * (strlen(column_type) + 1)); // 1 for \0
    if (table->column_types[column_number] == NULL) {
        fputs("Could not allocate memory\n", stderr);
        return;
    }
	memcpy(table->column_types[column_number], column_type, sizeof(char) * (strlen(column_type) + 1));

	table->column_names[column_number] = malloc(sizeof(char) * (strlen(column_name) + 1)); // 1 for \0
    if (table->column_names[column_number] == NULL) {
        fputs("Could not allocate memory\n", stderr);
        return;
    }
	memcpy(table->column_names[column_number], column_name, sizeof(char) * (strlen(column_name) + 1));

	table->column_null_flag[column_number] = column_null_flag;
}

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

	int rc = sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

    // 1 here means leftmost SQL parameter index
	rc = sqlite3_bind_text(stmt, 1, tableName, strlen(tableName), NULL);
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
 * Creates a table from table template and adds it to the database
 *
 * @param db the database to add the new table to
 * @param table table_template containing the information about new table
 * @return `0` if new table was created and added successfully, otherwise `-1` on error
 */
int create_table_from_template(sqlite3 *db, struct table_template *table) {

	if (table == NULL || table_exists(db, table->table_name)) {
		return -1; //cannot create the table if it already exists
	}

	char *statement_head1 = "CREATE TABLE ";
	char *statement_head2 = " (ID INT PRIMARY KEY NOT NULL,";
	char *statement_tail = ");";
	char *statement_full;
	char *errmsg;
	size_t statement_length = strlen(statement_head1) * sizeof(char) + strlen(table->table_name) * sizeof(char) +
            strlen(statement_head2) * sizeof(char) + strlen(statement_tail) * sizeof(char) + 1; // 1 for \0
	size_t i;
	size_t offset;

	// calculating statement_length
	for (i=0; i<table->number_of_columns; i++) {
        statement_length += strlen(table->column_types[i]) * sizeof(char);
        statement_length += 1; // whitespace
		statement_length += strlen(table->column_names[i]) * sizeof(char);
		if (table->column_null_flag[i] == CANNOT_BE_NULL) {
            statement_length += 1 + strlen("NOT NULL") * sizeof(char); // 1 for whitespace
		}
        statement_length += 1; // 1 for comma
	}
	statement_length--; // remove last comma

	// allocating space for statement
	statement_full = malloc(statement_length * sizeof(char));
	if (!statement_full) {
		fprintf(stderr, "Error allocating memory for statement buffer\n");
		return -1;
	}

	// creating statement
	offset = 0;

	memcpy(statement_full, statement_head1, strlen(statement_head1) * sizeof(char));
	offset += strlen(statement_head1) * sizeof(char);
	memcpy(statement_full+offset, table->table_name, strlen(table->table_name) * sizeof(char));
	offset += strlen(table->table_name) * sizeof(char);
	memcpy(statement_full+offset, statement_head2, strlen(statement_head2) * sizeof(char));
	offset += strlen(statement_head2) * sizeof(char);


	for (i=0; i<table->number_of_columns; i++) {
		memcpy(statement_full+offset, table->column_names[i], strlen(table->column_names[i]) * sizeof(char));
		offset += strlen(table->column_names[i]) * sizeof(char);
		statement_full[offset] = ' ';
		offset += 1; // 1 for whitespace
		memcpy(statement_full+offset, table->column_types[i], strlen(table->column_types[i]) * sizeof(char));
		offset += strlen(table->column_types[i]) * sizeof(char);
		if (table->column_null_flag[i] == CANNOT_BE_NULL) {
			statement_full[offset] = ' ';
			offset += 1; // 1 for whitespace
			memcpy(statement_full+offset, "NOT NULL", strlen("NOT NULL") * sizeof(char));
			offset += strlen("NOT NULL") * sizeof(char);
		}
		statement_full[offset] = ',';
		offset += 1; // 1 for comma
	}
	offset--; // remove last comma
	memcpy(statement_full+offset, statement_tail, strlen(statement_tail) * sizeof(char));
	offset += strlen(statement_tail) * sizeof(char);
	statement_full[offset] = '\0';

	//fprintf(stderr, "Resulting SQL Statement: %s\n", statement_full);
	if (sqlite3_exec(db, statement_full, NULL, 0, &errmsg) != SQLITE_OK) {
		fprintf(stderr, "SQL error when trying to create new table from table_template: %s\n", errmsg);
		sqlite3_free(errmsg);
		free(statement_full);
		return -1;
	} else {
		free(statement_full);
		return 0;
	}
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
		struct table_template *listings_table = create_table_template(LISTINGS_TABLE_NAME, 3);
        if (listings_table == NULL) {
            fputs("Could not create table_template for listings table\n", stderr);
            return -1;
        }

		add_column_to_table_template(listings_table, 0, "TEXT", "NAME", CANNOT_BE_NULL);
		add_column_to_table_template(listings_table, 1, "TEXT", "LINUX_PATH", CAN_BE_NULL);
		add_column_to_table_template(listings_table, 2, "TEXT", "WINDOWS_PATH", CAN_BE_NULL);
		if (!create_table_from_template(db, listings_table)) {
			fputs("Main Listings table created successfully\n", stderr);
			free_table_template(listings_table);
		} else {
			fputs("Main Listings table could not be created\n", stderr);
			free_table_template(listings_table);
			return -1;
		}
	}

	// Table with tags
	if (!table_exists(db, TAGS_TABLE_NAME)) {
		fputs("Tags table not found, creating new one...\n", stderr);
		// Creating TAGS table
		struct table_template *tags_table = create_table_template(TAGS_TABLE_NAME, 1);
        if (tags_table == NULL) {
            fputs("Could not create table_template for tags table\n", stderr);
            return -1;
        }

		add_column_to_table_template(tags_table, 0, "TEXT", "NAME", CANNOT_BE_NULL);
		if (!create_table_from_template(db, tags_table)) {
			fputs("Tags table created successfully\n", stderr);
			free_table_template(tags_table);
		} else {
			fputs("Tags table could not be created\n", stderr);
			free_table_template(tags_table);
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
