/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_console.h"
// #include <sys/dirent.h>
#include "base64.h"
#include "spifs.h"
#if CONFIG_RF433



#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) )
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) )
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "rf433";
static QueueHandle_t rf433_msg_queue = NULL;

gptimer_handle_t gptimer = NULL;
volatile int full=0;

#include "rf433.h"

typedef struct {  
    // uint16_t counter;
    uint16_t c[4*4024];
} capture_t;
static capture_t capture = {0};
static uint32_t noise_ctr = 0;
const int max_idle = 12000;
const uint32_t noise_threshold = 140;

// Capture interrupt timings and send to capture task once a longer break is detected
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
	static long long  last_up=0;
	static long long last_down=0;
	unsigned long long count=0;

	ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &count));
	uint32_t level = gpio_get_level(GPIO_INPUT_IO_0);

//      ,-----------------------------------,               ,------------
//      '                                   '---------------'
//      ,----------, ,---------------, ,----,               ,------------
//      '          '-'               '-'    '---------------'
//      ,----------------------------, ,----,               ,------------
//      '                            '-'    '---------------'
//                         ^
//                         '- tsig
//      Start on up
//                                     Stop  on up ( last delta < noise ) -> ignore
//                                                          - last delta 
        if ( !level ) { // falling
		last_down = count;
 		return;
        }
        // level -> 1 // rising
	uint32_t dtL = count - last_down;
	if ( dtL > max_idle )
		dtL = max_idle;
        if ( dtL < noise_threshold ) {
		noise_ctr++;
	        return;
        }
        // rising && >noise
        // capture both values
	uint32_t dtH = last_down - last_up;;
	if ( dtH > max_idle )
		dtH = max_idle;

	last_up = count;

	int  ct = capture.c[0]+1;
	if ( ct  < (sizeof(capture.c)/sizeof(capture.c[0]) - 1) ){
		capture.c[ct++] = (dtH << 1) + 0;
		capture.c[ct] = (dtL << 1) + 1;
		capture.c[0] = ct;
	} else {
	    // stop capture
	    // overflow 
	    // reset pattern capture...
	    // capture.c[0] = 0;
	    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
	}
}

static void capture_output(void)
{
    uint16_t *p=capture.c;
    uint16_t cnt = *p++;
    uint16_t *pend = p;
    p[cnt] = (max_idle<<1) + 1;
    cnt++;
    printf("\nCapture:  %u Noise: %lu\n",cnt,noise_ctr);
    while(cnt-- > 0 ) {
	if ( *pend++ == ((max_idle<<1) + 1) ) {
	    int ncnt = pend - p;
	    printf("\nLength: %d Data:",ncnt);
	    while( p < pend ) {
		int dt=*p++;
		int l = dt&1;
		dt >>=1;
		printf(" %c%u",l? '+' : '-',dt);
	    }
	    printf("\n");
	}
   }
   capture.c[0] = 0;
}

static int capture_running=0;
static int cmd_capture(int argc, char **argv)
{
	if ( capture_running )
		return 1;
	printf("Start Capturing\n");

        capture.c[0] = 0;
        gpio_set_level(GPIO_OUTPUT_IO_0, 0);
	gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
       ESP_ERROR_CHECK(gptimer_enable(gptimer));  // init -> enable
       ESP_ERROR_CHECK(gptimer_start(gptimer));

	capture_running = 1;
	return 0;
}
static int cmd_stop(int argc, char **argv)
{
    if ( !capture_running )
		return 1;
    capture_running = 0;
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    ESP_ERROR_CHECK(gptimer_stop(gptimer)); // run -> enable
    ESP_ERROR_CHECK(gptimer_disable(gptimer));  // enable -> init

    capture_output();
    return 0;
}
void start_timer()
{
    ESP_LOGI(TAG, "Create timer handle");
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
}

esp_err_t send_file(char *file) 
{
    esp_err_t rv = ESP_FAIL;
    char *buffer = malloc(sizeof(pattern_t));
    sprintf(buffer,"/data/%s",file);
    FILE *fd = fopen(buffer,"r");
    if ( !fd  ){
	printf("Failed open %s\n",buffer);
    } else {
	int len = fread(buffer,1,sizeof(pattern_t),fd);
	if ( len > 0 ) {
	    printf("Send %d\n",len);
	    if (errQUEUE_FULL == xQueueSend(rf433_msg_queue, buffer, pdMS_TO_TICKS(1000)) ) {
		printf("Queue full...\n");
	    }
	    else {
		rv = ESP_OK;
	    }
	}
	fclose(fd);
    }
    if ( buffer) 
	free(buffer);
return rv;
}

int cmd_play2(int argc, char **argv)
{
    if (argc < 2 )
    	return 1;
    while( argc-- > 1 ) {
	if (send_file(*++argv) != ESP_OK)
		return 1;
    }
    return 0;
}
static int cmd_send2(int argc, char **argv)
{
    int verbose=0;
	if ( argc > 2 && strcmp(argv[1],"-v") == 0) {
		argc--;
		argv++;
		verbose++;
	}
	if ( argc != 2 ) {
		printf("Usage:  send2 [-v] {base64}\n");
		return 1;
	}

	argv++;
	size_t len = strlen(*argv);
	void *bin = base64_decode(*argv,len,&len);
	if (!bin) {
		printf("Data error in pattern\n");
		return 1;
	}
	if (len > sizeof(pattern_t) ) {
		printf("Data overflow in pattern\n");
		free(bin);
		return 1;
	}
	if (verbose) {
	    pattern_t *pat = bin;
	    for(int i=0;i<8;i++) {
		printf("L %d : %d\n",i,pat->lkup[i]);
	    }
	    uint8_t *pt = pat->pattern;
	    uint32_t p=1;
	    uint32_t cnt = pat->cnt;
	    while( cnt-- > 0){
		if ( p == 1 ) {
		    p = 1<<24;
		    p += *pt++;
		    p += *pt++ << 8;
		    p += *pt++ << 16;
		    //printf(" %lx",p);
		}
		printf(" %d", pat->lkup[(p&7)]);
		p >>= 3;
	    }
	    printf("\n");
	}
	if (errQUEUE_FULL == xQueueSend(rf433_msg_queue, bin, pdMS_TO_TICKS(1000)) ) {
	    printf("Buffer full...\n");
	}

	free(bin);
	return 0;
}

static void setup_hw(void) 
{

    // Output config
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Input config
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_level(GPIO_OUTPUT_IO_0, 0);

    // gpio_evt_queue = xQueueCreate(30, sizeof(capture_t));
    // assert(gpio_evt_queue != NULL);

    start_timer();

    rf433_msg_queue = create_rf433_manager(gptimer);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());

}
static int cmd_status(int argc, char **argv)
{
	printf("Noise: %lu\n",noise_ctr);
	noise_ctr = 0;
return 0;
}

void register_rf433(void)
{
    setup_hw();
    const esp_console_cmd_t cmd[] = {
    {
        .command = "capture",
        .help = "Capture RF433 signals",
        .hint = NULL,
        .func = &cmd_capture,
    },{
        .command = "stop",
        .help = "Stop Capture RF433 signals",
        .hint = NULL,
        .func = &cmd_stop,
    },{
        .command = "send2",
        .help = "send pattern from base64 ",
        .hint = NULL,
        .func = &cmd_send2,
    },{
        .command = "status",
        .help = "Get current RF433 Status",
        .hint = NULL,
        .func = &cmd_status,
    },{
        .command = "play2",
        .help = "Play file BIN",
        .hint = NULL,
        .func = &cmd_play2,
    }
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	    ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
}
#endif
