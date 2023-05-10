
all:
	idf.py build
	/Users/thilo/Downloads/ESP/flash_beagle1.sh \
	-b 460800 --before default_reset --after hard_reset --chip esp32s3  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/generic_gpio.bin 0x110000 build/storage.bin

