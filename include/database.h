#include "sqlite3.h"
#include <stdint.h>

enum LISTING_TYPE {DIR_AS_ITEM = 0, FILE_AS_ITEM = 1, ANY_AS_ITEM = 2};

int add_new_tag(sqlite3 *db, char *tagName);
int add_new_listing(sqlite3 *db, char *name, enum LISTING_TYPE type, char *path);
int refresh_listing(sqlite3 *db, int64_t listing_id);
int init_tables(sqlite3 *db);
sqlite3* open_database(char *database_location);
void close_database(sqlite3 *db);
