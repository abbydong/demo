#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#define MAXLINE 1024

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readline(int fd, void *vptr, size_t maxlen);
