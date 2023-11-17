#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "easy_config.h"

static const char *TAG = "http";
static const char *page_content = NULL;
static TaskHandle_t g_waiting_task_handle = 0;

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, page_content, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t *req)
{
    char*  buf = malloc(req->content_len + 1);

    size_t off = 0;
    while (off < req->content_len) {
        /* Read data received in the request */
        int ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free (buf);
            return ESP_FAIL;
        }
        off += ret;
    }
    buf[off] = '\0';

    char *form_key = buf;
    char *form_value = NULL;

    for(size_t i = 0; i < req->content_len; i++) {
        if(buf[i] == '=') {
            buf[i] = 0;
            // This is safe because at worst it will be form_value pointing at the null terminator, so an empty string
            form_value = buf + i + 1;
        } else if(buf[i] == '&') {
            // New key
            buf[i] = 0;
            
            internal_set_config_from_html_form(form_key, form_value);

            form_key = buf + i + 1;
            form_value = NULL;
        } else if(i == req->content_len - 1) {
            // End of form data - process final key
            internal_set_config_from_html_form(form_key, form_value);
        }
    }
    
    if(easy_config_save_to_nvs()) {
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        xTaskNotifyGive(g_waiting_task_handle);
    } else {
        httpd_resp_send(req, "Save failed!", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

/* A HTTP GET handler */
static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");

    const char* resp_str = "Redirecting...";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static httpd_uri_t gen204_route = {
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = redirect_handler
};

static httpd_uri_t root_route = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler
};

static httpd_uri_t config_route = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_post_handler
};

static httpd_handle_t server = NULL;
void internal_start_http_server(const char *html_page, TaskHandle_t waiting_task_handle)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    page_content = html_page;

    g_waiting_task_handle = waiting_task_handle;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root_route);
        httpd_register_uri_handler(server, &config_route);
        httpd_register_uri_handler(server, &gen204_route);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

esp_err_t internal_stop_http_server()
{
    // Stop the httpd server
    return httpd_stop(server);
}