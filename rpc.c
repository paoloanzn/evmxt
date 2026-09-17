#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#include "rpc.h"

#define BODY_BUF_SIZE 512

struct callback_buf {
    char *data;
    size_t len;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct callback_buf *buf = userp;
    char *tmp = realloc(buf->data, buf->len + total + 1);
    if (!tmp) return 0;

    buf->data = tmp;
    // Here buf->len is the offset to where we wrote
    // the last element in buf->data; 1st iteration is 0.
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

// Sends a JSON-RPC request; return the result string.
// The caller must free() the returned string.
char *rpc_call(const char *url, const char *method, const char *params)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char body_buf[BODY_BUF_SIZE];
    snprintf(body_buf, sizeof(body_buf), 
        "{\"jsonrpc\": \"2.0\", \"method\": \"%s\", \
        \"params\": %s, \"id\": 1}", method, params);

    struct callback_buf buf = {.data = NULL, .len = 0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, 
        "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_buf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    CURLcode return_code = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (return_code != CURLE_OK || !buf.data) {
        free(buf.data);
        return NULL;
    }

    cJSON *json = cJSON_Parse(buf.data);
    free(buf.data);
    if (!json) return NULL;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    char *out = NULL;
    if (result && cJSON_IsString(result)) { 
        out = strdup(result->valuestring);
    }

    cJSON_Delete(json);
    return out;
}
