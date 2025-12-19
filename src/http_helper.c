#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "http_helper.h"

/* Global variables ***********************************************************/
/* Function prototypes ********************************************************/

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata);

/* Callbacks ******************************************************************/
/* Function definitions *******************************************************/

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	size_t realsize = size * nmemb; 
	http_get_req_t* req = (http_get_req_t*) userdata;

	while (req->buflen < req->len + realsize + 1) {
		req->buffer = realloc(req->buffer, req->buflen + CHUNK_SIZE);
		req->buflen += CHUNK_SIZE;
	}
	memcpy(&req->buffer[req->len], ptr, realsize);
	req->len += realsize;
	req->buffer[req->len] = 0;

	return realsize;
}

/* Public functions ***********************************************************/

int http_helper_get(http_get_req_t* req, char* url)
{
	int err = 0;

	CURL* curl;
	CURLcode res;

	req->buffer = NULL;
	req->len = 0;
	req->buflen = 0;

	curl = curl_easy_init();

	if (curl) {
		struct curl_slist* headers = NULL;
		headers = curl_slist_append(headers, "accept: application/json");
		req->buffer = malloc(CHUNK_SIZE);
		req->buflen = CHUNK_SIZE;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)req);

		res = curl_easy_perform(curl);
		int retcode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
		if (res != CURLE_OK) {
			err = 1;
		}
	}
	curl_easy_cleanup(curl);

	return err;
}

int http_helper_post(http_get_req_t* req, char* url, char* post_data)
{
	int err = 0;

	CURL* curl;
	CURLcode res;

	req->buffer = NULL;
	req->len = 0;
	req->buflen = 0;

	curl = curl_easy_init();

	if (curl) {
		struct curl_slist* headers = NULL;
		headers = curl_slist_append(headers, "accept: application/json");
		if (post_data) {
			headers = curl_slist_append(headers, "Content-Type: application/json");
		}
		req->buffer = malloc(CHUNK_SIZE);
		req->buflen = CHUNK_SIZE;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)req);
		if (post_data) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
		}

		res = curl_easy_perform(curl);
		int retcode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
		if (res != CURLE_OK) {
			err = 1;
		}
	}
	curl_easy_cleanup(curl);

	return err;
}

int http_helper_put(http_get_req_t* req, char* url, char* put_data)
{
	int err = 0;

	CURL* curl;
	CURLcode res;

	req->buffer = NULL;
	req->len = 0;
	req->buflen = 0;

	curl = curl_easy_init();

	if (curl) {
		struct curl_slist* headers = NULL;
		headers = curl_slist_append(headers, "accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		req->buffer = malloc(CHUNK_SIZE);
		req->buflen = CHUNK_SIZE;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, put_data);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)req);

		res = curl_easy_perform(curl);
		int retcode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
		if (res != CURLE_OK) {
			err = 1;
		}
	}
	curl_easy_cleanup(curl);

	return err;
}

void http_helper_free(http_get_req_t* req)
{
	free(req->buffer);
}
