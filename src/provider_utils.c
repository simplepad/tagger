#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../include/provider_utils.h"

struct response* init_response() {
	struct response* r;
	r = malloc(sizeof(struct response));
	if (r == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}

	r->len = 0;

	r->ptr = malloc(r->len+1);
	if (r->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	r->ptr[0] = '\0';

	return r;
}

void free_response(struct response *r) {
	if (r == NULL) return;
	
	if (r->ptr != NULL) {
		free(r->ptr);
		r->ptr = NULL;
	}

	free(r);
	r = NULL;
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct response *s) {
	size_t new_len = s->len + size*nmemb;
	s->ptr = realloc(s->ptr, new_len+1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr+s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

/**
 * @brief Get response from url
 *
 * @param url url to get the response from
 * @return a pointer to a response struct, or `NULL` on error
 */
struct response* get_response_from_url(char* url) {
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();

	if(curl) {
		struct response* response = init_response();

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
		res = curl_easy_perform(curl);

		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			free_response(response);
		}

		curl_easy_cleanup(curl);

		return response;
	}

	return NULL;
}
