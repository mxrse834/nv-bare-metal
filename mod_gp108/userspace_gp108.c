#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "gp108_info_header.h"

int main()
{
    int fd = open("/dev/gp108", O_RDONLY);
    if (fd < 0)
    {
        perror("OPEN");
        return -1;
    }
    struct gp108_info gpinf = {};
    int ret = 0;
    if ((ret = ioctl(fd, GPU_INFO, &gpinf)) < 0)
    {
        printf("%d", ret);
        fflush(stdout);
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

/*
#define GPU_INFO _IOR('G', 0x00, struct gp108_info)

Expands to:

(((2U) << (((0+8)+8)+14)) | ((('G')) << (0+8)) | (((0x00)) << 0) | ((((sizeof(struct gp108_info)))) << ((0+8)+8)))

*/

/*
[pnglinkpc@pnglinkpc-h81 mod_gp108]$ sudo ./userspace_gp108
GPU_INFO
vendor   : 10de
device   : 1d01
gpu_id   : 38
revision : a1

*/