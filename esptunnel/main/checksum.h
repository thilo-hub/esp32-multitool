
#ifdef __cplusplus
extern "C"
{
#endif
int checksum_check(unsigned int *hdr,void *buffer,int len);
void checksum_make(unsigned int *hdr, void *buffer,int len);
/* build output buffer which is guaranteed to be len + 4bytes */
void checksum_build(void *obuffer, void *inbuffer, int len);
#ifdef __cplusplus
}
#endif

