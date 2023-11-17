idf_component_register(
    SRCS src/easy_config.c src/easy_wifi.c src/easy_http.c src/dns_server.c
    INCLUDE_DIRS include
    REQUIRES nvs_flash esp_wifi esp_http_server
    EMBED_TXTFILES index_header.html
)
