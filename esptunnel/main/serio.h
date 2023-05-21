// #define UART_DEFAULT UART_NUM_2

void initializeUartNetworkHw(int baudRate);
void startUartNetwork(int baudRate);
void uartStatus(void);
void spiStatus(void);

extern RingbufHandle_t wifiToSerial, serialToWifi;
