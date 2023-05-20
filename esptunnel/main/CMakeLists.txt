idf_component_register(
	SRCS
		main.cpp spio.cpp checksum.c serio_task.c console.c wifi_comm.c console_main.c
		../../rf433/main/cfg_parse.c
        INCLUDE_DIRS 
		"." "../../includes" "${IDF_PATH}/components/wpa_supplicant/src/utils"
)
spiffs_create_partition_image(storage ../esp_filesystem FLASH_IN_PROJECT)
