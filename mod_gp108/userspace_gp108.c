#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include "gp108_info_header.h"

int main()
{
    int fd = open("/dev/gp108", O_RDONLY);
    struct gp108_info gpinf = {};
    if (ioctl(fd, GPU_INFO, &gpinf) < 0)
    {
        perror("IOCTL");
        close(fd);
        return -1;
    }
    printf("GPU_INFO\n");
    printf("vendor   : %04x\n", gpinf.vendor);
    printf("device   : %04x\n", gpinf.device);
    printf("gpu_id   : %02x\n", gpinf.gpu_id);
    printf("revision : %02x\n", gpinf.revision);
    close(fd);
    return 0;
}
