#if defined __linux__
    #include "sqlite3.h"
#elif defined __CYGWIN__ || defined _WIN32
    #include "..\windows\sqlite3.h"
#endif

int init_tables(sqlite3 *db);
sqlite3* open_database(char *database_location);
void close_database(sqlite3 *db);