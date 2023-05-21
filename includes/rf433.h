
#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_RF433_TX
#define GPIO_INPUT_IO_0     CONFIG_GPIO_RF433_RX


typedef struct __attribute__((__packed__)) {
	uint16_t lkup[8];
	uint16_t cnt;
	uint8_t  pattern[256*3];
} pattern_t;


//
// Create task waiting for rf433 messages
// the task manages sending from queue
// The initializer expects a configured timer and returns the queue identifier
// 
QueueHandle_t create_rf433_manager(gptimer_handle_t lgptimer);
