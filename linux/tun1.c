#include <linux/if.h>
#include <linux/if_tun.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

ssize_t strscpy_pad(char *dest, const char *src, size_t count)
{
 ssize_t written=0;
 while(*src && count-- >0) {
	 *dest++ = *src++;
	 written++;
 }
 while(count-- >0) {
	 *dest++ = 0;
 }
 return written;
}


int tun_alloc(char *dev)
{
    struct ifreq ifr;
    int fd, err;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
       exit(1); // return tun_alloc_old(dev);

    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if( *dev )
       strscpy_pad(ifr.ifr_name, dev, IFNAMSIZ);

    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
       close(fd);
       return err;
    }
    strcpy(dev, ifr.ifr_name);
    return fd;
}
