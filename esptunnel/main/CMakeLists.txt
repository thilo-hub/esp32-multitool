idf_component_register(
	SRCS
		tunnel_main.cpp spio.cpp checksum.c serio_task.c console.c wifi_comm.c console_main.c
		../../common/cfg_parse.c ../../common/spifs_main.c ../../common/rf433_main.c ../../common/rf433_task.c
		www_main.c uart2_io.c wifi_main.c
        INCLUDE_DIRS 
		"." "../../includes" "${IDF_PATH}/components/wpa_supplicant/src/utils"
)
spiffs_create_partition_image(storage ../esp_filesystem FLASH_IN_PROJECT)
