#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#define IOCTL_BASE 'W'
#define GET_HDDGEO _IOR(IOCTL_BASE, 1, struct hd_geometry)


int main() {
    int fd = open("/dev/lab2", O_RDWR);

    if (fd == -1) {
        fputs("Can't open device\n", stderr);
        exit(EXIT_FAILURE);
    }

    struct hd_geometry geo;
    if (ioctl(fd, GET_HDDGEO, &geo)) {
        fprintf(stderr, "Can't perform HDDGEO. Error code: %lu\n", geo.start);
        exit(EXIT_FAILURE);
    }

    printf("Start: %lu\n", geo.start);
    printf("Heads: %d\n", geo.heads);
    printf("Cylinders: %d\n", geo.cylinders);
    printf("Sectors: %d\n", geo.sectors);
    return 0;
}
