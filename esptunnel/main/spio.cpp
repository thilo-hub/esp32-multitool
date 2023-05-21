/* SPI Slave example, receiver (uses SPI Slave driver to communicate with sender)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

//#include <unistd.h>

#include "checksum.h"
#include <freertos/ringbuf.h>

#include "hw_config.h"
#if CONFIG_SPIO_ENABLED
//Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
void my_post_setup_cb(spi_slave_transaction_t *trans);
void my_post_trans_cb(spi_slave_transaction_t *trans);

#define CONFIG_SPIO_RX_ENABLED 1  // always enabled..
#define CONFIG_SPIO_TX_ENABLED 1

#define SPIO_GPIO_HANDSHAKE ((gpio_num_t) CONFIG_SPIO_GPIO_HANDSHAKE)
#define SPIO_GPIO_CS ((gpio_num_t) CONFIG_SPIO_GPIO_CS)
#define SPIO_GPIO_SCLK ((gpio_num_t) CONFIG_SPIO_GPIO_SCLK)
#define SPIO_GPIO_MOSI ((gpio_num_t) CONFIG_SPIO_GPIO_MOSI)
#define SPIO_GPIO_MISO ((gpio_num_t) CONFIG_SPIO_GPIO_MISO)

void setup_spi(void)
{
int ret;

    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=SPIO_GPIO_MOSI,
        .miso_io_num=SPIO_GPIO_MISO,
        .sclk_io_num=SPIO_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
	.data4_io_num = -1,
	.data5_io_num = -1,
	.data6_io_num = -1,
	.data7_io_num = -1,
	.max_transfer_sz = 0,
	.flags = 0,
	.isr_cpu_id = INTR_CPU_ID_AUTO,
	.intr_flags = 0
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg={
        .spics_io_num=SPIO_GPIO_CS,
        .flags=0,
        .queue_size=3,
        .mode=0,
        .post_setup_cb=my_post_setup_cb,
        .post_trans_cb=my_post_trans_cb
    };

    //Configuration for the handshake line
    gpio_config_t io_conf={
        .pin_bit_mask=(1<<SPIO_GPIO_HANDSHAKE),
        .mode=GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type=GPIO_INTR_DISABLE,
    };

    //Configure handshake line as output
    gpio_config(&io_conf);
    //Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
    gpio_set_pull_mode(SPIO_GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPIO_GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SPIO_GPIO_CS, GPIO_PULLUP_ONLY);

    //Initialize SPI slave interface
    ret=spi_slave_initialize(RCV_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    gpio_set_level(SPIO_GPIO_HANDSHAKE, 0);
}

void my_post_setup_cb(spi_slave_transaction_t *trans) {
    gpio_set_level(SPIO_GPIO_HANDSHAKE, 0);
}


//Called after transaction is sent/received. We use this to set the handshake line low.
extern RingbufHandle_t serialToWifi,wifiToSerial;

void my_post_trans_cb(spi_slave_transaction_t *trans) {
    
    gpio_set_level(SPIO_GPIO_HANDSHAKE, 1);
#if 0
    if ( trans->tx_buffer )
	    vRingbufferReturnItem(wifiToSerial, (void*)trans->tx_buffer);
    trans->tx_buffer = NULL;
#endif
}


static unsigned int spi_read=0, spi_write=0, spi_err=0;

extern "C" 
void spiStatus(void)
{
  printf("SPI Received: %u\n",spi_read);
  printf("SPI Send:     %u\n",spi_read);
  printf("SPI Errors:   %u\n",spi_err);
}

void spiRxTxTask(void *)
{
  size_t len;
#define portMIN_DELAY 2
#define SPI_MAXLEN 4000  
 static WORD_ALIGNED_ATTR char sendbuf[4097]="";
 static WORD_ALIGNED_ATTR char recvbuf[4097]="";

  spi_slave_transaction_t t;
  while(true) {
	memset(&t, 0, sizeof(t));
#if CONFIG_SPIO_TX_ENABLED
	char *buffer = (char*)xRingbufferReceive(wifiToSerial, &len, 0);
	if (buffer) {
		spi_write += len;
		checksum_make((unsigned int*)buffer,(unsigned int *)(buffer+4),len-4);
		memcpy(sendbuf,buffer,len);
		vRingbufferReturnItem(wifiToSerial, (void*)buffer);
		t.tx_buffer=sendbuf;
	
		// t.tx_buffer=buffer;
	} else {
		*(int *)sendbuf = 0; //flag no data
	}
#else
		*(int *)sendbuf = 0; //flag no data
#endif
        //Set up a transaction of ?? bytes to send/receive
        t.length=SPI_MAXLEN*8;
        t.rx_buffer=recvbuf;
	*(int *)recvbuf = 0;

	int ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
	if ( ret != ESP_OK ) {
		printf("tx error: %d\n",ret);
	}
	unsigned int *hdr = (unsigned int *)recvbuf; // trans->rx_buffer;
	int len;
	if ( (len = checksum_check(hdr,hdr+1,t.trans_len/8)) > 2 ) {
	    // TODO: check if fewer copies possible...
	    xRingbufferSend(serialToWifi, &hdr[1], len, 1000);
	    spi_read += len;
	} else if (len != 0) {
	    spi_err++;
	}
  }
}

extern "C" {
void dummy_tests(void)
{
#define COMPILE_CHECK( should_size, is_size ) do{ switch(1==1){ case is_size != is_size: /*FALSE*/; case (should_size == is_size): /*noop*/ return;  }}while(0)
	COMPILE_CHECK(2, sizeof(short));
	COMPILE_CHECK(4, sizeof(int));
	COMPILE_CHECK(8, sizeof(long long));
}
}

void initializeSpi(void)
{
	xTaskCreate(spiRxTxTask, "spi", 2048, nullptr, tskIDLE_PRIORITY , nullptr);
}


#endif
