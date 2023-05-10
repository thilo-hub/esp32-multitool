all: flash

HOST="10.1.1.73"
FLASHER= esptool
FLASHER= /Users/thilo/Downloads/ESP/flash_beagle1.sh --host ${HOST}

compile: version
	test -d folder || mkdir folder
	idf.py build

flash: compile
	${FLASHER} \
		-b 460800 --before default_reset --after hard_reset \
		--chip esp32s3  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 \
		build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/esp32-tuntap.bin \
		0x110000 build/storage.bin

version:
	@git log --pretty=format:'#define GIT_INFO_PRESENT%n static const char* GIT_INFO = "Version Information=[%H,%d]\r\n";' -n 1 >main/version.h.new
	@cmp main/version.h.new main/version.h || mv main/version.h.new main/version.h 


