#include "sqlite3.h"
#include <stdint.h>

typedef enum {DIR_AS_ITEM = 0, FILE_AS_ITEM = 1, ANY_AS_ITEM = 2} LISTING_TYPE;
typedef enum {AUTO_ADD_TAGS, DONT_AUTO_ADD_TAGS} ON_NEW_TAGS;

sqlite3_int64 add_new_tag(sqlite3 *db, char *tagName);
int get_item_tag_ids(sqlite3 *db, sqlite3_int64 item_id, int *tags_array_size, sqlite3_int64 **tags_array);
int update_tags(sqlite3 *db, sqlite3_int64 item_id, char **tags, ON_NEW_TAGS on_new_tags);
int add_new_listing(sqlite3 *db, char *name, LISTING_TYPE type, char *path);
int refresh_listing(sqlite3 *db, sqlite3_int64 listing_id);
int get_listing_size(sqlite3 *db, sqlite3_int64 listing_id);
int init_tables(sqlite3 *db);
sqlite3* open_database(char *database_location);
void close_database(sqlite3 *db);
