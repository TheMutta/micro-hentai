#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>
#include <jsmn.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct memory {
	char *response;
	size_t size;
};
 
size_t copy_buffer(char *data, size_t size, size_t nmemb, void *clientp) {
	size_t realsize = size * nmemb;
	struct memory *mem = (struct memory *)clientp;
 
	char *ptr = (char*)realloc(mem->response, mem->size + realsize + 1);
	if(!ptr) {
		return 0;  /* out of memory! */
	}
 
	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;
 
	return realsize;
}

void reset_buffer(struct memory *memory) {
	memset(memory->response, 0, memory->size);
	memory->size = 0;
}

void remote_request(const char *url, struct memory *chunk) {
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);

		/*
		 * If you want to connect to a site who is not using a certificate that is
		 * signed by one of the certs in the CA bundle you have, you can skip the
		 * verification of the server's certificate. This makes the connection
		 * A LOT LESS SECURE.
		 *
		 * If you have a CA cert for the server stored someplace else than in the
		 * default bundle, then the CURLOPT_CAPATH option might come handy for
		 * you.
		 */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		/*
		 * If the site you are connecting to uses a different host name that what
		 * they have mentioned in their server certificate's commonName (or
		 * subjectAltName) fields, libcurl refuses to connect. You can skip this
		 * check, but it makes the connection insg/364591ecure.
		 */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		/* cache the CA cert bundle in memory for a week */
		curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, copy_buffer);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, chunk);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);


		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));
		/* always cleanup */
		curl_easy_cleanup(curl);

	}
}

int main(int argc, char **argv) {
	CURL *curl;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	struct memory chunk = { 0 };

	/*
		search: remote_request("/api/galleries/search?query={QUERY}", &chunk);
		searchLike: remote_request("/api/gallery/{BOOK_ID}/related", &chunk);
		searchTagged: remote_request("/api/galleries/tagged?tag_id={TAG_ID}", &chunk);
		bookDetails: remote_request("/api/gallery/{BOOK_ID}", &chunk);
		getPage: remote_request("/galleries/{MEDIA_ID}/{PAGE}.jpg", &chunk);
		getThumb: remote_request("/galleries/{MEDIA_ID}/{PAGE}t.jpg", &chunk);
		getCover: remote_request("/galleries/{MEDIA_ID}/cover.jp", &chunk);
	*/

	const char *mainHost = "nhentai.net";
	const char *imageHost = "i5.nhentai.net";

	bool isInvalid = false;
	do {
		srand(time(NULL));
		int number = rand() % 999999;

		char request[128] = { '\0' };
		sprintf(request, "https://%s/api/gallery/%d", mainHost, number); // 364591
		reset_buffer(&chunk);
		remote_request(request, &chunk);
		memset(request, 0, 128);

		json data = json::parse(chunk.response);
		if(data["error"] == "does not exist") {
			isInvalid = true;
			continue;
		} else {
			isInvalid = false;
		}

		printf("ID: %s\n", data["id"].dump().c_str());
		printf("Num pages: %s\n", data["num_pages"].dump().c_str());
		printf("Title: %s\n", data["title"]["english"].dump().c_str());
		printf("Tags:\n");
		for (auto tag : data["tags"]) {
			printf(" %s\n", tag["name"].dump().c_str());
		}
	} while(isInvalid);

	free(chunk.response);

	curl_global_cleanup();

	return 0;
}
