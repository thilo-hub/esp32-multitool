// Inspired by esp-idf/examples/ethernet/eth2ap/main/ethernet_example_main.c

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
extern "C" {
#include "serio.h"
#include "console.h"
#include "wifi_comm.h"
#include "cfg_parse.h"
}

extern void setup_spi(void);
extern void initializeSpi(void);
// Parameters received at initialization time
static char networkName[64];
static char networkPass[64] = ""; // unprotected by default
static int wirelessChannel;
static int baudRate = 115200;
wifi_interface_t wirelessInterface = (wifi_interface_t)-1;
static int staticIpAddress[4], staticGateway[4], staticNetMask[4]; // STA only
static uint8_t wirelessProtocols;
static esp_netif_t *netif_sta;
uint8_t mac[NETIF_MAX_HWADDR_LEN];


// static 
RingbufHandle_t wifiToSerial, serialToWifi;

static void networkStatus(void)
{
	printf("got ssid %s\n", networkName);
	printf("got password %s\n", networkPass);
	printf("got channel %d\n", wirelessChannel);
	printf("got protocols %d\n", wirelessProtocols);
	printf("got sta with ip %d.%d.%d.%d gw %d.%d.%d.%d netmask %d.%d.%d.%d baudrate %d\n",
		staticIpAddress[0], staticIpAddress[1], staticIpAddress[2], staticIpAddress[3],
		staticGateway[0], staticGateway[1], staticGateway[2], staticGateway[3],
		staticNetMask[0], staticNetMask[1], staticNetMask[2], staticNetMask[3],
		baudRate);
	printf( "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

static void initializeWifi(void)
{
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	ESP_ERROR_CHECK(esp_netif_init());
	netif_sta = esp_netif_create_default_wifi_sta();
	assert(netif_sta != NULL);

	esp_netif_ip_info_t ip_info;
	ip_info.ip.addr = ESP_IP4TOADDR(staticIpAddress[0], staticIpAddress[1], staticIpAddress[2], staticIpAddress[3]);
	ip_info.gw.addr = ESP_IP4TOADDR(staticGateway[0], staticGateway[1], staticGateway[2], staticGateway[3]);
	ip_info.netmask.addr = ESP_IP4TOADDR(staticNetMask[0], staticNetMask[1], staticNetMask[2], staticNetMask[3]);
	esp_netif_dhcpc_stop(netif_sta);
	esp_netif_set_ip_info(netif_sta, &ip_info);

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
	ESP_ERROR_CHECK( esp_netif_get_mac(netif_sta, mac));
	// xTaskCreate(wifiTxTask, "wifi_tx", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}
static char *tunnel= NULL;

static void load_configuration(void) {

#define Xstrcpy(a,b) do { char *p=b; if (b) { strcpy(a,p); } } while(0)
	Xstrcpy(networkName ,getcfg("SSID"));
        printf("WIFI config:  %s\n",networkName);
	Xstrcpy( networkPass , getcfg("PW") );
        printf("WIFI PW:  %s\n",networkPass);
	char *tmp = getcfg("PROTO");
			wirelessProtocols = 0;

			if (strchr(tmp, 'b') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11B;
			if (strchr(tmp, 'g') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11G;
			if (strchr(tmp, 'n') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11N;
			if (strchr(tmp, 'l') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_LR;

        printf("WIFI proto:  %s\n",tmp);
	tunnel = getcfg("TUNNEL");
        printf("WIFI tunnel:  %s\n",tunnel);

	myPrintf("ready to connect: %s\n",tunnel);
	// "ready to connect:") ) { // sta 10.1.1.73 10.1.1.9 255.255.255.0 3500000
}

static void configure_network(void) 
{
	char *nw = getcfg("SSID");
        printf("WIFI config:  %s\n",nw ? nw : " -- NULL --");
	while ( wirelessInterface == (wifi_interface_t) -1)
	{
		static char line[256];
		readLine(UART_DEFAULT,line,sizeof(line));
		printf("input line: %s\n", line);

		if (strstr(line,"Start")  != NULL ) {
		    	printf("Configuration set..\n");
			if (sscanf(tunnel, "sta %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d", 
					&staticIpAddress[0], &staticIpAddress[1], &staticIpAddress[2], &staticIpAddress[3], 
					&staticGateway[0], &staticGateway[1], &staticGateway[2], &staticGateway[3], 
					&staticNetMask[0], &staticNetMask[1], &staticNetMask[2], &staticNetMask[3], 
					&baudRate) == 13){
			    printf("Go\n");
			    wirelessInterface = WIFI_IF_STA;
			    break;
			}
		}

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

			if (strchr(tmp, 'b') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11B;
			if (strchr(tmp, 'g') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11G;
			if (strchr(tmp, 'n') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_11N;
			if (strchr(tmp, 'l') != NULL)
				wirelessProtocols |= WIFI_PROTOCOL_LR;

			printf("got protocols %d\n", wirelessProtocols);
		}
#if 0
		else if (sscanf(line, "ap%d", &baudRate) == 1)
		{
			printf("got ap with baudrate %d\n", baudRate);
			wirelessInterface = WIFI_IF_AP;
			break;
		}
#endif
		else if (sscanf(line, "sta %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d", 
					&staticIpAddress[0], &staticIpAddress[1], &staticIpAddress[2], &staticIpAddress[3], 
					&staticGateway[0], &staticGateway[1], &staticGateway[2], &staticGateway[3], 
					&staticNetMask[0], &staticNetMask[1], &staticNetMask[2], &staticNetMask[3], 
					&baudRate) == 13)
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

}

void wifiTunnel(void *) {
    initializeUartHw(baudRate); // set baudrate for config

    myPrintf("\n\nready to receive configuration\n\n");
    printf("\n\nready to receive configuration\n\n");

    setup_spi();
    load_configuration();
    configure_network();

    myPrintf("Starting Wifi\n");

    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    wifiToSerial = xRingbufferCreate(1024 * 32, RINGBUF_TYPE_NOSPLIT);
    serialToWifi = xRingbufferCreate(1024 * 64, RINGBUF_TYPE_NOSPLIT);
    if (!serialToWifi || !wifiToSerial ){
	    printf("Failuer ringbuff\n");
	    while(1) {;} //hang
    }

    startUart(baudRate);
    initializeWifi();
    initializeSpi();

    // Keep as wifi-tx task...
    wifiTxTask(NULL);
}


static int cmd_tunnelStatus(int argc, char **argv)
{
	networkStatus();
        spiStatus();
	uartStatus();
	return 0;
}
#include "esp_console.h"
extern "C" void register_wifitun(void)
{
    const esp_console_cmd_t cmd[] = {
	    {
		.command = "tunnel",
		.help = "Start tunneling",
		.hint = NULL,
		.func = &cmd_tunnelStatus,
		.argtable = NULL,
	    }
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	    ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
    xTaskCreate(wifiTunnel, "wifi_start", 4*2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}
