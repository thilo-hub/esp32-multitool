/*
 * Basic console example (esp_console_repl API)
 * 
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * 
 * Unless required by applicable law or agreed to in writing, this software is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.
 */

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#if CONFIG_WEBSERVER
//
#include "cmd_wifi.h"
#include "wifi_comm.h"
#endif
#include "cmd_nvs.h"
#if CONFIG_RF433
#include "cmd_rf433.h"
#endif
#include "spifs.h"
#if CONFIG_WEBSERVER
#include "wifi_comm.h"
#endif
#include "uart2_io.h"
#include "utimer.h"
#include "wifi.h"
#include "version.h"

static const char *TAG = "multitool";
#define PROMPT_STR CONFIG_IDF_TARGET

/*
 * Console command history can be stored to and loaded from a file. The
 * easiest way to do this is to use FATFS filesystem on top of wear_levelling
 * library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"
#define USE_FATFS 0

#if USE_FATFS
static void 
initialize_filesystem(void)
{
	static wl_handle_t wl_handle;
	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files = 4,
		.format_if_mount_failed = true
	};
	esp_err_t	err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return;
	}
}

#else
/* Function to initialize SPIFFS */
//esp_err_t example_mount_storage(const char *base_path)
	static void	initialize_filesystem(void)
{
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = MOUNT_PATH,
		.partition_label = NULL,
		.max_files = 5, //This sets the maximum number of files that can be open at the same time
		.format_if_mount_failed = false // true
	};

	esp_err_t	ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}
	size_t		total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	return;
}
#endif

#endif	/* // CONFIG_STORE_HISTORY */

static void 
initialize_nvs(void)
{
	esp_err_t	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
}

void 
app_main(void)
{
	esp_console_repl_t *repl = NULL;
	esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	/*
	 * Prompt to be printed before each line. This can be customized,
	 * made dynamic, etc.
	 */
	repl_config.prompt = PROMPT_STR ">";
	repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;
	ESP_LOGI(TAG, "%s", GIT_INFO);

	initialize_nvs();

#if CONFIG_CONSOLE_STORE_HISTORY
	initialize_filesystem();
	repl_config.history_save_path = HISTORY_PATH;
	ESP_LOGI(TAG, "Command history enabled");
#else
	ESP_LOGI(TAG, "Command history disabled");
#endif

	/* Register commands */
	esp_console_register_help_command();
	register_system();
	register_nvs();
	register_spifs();
#if CONFIG_RF433
	registerRf433();
#endif
	registerWifi();
	//must be before wifi users...
#ifdef CONFIG_UARTCON_ENABLE
		registerUartCon();
#endif
#if CONFIG_WIFI_TUNNEL
	registerWifitun();
#endif

	registerTimer();

#if CONFIG_WEBSERVER
	registerHttpdServer();
#endif

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
	esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
	esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
	esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

	ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
