#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

#include <stdlib.h>

#define CHUNK_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_get_req_t {
	char* buffer;
	size_t len;
	size_t buflen;
} http_get_req_t;

int http_helper_get(http_get_req_t* req,  char* url);
int http_helper_post(http_get_req_t* req, char* url, char* post_data);
int http_helper_put(http_get_req_t* req, char* url, char* put_data);
void http_helper_free(http_get_req_t* req);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HTTP_HELPER_H */