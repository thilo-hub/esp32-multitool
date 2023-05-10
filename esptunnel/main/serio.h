#define UART_DEFAULT UART_NUM_2
void initializeUartHw(int baudRate);
void startUart(int baudRate);
void uartStatus(void);
void initializeUartConsole(int baudRate);
void spiStatus(void);

extern RingbufHandle_t wifiToSerial, serialToWifi;
