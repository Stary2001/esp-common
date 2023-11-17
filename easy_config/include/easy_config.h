#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum easy_config_entry_type {
	CONFIG_TYPE_STRING,
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_END
} easy_config_entry_type;

typedef struct easy_config_entry_info {
	const char *name;
	const char *id;
	easy_config_entry_type type;
} easy_config_entry_info;

typedef struct easy_config_entry {
	// etc
	union {
		char *string_value;
		int32_t int_value;
		bool bool_value;
	};
} easy_config_entry;

extern void easy_config_setup(easy_config_entry_info *info);
extern bool easy_config_load_from_nvs();
extern bool easy_config_save_to_nvs();
extern void easy_config_setup_wifi_ap();

extern bool easy_config_get_boolean(size_t index);
extern int easy_config_get_integer(size_t index);
extern const char *easy_config_get_string(size_t index);

extern void easy_config_set_boolean(size_t index, bool value);
extern void easy_config_set_integer(size_t index, int32_t value);

// set_string will COPY your string
extern void easy_config_set_string(size_t index, const char *value);

// these are internal but. shh
extern void internal_wifi_init_softap(void);
extern void internal_start_http_server(const char *html_page, TaskHandle_t task_handle);
extern void internal_start_dns_server(void);

extern void internal_wifi_stop_softap(void);
extern esp_err_t internal_stop_http_server(void);
extern void internal_stop_dns_server(void);

extern void internal_set_config_from_html_form(const char *key, const char *value);