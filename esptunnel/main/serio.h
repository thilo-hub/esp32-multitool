//
#define UART_DEFAULT UART_NUM_2

void		uartTunInitHw(int baudRate);
void		uartTunStart(int baudRate);
void		uartTunStatus(void);
void		spiStatus (void);

extern RingbufHandle_t wifiToSerial, serialToWifi;
