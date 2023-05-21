idf_component_register(SRCS "console_main.c" "wifi_main.c" "checksum.c" "uart2_io.c"
			"../../common/rf433_main.c" "../../common/rf433_task.c" 
			"../../common/cfg_parse.c" "../../common/spifs_main.c"
                    INCLUDE_DIRS "." "../../includes" "${IDF_PATH}/components/wpa_supplicant/src/utils" )
spiffs_create_partition_image(storage ../esp_filesystem FLASH_IN_PROJECT)


