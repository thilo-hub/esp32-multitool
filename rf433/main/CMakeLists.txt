idf_component_register(SRCS "console_main.c" "rf433_main.c" "rf433_task.c" "wifi_main.c" "checksum.c" "spio.cpp" "uart2_io.c"
                    INCLUDE_DIRS "." "${IDF_PATH}/components/wpa_supplicant/src/utils" )
spiffs_create_partition_image(storage ../esp_filesystem FLASH_IN_PROJECT)


