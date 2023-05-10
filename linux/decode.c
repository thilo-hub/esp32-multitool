#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "decode.h"
#include <termios.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
char *
write_formatted(int fd, char *buffer, int *plen)
{
	int len = *plen;
	static uint8_t		header [4 + MAXPAKETSIZE];
	uint8_t        *p = header;
	uint8_t		cksum = 0;
	// skip ipv4 header
	// len -= 4;
	// buffer +=4;
	cksum += (*p++ = 0xAA);
	p++;
	cksum += (*p++ = len & 0xff);
	cksum += (*p++ = (len >> 8) & 0xFF);

	//Compute checksum
	for (size_t i = 0; i < len; i++)
		cksum += (*p++ = (uint8_t) *buffer++);
	header[1] = -cksum;
	// tcdrain(fd);
	len += 4;
	int l = write(fd, (void *)header, len);
	if ( l != len ) {
		if (l<4) {
			fprintf(stderr,"BAAAD");
			exit(1);
		}

		*plen = len - l;
		return (void *)header+l;
	}
	*plen = 0;
	return NULL;

}

ssize_t blk_read(int fd,char *buffer,ssize_t len)
{
	char *rb=buffer;
	while(len >0) {
		ssize_t l=read(fd,rb,len);
		if (l < 0 ) {
			perror("Failed reading");
			exit(0);
		}
		if (l>0) {
			len -= l;
			rb += l;
		} 
	}
	len = rb-buffer;
	// fprintf(stderr,"R: %d\n",len);
	return len;
}


ssize_t Yread_formatted(int fd, int maxlen, int fdOut)
{
	static uint8_t	read_buffer[1600 * 4];
	ssize_t		len;
       
	while( blk_read(fd, read_buffer,1) == 1 ) {
		if ( read_buffer[0] != 0xAA )
			continue;
		if ( blk_read(fd,read_buffer,3) == 3 ) {
			uint8_t cksum=0xAA;
			uint8_t *s = read_buffer;
			cksum += *s++;
			uint8_t c1,c2;
			cksum += (c1=*s++);
			cksum += (c2=*s++);
			len = c1 | c2<<8;
			if ( len > maxlen )
				continue;
			if ( len != blk_read(fd,read_buffer,len) )
				continue;
			s = read_buffer;
			for(int i=0;i<len;i++) {
				cksum += *s++;
			}
			if ( cksum != 0 ){
				fprintf(stderr,"Bad 0x%x %d\n",cksum,len);
				continue;
			}
			write(fdOut,read_buffer,len);
			return 0;
		}
	}
}

//Read buffered in and write packets
ssize_t forward_formatted(int fdSerial, int maxlen, int fdNet)
{
	static uint8_t	read_buffer[1600 * 4];
	static uint8_t *end_buffer = read_buffer;
	static uint8_t *sync_found = NULL;
	if (!sync_found)
		end_buffer = read_buffer;
	ssize_t		len = read(fdSerial, end_buffer, sizeof(read_buffer) - (end_buffer - read_buffer));
	uint8_t        *s = end_buffer;
	ssize_t rv = 0;
	end_buffer += len;

	while (len > 0) {
		if (!sync_found)
			sync_found = memchr(s, 0xaa, len);
		if (!sync_found || (end_buffer - sync_found) < 4)
			return rv;
		//need more data
		s = sync_found;
		uint8_t		csum = *s++;
		csum += *s++;
		uint8_t		l1    , l2;
		csum += (l1 = *s++);
		csum += (l2 = *s++);
		uint16_t	plen = l1 | (l2 << 8);
		len = end_buffer - s;
		if (plen > maxlen) {
			//bad packet length probably sync wrong..
			sync_found++;
			len = end_buffer - sync_found;
			if ( len > 0)
				memmove(read_buffer, sync_found, len);
			end_buffer = read_buffer + len;
			sync_found = NULL;
			fprintf(stderr,"Bad plen (%d)\n",plen);
			continue;
		}
		
		if (plen > len) // not yet complete
			return 0;
		
		// we have a packet - check csum
		for (uint16_t l = plen; l > 0; l--)
			csum += *s++;
		if (csum != 0) {
			//try next sync...
			fprintf(stderr, "Bad\n");
			sync_found++;
			len = end_buffer - sync_found;
			if ( len > 0)
				memmove(read_buffer, sync_found, len);
			end_buffer = read_buffer + len;
			sync_found = NULL;
			continue;
		}
		int w = write(fdNet, sync_found+4 , plen);
		if ( w != plen )
			fprintf(stderr, "Write failuer: %d (%d)\n",plen,w);
		rv += plen;

		len = end_buffer - s;
		if ( len > 0 )
			memmove(read_buffer, s, len);
		sync_found = NULL;
	}
	return rv;
}



