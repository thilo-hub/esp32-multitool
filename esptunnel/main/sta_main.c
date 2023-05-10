// Inspired by esp-idf/examples/ethernet/eth2ap/main/ethernet_example_main.c

#include <string.h>
#include <stdlib.h>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_event.h>
#include <esp_private/wifi.h>
#include <esp_wifi_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/raw.h>
#include <nvs_flash.h>

static char networkName[64] = "here_backbone";
static wifi_interface_t wirelessInterface = WIFI_IF_STA;
wifi_netif_driver_t wirelessInterface_netif;

 static char networkPass[64] = "SilverSpoon65";
 static int staticIpAddress[4]={10,2,1,124};
 static int staticGateway[4]  = {10,2,1,9};
 static int staticNetMask[4]  = {255,255,255,0};
 static int baudRate = 1*115200;
 static uint8_t wirelessProtocols = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR;
 static int wirelessChannel;

static RingbufHandle_t wifiToSerial, serialToWifi;

static void wifiTxTask(void *)
{
	size_t len;

	while (true)
	{
		char *buffer = (char*)xRingbufferReceive(serialToWifi, &len, portMAX_DELAY);
			esp_wifi_internal_tx(wirelessInterface, buffer, len); // ignore errors
		vRingbufferReturnItem(serialToWifi, buffer);
	}
}

static esp_err_t wifiRxCallbackNetif(esp_netif_t *esp_netif, void *buffer, size_t len, void *eb)
{
	xRingbufferSend(wifiToSerial, buffer, len, 0);
	esp_wifi_internal_free_rx_buffer(eb);
	return ESP_OK;
}

static void onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	static uint8_t clientCounter = 0;

	switch (event_id)
	{
		case WIFI_EVENT_AP_STACONNECTED:
		case WIFI_EVENT_AP_STADISCONNECTED:
			break;
		case WIFI_EVENT_STA_START:
printf("Start\n");
			wirelessInterface_netif = esp_wifi_create_if_driver(wirelessInterface);
			ESP_ERROR_CHECK(esp_wifi_register_if_rxcb(wirelessInterface_netif, wifiRxCallbackNetif,NULL));
			esp_wifi_connect();
			break;
		case WIFI_EVENT_STA_CONNECTED:
printf("COnnected\n");
			break;
		case WIFI_EVENT_STA_DISCONNECTED: // connection lost or AP not found during scan
printf("Disconnected\n");
			esp_wifi_connect(); // try to reconnect
			break;
		default:
			break;
	}
}

static void initializeWifi(void)
{
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

		ESP_ERROR_CHECK(esp_netif_init());
		esp_netif_t *netif_sta = esp_netif_create_default_wifi_sta();
		assert(netif_sta != nullptr);

#if 0
		esp_netif_ip_info_t ip_info;
		ip_info.ip.addr = ESP_IP4TOADDR(staticIpAddress[0], staticIpAddress[1], staticIpAddress[2], staticIpAddress[3]);
		ip_info.gw.addr = ESP_IP4TOADDR(staticGateway[0], staticGateway[1], staticGateway[2], staticGateway[3]);
		ip_info.netmask.addr = ESP_IP4TOADDR(staticNetMask[0], staticNetMask[1], staticNetMask[2], staticNetMask[3]);
		esp_netif_dhcpc_stop(netif_sta);
		esp_netif_set_ip_info(netif_sta, &ip_info);
#endif

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

		wifi_config_t wifi_config;
		strcpy((char*)wifi_config.sta.ssid, networkName);
		strcpy((char*)wifi_config.sta.password, networkPass);
		wifi_config.sta.scan_method = WIFI_FAST_SCAN;
		wifi_config.sta.bssid_set = false;
		wifi_config.sta.channel = wirelessChannel;
		wifi_config.sta.listen_interval = 0;
		wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
		wifi_config.sta.threshold.rssi = 0;
		wifi_config.sta.pmf_cfg.capable = true;
		wifi_config.sta.pmf_cfg.required = false;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_protocol(wirelessInterface, wirelessProtocols));

	ESP_ERROR_CHECK(esp_wifi_start());
	xTaskCreate(wifiTxTask, "wifi_tx", 2048, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}

static void uartTxTask(void *)
{
	size_t len;

	while (true)
	{
		char *buffer = (char*)xRingbufferReceive(wifiToSerial, &len, portMAX_DELAY);

		// Build header
		uint8_t header[4];
		header[0] = 0xAA;
		header[2] = len & 0xFF;
		header[3] = (len >> 8) & 0xFF;

		// Compute checksum
		header[1] = -(header[0] + header[2] + header[3]);
		for (size_t i = 0; i < len; i++)
			header[1] -= buffer[i];

		// Transmit
printf("+(%d)%c\n",len, buffer[0] == 0x45 ? 'X': '.');
		uart_write_bytes(UART_NUM_0, (char*)header, 4);
		uart_write_bytes(UART_NUM_0, buffer, len);

		vRingbufferReturnItem(wifiToSerial, buffer);
	}
}

static void uartRxTask(void *)
{
	while (true)
	{
		static uint8_t buffer[2000]; // bigger than MTU

		// Sync and receive header
		do
			uart_read_bytes(UART_NUM_0, &buffer[0], 1, portMAX_DELAY);
		while (buffer[0] != 0xAA);

		uart_read_bytes(UART_NUM_0, &buffer[1], 1, portMAX_DELAY);
		uart_read_bytes(UART_NUM_0, &buffer[2], 1, portMAX_DELAY);
		uart_read_bytes(UART_NUM_0, &buffer[3], 1, portMAX_DELAY);

		uint16_t len = buffer[2] | (buffer[3] << 8);
		if (4 + len > sizeof(buffer))
			continue; // packet too big

		for (size_t i = 0; i != len; i++)
			uart_read_bytes(UART_NUM_0, &buffer[4 + i], 1, portMAX_DELAY);

		uint8_t acc = 0;
		for (size_t i = 0; i != 4 + len; i++)
			acc += buffer[i];

		if (acc == 0) // good checksum
		{
printf("-(%d)\n",len);
			xRingbufferSend(serialToWifi, &buffer[4], len, 0);
		}
	}
}

static void initializeUart()
{
	uart_config_t uartConfig;
	uartConfig.baud_rate = baudRate;
	uartConfig.data_bits = UART_DATA_8_BITS;
	uartConfig.parity = UART_PARITY_DISABLE;
	uartConfig.stop_bits = UART_STOP_BITS_1;
	uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uartConfig.rx_flow_ctrl_thresh = 0;
	uartConfig.source_clk = UART_SCLK_APB;
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 256, 0, nullptr, 0));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uartConfig));

	xTaskCreate(uartRxTask, "uart_rx", 2048, nullptr, tskIDLE_PRIORITY + 2, nullptr);
	xTaskCreate(uartTxTask, "uart_tx", 2048, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}

static char *readLine()
{
		static char line[256];
		size_t pos = 0;

		while (1)
		{
			char ch = getchar();
			if (ch == 255) // non-blocking response
			{
				vTaskDelay(1);
				continue;
			}
			else if (ch == '\n')
			{
				break;
			}

			line[pos++] = ch;
		}

		line[pos] = '\0';

		return line;
}

extern "C" void app_main(void)
{
	// Ring buffers
	wifiToSerial = xRingbufferCreate(1024 * 32, RINGBUF_TYPE_NOSPLIT);
	serialToWifi = xRingbufferCreate(1024 * 16, RINGBUF_TYPE_NOSPLIT);

	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	printf("\n\nready to receive configuration (%d)\n\n",wirelessInterface);
	printf("got ssid %s\n", networkName);

	while ( wirelessInterface == (wifi_interface_t) -1)
	{
		const char *line = readLine();
		printf("input line: %s\n", line);

		char tmp[16];
		if (sscanf(line, "ssid%s", networkName) == 1)
		{
			printf("got ssid %s\n", networkName);
		}
		else if (sscanf(line, "password%s", networkPass) == 1)
		{
			printf("got password %s\n", networkPass);
		}
		else if (sscanf(line, "channel%d", &wirelessChannel) == 1)
		{
			printf("got channel %d\n", wirelessChannel);
		}
		else if (sscanf(line, "protocols%s", tmp) == 1)
		{
			wirelessProtocols = 0;

			if (strchr(tmp, 'b') != nullptr)
				wirelessProtocols |= WIFI_PROTOCOL_11B;
			if (strchr(tmp, 'g') != nullptr)
				wirelessProtocols |= WIFI_PROTOCOL_11G;
			if (strchr(tmp, 'n') != nullptr)
				wirelessProtocols |= WIFI_PROTOCOL_11N;
			if (strchr(tmp, 'l') != nullptr)
				wirelessProtocols |= WIFI_PROTOCOL_LR;

			printf("got protocols %d\n", wirelessProtocols);
		}
		else if (sscanf(line, "ap%d", &baudRate) == 1)
		{
			printf("got ap with baudrate %d\n", baudRate);
			wirelessInterface = WIFI_IF_AP;
			break;
		}
		else if (sscanf(line, "sta %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d", &staticIpAddress[0], &staticIpAddress[1], &staticIpAddress[2], &staticIpAddress[3], &staticGateway[0], &staticGateway[1], &staticGateway[2], &staticGateway[3], &staticNetMask[0], &staticNetMask[1], &staticNetMask[2], &staticNetMask[3], &baudRate) == 13)
		{
			printf("got sta with ip %d.%d.%d.%d gw %d.%d.%d.%d netmask %d.%d.%d.%d baudrate %d\n",
				staticIpAddress[0], staticIpAddress[1], staticIpAddress[2], staticIpAddress[3],
				staticGateway[0], staticGateway[1], staticGateway[2], staticGateway[3],
				staticNetMask[0], staticNetMask[1], staticNetMask[2], staticNetMask[3],
				baudRate);
			wirelessInterface = WIFI_IF_STA;
			break;
		}
	}
printf("got ssid %s\n", networkName);
printf("got password %s\n", networkPass);
printf("got channel %d\n", wirelessChannel);
printf("got protocols %d\n", wirelessProtocols);
printf("got sta with ip %d.%d.%d.%d gw %d.%d.%d.%d netmask %d.%d.%d.%d baudrate %d\n",
	staticIpAddress[0], staticIpAddress[1], staticIpAddress[2], staticIpAddress[3],
	staticGateway[0], staticGateway[1], staticGateway[2], staticGateway[3],
	staticNetMask[0], staticNetMask[1], staticNetMask[2], staticNetMask[3],
	baudRate);

	vTaskDelay(500 / portTICK_PERIOD_MS);
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	initializeUart();
	initializeWifi();
}
