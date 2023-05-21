#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "decode.h"
#include <linux/if_tun.h>
#include <string.h>
#include <signal.h>
#include <poll.h>

#define max(a,b) ((a)>(b) ? (a):(b))
#define USE_SPI 1
#define  USE_FORK

#include "spi/spi.h"
int chat_wifisetup(int fd,char *tundev);
extern void reboot_esp( void );

int writetun(int handle,unsigned char *buffer,int len)
{
	// fprintf(stderr,"pass  %d\n",len);
	int l = write(handle,(char *)buffer,len);
	// fprintf(stderr," X:  %d\n",l);
	return l;
}


int main(int argc, char *argv[])
{
   int fdUart = -1,fdTun = -1,l,fm;
   fd_set fds;

   if(argc < 3) {
      fprintf(stderr,"Usage: bridge tty tap|tun\n");
      exit(1);
   }
   configureBeagle();
   reboot_esp();

   if ( fdUart < 0 && strstr(argv[1],"/dev/tty") ) {
	   fprintf(stderr,"Open Uart %s\n",argv[1]);
	   fdUart = open_uart(argv[1]);
   }
   if ( fdUart < 0 )
	   fdUart = tun_alloc(argv[1]);
   if ( fdUart < 0 )
	   fdUart = open(argv[1],O_RDWR);
   if ( fdUart < 0 ) {
	   fprintf(stderr,"Error: %d %s\n",fdUart,argv[1]);
	   exit(1);
   }

   fdTun = tun_alloc(argv[2]);
   if ( fdTun < 0 )
	   fdTun = open(argv[2],O_RDWR);
   if ( fdTun < 0 ) {
	   fprintf(stderr,"Error: %d %s\n",fdTun,argv[2]);
	   exit(1);
   }

   fm = max(fdUart, fdTun) + 1;
   int fdEsp = connectESP();
   int fdSpi = openSPI();
#ifdef USE_FORK
   int r;
   pid_t pid;
   char buf[1600];
   while(!chat_wifisetup(fdUart,argv[2]) ) {
	   fprintf(stderr,"Waiting..\n");
   }
   if ( (pid=fork()) == 0 ) {
	   while( 1 ){
	       l = Yread_formatted(fdUart, sizeof(buf), fdTun) ;
	   }
	   fprintf(stderr,"Exit UART reader...\n");
	   exit(0);
   } 
   while( l = read(fdTun,buf,sizeof(buf)) ) {
#if USE_SPI
	char rdy[2];
	lseek(fdEsp,0,SEEK_SET);
	read(fdEsp,rdy,sizeof(rdy));
	while ( rdy[0] == '0'  ) {
		// fprintf(stderr,"S");
		l=write_spi(fdSpi,buf,l,writetun,fdTun);
		if ( l == 0 ) 
			break;
		l=0;
		lseek(fdEsp,0,SEEK_SET);
		read(fdEsp,rdy,sizeof(rdy));
	} 
	if (l>0){
		// fprintf(stderr,"U");
           char *p=write_formatted(fdUart,buf,&l);
	   if ( l != 0 )
		   fprintf(stderr,"Failure writing..");
	}

#else
           char *p=write_formatted(fdUart,buf,&l);
	   if ( l != 0 )
		   fprintf(stderr,"Failure writing..");
#endif
   }
   fprintf(stderr,"Exit tun reader...\n");

   kill(pid, SIGKILL);
   exit(0);
#else
   // use poll
   //
struct pollfd pfd[3];
	int spiRdy=0;
	if ( fdSpi < 0 ) {
		perror(" Failed open..");
		exit(1);
	}
	int nfds = 0;
	int connected = 0;
	pfd[0].fd = fdUart;
	pfd[0].events = POLLIN;
	nfds++;

	pfd[1].fd = fdTun;
	pfd[1].events = POLLIN;
	nfds++;

	pfd[2].fd = fdEsp;
	pfd[2].events = POLLPRI;
//	nfds++;

	unsigned char *obuff=NULL;
	unsigned int   olen=0;
	char buf[1600];

	if(1){
			char buf[64];
			lseek(pfd[2].fd,0,SEEK_SET);
			int len = read(pfd[2].fd,buf,sizeof(buf));
			fprintf(stderr,"GPIO: %s\n",buf);
			spiRdy =  buf[0] == '0';
	}

	while (1) {


		int rc = poll(pfd, nfds, 1000);      
		if (rc < 0) {

			printf("\npoll() failed!\n");
			return -1;
		}
		if (rc == 0) {
			printf(".");
		}
	    

		// Write to UART
		if (pfd[0].revents & POLLOUT) {
			fprintf(stderr,"#");
			pfd[0].revents &= ~POLLOUT;
			if ( olen > 0) {
				int l = write(pfd[0].fd,obuff,olen);
				if ( l < 0 ) {
					perror("Failed..");
					exit(1);
				}else{
				olen -= l;
				obuff +=l;
				fprintf(stderr,"*%d",l);
				if ( olen == 0 ) {
					pfd[0].events &= ~POLLOUT;
				}
				}
			}
		}
			
		// Read from UART
		if (pfd[0].revents & POLLIN) {
			pfd[0].revents &= ~ POLLIN;
			// fprintf(stderr,"0");
			if ( connected )
			       forward_formatted(pfd[0].fd, 1600, fdTun) ;
			else
			       connected = chat_wifisetup(pfd[0].fd);
		}
		// Read from tun
		if (pfd[1].revents & POLLIN) {
			pfd[1].revents = 0;
			 fprintf(stderr,"1");
			l = read(pfd[1].fd,buf,sizeof(buf));
#if USE_SPI
			char rdy[2];
			lseek(pfd[2].fd,0,SEEK_SET);
			read(pfd[2].fd,rdy,sizeof(rdy));
			if ( rdy[0] == '0'  ) {
			 // fprintf(stderr," Write SPI  %d\n",l);
				int len ;
				while( (len = write_spi(fdSpi,buf,l,writetun,fdTun)) > 2 ) {
					spiRdy = 1;
				 // fprintf(stderr," Read SPI %d\n",len);
				 fprintf(stderr," Read SPI %d\n",len);
					l=0;
				}
				if (len < 0 ){
					fprintf(stderr," cnt: %d -- %ld\n",len,0);
					len=0; 
					//break;
				}
			} else 
#endif
			{
				olen = read(pfd[1].fd,buf,sizeof(buf));
				obuff = write_formatted(fdUart,buf,&olen);
				if ( olen != 0 ) {
					pfd[0].events |= POLLOUT;
					fprintf(stderr,"%%%d",olen);
				}
			}
		}
		if (pfd[2].revents & POLLPRI) {
			fprintf(stderr,"2");
			pfd[2].revents = 0;

#if USE_SPI
			char buf[64];
			lseek(pfd[2].fd,0,SEEK_SET);
			int len = read(pfd[2].fd,buf,sizeof(buf));
			// fprintf(stderr,"GPIO: %s\n",buf);
			spiRdy =  buf[0] == '0';
			fprintf(stderr,"%c",spiRdy ? '+' : '-');
#endif
		}
	}
			

#endif

}


int chat_wifisetup(int fd,char *tundev)
{
	static int blen=0;
	static char buf[1256];
	int l = read(fd,buf+blen,sizeof(buf)-blen-1);
	// write(1,buf+blen,l);
	blen += l;
	buf[blen]=0;
	// fprintf(stderr," Got fdUart %d %d\n",l,blen);
	char *nl;
	char if1[256];
	char route[256];
	while (blen>0 && (nl = memchr(buf,'\n',blen)) != NULL ){
		*nl++ = 0;
		int sl = nl-buf;
		fprintf(stderr," S: %d <%s>\n",sl,buf);
#if 1
		char dns[60],search[60];
		if ( strstr(buf,"DNS:") &&
		       sscanf(buf,"DNS: %s %s",&dns,&search) == 2 ) {
			fprintf(stderr,"D:%s\n",dns ? dns : "--NO--");
			fprintf(stderr,"S:%s<\n",search);
			FILE *fp=fopen("/etc/resolv.conf","w");
			if ( fp ) {
				fprintf(fp,"search %s\nnameserver %s\n",search,dns);
				fclose(fp);
				fprintf(stderr,"Resolv.conf set %s\n",dns);
			}
		}
#endif
		if ( strstr(buf,"ready to connect:") ) {
			// sta 10.1.1.73 10.1.1.9 255.255.255.0 3500000
			int ip[4],gw[4],ms[4],baud;
			if ( sscanf(buf,"ready to connect: sta %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d",
						ip,ip+1,ip+2,ip+3,
						gw,gw+1,gw+2,gw+3,
						ms,ms+1,ms+2,ms+3,
						&baud) == 13 ) {
				sprintf(if1,"ifconfig %s %d.%d.%d.%d netmask %d.%d.%d.%d up",tundev,ip[0],ip[1],ip[2],ip[3],ms[0],ms[1],ms[2],ms[3]);
				sprintf(route,"route add default gw %d.%d.%d.%d",gw[0],gw[1],gw[2],gw[3]);
				fprintf(stderr,"Got config for \n%s\n%s\nbaud:%d\n",if1,gw,baud);
				//sleep(1);
				write(fd,"Start\n\n\n",6);
			}
		} else if ( strstr(buf,"XXready to receive configuration") ) 
		{ 
			// fprintf(stderr,"Got %s\n",buf); 
			int fi = open("cfg.txt",O_RDONLY);
			while( (l = read(fi,buf,sl)) > 0){
				write(fd,buf,l);
			}
		}
		else if ( strstr(buf,"Starting Wifi") ) {
				fullspeed_uart(fd);
				fprintf(stderr,"%s\n",buf); 
				system("ifconfig eth0 down");
				system(if1);
				system(route);
				return 1;
		}
		memmove(buf,nl,blen-sl);
		blen -= sl;
	}
	return 0;
}

