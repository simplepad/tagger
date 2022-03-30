#include "sqlite3.h"

int doesTableExist(sqlite3 *db, char *tableName);
int init_tables(sqlite3 *db);
sqlite3* open_database(char *database_location);
void close_database(sqlite3 *db);