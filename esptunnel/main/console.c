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


/*
 * Output at network uart for handshakes
 */

int 
myPrintf(const char *fmt,...)
{
	char           *buffer = NULL;
	va_list		ap;
	va_start(ap, fmt);
	int		l = vasprintf(&buffer, fmt, ap);
	uart_write_bytes(CONFIG_UARTIF_UART, buffer, l);
	free(buffer);
	return l;
}

/*
 * Universal readline with character echoes
 */

char           *
readLine(int uart, char *line, int len)
{
	//static char	line[256];
	size_t		pos = 0;

	while (1) {
		char		ch = 255;

		if (uart == UART_NUM_0) {
			ch = getchar();
			putchar(ch);
			fflush(stdout);
		} else {
			uart_read_bytes(uart, &ch, 1, portMAX_DELAY);
		}
		if (ch == 255)
			//non - blocking response
		{
			vTaskDelay(1);
			continue;
		} else if (ch == '\n') {
			break;
		}
		line[pos++] = ch;
		if ((pos + 1) >= len)
			break;
	}

	line[pos] = '\0';

	return line;
}


