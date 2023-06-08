
// Data transfer UART

//    config GPIO_UARTIF_TX
//	depends on WIFI_TUNNEL
//	int "UART tx-pin output for ip comm"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
//	default 17
//	help
//		Uart for ip communications should have RTS/CTS pins
#define CONFIG_GPIO_UARTIF_TX 17


//    config GPIO_UARTIF_RX
//	depends on WIFI_TUNNEL
//	int "UART rx-pin input for ip comm"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
//	default 16
//	help
//		Uart for ip communications should have RTS/CTS pins
#define CONFIG_GPIO_UARTIF_RX 16


//    config GPIO_UARTIF_RTS
//	depends on WIFI_TUNNEL
//	int "UART rts-pin input for ip comm"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
//	default 14
//	help
//		Uart for ip communications should have RTS/CTS pins
#define CONFIG_GPIO_UARTIF_RTS 14


//    config GPIO_UARTIF_CTS
//	depends on WIFI_TUNNEL
//	int "UART cts-pin output for ip comm"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
//	default 15
//	help
//		Uart for ip communications should have RTS/CTS pins
#define CONFIG_GPIO_UARTIF_CTS 15


// SPIO configuration

//    config SPIO_GPIO_HANDSHAKE 
//    	depends on SPIO_ENABLED
//	int "SPI handshake output to host"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//	default 0 
#define CONFIG_SPIO_GPIO_HANDSHAKE 0

//    config SPIO_GPIO_MOSI 
//    	depends on SPIO_ENABLED
//	int "SPI input"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//        default 11
#define CONFIG_SPIO_GPIO_MOSI 11

//    config SPIO_GPIO_MISO 
//    	depends on SPIO_ENABLED
//	int "SPI output"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//        default 13
#define CONFIG_SPIO_GPIO_MISO 13

//    config SPIO_GPIO_SCLK 
//    	depends on SPIO_ENABLED
//	int "SPI SCLK input"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//        default 12
#define CONFIG_SPIO_GPIO_SCLK 12

//    config SPIO_GPIO_CS 
//    	depends on SPIO_ENABLED
//	int "SPI CS input"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//        default 10
#define CONFIG_SPIO_GPIO_CS 10

// UART boot interface

//    config GPIO_UARTCON_TX  
//        depends on UARTCON_ENABLE
//	default 37
//        int "Uart TX pin"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
#define CONFIG_GPIO_UARTCON_TX 37

//    config GPIO_UARTCON_RX  
//        depends on UARTCON_ENABLE
//	default 36
//        int "Uart RX pin"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
#define CONFIG_GPIO_UARTCON_RX 36

#define CONFIG_GPIO_UARTCON_RTS  -1
#define CONFIG_GPIO_UARTCON_CTS  -1

//    config GPIO_RESET_PIN
//        depends on BASIC_AUTH
//	int "Gpio to toggle to reset Beaglebone"
//	default 38
//	help
//	    GPIO pin connected to BeagleBone reset. Watchout, some GPIO's (<24 I think) emit spikes/glitches during
//	    reboot. This can cause unwanted results.
//	    Consult the ESP32* documentations which pin's are OK.
//	    the default works on esp32-S3
#define CONFIG_GPIO_RESET_PIN 38


// RF433 com
//    config GPIO_RF433_TX
//        int "GPIO output pin to rf433 transmitter"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_OUT_RANGE_MAX
//        default 8 if IDF_TARGET_ESP32C2 || IDF_TARGET_ESP32H4 || IDF_TARGET_ESP32H2
//        default 20
//        help
//            GPIO pin number to be used to send rf433 data stream
#define CONFIG_GPIO_RF433_TX 18

//    config GPIO_RF433_RX
//        int "GPIO input pin from rf433 receiver"
//        range ENV_GPIO_RANGE_MIN ENV_GPIO_IN_RANGE_MAX
//        default 3
//        help
//            GPIO pin number to be used as rf433 rx
#define CONFIG_GPIO_RF433_RX 3
