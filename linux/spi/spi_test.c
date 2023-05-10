#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include "spi.h"




int main(int argc, char **argv)
{
	struct timespec last_stat;
	ssize_t len;
	unsigned long total_received=0;
	unsigned long total_send=0;
	unsigned char buffer[4088];
	int ready;
	struct pollfd pfd = {
		.fd = connectESP(),
		.events = POLLPRI | POLLERR
	};
	int fd = openSPI();
	clock_gettime(CLOCK_MONOTONIC, &last_stat);
	while( (len = (read(0,buffer,sizeof(buffer)-4000)))) {
		// fprintf(stderr," - %d ",len);
			total_send += len;
			len = write_spi(fd,buffer,len,NULL);
			if (len < 0 ){
				fprintf(stderr," cnt: %d -- %ld\n",len,total_send);
				len=0; 
				//break;
			}
			write(1,buffer,len);
			total_received += len;
			{ 
#define MAX_BUF 64
			char buf[MAX_BUF];

			ready = poll(&pfd, 1, 1000); //-1);
			lseek(pfd.fd, 0, SEEK_SET);
			len = read(pfd.fd, buf, MAX_BUF);
			buf[len-1]=0;
			//fprintf(stderr,"P: %d %d %ld - %s\n",ready, len, total_send,buf);
			//for(int i=0;i<len;i++) { fprintf(stderr ," - %d",buf[i]); }
			//fprintf(stderr,"\n");
			}
								        
	}
	close(fd);
	struct timespec current;
	clock_gettime(CLOCK_MONOTONIC, &current);
	long s = current.tv_sec - last_stat.tv_sec;
	long ns= current.tv_nsec - last_stat.tv_nsec;
	double dt = s + ns/1e9;
	long total = total_send+total_received;
	double rate = total*8/dt/1000.0;
	fprintf(stderr,"Rate: %.1fkbps  Send:%.1fMbit Recv:%.1fMbit\n",rate,
			total_send*8.0/1000000.0,
			total_received*8.0/1000000.0);
	return 0;
}
