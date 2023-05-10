#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "decode.h"


#define max(a,b) ((a)>(b) ? (a):(b))


int 
main(int argc, char *argv[])
{
	char		buf       [MAXPAKETSIZE];
	int		fdNetwork , fdUart, l, fm;

	fd_set		fds;

	fdNetwork = 1;
	fdUart = 0;

	int		fd_active = 2;
	FD_ZERO(&fds);
	FD_SET(fdNetwork, &fds);
	FD_SET(fdUart, &fds);
	fm = max(fdUart, fdNetwork) + 1;
	while (fd_active > 0) {
		fd_set		fdsn = fds;

		select(fm, &fdsn, NULL, NULL, NULL);

		if (FD_ISSET(fdNetwork, &fds)) {
			l = read(fdNetwork, buf, sizeof(buf));
			fprintf(stderr, "Read from %d %d\n", fdNetwork, l);
			if (l == 0) {
				exit(0);
			} else if (l < 0) {
				FD_CLR(fdNetwork, &fds);
				fd_active--;
			} else
				write_formatted(fdUart, buf, l);
		}
		if (FD_ISSET(fdUart, &fds)) {
			l = read_formatted(fdUart, sizeof(buf), fdNetwork);
			if (l < 0) {
				fd_active--;
			}
		}
	}
}

