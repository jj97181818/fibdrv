#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char write_buf[] = "testing writing";
    int offset = 93; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        long long time1 = write(fd, write_buf, 0);
        long long time2 = write(fd, write_buf, 1);
        long long time3 = write(fd, write_buf, 2);

        printf("%d %lld %lld %lld\n", i, time1, time2, time3);
    }

    close(fd);
    return 0;
}
