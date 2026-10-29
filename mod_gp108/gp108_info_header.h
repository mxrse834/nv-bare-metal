#ifndef GP108_IOCTL
#define GP108_IOCTL

#include <ioctl.h>
#include <types.h>

struct gp108_info
{
    __u32 vendor;
    __u32 device;
    __u32 gpu_id;
    __u32 revision;
};

#define GPU_INFO _IOR('G', 0x00, struct gp108_info)

#endif