#include <driver/gpio.h>
#include <driver/uart.h>
#include "serio.h"
static long	uart_rsum = 0;
static long	uart_rerr = 0;
static long	uart_wsum = 0;


//Main uart - 0 console
void 
initializeUartConsole(int baudRate)
{
#ifdef CONFIG_IDF_TARGET_ESP32S3
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_45, GPIO_NUM_44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#else
// THere are a number of places where the S3 is different. Currently only the S3 is ported.
// most changes would be trivial (pin numbers for example)
#error - not maintained..
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#endif
}

//Uart - 2 network IN
void 
uartTunInitHw(int baudRate)
{
	uart_config_t	uartConfig;
	uartConfig.baud_rate = baudRate;
	uartConfig.data_bits = UART_DATA_8_BITS;
	uartConfig.parity = UART_PARITY_DISABLE;
	uartConfig.stop_bits = UART_STOP_BITS_1;
	uartConfig.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
	uartConfig.rx_flow_ctrl_thresh = 122;
	uartConfig.source_clk = UART_SCLK_APB;
	ESP_ERROR_CHECK(uart_set_pin(CONFIG_UARTIF_UART, CONFIG_GPIO_UARTIF_TX, CONFIG_GPIO_UARTIF_RX, CONFIG_GPIO_UARTIF_RTS, CONFIG_GPIO_UARTIF_CTS));
	ESP_ERROR_CHECK(uart_driver_install(CONFIG_UARTIF_UART, 256, 256, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(CONFIG_UARTIF_UART, &uartConfig));
}

#if CONFIG_UARTIF_ENABLED
static void 
uartTunRxTask(void *)
{
	while (true) {
		static uint8_t	buffer[2000];
		//bigger than MTU

		// Sync and receive header
		do {
			uart_read_bytes(CONFIG_UARTIF_UART, &buffer[0], 1, portMAX_DELAY);
		} while (buffer[0] != 0xAA);

		uart_read_bytes(CONFIG_UARTIF_UART, &buffer[1], 3, portMAX_DELAY);

		uint16_t	len = buffer[2] | (buffer[3] << 8);
		if ((4 + len) > sizeof(buffer)) {
			uart_rerr++;
			continue;
			//packet too big
		}
		uart_read_bytes(CONFIG_UARTIF_UART, &buffer[4], len, portMAX_DELAY);

		uint8_t		acc = 0;
		for (size_t i = 0; i != 4 + len; i++)
			acc += buffer[i];

		if (acc == 0)
			//good checksum
		{
			xRingbufferSend(serialToWifi, &buffer[4], len, 0);
			uart_rsum += len;
		} else {
			uart_rerr++;
			//printf("Bad %d - %d\n", acc, len);
		}
	}
}


static void 
uartTunTxTask(void *)
{
	size_t		len;

	while (true) {
		char           *buffer = (char *)xRingbufferReceive(wifiToSerial, &len, portMAX_DELAY);
		char           *dp = buffer + 4;
		char           *ep = buffer + len;
		uint8_t		C = 0xaa;
		len -= 4;
		while (ep-- > dp)
			C += *ep;
		C += *ep-- = (len >> 8) & 0xff;
		C += *ep-- = (len) & 0xff;
		*ep-- = -C;
		*ep = 0xaa;
		uart_write_bytes(CONFIG_UARTIF_UART, ep, len + 4);
		uart_wsum += len + 4;
		vRingbufferReturnItem(wifiToSerial, buffer);
	}
}
#endif

void 
uartTunStatus(void)
{
	printf("UART Received: %ld\n", uart_rsum);
	printf("UART Send:     %ld\n", uart_wsum);
	printf("UART Errors:   %ld\n", uart_rerr);
}

#if CONFIG_UARTIF_ENABLED
void 
uartTunStart(int baudRate)
{
	ESP_ERROR_CHECK(uart_wait_tx_done(CONFIG_UARTIF_UART, portMAX_DELAY));
	ESP_ERROR_CHECK(uart_set_baudrate(CONFIG_UARTIF_UART, baudRate));

	xTaskCreate(uartTunRxTask, "uart_rx", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
	xTaskCreate(uartTunTxTask, "uart_tx", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}
#endif
