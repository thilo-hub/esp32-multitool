GIT_HASH=`git rev-parse HEAD`
COMPILE_TIME=`date -u +'%Y-%m-%d %H:%M:%S UTC'`
GIT_BRANCH=`git branch | grep "^\*" | sed 's/^..//'`
export VERSION_FLAGS=-DGIT_HASH="\"$(GIT_HASH)\"" -DCOMPILE_TIME="\"$(COMPILE_TIME)\"" -DGIT_BRANCH="\"$(GIT_BRANCH)\""

HOST=beagle1.nispuk.com
HOST=192.168.7.2

all: flash

compile: version
	idf.py build

flash: compile
	/Users/thilo/Downloads/ESP/flash_beagle1.sh --host ${HOST}\
		-b 460800 --before default_reset --after hard_reset --chip esp32s3  \
		write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
		0x0      build/bootloader/bootloader.bin \
		0x8000   build/partition_table/partition-table.bin \
		0x10000  build/generic_gpio.bin \
		0x110000 build/storage.bin


version:
	@git log --pretty=format:'#define GIT_INFO_PRESENT%n static const char* GIT_INFO = "Version Information=[%H,%d]\r\n";' -n 1 >main/version.h.new
	@cmp main/version.h.new main/version.h || mv main/version.h.new main/version.h 

