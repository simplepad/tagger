#include "../include/tagger.h"

int main(int argc, char **argv) {
	sqlite3 *database;
	database = open_database(NULL);

	if (database == NULL) {
		fputs("Could not open database\n", stderr);
		return -1;
	}

	// do something
	if (init_tables(database)) {
		// something went wrong when initializing tables
		fputs("Could not initialize tables\n", stderr);
		return -1;
	}

	close_database(database);
	return 0;
}