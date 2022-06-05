#if defined __linux__
    #include "sqlite3.h"
#elif defined __CYGWIN__ || defined _WIN32
    #include "..\windows\sqlite3.h"
#endif

int add_new_tag(sqlite3 *db, char *tagName);
int init_tables(sqlite3 *db);
sqlite3* open_database(char *database_location);
void close_database(sqlite3 *db);
