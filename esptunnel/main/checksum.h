
#ifdef __cplusplus
extern		"C"
{
#endif
	int		checksumCheck(unsigned int *hdr, void *buffer, int len);
	void		checksumMake(unsigned int *hdr, void *buffer, int len);
	/* build output buffer which is guaranteed to be len + 4bytes */
	void		checksumBuild(void *obuffer, void *inbuffer, int len);
#ifdef __cplusplus
}
#endif
