#include <stdio.h>
#include "checksum.h"

// Return:
// >4  : length of correct message
// <0  : message  failure
//  0  : empty content
//  1  : incomplete message
static const unsigned int mask[] = { 0xff,0xffff,0xffffff };
int checksum_check(unsigned int *hdr,void *buffer,int len)
{
    // Message:
    unsigned int C = *hdr++;
    int  dlen = C & 0xffff;
    if ( !dlen || dlen == 0xffff )
	    return 0; // empty buffer
    if ( dlen > 4092 )
        return -1; 
    if ( dlen > len )
        return 1;  
    int rlen = dlen & ~3;
    // if ( dlen & 3 ) return -2;  // only 4- even length...

    unsigned int *eb = (unsigned int *)(buffer+rlen);
    // unsigned int C2 = *eb;
    // unsigned int C3 = *eb & mask[(dlen&3) -1];
    if ( dlen & 3 ) {
	    C += *eb & mask[(dlen&3) -1];
    }
	    
    while(eb-- > (unsigned int *)buffer) 
        C += *eb;
    C += (C >> 16);
    if ( (C&0xffff) != 0 ) 
    {
	fprintf(stderr," %08x",hdr[-1]);
	int i; for(i=0;i<dlen;i++){ fprintf(stderr," %02x",((unsigned char *)buffer)[i]);} fprintf(stderr," (%x/%x C:%x)\n",dlen,rlen,C);
        return -2; 
    }
    return dlen;
}

void checksum_make(unsigned int *hdr, void *buffer,int len)
{
  unsigned int  C=len;
    int rlen = len & ~3;
  unsigned int *eb = (unsigned int *)(buffer+rlen);
  
    if ( len & 3 ) {
	    C += *eb & mask[(len&3) -1];
    }
  // fprintf(stderr,"L: %x - R: %x\n",len,rlen);
  while( eb-- > (unsigned int *)buffer) 
        C += *eb;
  C += (C >> 16) & 0xFFFF;
  *hdr++  = len | (-C << 16);
}

