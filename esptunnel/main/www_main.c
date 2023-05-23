/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include "driver/gpio.h"
#include <esp_http_server.h>
#include <string.h>
//#include "serio.h"
#include "uart2_io.h"
#include "cfg_parse.h"
#include "wifi.h"

#define DEBUG_UPLOAD 
#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

#if CONFIG_WEBSERVER
static const char *TAG = "webserver";

#if CONFIG_BASIC_AUTH

typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */

void resetBeaglebone(void *p) 
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<CONFIG_GPIO_RESET_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_set_level(CONFIG_GPIO_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(CONFIG_GPIO_RESET_PIN, 1);
    vTaskDelete(NULL);
}


static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
static esp_err_t httpdAuthGetHandler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            char *basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
            if (!basic_auth_resp) {
                ESP_LOGE(TAG, "No enough memory for basic authorization response");
                free(auth_credentials);
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
            free(basic_auth_resp);
#if CONFIG_UARTCON_ENABLE
	    uartConStart(0,NULL);
#endif
	    httpd_req_get_url_query_str(req, buf, buf_len);
	    if (strcmp(buf,"reset") == 0) {
		ESP_LOGI(TAG,"Reset Beaglebone");
		xTaskCreate(resetBeaglebone, "reset beagle", 4096, NULL, tskIDLE_PRIORITY , NULL);
	    }
        }
        free(auth_credentials);
        free(buf);
    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

static httpd_uri_t basic_auth = {
    .uri       = "/basic_auth",
    .method    = HTTP_GET,
    .handler   = httpdAuthGetHandler,
};


static void httpdRegisterBasicAuth(httpd_handle_t server)
{
    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info) {
        basic_auth_info->username = getcfg("AUTHUSER");
        basic_auth_info->password = getcfg("AUTHPASS");

        basic_auth.user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, &basic_auth);
    }
}
#endif

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

#define FILE_PATH_MAX 60
/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[32 + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t httpdIndexHtmlGetHandler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}


/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    char*  buf;
    size_t buf_len;
    FILE *fd;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));


    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

#if 0
    { char *ep =rindex(filepath,'.');
	    if ( ep != NULL ) {
		ep[4] = 0;
	    }
    }
#endif
    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            return httpdIndexHtmlGetHandler(req);
#if 0
        } else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
#endif
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }


    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }
    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        // httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No such button");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, "OK", HTTPD_RESP_USE_STRLEN);

    /* Respond with an empty chunk to signal HTTP response completion */
// #ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
// #endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t send_file(char *file) ;

static esp_err_t httpdButtonsGetHandler(httpd_req_t *req)
{
    char*  buf;

    DIR *dir=opendir("/data/buttons");
    struct dirent *e;
    buf = malloc(512);
    *buf=0;
    while( dir != NULL  && (e=readdir(dir)) != NULL) {
	if (e->d_type == DT_REG){
	    strlcat(buf,"buttons/",512);
	    strlcat(buf,e->d_name,512);
	    strlcat(buf,"\n",512);
	}
    }
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return ESP_OK;
}

static esp_err_t httpdRf433PostHandler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
#ifdef DEBUG_UPLOAD
    FILE *fhd = NULL;
#endif
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        remaining -= ret;
#ifdef DEBUG_UPLOAD
        if (fhd == NULL  && strncmp(buf,"<!DOCTYPE html>",15) == 0 ) {
		ESP_LOGI(TAG,"Creating test.html");
		fhd = fopen("/data/public/test.html","w");
        }
        if (fhd != NULL ) {
		ESP_LOGI(TAG,"Saving.. %d\n",ret);
		fwrite(buf,1,ret,fhd);
		continue;
	}
#endif
		
		

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "==============%d of %d======================",remaining,ret);
#if CONFIG_RF433
	if (send_file(buf) != ESP_OK ){
	    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
	    return ESP_FAIL;
	} else {
            httpd_resp_send_chunk(req, "OK", HTTPD_RESP_USE_STRLEN);
	}
#endif
    }
#ifdef DEBUG_UPLOAD
    if ( fhd != NULL)
	fclose(fhd);
#endif
   

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

int httpd_server_port = 80;
static httpd_handle_t httpdStartServer(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = httpd_server_port;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    static struct file_server_data *server_data = NULL;
    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
	return NULL;
        // return ESP_ERR_INVALID_STATE;
    }
    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
	return NULL;
        //return ESP_ERR_NO_MEM;
    }
    const char *base_path="/data/public";
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
#if CONFIG_BASIC_AUTH
	// need to register before wildcard...
	httpdRegisterBasicAuth(server);
#endif
	httpd_uri_t uris[] = {
	    { .uri       = "/send",       .method    = HTTP_POST, .handler   = httpdRf433PostHandler,  .user_ctx  = NULL },
	    { .uri       = "/buttons",    .method    = HTTP_GET,  .handler   = httpdButtonsGetHandler, .user_ctx  = NULL },
	    { .uri       = "/*",           .method    = HTTP_GET,  .handler   = hello_get_handler,   .user_ctx  = server_data },
#if 0 //  CONFIG_BASIC_AUTH
	    { .uri       = "/basic_auth", .method    = HTTP_GET, .handler   = httpdAuthGetHandler, }
#endif
	};
	for(const httpd_uri_t *up=uris;up<(uris+sizeof(uris)/sizeof(uris[0]));up++){
	    httpd_register_uri_handler(server, up);
	}
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t httpdStopServer(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void httpdDisonnectHandler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (httpdStopServer(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void httpdConnectHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = httpdStartServer();
    }
}

int cmdHttpdStartServer(int argc, char **argv)
{
    static httpd_handle_t server = NULL;

    if (server ) 
    {
	if (argc > 1 && strcmp("stop",argv[1]) == 0 ){
	    ESP_LOGI(TAG, "Stopping webserver");
	    if (httpdStopServer(server) == ESP_OK) {
		server = NULL;
	    } else {
		ESP_LOGE(TAG, "Failed to stop http server");
	    }
	}
    } else {
	// ESP_ERROR_CHECK(nvs_flash_init());
	// ESP_ERROR_CHECK(esp_netif_init());
	// ESP_ERROR_CHECK(esp_event_loop_create_default());

	// ESP_ERROR_CHECK(wifi_connect());
	//initializeWifi();
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &httpdConnectHandler, &server));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &httpdDisonnectHandler, &server));

	server = httpdStartServer();
    }
return 0;
}


#include "esp_console.h"
void registerHttpdServer(void)
{
    const esp_console_cmd_t cmd[] = {
    {
        .command = "www",
        .help = "Start or stop  webserver",
        .hint = NULL,
        .func = &cmdHttpdStartServer
	},
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	    ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
#if 0 &&  CONFIG_WEBSERVER_AUTOSTART
    char *ssid = getcfg("SSID");
    char *password   = getcfg("PW");
    if ( ssid && password ) {
	cmdHttpdStartServer(0,NULL);
    }
#endif
}
#endif
