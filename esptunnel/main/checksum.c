#include "checksum.h"
#include <stdio.h>


// Return:
// >4  : length of correct message
// <0  : message  failure
//  0  : empty content
//  1  : incomplete message
static const unsigned int mask[] = { 0xff,0xffff,0xffffff };

int checksumCheck(unsigned int *hdr,void *buffer,int len)
{
    // Message:
    unsigned int C = *hdr++;
    int  dlen = C & 0xffff;
    if ( !dlen || dlen == 0xffff )
	    return 0; // empty buffer
    if ( dlen > 4092 ) {
	// fprintf(stderr,"LE: %x\n",C);
        return -1; 
     }
    if ( dlen > len )
        return 1;  
   
    int rlen = dlen & ~3;
    unsigned int *eb = (unsigned int *)(buffer+rlen);

    // unsigned int C3 = *eb & mask[(dlen&3) -1];
    if ( dlen & 3 ) { 
            C += *eb & mask[(dlen&3) -1];
    }           

    // unsigned int C2 = C;
    while(eb-- > (unsigned int *)buffer) 
        C += *eb;
    C += (C >> 16);
    if ( (C&0xffff) != 0 ) {
	 // fprintf(stderr," %08x %08x",C2,C3);
	 // for(int i=-4;i<dlen;i++){ fprintf(stderr," %02x",((unsigned char *)buffer)[i]);} fprintf(stderr," (%x/%x C:%x)\n",dlen,rlen,C);
        return -2; 
	}
    return dlen;
}

void checksumBuild(void *obuffer, void *buffer,int len)
{
  unsigned int  C=len;
  unsigned int  rlen = len & ~3;

  unsigned int *eb = (unsigned int *)(buffer+rlen);
  unsigned int *op = (unsigned int *)(obuffer+rlen);

  if ( len & 3 ) {
         C += *op = *eb & mask[(len&3) -1];
  }

  while( eb-- > (unsigned int *)buffer) 
        C += (*--op) = *eb;
  C += (C >> 16) & 0xFFFF;
  (*--op)  = len | (-C << 16);
}
#if 1

void checksumMake(unsigned int *hdr, void *buffer,int len)
{
  unsigned int  C=len;
  unsigned int  rlen = len & ~3;
  unsigned int *eb = (unsigned int *)(buffer+rlen);
    if ( len & 3 ) {
            C += *eb & mask[(len&3) -1];
    }
  
  while( eb-- > (unsigned int *)buffer) 
        C += *eb;
  C += (C >> 16) & 0xFFFF;
  *hdr++  = len | (-C << 16);
}
#endif

#ifdef TEST_CK
// Test using:
/* 
  cc -DTEST_CK xdata-1.c -o xdata-ck && dd if=/dev/urandom bs=508 count=1 2>/dev/null | ./xdata-ck 
 */
void main(){
	char buffer[4000];
	unsigned int hdr;
	int len;
	while( (len = read(0,buffer,sizeof(buffer))) > 0){
		checksumMake(&hdr,buffer,len);
		//buffer[4] ^= 1;
		int v = checksumCheck(&hdr,buffer,len);
		if ( v != len ) {
			fprintf(stderr,"Failed checksum %d\n",v);
			fprintf(stderr,"Header:  %08x\n",hdr);
			exit (1);
		}
	}
	exit (0);
}

#endif
