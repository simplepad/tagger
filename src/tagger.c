#include "../include/tagger.h"

/**
 * Main function
 *
 * @param argc argument count
 * @param argv arguments vector
 * @return
 */
int main(void) {
	sqlite3 *database;
	database = open_database(NULL);

	if (database == NULL) {
		fputs("Could not open database\n", stderr);
		return -1;
	}

	if (init_tables(database)) {
		// something went wrong when initializing tables
		fputs("Could not initialize tables\n", stderr);
		close_database(database);
		return -1;
	}

	close_database(database);
	return 0;
}
