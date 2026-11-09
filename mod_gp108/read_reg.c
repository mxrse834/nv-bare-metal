#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include"gp108_info_header.h"

typedef unsigned long long ull;

int main(int argc,char* argv[])
{
    if(argc != 2)
    {
        printf("invalid number of args");
        return -1;
    }
    ull mem_addr = strtoull(argv[1],'\0',16);
    int fd = open("/dev/gp108",O_RDONLY);
    struct bar0_addr rg = {};
    ioctl(fd,gpu_reg_read,&rg);

    return 0;
}
