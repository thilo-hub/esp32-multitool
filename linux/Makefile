
CFLAGS=-g -O
all: decode-esp
OBJS=decode.o main_decode.o serio.o tun1.o spi/xdata-1.o spi/checksum.o

decode-esp: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

install: decode-esp esp32-wifi.service
	install decode-esp /usr/local/bin/
	install esp32-wifi.service /lib/systemd/system/
	systemctl enable esp32-wifi


# tun1.c
# main_tst_decode.c
#
clean:
	rm -f decode *.o spi/*.o
