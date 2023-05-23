#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
//
#include "driver/gpio.h"
//
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_console.h"
#include <sys/dirent.h>
#include "base64.h"
#include "spifs.h"

static int 
cmd_cat(int argc, char **argv)
{
	if (argc < 2) {
		return 1;
	}
	char           *file = argv[1];

	char		buffer    [1024];
	sprintf(buffer, "/data/%s", file);
	FILE           *fp = fopen(buffer, "r");
	int		l = fread(buffer, 1, sizeof(buffer), fp);
	printf("R: %d\n%s\n===\n", l, buffer);
	fclose(fp);
	return 0;
}
static int 
cmd_store(int argc, char **argv)
{
	if (argc < 3) {
		return 1;
	}
	char           *file = argv[1];
	char           *b64 = argv[2];

	size_t		len = strlen(b64);
	void           *bin = base64_decode(b64, len, &len);
	if (!bin) {
		printf("Data error in pattern\n");
		return 1;
	}
	char		buffer    [256];
	sprintf(buffer, "/data/%s", file);
	FILE           *fp = fopen(buffer, "w");
	fwrite(bin, 1, len, fp);
	fclose(fp);
	free(bin);
	return 0;
}

static int 
cmd_ls(int argc, char **argv)
{
	char           *p = "/data";
	if (argc > 1)
		p = argv[1];
	DIR            *dir = opendir(p);
	struct dirent  *e;
	printf("Folder: %s\n", p);
	while (dir != NULL && (e = readdir(dir)) != NULL) {
		char		t = 'f';
		if (e->d_type == DT_DIR)
			t = 'd';
		printf("E: %c %s\n", t, e->d_name);
	}
	if (dir)
		closedir(dir);
	return 0;
}


void 
register_spifs(void)
{
	//setup_hw();
	const esp_console_cmd_t cmd[] = {
		{
			.command = "Store",
			.help = "Store play file",
			.hint = NULL,
			.func = &cmd_store,
		}, {
			.command = "cat",
			.help = "catenate file ",
			.hint = NULL,
			.func = &cmd_cat,
		}, {
			.command = "ls",
			.help = "List files",
			.hint = NULL,
			.func = &cmd_ls,
		}
	};
	for (int i = 0; i < (sizeof(cmd) / sizeof(cmd[0])); i++) {
		ESP_ERROR_CHECK(esp_console_cmd_register(cmd + i));
	}
}
