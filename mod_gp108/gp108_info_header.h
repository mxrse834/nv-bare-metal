#ifndef GP108_IOCTL
#define GP108_IOCTL

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <linux/types.h>
#include <sys/ioctl.h>
#endif
struct gp108_info
{
    __u32 vendor;
    __u32 device;
    __u32 gpu_id;
    __u32 revision;
};

struct bar0_addr
{
    __u64 offset;
    __u64 value;
};

#define GPU_INFO _IOR('G', 0x00, struct gp108_info);
#define gpu_reg_read _IOR('G',0x01, struct bar0_addr);

#endif