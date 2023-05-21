#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include "lwip/sockets.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/queue.h"
#include "serio.h"
#if CONFIG_UARTCON_ENABLE

// #define CONFIG_UARTCON_UART  2
// #define CONFIG_GPIO_UARTCON_TX  17
// #define CONFIG_GPIO_UARTCON_RX  16
#define CONFIG_GPIO_UARTCON_RTS  14
#define CONFIG_GPIO_UARTCON_CTS  15
// #define CONFIG_UARTCON_PORT 9876

#define CONFIG_EXAMPLE_IPV4 1

#define KEEPALIVE_IDLE              30
#define KEEPALIVE_INTERVAL          30
#define KEEPALIVE_COUNT             30



static const char *TAG = "UartIO";

//RingbufHandle_t wifiToSerial = NULL, serialToWifi = NULL;

// callback receiving bytes from tcp-port -- handing to uart

// task waiting form bytes from uart and sending to tcp connection


void initializeUartConsoleHw(int baudRate)
{
	uart_config_t uartConfig;
	uartConfig.baud_rate = baudRate;
	uartConfig.data_bits = UART_DATA_8_BITS;
	uartConfig.parity = UART_PARITY_DISABLE;
	uartConfig.stop_bits = UART_STOP_BITS_1;
	uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uartConfig.rx_flow_ctrl_thresh = 0; // 122;
	uartConfig.source_clk = UART_SCLK_APB;
	ESP_ERROR_CHECK(uart_set_pin(CONFIG_UARTCON_UART, CONFIG_GPIO_UARTCON_TX, CONFIG_GPIO_UARTCON_RX, CONFIG_GPIO_UARTCON_RTS, CONFIG_GPIO_UARTCON_CTS));
	ESP_ERROR_CHECK(uart_driver_install(CONFIG_UARTCON_UART, 256, 256, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(CONFIG_UARTCON_UART, &uartConfig));

}


static void uartRxTask(void *p)
{
	const int sock = (int) p;
	int len=1;
	int written=1;
	printf("Starting U-RX\n");
	uart_write_bytes(CONFIG_UARTCON_UART, "Starting\n",9);
	while (len>0) {
	    uint8_t buffer[256]; // bigger than MTU
	    len = uart_read_bytes(CONFIG_UARTCON_UART, buffer, 1, portMAX_DELAY);
	    if ( len > 0 ) {
		len += uart_read_bytes(CONFIG_UARTCON_UART, buffer+1, sizeof(buffer)-2, 0); // Maybe - get remaining....
		buffer[len]=0;
		written = send(sock, buffer,len,0);
		if (written < 0) {
		    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
		    // Failed to retransmit, giving up
		    vTaskDelete(NULL);
		    return;
		} else {
		    // uart_rsum += len;
		}
	    }
	}
}


static void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[128];
    TaskHandle_t  rxTask = NULL;
    xTaskCreate(uartRxTask, "uart_rx", 2048, (void *)sock, tskIDLE_PRIORITY + 2, &rxTask);
    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            // ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
	    uart_write_bytes(CONFIG_UARTCON_UART, rx_buffer,len);
        }
    } while (len > 0);
    if ( rxTask != NULL )	
	vTaskDelete(rxTask);
}


static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

#ifdef CONFIG_EXAMPLE_IPV4
    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(CONFIG_UARTCON_PORT);
        ip_protocol = IPPROTO_IP;
    }
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    if (addr_family == AF_INET6) {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(CONFIG_UARTCON_PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_UARTCON_PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
#ifdef CONFIG_EXAMPLE_IPV4
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
#endif
#ifdef CONFIG_EXAMPLE_IPV6
        if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
#endif
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}


#if 0
static void uartTxTask(void *)
{
	size_t len;

	while (true)
	{
		char *buffer = (char*)xRingbufferReceive(wifiToSerial, &len, portMAX_DELAY);
		uart_write_bytes(CONFIG_UARTCON_UART, ep,len);
		// uart_wsum += len;
		vRingbufferReturnItem(wifiToSerial, buffer);
	}
}
#endif

int startUartConsole(int argc, char **argv)
{
    	static TaskHandle_t  tcpServer = NULL;
	if ( tcpServer == NULL ) {
	    initializeUartConsoleHw(115200);
	    int baudRate=115200;
	    ESP_ERROR_CHECK( uart_wait_tx_done(CONFIG_UARTCON_UART, portMAX_DELAY) );
	    ESP_ERROR_CHECK( uart_set_baudrate(CONFIG_UARTCON_UART, baudRate));

	    // xTaskCreate(uartTxTask, "uart_tx", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
	    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, &tcpServer);
	}
	return 0;
}

void register_uart(void)
{
    

    const esp_console_cmd_t cmd[] = {
    {
        .command = "uart",
        .help = "Start secondary uart interface",
        .hint = NULL,
        .func = &startUartConsole,
    }
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	    ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
}

#endif

