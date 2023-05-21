// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <poll.h>

#include "spi.h"

#define SYSFS  "/sys/class/gpio/"
#include "checksum.h"

#define GPIO_S(cmd,value)      do                        \
	{ int fd=open(SYSFS cmd,O_WRONLY);             \
	  if ( fd < 0 ) {                                \
		  fprintf(stderr,"%s\n",SYSFS cmd);    \
		  fprintf(stderr,"Fail %d\n",__LINE__);  \
		  exit(1);                               \
	  }                                              \
	  write(fd,value,strlen(value));                 \
	  close(fd);                                     \
	} while(0)
//
// 
// use GPIO_C("P9_29","spi")   to set/sys/devices/platform/ocp/ocp\:P9_29_pinmux/state
#define GPIO_C(pin,value)  GPIO_S("../../devices/platform/ocp/ocp:" pin "_pinmux/state",value)

#define RESET_ESP(lvl)  GPIO_S("gpio49/value",lvl);
void reboot_esp()
{


	GPIO_S("gpio49/direction","out");  // reset-pin
	RESET_ESP("0");

	GPIO_S("gpio117/direction","out"); // GPIO-0
	GPIO_S("gpio117/value","1");
	RESET_ESP("1");
	usleep(10000);
	GPIO_S("gpio117/direction","in");  // Use as remote confirm

	//GPIO_S("gpio117/edge","rising");
	GPIO_S("gpio117/edge","falling");

}

int write_spi(int fd,unsigned char *buffer, int len, spi_reader_t *reader,int handle)
{
	struct spi_ioc_transfer	xfer[2];
	int			status;
	unsigned int hdr[1];


	checksum_make(hdr,buffer,len);

	memset(xfer, 0, sizeof xfer);

	// Load header
	// xfer[0].delay_usecs = 100;
	xfer[0].rx_buf = (unsigned long) hdr;
	xfer[0].tx_buf = (unsigned long) hdr;
	xfer[0].len = sizeof(hdr);
	xfer[0].cs_change = 0;

	// xfer[1].delay_usecs = 100;
	xfer[1].rx_buf = (unsigned long) buffer;
	xfer[1].tx_buf = (unsigned long) buffer;
	xfer[1].len = len;
	xfer[1].cs_change = 1;

	// fprintf(stderr,"X %d %d\n",xfer[0].len,xfer[1].len);
	status = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	if (status < 0) {
		perror("SPI_IOC_MESSAGE 1");
		return 0;
	}

	int rlen= 0xffff & *hdr;
	if ( rlen > len && rlen <= 4088 ) {
		// other side has more data... adjust read length...
		xfer[1].len = rlen-len;
		xfer[1].rx_buf += len;
		// fprintf(stderr,"Y %d\n",xfer[1].len);
		status = ioctl(fd, SPI_IOC_MESSAGE(1), xfer+1);
		if (status < 0) {
			perror("SPI_IOC_MESSAGE 2");
			return 0;
		}
		len = rlen;
	}
	// Fix CS 
	xfer[0].len = 0;
	xfer[0].rx_buf = 0;
	xfer[0].tx_buf = 0;
	xfer[0].cs_change = 0;
	status = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
	if (status < 0) {
		perror("SPI_IOC_MESSAGE-3");
		return 0;
	}
	int C = checksum_check(hdr,buffer,len);
	// fprintf(stderr,": response(%2d, %2d %2x): \n", rlen, status,C);
	if ( C < 0 )
		fprintf(stderr,"ERR: response(%2d, %2d %2x): \n", rlen, status,C);
	else if ( C > 1 && reader )
		return reader(handle,buffer,C);
	// write(1,hdr,4); write(1,buffer,4090); exit(0);
	return C;
}

static void dumpstat(const char *name, int fd)
{
	__u8	lsb, bits;
	__u32	mode, speed;

#if 1
	mode = 0x0;
	if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0) {
		perror("SPI rd_mode");
		return;
	}
	mode = 0xffff;
#endif
	if (ioctl(fd, SPI_IOC_RD_MODE32, &mode) < 0) {
		perror("SPI rd_mode");
		return;
	}
	if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb) < 0) {
		perror("SPI rd_lsb_fist");
		return;
	}
	if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
		perror("SPI bits_per_word");
		return;
	}
#if 1
	speed = 48000000;
	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		perror("SPI max_speed_hz");
		return;
	}
#endif
	speed = 0;
	if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
		perror("SPI max_speed_hz");
		return;
	}

	fprintf(stderr,"%s: spi mode 0x%x, %d bits %sper word, %d Hz max\n",
		name, mode, bits, lsb ? "(lsb first) " : "", speed);
}
int openSPI(void)
{
	const char      *name = "/dev/spidev1.0";
	int fd = open(name, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	dumpstat(name,fd);
	return fd;
}

void configureBeagle(void)
{
	GPIO_S("unexport","49");
	GPIO_S("unexport","117");
	GPIO_C("P9_24", "uart");
	GPIO_C("P9_26", "uart");
	GPIO_C("P9_28", "spi_cs");
	GPIO_C("P9_29", "spi");
	GPIO_C("P9_30", "spi");
	GPIO_C("P9_31", "spi_sclk");

	// echo spi_cs  > /sys/devices/platform/ocp/ocp:P9_28_pinmux/state

	GPIO_S("export","49");
	GPIO_S("export","117");
}
int connectESP(void)
{


	const char *gpio = SYSFS "gpio117/value";
	struct pollfd pfd;
	pfd.fd = open(gpio,O_RDONLY);
	pfd.events = POLLPRI | POLLERR; 
	if ( pfd.fd < 0 ) {
		fprintf(stderr,"Fail GPIO\n");
		exit(1);
	}

	GPIO_S("gpio117/direction","in");

	//GPIO_S("gpio117/edge","rising");
	GPIO_S("gpio117/edge","falling");

	{
		char buf[65];
		// check if  target ready
		// if not reboot and wait...
		int l=read(pfd.fd,buf,sizeof(buf));
		if ( buf[0] == '1' ) {
			reboot_esp();
			lseek(pfd.fd, 0, SEEK_SET);
			read(pfd.fd,buf,sizeof(buf));
		}
		buf[l]=0;
		while( buf[0] != '0' ) {
			poll(&pfd, 1, -1);
			lseek(pfd.fd, 0, SEEK_SET);
			read(pfd.fd,buf,sizeof(buf));
		}
		fprintf(stderr,"GPIO: %s\n",buf);
	}

	return pfd.fd;
}
