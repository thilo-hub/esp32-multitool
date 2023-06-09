#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs.h"

#define MAX_UINT32 (0xffffffff)
const char TAG[]="CFG";

static nvs_handle_t nvs=MAX_UINT32;
void commit(void)
{
    nvs_commit(nvs);
}

int importDefaultCfg(void)
{
    FILE           *fp = fopen("/data/system.cfg", "r");
    if (fp == NULL) {
	    return 1;
    }
    char           *cfg = malloc(4096);
    int		l = fread(cfg, 1, 4096, fp);
    fclose(fp);
    cfg[l] = 0;
    cfg[l + 1] = 0;
    char *equal=NULL;
    char *tag=cfg;
    for (char *p = cfg; *p; p++) {
	    if (!equal && *p == '=')
		equal = p;
	    if (*p == '\n'){
		    *p++ = 0;
		    if ( equal ) {
			*equal = 0;
			ESP_LOGI(TAG," %s -> %s",tag,equal+1);
			nvs_set_str(nvs,tag,equal+1);
			equal = NULL;
		    }
		    tag = p;
	    }
    }
    nvs_commit(nvs);
    free(cfg);
    return 0;
}
char           *
getCfg(const char *tag)
{
    static int cfg_read = 0;
    if (nvs == MAX_UINT32 ) {
	    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
	    if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	    }
	}
    if (nvs != MAX_UINT32 ) {
	char *value=NULL;
	size_t l=0;
	esp_err_t err = nvs_get_str(nvs, tag, NULL, &l);
	if (err == ESP_OK && l>0){
	    value = malloc(l);
	    err = nvs_get_str(nvs, tag, value,&l);
	    if ( err == ESP_OK )
		return value;
	}

    }
    if ( cfg_read++ )
	return NULL; // not available
	
    importDefaultCfg();
    return getCfg(tag); // 2nd try
}


#ifdef TEST
int 
main(int argc, char *argv[])
{
	while (argc-- > 1) {
		char           *s = getcfg(*++argv);
		printf("R: %s ->%s<\n", *argv, s);
	}
	return 0;
}
#endif
