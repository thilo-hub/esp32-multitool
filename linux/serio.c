#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

#define BAUD B115200
#define BAUD2 B3500000

int fullspeed_uart(int fd)
{
	struct termios  config;
	if(tcgetattr(fd, &config) < 0) { 
		fprintf(stderr,"Error...\n"); 
		exit(1);  
	}
	if(cfsetispeed(&config, BAUD2) < 0 || cfsetospeed(&config, BAUD2) < 0) {
	    fprintf(stderr,"Error..."); 
	    exit(1); 
	}
	if(tcsetattr(fd, TCSAFLUSH, &config) < 0) { 
		fprintf(stderr,"Error...\n"); 
		exit(1);  
	}
}

int open_uart(const char *device)
{
	int fd = open(device, O_RDWR | O_NOCTTY );
	// int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	// int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	struct termios  config;
	if(fd == -1) {
	  fprintf(stderr, "failed to open port\n" );
	  exit(1);
	}
	if(!isatty(fd)) { 
		fprintf(stderr,"No uart...\n");
		exit(1);
	}
	if(tcgetattr(fd, &config) < 0) { 
		fprintf(stderr,"Error...\n"); 
		exit(1);  
	}
	config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
			    INLCR | PARMRK | INPCK | ISTRIP | IXON);

	config.c_oflag = 0;
	config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	config.c_cflag &= ~(CSTOPB | CSIZE | PARENB);
	config.c_cflag |= CS8;
	config.c_cflag |= CRTSCTS;;
	config.c_cc[VMIN]  = 1;
	config.c_cc[VTIME] = 0;
	if(cfsetispeed(&config, BAUD) < 0 || cfsetospeed(&config, BAUD) < 0) {
	    fprintf(stderr,"Error..."); 
	    exit(1); 
	}
	if(tcsetattr(fd, TCSAFLUSH, &config) < 0) { 
		fprintf(stderr,"Error...\n"); 
		exit(1);  
	}
	{ 
		struct termios newconf;
		if(tcgetattr(fd, &newconf) < 0) { 
			fprintf(stderr,"Error...\n"); 
			exit(1);  
		}
		if ( config.c_cflag != newconf.c_cflag ) {
			fprintf(stderr,"c_cflag  %x != %x\n",config.c_cflag,newconf.c_cflag);
		}
	}

	return fd;
}

#ifdef TEST_MAIN

int main(int argc,char *argv[]) {

	int fd = open_uart(argv[1]);
#define write_io write
#define read_io  read
	int fm = fd+1;
	char buf[2048];
	size_t l;
        fd_set fds;
	while(1){
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		FD_SET(0, &fds);

		select(fm, &fds, NULL, NULL, NULL);

		if( FD_ISSET(0, &fds) ) {
		   l = read(0,buf,sizeof(buf));
		   write_io(fd,buf,l);
		}
		if( FD_ISSET(fd, &fds) ) {
		   l = read_io(fd,buf,sizeof(buf));
		   write(1,buf,l);
		}
        }
	exit(0);
}
#endif
