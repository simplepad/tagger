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

struct table_template {
	char *table_name;
	size_t number_of_columns;
	char **column_names;
	char **column_types;
	int *column_can_be_null;
};

// Creates table template with the name `name` and number of columns `number_of_columns`
struct table_template* create_table_template(char *name, size_t number_of_columns) {
	struct table_template *table;
	table = malloc(sizeof(struct table_template));

	table->table_name = malloc(strlen(name) * sizeof(char) + 1);
	memcpy(table->table_name, name, strlen(name) * sizeof(char) + 1);

	table->number_of_columns = number_of_columns;
	table->column_names = malloc(number_of_columns * sizeof(char*));
	table->column_types = malloc(number_of_columns * sizeof(char*));
	table->column_can_be_null = malloc(number_of_columns * sizeof(int));
	return table;
}

// Frees the memory occupied by `table`
void free_table_template(struct table_template *table) {
	size_t i;
	for (i=0; i<table->number_of_columns; i++) {
		free(table->column_types[i]);
		free(table->column_names[i]);
	}

	free(table->column_can_be_null);
	free(table->table_name);
	free(table);
	table = NULL; // prevent use after free?
}

// Tries to add a column to the table template `table` with the id `column_number`,
// type `column_type`, name `column_name` and flag `column_can_be_null` set to `0` if column can be NULL, or other value if it cannot be NULL
void add_column_to_table_template(struct table_template *table, size_t column_number, char *column_type, char *column_name, int column_can_be_null) {
	if (column_number >= table->number_of_columns) {
		fputs("Bad column number\n", stderr);
		return;
	}

	table->column_types[column_number] = malloc(sizeof(char) * (strlen(column_type) + 1)); // 1 for \0
	memcpy(table->column_types[column_number], column_type, sizeof(char) * (strlen(column_type)));

	table->column_names[column_number] = malloc(sizeof(char) * (strlen(column_name) + 1)); // 1 for \0
	memcpy(table->column_names[column_number], column_name, sizeof(char) * (strlen(column_name)));

	table->column_can_be_null[column_number] = column_can_be_null;
}

// Returns `1` if table does exist, returns `0` if table does not exist, otherwise returns `-1` on error
int table_exists(sqlite3 *db, char *tableName) {
	sqlite3_stmt *stmt;

	int rc = sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error when preparing SQL query: %s\n", sqlite3_errmsg(db));
		return -1;
	}

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

// Creates table from table template `table` inside the database `db`,
// returns `0` if the table was created successfully, otherwise returns `-1` on error
int create_table_from_template(sqlite3 *db, struct table_template *table) {

	if (table_exists(db, table->table_name)) {
		return -1; //cannot create the table if it already exists
	}

	char *statement_head1 = "CREATE TABLE ";
	char *statement_head2 = " (ID INT PRIMARY KEY NOT NULL,";
	char *statement_tail = ");";
	char *statement_full;
	char *errmsg;
	size_t statement_lenght = strlen(statement_head1) * sizeof(char) + strlen(table->table_name) * sizeof(char) + strlen(statement_head2) * sizeof(char)  + strlen(statement_tail) * sizeof(char) + 1; // 1 for \0
	size_t i;
	int offset;

	// calculating statement_lenght
	for (i=0; i<table->number_of_columns; i++) {
		statement_lenght += strlen(table->column_types[i]) * sizeof(char);
		statement_lenght += 1; // blankspace
		statement_lenght += strlen(table->column_names[i]) * sizeof(char);
		if (!table->column_can_be_null[i]) {
			statement_lenght += 1 + strlen("NOT NULL") * sizeof(char); // 1 for blankspace
		}
		statement_lenght += 1; // 1 for comma
	}
	statement_lenght--; // remove last comma

	// allocating space for statement
	statement_full = malloc(statement_lenght * sizeof(char));
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
		offset += 1; // 1 for blankspace
		memcpy(statement_full+offset, table->column_types[i], strlen(table->column_types[i]) * sizeof(char));
		offset += strlen(table->column_types[i]) * sizeof(char);
		if (!table->column_can_be_null[i]) {
			statement_full[offset] = ' ';
			offset += 1; // 1 for blankspace
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
		fprintf(stderr, "SQL error when trying to create : %s\n", errmsg);
		sqlite3_free(errmsg);
		free(statement_full);
		return -1;
	} else {
		free(statement_full);
		return 0;
	}
}

// Make sure that all required tables exist in the database,
// returns `0` if all tables are initialized and present in the database,
// otherwise returns `-1` on error
int init_tables(sqlite3 *db) {
	// Table with Listings
	if (!table_exists(db, LISTINGS_TABLE_NAME)) {
		fputs("Main Listings table not found, creating new one...\n", stderr);
		// Creating LISTINGS table
		struct table_template *listings_table = create_table_template(LISTINGS_TABLE_NAME, 3);
		add_column_to_table_template(listings_table, 0, "TEXT", "NAME", 0);
		add_column_to_table_template(listings_table, 1, "TEXT", "LINUX_PATH", 1);
		add_column_to_table_template(listings_table, 2, "TEXT", "WINDOWS_PATH", 1);
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
		add_column_to_table_template(tags_table, 0, "TEXT", "NAME", 0);
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

// Opens the database in the location `database_location` if not NULL,
// otherwise opens the database in the location DATABASE_DEFAULT_LOCATION,
// returns a pointer to the opened database
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

// Closes the database
void close_database(sqlite3 *db) {
	sqlite3_close(db);
}