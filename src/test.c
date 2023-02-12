#include <stdio.h>
#include "../include/database.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int test_listing_refresh(sqlite3 *database) {
	// Testing listing addition
	char pattern[] = "/tmp/tmp.XXXXXX";
	char* temp_dir = mkdtemp(pattern);
	if (temp_dir == NULL) {
		fputs("Could not create a temp directory\n", stderr);
		return -1;
	}
	fprintf(stderr, "Using temp directory: %s\n", temp_dir);

	char dir1[sizeof(pattern)+4];
	sprintf(dir1, "%s/d1", temp_dir);
	mkdir(dir1, 0700);
	
	char files[5][sizeof(pattern)+7];
	FILE* f;
	for (size_t i = 0; i<5; i++) {
		if (i < 3) {
			sprintf(files[i], "%s/f%zu", temp_dir, i);
		} else {
			sprintf(files[i], "%s/d1/f%zu", temp_dir, i);
		}
	}
	for (size_t i = 0; i<5; i++) {
		f = fopen(files[i], "w");
		if (f == NULL) {
			fprintf(stderr, "Could not create temp file %s\n", files[i]);
			return -1;
		}
		fclose(f);
	}

	if (add_new_listing(database, "test", FILE_AS_ITEM, temp_dir) != 1) {
		fputs("Could not add a FILE_AS_ITEM listing\n", stderr);
		return -1;
	}

	if (get_listing_size(database, 1) != 0) {
		fputs("Listing should be empty!\n", stderr);
		return -1;
	}

	if (refresh_listing(database, 1) != 0) {
		fputs("Could not refresh a FILE_AS_ITEM listing\n", stderr);
		return -1;
	}

	if (get_listing_size(database, 1) != 5) {
		fputs("Listing has wrong size!\n", stderr);
		return -1;
	}
	
	if (refresh_listing(database, 1) != 0) {
		fputs("Could not refresh a FILE_AS_ITEM listing the second time\n", stderr);
		return -1;
	}

	if (get_listing_size(database, 1) != 5) {
		fputs("Listing has wrong size after the second refresh!\n", stderr);
		return -1;
	}

	// TODO test DIR_AS_ITEM

	// Removing temp files
	for (size_t i = 0; i<5; i++) {
		remove(files[i]);
	}
	remove(dir1);
	remove(temp_dir);

	return 0;
}

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
	fputs("Tables init test passed\n", stderr);

	if (test_listing_refresh(database)) {
		fputs("Listings tests failed\n", stderr);
		close_database(database);
		return -1;
	}
	fputs("Listings tests passed\n", stderr);

	close_database(database);

	fputs("All tests passed\n", stderr);
	return 0;
}

