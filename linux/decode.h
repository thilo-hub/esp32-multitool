ssize_t forward_formatted(int fd, int maxlen, int fdOut);
ssize_t Yread_formatted(int fd, int maxlen, int fdOut);
char *write_formatted(int fd, char *buffer, int *len);
ssize_t read_tunnel(int fd, int maxlen, int fdOut);
ssize_t blk_read(int fd,char *buffer,ssize_t len);
#define MAXPAKETSIZE  1600

int tun_alloc(char *dev);
int open_uart(const char *device);
int fullspeed_uart(int fd);
