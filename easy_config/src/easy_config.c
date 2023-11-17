#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "easy_config.h"

#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "easy_config";

static size_t num_config_infos = 0;
static easy_config_entry_info *config_infos = NULL;
static easy_config_entry *config_entries = NULL;

void easy_config_setup(easy_config_entry_info *info) {
    config_infos = info;

    size_t length = 0;
    easy_config_entry_info *inf = info;
    while(inf->type != CONFIG_TYPE_END) {
        length++;
        inf++;
    }
    num_config_infos = length;

    config_entries = malloc(sizeof(easy_config_entry) * num_config_infos);
    memset(config_entries, 0, sizeof(easy_config_entry) * num_config_infos);
}

struct buffer {
    char *data;
    size_t length;
    size_t current_offset;
};

void buffer_snprintf(struct buffer *self, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    if(self->data == NULL) {
        self->current_offset += vsnprintf(NULL, 0, fmt, args);
    } else {
        ssize_t remaining = self->length - self->current_offset;
        self->current_offset += vsnprintf(self->data + self->current_offset, remaining, fmt, args);
    }

    va_end(args);
}

extern const char html_header_start[] asm("_binary_index_header_html_start");
extern const char html_header_end[]   asm("_binary_index_header_html_end");
extern const size_t html_header_length;

static void build_html(char *output, size_t *length) {
    struct buffer buff = {
        .data = output,
        .length = *length,
        .current_offset = 0
    };

    buffer_snprintf(&buff, "%s", html_header_start);

    for(int i = 0; i < num_config_infos; i++) {
        buffer_snprintf(&buff, "<label>%s ", config_infos[i].name);
        switch(config_infos[i].type) {
            case CONFIG_TYPE_BOOL:
                buffer_snprintf(&buff, "<input name=\"%s\" type=\"checkbox\" %s /></br>", config_infos[i].id, config_entries[i].bool_value ? "checked" : "");
            break;

            case CONFIG_TYPE_INT:
                buffer_snprintf(&buff, "<input name=\"%s\" type=\"number\" value=\"%i\" /><br/>", config_infos[i].id, config_entries[i].int_value);
            break;

            case CONFIG_TYPE_STRING:
                {
                    const char *value = "";
                    if(config_entries[i].string_value != NULL) {
                        value = config_entries[i].string_value;
                    }
                    buffer_snprintf(&buff, "<input name=\"%s\" type=\"text\" value=\"%s\" /><br/>", config_infos[i].id, value);
                }
            break;

            default:
            break;
        }
        buffer_snprintf(&buff, "</label>");
    }
    buffer_snprintf(&buff, "<input type=\"submit\" value=\"Save\" />");
    buffer_snprintf(&buff, "</form></body></html>");

    *length = buff.current_offset + 1;
}

void easy_config_setup_wifi_ap() {
    char *final_html = NULL;
    size_t length = 0;

    internal_wifi_init_softap();

    build_html(NULL, &length);
    final_html = malloc(length);
    build_html(final_html, &length);

    TaskHandle_t waiting_task_handle = xTaskGetCurrentTaskHandle();
    internal_start_http_server(final_html, waiting_task_handle);
    internal_start_dns_server();

    /* Wait for exit... */
    ulTaskNotifyTake(pdTRUE, 0xFFFFFFFF);

    internal_stop_http_server();
    // todo: properly stop server
    //internal_stop_dns_server();
}

static void url_decode_string(const char *input, char *output) {
    size_t len = strlen(input);
    size_t out = 0;

    for(size_t i = 0; i < len; i++) {
        if(input[i] == '%') {
            char hex_value[3] = {0};
            hex_value[0] = input[i+1];
            hex_value[1] = input[i+2];
            char *endptr = NULL;
            char value = (char)strtol(hex_value, &endptr, 16);
            if(endptr == NULL) {
                ESP_LOGI(TAG, "Invalid hex '%s' when decoding URL!", hex_value);
            }
            ESP_LOGI(TAG, "hex: '%s' -> %i", hex_value, value);

            output[out++] = value;
            i += 2;
        } else {
            output[out++] = input[i];
        }
    }
    output[out] = 0;
}

void internal_set_config_from_html_form(const char *k, const char *v) {
    ESP_LOGI(TAG, "Setting config '%s' = '%s'", k, v);
    for(size_t i = 0; i < num_config_infos; i++) {
        if(strcmp(k, config_infos[i].id) == 0) {
            switch(config_infos[i].type) {
                case CONFIG_TYPE_BOOL:
                    {
                        bool value = strcmp(v, "on") == 0;
                        easy_config_set_boolean(i, value);
                    }
                break;

                case CONFIG_TYPE_INT:
                    {
                        char *endptr = NULL;
                        int32_t value = strtol(v, &endptr, 10); /* todo: base? 10 always is probably fine. */
                        if(endptr == NULL) {
                            /* Conversion failed! */
                            ESP_LOGI(TAG, "Got invalid integer '%s' for key '%s'", v, k);
                        } else {
                            easy_config_set_integer(i, value);
                        }
                    }
                break;

                case CONFIG_TYPE_STRING:
                    /* url decode */
                    char *new_string = malloc(strlen(v) + 1);
                    url_decode_string(v, new_string);
                    easy_config_set_string(i, new_string);
                break;
                
                default:
                break;
            }
        }
    }
}

bool easy_config_get_boolean(size_t index) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_BOOL) {
        return config_entries[index].bool_value;
    } else {
        abort();
    }
}

int easy_config_get_integer(size_t index) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_INT) {
        return config_entries[index].int_value;
    } else {
        abort();
    }
}

const char *easy_config_get_string(size_t index) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_STRING) {
        return config_entries[index].string_value;
    } else {
        abort();
    }
}

void easy_config_set_boolean(size_t index, bool value) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_BOOL) {
        config_entries[index].bool_value = value;
    } else {
        abort();
    }
}

void easy_config_set_integer(size_t index, int32_t value) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_INT) {
        config_entries[index].int_value = value;
    } else {
        abort();
    }
}

void easy_config_set_string(size_t index, const char *value) {
    if(index < num_config_infos && config_infos[index].type == CONFIG_TYPE_STRING) {
        /* this should always be a heap str... free it ig */
        if(config_entries[index].string_value != NULL) {
            free(config_entries[index].string_value);
        }

        size_t length = strlen(value);
        char *new_string = malloc(length + 1);
        strncpy(new_string, value, length + 1);

        config_entries[index].string_value = new_string;
    } else {
        abort();
    }
}

bool easy_config_load_from_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t nvs;
    ret = nvs_open("easy_config", NVS_READWRITE, &nvs);
    ESP_ERROR_CHECK(ret);

    for(size_t i = 0; i < num_config_infos; i++) {
        switch(config_infos[i].type) {
            case CONFIG_TYPE_BOOL: 
            {
                uint8_t temp = 0;
                ret = nvs_get_u8(nvs, config_infos[i].id, &temp);
                config_entries[i].bool_value = temp != 0;
            }
            break;
            
            case CONFIG_TYPE_INT:
                ret = nvs_get_i32(nvs, config_infos[i].id, &config_entries[i].int_value);
            break;
            
            case CONFIG_TYPE_STRING:
            {
                size_t length = 0;

                ret = nvs_get_str(nvs, config_infos[i].id, NULL, &length);
                if(ret == ESP_OK) {
                    char *str = malloc(length);
                    nvs_get_str(nvs, config_infos[i].id, str, &length);
                    config_entries[i].string_value = str;
                }
            }   
            break;

            default:
            break;
        }
        
        if(ret == ESP_ERR_NVS_NOT_FOUND) {
            return false;
        } else {
            ESP_ERROR_CHECK(ret);
        }
    }

    nvs_close(nvs);
    ESP_ERROR_CHECK(ret);
    return true;
}

bool easy_config_save_to_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t nvs;
    ret = nvs_open("easy_config", NVS_READWRITE, &nvs);
    ESP_ERROR_CHECK(ret);

    for(size_t i = 0; i < num_config_infos; i++) {
        switch(config_infos[i].type) {
            case CONFIG_TYPE_BOOL: 
                ret = nvs_set_u8(nvs, config_infos[i].id, config_entries[i].bool_value ? 1 : 0);
            break;
            
            case CONFIG_TYPE_INT:
                ret = nvs_set_i32(nvs, config_infos[i].id, config_entries[i].int_value);
            break;
            
            case CONFIG_TYPE_STRING:
                if(config_entries[i].string_value != NULL) {
                    ret = nvs_set_str(nvs, config_infos[i].id, config_entries[i].string_value);
                } else {
                    ret = nvs_set_str(nvs, config_infos[i].id, "");
                }
            break;

            default:
            break;
        }
        
        if(ret == ESP_ERR_NVS_NOT_FOUND) {
            return false;
        } else {
            ESP_ERROR_CHECK(ret);
        }
    }

    nvs_close(nvs);
    ESP_ERROR_CHECK(ret);
    return true;
}