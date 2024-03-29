#include <stdio.h>
#include "../include/database.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char * sql_expand_param_into_array(char *sql, size_t param_num, size_t array_size);
int test_sql_expand_param_into_array(void) {
	static const char unexpanded[] = "SELECT * FROM TEST WHERE COL1 = ? AND COL2 IN (?)";
	static const char expanded[] = "SELECT * FROM TEST WHERE COL1 = ? AND COL2 IN (?,?,?)";

	char * result = sql_expand_param_into_array((char*)unexpanded, 2, 3);

	if (result == NULL) {
		fputs("Error, expected the return value to be a valid pointer\n", stderr);
		return -1;
	}

	if (strncmp(expanded, result, sizeof(expanded))) {
		fputs("Error, expected the return string to be equal to the expanded string\n", stderr);
		fprintf(stderr, "Expected: %s\n", expanded);
		fprintf(stderr, "Received: %s\n", result);
		return -1;
	}

	free(result);

	return 0;
}

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

extern sqlite3_int64 get_tag_id(sqlite3 *db, char *tag_name);
int test_add_tag(sqlite3 *database) {
	if (get_tag_id(database, "tag1") != 0) {
		fputs("Error when getting tag id or tag exists already\n", stderr);
		return -1;
	}

	sqlite3_int64 tag_id;
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

extern int get_item_tags_count(sqlite3 *db, sqlite3_int64 item_id);
extern int get_total_tags_count(sqlite3 *db);
int test_update_tags(sqlite3 *database) {
	const sqlite3_int64 item_id = 1;
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

int test_get_item_tag_ids(sqlite3 *database) {
	const sqlite3_int64 item_id = 1;
	const int item_tag_ids[] = {2, 3, 4};
	const int item_tag_ids_size = sizeof(item_tag_ids) / sizeof(int);

	sqlite3_int64 *tags_array;
	int tags_array_size;

	// item with id item_id already has tags with ids item_tag_ids

	if (!get_item_tag_ids(database, 5, &tags_array_size, &tags_array)) {
		fputs("Error, expected an error when getting tag ids for a non-existing item\n", stderr);
		return -1;
	}

	if (get_item_tag_ids(database, item_id, &tags_array_size, &tags_array)) {
		fputs("Error, expected the function to successfully retrieve tag ids\n", stderr);
		return -1;
	}

	if (tags_array_size != item_tag_ids_size) {
		fprintf(stderr, "Error, expected the item to have %d tags\n", item_tag_ids_size);
		return -1;
	}

	int tag_found;
	for (int i = 0; i < item_tag_ids_size; i++) {
		tag_found = 0;
		for (int j = 0; j < tags_array_size; j++) {
			if (item_tag_ids[i] == tags_array[j]) tag_found = 1;
		}

		if (!tag_found) {
			fprintf(stderr, "Error, could not found tag with id %d in returned item tags\n", item_tag_ids[i]);
			free(tags_array);
			return -1;
		}
	}

	free(tags_array);
	return 0;
}

int test_add_tag_to_item(sqlite3 *database) {
	const sqlite3_int64 item_id = 2;
	const sqlite3_int64 tag_id = 1;

	int tags_array_size;
	sqlite3_int64 *tags_array;


	if (get_item_tags_count(database, item_id) != 0) {
		fprintf(stderr, "Expected the item with id %lld not to have any tags\n", item_id);
		return -1;
	}

	if (add_tag_to_item(database, item_id, tag_id) != 1) {
		fprintf(stderr, "Expected the tag to be successfully added to the item\n");
		return -1;
	}

	if (get_item_tags_count(database, item_id) != 1) {
		fprintf(stderr, "Expected the item with id %lld to have 1 tag\n", item_id);
		return -1;
	}

	if (get_item_tag_ids(database, item_id, &tags_array_size, &tags_array) != 0) {
		fprintf(stderr, "Expected the item tags to be fetched successfully\n");
		return -1;
	}

	if (tags_array_size != 1 || tags_array[0] != tag_id) {
		fprintf(stderr, "Expected the item with id %lld to have 1 tag with id %lld\n", item_id, tag_id);
		free(tags_array);
		return -1;
	}

	free(tags_array);
	return 0;
}


int main(void) {
	// testing helper functions
	
	if (test_sql_expand_param_into_array()) {
		fputs("sql_expand_param_into_array test failed!\n", stderr);
		return -1;
	}

	// testing database functions
	
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

	if (test_get_item_tag_ids(database)) {
		fputs("get_item_tag_ids() test failed\n", stderr);
		close_database(database);
		return -1;
	}
	fputs("get_item_tag_ids() test passed\n", stderr);

	if (test_add_tag_to_item(database)) {
		fputs("add_tag_to_item() test failed\n", stderr);
		close_database(database);
		return -1;
	}
	fputs("add_tag_to_item() test passed\n", stderr);

	close_database(database);

	fputs("----- All tests passed -----\n", stderr);
	return 0;
}

