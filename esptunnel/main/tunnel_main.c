// Inspired by esp - idf / examples / ethernet / eth2ap / main / ethernet_example_main.c

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_private/wifi.h>
#include <esp_wifi_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/raw.h>
#include <nvs_flash.h>
#include "hardware.h"
#include "serio.h"
#include "console.h"
#include "wifi_comm.h"
#include "cfg_parse.h"
#include "version.h"
#include "wifi.h"
void		tunnelWaitForPeer(void);

#if CONFIG_WIFI_TUNNEL

#if !CONFIG_SPIO_ENABLED && !CONFIG_UARTIF_ENABLED
#error Invalid configuration without comm channel.
#endif

extern void	setup_spi(void);
extern void	initializeSpi(void);
//Parameters received at initialization time
int		baudRate = 115200;

RingbufHandle_t	wifiToSerial, serialToWifi;

extern wifi_interface_t wirelessInterface;

void 
wifiTunnel(void *)
{
	uartTunInitHw(115200);
	//set baudrate for config
	myPrintf("\n\nready to receive configuration\n\n");
	printf("\n\nready to receive configuration\n\n");


#if CONFIG_SPIO_ENABLED
	setup_spi();
#endif

	myPrintf("Register tunnel\n");
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, NULL));
	installRawIf();
	//vTaskDelay(500 / portTICK_PERIOD_MS);
	//ESP_ERROR_CHECK(esp_event_loop_create_default());


	wifiToSerial = xRingbufferCreate(1024 * 32, RINGBUF_TYPE_NOSPLIT);
	serialToWifi = xRingbufferCreate(1024 * 64, RINGBUF_TYPE_NOSPLIT);
	ESP_ERROR_CHECK(!serialToWifi || !wifiToSerial);

	tunnelWaitForPeer();

#if CONFIG_UARTIF_ENABLED
	uartTunStart(baudRate);
#endif
#if CONFIG_SPIO_ENABLED
	initializeSpi();
#endif

	//Continue as endless wifi - tx task...
		wifiTunTxTask(NULL);
}

#define CFG_IS(v)  "Config: " # v ": %s\n", (v ? "Enabled" : "No")
void osStatus(void)
{
    printf("OS: %s\n",GIT_INFO);
    printf( CFG_IS(CONFIG_WIFI_TUNNEL) );
    printf( CFG_IS(CONFIG_UARTIF_ENABLED) );
    printf( CFG_IS(CONFIG_SPIO_ENABLED) );
    printf( "Reset-pin: %d\n",CONFIG_GPIO_RESET_PIN);
}

static int 
cmd_tunnelStatus(int argc, char **argv)
{
	osStatus();
	networkStatus();
#if CONFIG_SPIO_ENABLED
	spiStatus();
#endif
	uartTunStatus();
	return 0;
}
#include "esp_console.h"
static int load_defaults(int argc,char **argv)
{
    int  importDefaultCfg(void);
    return importDefaultCfg();
}

void registerWifitun(void)
{
	const esp_console_cmd_t cmd[] = {
		{
			.command = "tunnel",
			.help = "Tunnel status",
			.hint = NULL,
			.func = &cmd_tunnelStatus,
			.argtable = NULL,
		},
		{
			.command = "load_defaults",
			.help = "Load default configuration",
			.hint = NULL,
			.func = &load_defaults,
			.argtable = NULL,
		}
	};
	for (int i = 0; i < (sizeof(cmd) / sizeof(cmd[0])); i++) {
		ESP_ERROR_CHECK(esp_console_cmd_register(cmd + i));
	}
	xTaskCreate(wifiTunnel, "wifi_start", 4 * 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}
#endif
