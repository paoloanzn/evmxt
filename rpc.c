#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#include "rpc.h"

struct callback_buf {
    char *data;
    size_t len;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    struct callback_buf *buf = userp;
    if (size && nmemb > SIZE_MAX / size) return 0;
    size_t total = size * nmemb;
    if (total > SIZE_MAX - buf->len - 1) return 0;
    char *tmp = realloc(buf->data, buf->len + total + 1);
    if (!tmp) return 0;

    buf->data = tmp;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

// Serialize the request dynamically, including arbitrary-length calldata.
static char *compose_json_rpc_request(const char *method, const char *params)
{
    cJSON *request = cJSON_CreateObject();
    cJSON *arguments = cJSON_Parse(params);
    char *body = NULL;
    if (!request || !cJSON_IsArray(arguments)) goto cleanup;
    if (!cJSON_AddStringToObject(request, "jsonrpc", "2.0")
        || !cJSON_AddStringToObject(request, "method", method)
        || !cJSON_AddNumberToObject(request, "id", 1)
        || !cJSON_AddItemToObject(request, "params", arguments)) goto cleanup;
    arguments = NULL; // The request now owns the parsed parameters.
    body = cJSON_PrintUnformatted(request);
cleanup:
    cJSON_Delete(arguments);
    cJSON_Delete(request);
    return body;
}

// Sends a JSON-RPC request; the caller must free() the returned result string.
char *rpc_call(const char *url, const char *method, const char *params)
{
    if (!url || !method || !params) return NULL;
    char *body = compose_json_rpc_request(method, params);
    if (!body) return NULL;
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    struct callback_buf buf = {0};
    cJSON *json = NULL;
    char *out = NULL;
    if (!curl) goto cleanup;
    headers = curl_slist_append(NULL, "Content-Type: application/json");
    if (!headers) goto cleanup;
    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK
        || curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body) != CURLE_OK
        || curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK
        || curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf) != CURLE_OK
        || curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK
        || curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L) != CURLE_OK) goto cleanup;

    if (curl_easy_perform(curl) != CURLE_OK || !buf.data) goto cleanup;
    json = cJSON_Parse(buf.data);
    if (!json || cJSON_GetObjectItemCaseSensitive(json, "error")) goto cleanup;
    cJSON *result = cJSON_GetObjectItemCaseSensitive(json, "result");
    if (cJSON_IsString(result)) out = strdup(result->valuestring);
cleanup:
    cJSON_Delete(json);
    free(buf.data);
    curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    cJSON_free(body);
    return out;
}
