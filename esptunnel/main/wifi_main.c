#include <esp_wifi.h>
#include <esp_log.h>
#include "esp_netif.h"
#include <lwip/raw.h>
#include <string.h>
#include "cfg_parse.h"
#include "wifi_comm.h"
#include "console.h"
#include "version.h"

#include "wifi.h"
// #define CONFIG_USE_STATIC_IP 1
static const char *TAG = "wifi";

// TODO: movie into struct
static esp_netif_t *netif_sta;
// static int staticIpAddress[4], staticGateway[4], staticNetMask[4]; // STA only
esp_netif_ip_info_t localNetwork;
static char networkName[64];
static char networkPass[64] = ""; // unprotected by default
static int wirelessChannel;
wifi_interface_t wirelessInterface = (wifi_interface_t)-1;
static uint8_t wirelessProtocols;
uint8_t mac[NETIF_MAX_HWADDR_LEN];

extern int baudRate;
char *tunnel= NULL;
char *dnsserver = NULL;

#include <esp_wifi_netif.h>

void esp_netif_free_rx_buffer(void *h, void* buffer);


esp_err_t esp_netif_receiveTj(esp_netif_t *esp_netif, void *buffer, size_t len, void *eb)
{
    // f412fa416628-589cfc10ffeb- 08 00 45 00 00 31 00 00 40 00 3f 06 b0 67 c0 a8 09 81 c0 a8 00 8e eb d0 26 94 
    // 0            6             12 13 14          18          22 23       26          30          34    36
    uint8_t *p=buffer;
    if ( len > 34 && p[12] == 8 && p[13] == 0 && p[14] == 0x45 && p[23] == 6 && p[36]== 0x26 && p[37] == 0x94 ) {
    ESP_LOGI(TAG, "Received data: ptr:%p, size:%d", buffer, len);
	    // It's ipv4-tcp dst port 9876 
	int i;
	for(i=0; i<len && i<64;i++) {
	    printf(" %02x",((uint8_t *)buffer)[i]);
	}
	printf("\n");
    }
    return esp_netif_receive(esp_netif,buffer,len,eb);

    // esp_netif_transmit(esp_netif, buffer, len);
    if (eb) {
        esp_netif_free_rx_buffer(esp_netif, eb);
    }
    return ESP_OK;
}


void dmp_frames(int start)
{
    // register CB to dump received
// esp_err_t esp_wifi_register_if_rxcb(wifi_netif_driver_t ifx, esp_netif_receive_t fn, void * arg)
        void *driver = esp_netif_get_io_driver(netif_sta);
	if ( driver == NULL ) {
	    ESP_LOGI(TAG,"Failed get_io_driver");
	    return;
	}
	esp_err_t ret;
	esp_netif_receive_t fn=esp_netif_receive;
	if ( start )
	    fn = esp_netif_receiveTj;
	if ((ret = esp_wifi_register_if_rxcb(driver,  fn, netif_sta)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_register_if_rxcb for if=%p failed with %d", driver, ret);
        return; // ESP_FAIL;
    }

    //ESP_ERROR_CHECK( esp_wifi_register_if_rxcb() )
}


void tstEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    char *e="?";
#define TxE(ev)  if (event_id == ev)   e = # ev
    TxE(IP_EVENT_STA_GOT_IP);
    TxE(WIFI_EVENT_WIFI_READY);
    TxE(WIFI_EVENT_SCAN_DONE);
    TxE(WIFI_EVENT_STA_START);
    TxE(WIFI_EVENT_STA_STOP);
    TxE(WIFI_EVENT_STA_CONNECTED);
    TxE(WIFI_EVENT_STA_DISCONNECTED);
    TxE(WIFI_EVENT_STA_AUTHMODE_CHANGE);
    ESP_LOGI(TAG,"Event: %ld %s",event_id,e);
    if ( event_id == WIFI_EVENT_STA_START ) {
		esp_wifi_connect();
    }

}
#include <freertos/event_groups.h>

static EventGroupHandle_t s_wifi_event_group;
int s_retry_num =0;
#define EXAMPLE_ESP_MAXIMUM_RETRY 4

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
	localNetwork = event->ip_info;

        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&localNetwork.ip));
        ESP_LOGI(TAG, "got gw:" IPSTR, IP2STR(&localNetwork.gw));
        ESP_LOGI(TAG, "got msk:" IPSTR, IP2STR(&localNetwork.netmask));

	static char tun[64];
	ESP_LOGI(TAG, "TN: %s ->",tunnel);
	sprintf(tun,"sta " IPSTR " " IPSTR " " IPSTR " %d",
	    IP2STR(&localNetwork.ip), IP2STR(&localNetwork.gw), IP2STR(&localNetwork.netmask), baudRate);
	tunnel = tun;

	// ip_info_t UNNEL=sta 10.1.1.73 10.1.1.9 255.255.255.0 3500000

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void initializeWifi(void)
{
    static bool init_done=0;
    if (!init_done) {
	    init_done=1;

	s_wifi_event_group = xEventGroupCreate();


	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	netif_sta = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	const char *hn=getCfg("HOSTNAME");
	if ( hn ){
	    esp_netif_set_hostname(netif_sta,hn);
	    ESP_LOGI(TAG,"Hostname: %s",hn);
	    free(hn);
	}



	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, tstEvent, NULL));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));


	wifi_config_t wifi_config = { 0};
    	strlcpy((char *)wifi_config.sta.ssid,networkName,sizeof(wifi_config.sta.ssid));
    	strlcpy((char *)wifi_config.sta.password,networkPass,sizeof(wifi_config.sta.password));
#if CONFIG_USE_STATIC_IP
	esp_netif_dhcpc_stop(netif_sta);
	esp_netif_set_ip_info(netif_sta, &localNetwork);
        ESP_LOGI(TAG, "Set ip:" IPSTR, IP2STR(localNetwork.ip));
#endif

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );


	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);


	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
	    ESP_LOGI(TAG, "connected ");
	} else if (bits & WIFI_FAIL_BIT) {
	    ESP_LOGI(TAG, "Failed to connect");
	} else {
	    ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	ESP_ERROR_CHECK( esp_netif_get_mac(netif_sta, mac));
    }
}

void networkStatus(void)
{
	printf("got ssid %s\n", networkName);
	printf("got password %s\n", networkPass);
	printf("got channel %d\n", wirelessChannel);
	printf("got protocols %d\n", wirelessProtocols);
        printf("got ip: " IPSTR "\n", IP2STR(&localNetwork.ip));
        printf("got gw: " IPSTR "\n", IP2STR(&localNetwork.gw));
        printf("got msk: " IPSTR "\n", IP2STR(&localNetwork.netmask));
	printf("Baudrate: %d\n", baudRate);
	printf( "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	char *hn=NULL;
	esp_netif_get_hostname(netif_sta,&hn);
	printf("Hostname: %s\n",hn ? hn : "--");
	// esp_netif_t *esp_netif, const char **hostname)
	// const ip_addr_t *dns = dns_getserver(0);
	// const uint8_t *dns8 = (uint8_t*) dns;
	// printf("DNS: %d.%d.%d.%d\n",dns8[0],dns8[1],dns8[2],dns8[3]);
}


esp_err_t load_configuration(void) 
{

#define Xstrcpy(a,b) do { char *p=b; if (b) { strcpy(a,p); free(b); } } while(0)
	Xstrcpy(networkName ,getCfg("SSID"));
        printf("WIFI config:  %s\n",networkName);
	char *p;
	if ( (p=strchr(networkName,' ')) != 0 )
	    *p=0;
	Xstrcpy( networkPass , getCfg("PW") );
	if ( (p=strchr(networkPass,' ')) != 0 )
	    *p=0;
        printf("WIFI PW:  %s\n",networkPass);
	char *tmp = getCfg("PROTO");
	printf("WIFI proto:  %s\n",tmp ? tmp : " -- ");
	wirelessProtocols = 0;
	if ( tmp ) {
	    if (strchr(tmp, 'b') != NULL)
		    wirelessProtocols |= WIFI_PROTOCOL_11B;
	    if (strchr(tmp, 'g') != NULL)
		    wirelessProtocols |= WIFI_PROTOCOL_11G;
	    if (strchr(tmp, 'n') != NULL)
		    wirelessProtocols |= WIFI_PROTOCOL_11N;
	    if (strchr(tmp, 'l') != NULL)
		    wirelessProtocols |= WIFI_PROTOCOL_LR;
	    free(tmp);
	    tmp = NULL;
	}

	if ( !tunnel )
	    tunnel = getCfg("TUNNEL");
	printf("WIFI tunnel:  %s\n",tunnel ? tunnel : " -- ");
        int staticIpAddress[4], staticGateway[4], staticNetMask[4]; // STA only
        if (tunnel && sscanf(tunnel, "sta %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d",
			&staticIpAddress[0], &staticIpAddress[1], &staticIpAddress[2], &staticIpAddress[3],
			&staticGateway[0], &staticGateway[1], &staticGateway[2], &staticGateway[3],
			&staticNetMask[0], &staticNetMask[1], &staticNetMask[2], &staticNetMask[3],
			&baudRate) == 13){
	    wirelessInterface = WIFI_IF_STA;
	}

	dnsserver = getCfg("DNSSERVER");
	printf("DNS: %s\n",dnsserver ? dnsserver : " --- ");
	if ( wirelessInterface < 0  || (wirelessProtocols == 0) )
	    return ESP_FAIL;
	return ESP_OK;

}
// TODO:  make fail safe...
void tunnelWaitForPeer(void)
{
    char *nw = getCfg("SSID");
    int peerAnswered=0;
    while ( !peerAnswered ) 
    {
	if ( dnsserver )
	    myPrintf("DNS: %s\n",dnsserver);
	myPrintf("WIFI config:  %s\n",nw ? nw : " -- NULL --");
	myPrintf("ready to connect: %s\n",tunnel);
	static char line[256];
	readLine(CONFIG_UARTIF_UART,line,sizeof(line));
	printf("input line: %s\n", line);
	if (strstr(line,"Start")  != NULL ) {
	    peerAnswered=1;
	    myPrintf("\n%s",GIT_INFO);
	    printf("Go\n");
	    myPrintf("Starting Wifi\n");
	}
    }
    if(nw)
	free(nw);
}

int cmdWifiStart(int argc,char **argv)
{
    // Check config for options and if available ...
    ESP_ERROR_CHECK(load_configuration());
    initializeWifi();

    return ESP_OK;
}


#include "esp_console.h"
void registerWifi(void)
    {
    const esp_console_cmd_t cmd[] = {
	{
	.command = "wifi",
	.help = "Start or stop  wifi interface",
	.hint = NULL,
	.func = &cmdWifiStart  // useless -- shoulnd be there
	},
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
    ESP_LOGI(TAG,"Start wifi");
    // Autostart..
    cmdWifiStart(0,NULL);
}

