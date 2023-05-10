#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "checksum.h"
// Test using:
/* 
  cc -DTEST_CK xdata-1.c -o xdata-ck && dd if=/dev/urandom bs=508 count=1 2>/dev/null | ./xdata-ck 
 */

void main(){
	char buffer[4000];
	unsigned int hdr;
	int len;
	while( (len = read(0,buffer,sizeof(buffer))) > 0){
		checksum_make(&hdr,buffer,len);
		//buffer[4] ^= 1;
		buffer[len-1] ^= 1;
		int v = checksum_check(&hdr,buffer,len);
		if ( v == len ) { fprintf(stderr,"Failed recognize error\n"); exit(1); }
		buffer[len-1] ^= 1;
		v = checksum_check(&hdr,buffer,len);
		if ( v != len ) {
			fprintf(stderr,"Failed checksum %d\n",v);
			fprintf(stderr,"Header:  %08x\n",hdr);
			exit (1);
		}
	}
	fprintf(stderr,"Pass\n");
	exit (0);
}

