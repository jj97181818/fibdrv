#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[64];
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        printf("Writing to " FIB_DEV ", returned the sequence 1\n");
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    return 0;
}
