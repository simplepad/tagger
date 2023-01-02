#include <stddef.h>
struct response {
  char *ptr;
  size_t len;
};

void free_response(struct response *r);
struct response* get_response_from_url(char* url);
