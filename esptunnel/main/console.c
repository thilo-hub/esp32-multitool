#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "console.h"
#include "serio.h"



int myPrintf(const char *fmt,...)
{
   char *buffer=NULL;
   va_list ap;
   va_start(ap, fmt);
   int l = vasprintf(&buffer,fmt, ap);
    uart_write_bytes(CONFIG_UARTIF_UART, buffer,l);
    free(buffer);
    return l;
}


char *readLine(int uart,char *line,int len)
{
		// static char line[256];
		size_t pos = 0;

		while (1)
		{
			char ch = 255;

			if (uart == UART_NUM_0 ) {
				ch = getchar();
				putchar(ch);
				fflush(stdout);
			} else {
				uart_read_bytes(uart, &ch, 1, portMAX_DELAY);
			}
			if (ch == 255) // non-blocking response
			{
				vTaskDelay(1);
				continue;
			}
			else if (ch == '\n')
			{
				break;
			}

			line[pos++] = ch;
			if ((pos+1) >= len)
				break;
		}

		line[pos] = '\0';

		return line;
}


#if 0
void cmdLine(void *)
{
	char line[256];
	printf("*****  TOOL Waiting *****\n");
	while(1) {
		readLine(UART_NUM_0,line,sizeof(line));
		if ( strcmp(line,"status") == 0 ) {
			// networkStatus();
			uartStatus();
#if 0
		} else if ( strcmp(line,"reconnect") == 0 ) {
			initializeWifi();
#endif
		} else {
			printf("Read: %s\n",line);
		}
	}
}
#endif

