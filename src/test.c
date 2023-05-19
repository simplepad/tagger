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

extern int64_t get_tag_id(sqlite3 *db, char *tag_name);
int test_add_tag(sqlite3 *database) {
	if (get_tag_id(database, "tag1") != 0) {
		fputs("Error when getting tag id or tag exists already\n", stderr);
		return -1;
	}

	int64_t tag_id;
	if ((tag_id = add_new_tag(database, "tag1")) <= 0) {
		fputs("Could not add new tag\n", stderr);
		return -1;
	}

	if (get_tag_id(database, "tag1") != tag_id) {
		fputs("Error when getting tag id or tag doesn't exist or wrong tag id\n", stderr);
		return -1;
	}

	return 0;
}

extern int get_item_tags_count(sqlite3 *db, int64_t item_id);
extern int get_total_tags_count(sqlite3 *db);
extern int update_tags(sqlite3 *db, int64_t item_id, char **tags, ON_NEW_TAGS on_new_tags);
int test_update_tags(sqlite3 *database) {
	const int64_t item_id = 1;
	int total_number_of_tags = 1; // tags already in the database: tag1
	const char tag2_name[] = "tag2";
	const char tag3_name[] = "tag3";
	const char tag4_name[] = "tag4";
	const char *tag_names[] = {tag2_name, tag3_name, tag4_name, NULL};
	const char *tag_names2[] = {tag4_name, NULL};

	if (update_tags(database, item_id, (char**) tag_names, DONT_AUTO_ADD_TAGS) != -1) {
		fputs("Error, expected the update_tags() function to return -1\n", stderr);
		return -1;
	}

	if (get_item_tags_count(database, item_id) != 0) {
		fputs("Error, expected the item not to have any tags after a failed update\n", stderr);
		return -1;
	}

	if (get_total_tags_count(database) != total_number_of_tags) {
		fputs("Error, expected the total number of tags not to change after a failed update\n", stderr);
		return -1;
	}

	if (update_tags(database, item_id, (char**) tag_names, AUTO_ADD_TAGS) != 1) {
		fputs("Error, expected the update_tags() function to return 1\n", stderr);
		return -1;
	}
	total_number_of_tags += 3;

	if (get_tag_id(database, (char*) tag3_name) <= 0 || get_tag_id(database, (char*) tag4_name) <= 0) {
		fputs("Error, expected the new tags to exist in the database after item update\n", stderr);
		return -1;
	}

	if (get_item_tags_count(database, item_id) != sizeof(tag_names) / sizeof(char*) - 1) { // -1 for NULL at the end
		fputs("Error, expected the item to have the added tags\n", stderr);
		return -1;
	}

	if (get_total_tags_count(database) != total_number_of_tags) {
		fputs("Error, expected the total number of tags to increase by 3\n", stderr);
		return -1;
	}

	if (update_tags(database, item_id, (char**) tag_names2, AUTO_ADD_TAGS) != 0) {
		fputs("Error, expected the update_tags() function to return 0\n", stderr);
		return -1;
	}

	if (get_item_tags_count(database, item_id) != sizeof(tag_names) / sizeof(char*) - 1) { // -1 for NULL at the end
		fputs("Error, expected the item's number of tags not to change\n", stderr);
		return -1;
	}

	if (get_total_tags_count(database) != total_number_of_tags) {
		fputs("Error, expected the total number of tags not to change\n", stderr);
		return -1;
	}

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

	if (test_add_tag(database)) {
		fputs("add_tag() test failed\n", stderr);
		close_database(database);
		return -1;
	}
	fputs("add_tag() test passed\n", stderr);

	if (test_update_tags(database)) {
		fputs("update_tags() test failed\n", stderr);
		close_database(database);
		return -1;
	}
	fputs("update_tags() test passed\n", stderr);

	close_database(database);

	fputs("All tests passed\n", stderr);
	return 0;
}

