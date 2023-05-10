typedef int  spi_reader_t(int handle, unsigned char *buffer, int len);
int write_spi(int fd,unsigned char *buffer, int len, spi_reader_t *reader,int handle);
int openSPI(void);
int connectESP(void);

