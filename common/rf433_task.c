#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "rf433.h"


static gptimer_handle_t gptimer = NULL;
static QueueHandle_t rf433_msg_queue = NULL;

static pattern_t pat_send;
static uint8_t *pattern_ptr=NULL;
static uint32_t pattern_current=0;
static uint32_t pattern_count=0;

static bool IRAM_ATTR send_pattern(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
        BaseType_t high_task_wakeup = pdFALSE;
	static uint32_t level=0;
        TaskHandle_t task_to_notify = (TaskHandle_t)user_data;
	if ( pattern_ptr != NULL ) {
		if ( pattern_current == 1 ) {
		        uint32_t p = 1<<24;
			p += *pattern_ptr++;
			p += *pattern_ptr++ << 8;
			p += *pattern_ptr++ << 16;
			pattern_current = p;
		}
		if ( pattern_count-- > 0) {
			    level ^= 1;
			    uint16_t nxt = pat_send.lkup[pattern_current & 0x7];
			    gptimer_alarm_config_t alarm_config = {
			      .alarm_count = edata->alarm_value + nxt, 
			    };
			    gptimer_set_alarm_action(timer, &alarm_config);
			    pattern_current >>= 3;
		} else {
			// end of transmission...
			// maybe we want to send something??
			ESP_ERROR_CHECK(gptimer_stop(gptimer)); // run -> enable
			ESP_ERROR_CHECK(gptimer_disable(gptimer));  // enable -> init
			pattern_ptr = NULL;
			level = 0;
			xTaskNotifyFromISR(task_to_notify, edata->alarm_value, eSetValueWithOverwrite, &high_task_wakeup);
		}
	}
        gpio_set_level(GPIO_OUTPUT_IO_0, level);
    return high_task_wakeup == pdTRUE;
}

static void rf433_message_task(void *arg)
{
        uint32_t tof_ticks;
        TaskHandle_t cur_task = xTaskGetCurrentTaskHandle();
	for(;;) {
	    if(xQueueReceive(rf433_msg_queue, &pat_send, portMAX_DELAY)) {
		// start sending and wait for confirmation
		gpio_isr_handler_remove(GPIO_INPUT_IO_0);
		gpio_set_level(GPIO_OUTPUT_IO_0, 0);

		// Assume timer is in init state!
		// Setup sending..
		pattern_count = pat_send.cnt;
		pattern_ptr = pat_send.pattern;
		pattern_current = 1; 
		gptimer_event_callbacks_t cbs = {
		    .on_alarm = send_pattern
		};
		ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, cur_task));
		ESP_ERROR_CHECK(gptimer_enable(gptimer));  // init -> enable

		gptimer_set_raw_count(gptimer, 0ull);
		gptimer_alarm_config_t alarm_config1 = {
			.alarm_count = 1000,
		};
		ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));
		ESP_ERROR_CHECK(gptimer_start(gptimer));
		if (xTaskNotifyWait(0x00, ULONG_MAX, &tof_ticks, pdMS_TO_TICKS(10000)) == pdTRUE) {
		    printf("Notification: %ld\n",tof_ticks);
		}
	    }
	}
}

QueueHandle_t create_rf433_manager(gptimer_handle_t lgptimer) 
{
	gptimer = lgptimer;
	rf433_msg_queue = xQueueCreate(5, sizeof(pattern_t));
	assert(rf433_msg_queue != NULL);
	xTaskCreate(rf433_message_task, "rf433 Sender", 4*2048, NULL, 10, NULL);
 return rf433_msg_queue;
}

			
