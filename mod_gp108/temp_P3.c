#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include "gp108_info_header.h"

/*
HOLY GRAIL
static const struct nvkm_device_chip
nv138_chipset = {
    .name = "GP108",
    .acr      = { 0x00000001, gp108_acr_new },
    .bar      = { 0x00000001, gm107_bar_new },
    .bios     = { 0x00000001, nvkm_bios_new },
    .bus      = { 0x00000001, gf100_bus_new },
    .devinit  = { 0x00000001, gm200_devinit_new },
    .fault    = { 0x00000001, gp100_fault_new },
    .fb       = { 0x00000001, gp102_fb_new },
    .fuse     = { 0x00000001, gm107_fuse_new },
    .gpio     = { 0x00000001, gk104_gpio_new },
    .i2c      = { 0x00000001, gm200_i2c_new },
    .imem     = { 0x00000001, nv50_instmem_new },
    .ltc      = { 0x00000001, gp102_ltc_new },
    .mc       = { 0x00000001, gp100_mc_new },
    .mmu      = { 0x00000001, gp100_mmu_new },
    .therm    = { 0x00000001, gp100_therm_new },
    .pci      = { 0x00000001, gp100_pci_new },
    .pmu      = { 0x00000001, gp102_pmu_new },
    .privring = { 0x00000001, gm200_privring_new },
    .timer    = { 0x00000001, gk20a_timer_new },
    .top      = { 0x00000001, gk104_top_new },
    .ce       = { 0x0000000f, gp102_ce_new },
    .disp     = { 0x00000001, gp102_disp_new },
    .dma      = { 0x00000001, gf119_dma_new },
    .fifo     = { 0x00000001, gp100_fifo_new },
    .gr       = { 0x00000001, gp108_gr_new },
    .nvdec    = { 0x00000001, gm107_nvdec_new },
    .sec2     = { 0x00000001, gp108_sec2_new },
    .sw       = { 0x00000001, gf100_sw_new },


    static const struct nvkm_mmu_func
    gp100_mmu = {
    .dma_bits = 47,
    .mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
    .mem = {{ -1,  0, NVIF_CLASS_MEM_GF100}, gf100_mem_new, gf100_mem_map },
    .vmm = {{ -1,  0, NVIF_CLASS_VMM_GP100}, gp100_vmm_new },
    .kind = gm200_mmu_kind,
    .kind_sys = true,
};

};
*/

static int probe(struct pci_dev *dev, const struct pci_device_id *id);
static void remove(struct pci_dev *dev);

struct gp108_dev
{
    struct pci_dev *pdev;

    void __iomem *bar0;
    resource_size_t bar0_len;
    resource_size_t bar0_start;

    struct miscdevice miscdev;
    dma_addr_t pb_dma;
    void *pb_cpu;
};

static const struct pci_device_id ids[] = {
    {PCI_DEVICE(0x10de, 0x1d01)},
    {0}};

static struct pci_driver my_driver = {
    .name = "nv_probe",
    .id_table = ids,
    .probe = probe,
    .remove = remove,
};

static int gp108_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct gp108_dev *gp108;
    gp108 = container_of(miscdev, struct gp108_dev, miscdev);
    file->private_data = gp108;
    return 0;
}

static long gp108_gpu_info(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd != GPU_INFO)
    {
        return -ENOTTY;
    }
    /// GPU_INFO_OUPUT
    u32 pmc_id;
    struct gp108_dev *gp108;
    struct gp108_info gp108_info = {};
    gp108 = file->private_data;

    pmc_id = readl(gp108->bar0 + 0x0000); //[ 3250.696994] 0x138000a1

    u8 gpu_id = (pmc_id >> 20);
    u8 revision = (pmc_id) & 0xff;

    gp108_info.device = gp108->pdev->device;
    gp108_info.gpu_id = gpu_id;
    gp108_info.revision = revision;
    gp108_info.vendor = gp108->pdev->vendor;

    if (copy_to_user((void __user *)arg, &gp108_info, sizeof(gp108_info)))
    {
        return -EFAULT;
    }

    return 0;
}

static const struct file_operations gp108_fops = {
    .owner = THIS_MODULE,
    .open = gp108_open,
    .unlocked_ioctl = gp108_gpu_info,
};

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int err;

    struct gp108_dev *gp108 = kzalloc(sizeof(struct gp108_dev), GFP_KERNEL);
    if (!gp108)
    {
        err = -ENOMEM;
        goto struct_free;
    }
    gp108->pdev = dev;
    // u32 p0, p1, p2, p3, pfifo_intr;

    err = pci_enable_device(gp108->pdev);
    if (err)
        goto disable;

    pci_set_master(gp108->pdev);

    err = pci_request_regions(gp108->pdev, "nv_probe");
    if (err)
        goto disable;

    /// SETTING DMA MASK STREAMING AND COHERENT
    err = dma_set_mask_and_coherent(&gp108->pdev->dev, DMA_BIT_MASK(47));

    if (err)
    {
        dev_warn(&gp108->pdev->dev,
                 "47-bit DMA mask unavailable (%d); trying 32-bit\n",
                 err);

        err = dma_set_mask_and_coherent(&gp108->pdev->dev, DMA_BIT_MASK(32));
        if (err)
        {
            dev_err(&gp108->pdev->dev, "Failed to set DMA mask\n");
            goto release;
        }
    }

    /// MMIO
    gp108->bar0_start = pci_resource_start(gp108->pdev, 0); // pass 0 as offset for BAR0
    gp108->bar0_len = pci_resource_len(gp108->pdev, 0);
    gp108->bar0 = pci_iomap(dev, 0, gp108->bar0_len);
    if (!gp108->bar0)
    {
        err = -ENOMEM;
        goto release;
    }

    // MISC SETUP INIT
    gp108->miscdev.minor = MISC_DYNAMIC_MINOR;
    gp108->miscdev.name = "gp108";
    gp108->miscdev.fops = &gp108_fops;
    gp108->miscdev.parent = &gp108->pdev->dev;
    if (misc_register(&gp108->miscdev))
    {
        printk(KERN_ERR "Unable to register MISCDEVICE\n");
    }

    // SAVING STATE
    // pb_dma holds the physical device address of the allocated buffer may or may not be the direct physical address of the buffer, depending on the architecture and memory management(IOMMU).
    // while pb_cpu holds the virtual address that can be used by the CPU to access the buffer.
    // The dma_alloc_coherent function ensures that the allocated memory is suitable for DMA operations, meaning it is contiguous in physical memory and not cached, which is important for device drivers that perform direct memory access.
    gp108->pb_cpu = dma_alloc_coherent(&gp108->pdev->dev, 4096, &gp108->pb_dma, GFP_KERNEL);
    if (!gp108->pb_cpu)
    {
        printk(KERN_ERR "nv_probe: dma_alloc_coherent failed\n");
        err = -ENOMEM;
        goto free_mem;
    }
    printk(KERN_INFO "nv_probe: DMA address 0x%llx, CPU virtual %p\n", gp108->pb_dma, gp108->pb_cpu);
    pci_set_drvdata(gp108->pdev, gp108);

    return 0;

free_mem:
    dma_free_coherent(&gp108->pdev->dev, 4096, gp108->pb_cpu, gp108->pb_dma);
unmap:
    pci_iounmap(gp108->pdev, gp108->bar0);
release:
    pci_release_regions(gp108->pdev);
disable:
    pci_clear_master(gp108->pdev);
    pci_disable_device(gp108->pdev);
struct_free:
    kfree(gp108);
    return err;
}

static void remove(struct pci_dev *dev)
{
    struct gp108_dev *pvt = pci_get_drvdata(dev);
    dma_free_coherent(&pvt->pdev->dev, 4096, pvt->pb_cpu, pvt->pb_dma);
    pci_iounmap(dev, pvt->bar0);
    kfree(pvt);
    pci_release_regions(pvt->pdev);
    pci_disable_device(pvt->pdev);
    printk(KERN_INFO "nv_probe: removed\n");
}

static int __init imod(void)
{
    return pci_register_driver(&my_driver);
}

static void __exit rmod(void)
{
    pci_unregister_driver(&my_driver);
}

module_init(imod);
module_exit(rmod);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Experimental GP108 PCI driver");
