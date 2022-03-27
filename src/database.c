#include <stdio.h>
#include "sqlite3.h"
#include "../include/database.h"


int test_database() {
	char *zErrMsg = 0;
	char *create_tables_sql;
	sqlite3 *db;
	
	if (sqlite3_open("test.db", &db)) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		return 1;
	}
	
	fprintf(stdout, "Opened database successfully!\n");
	
	fprintf(stdout, "Creating table\n");
	create_tables_sql = "CREATE TABLE LISTINGS("  \
      "ID INT PRIMARY KEY     NOT NULL," \
      "NAME           TEXT    NOT NULL," \
      "LINUX_PATH     TEXT," \
      "WINDOWS_PATH   TEXT );";
	
	if (sqlite3_exec(db, create_tables_sql, NULL, 0, &zErrMsg) != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	} else {
		fprintf(stdout, "Table created successfully\n");
	}
	 
	sqlite3_close(db);
	return 0;
}

int init_tables() {
	
}