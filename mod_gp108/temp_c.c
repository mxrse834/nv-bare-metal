#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

static int probe(struct pci_dev *dev, const struct pci_device_id *id);
static void remove(struct pci_dev *dev);

static const struct pci_device_id ids[] = {
    { PCI_DEVICE(0x10de, 0x1d01) },
    { 0 }
};

static struct pci_driver my_driver = {
    .name     = "nv_probe",
    .id_table = ids,
    .probe    = probe,
    .remove   = remove,
};

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    void __iomem *bar0;
    u32 p0,p1,p2,p3;
    int err;

    err = pci_enable_device_mem(dev);
    if (err) return err;

    pci_set_master(dev);

    err = pci_request_regions(dev, "nv_probe");
    if (err) goto disable;

    bar0 = pci_iomap(dev, 0, 0);
    if (!bar0) { err = -ENOMEM; goto release; }

    p0 = ioread32(bar0);
    p1 = ioread32(bar0 + 0x200);
    p2 = ioread32(bar0 + 0x100);
    p3 = ioread32(bar0 + 0xa00);

    printk(KERN_INFO "nv_probe: PMC_ID = 0x%08x\n", p0);
    printk(KERN_INFO "nv_probe: PMC_ENABLE = 0x%08x\n", p1);
    printk(KERN_INFO "nv_probe: INTR_HOST = 0x%08x\n", p2);
    printk(KERN_INFO "nv_probe: PMC_NEW_ID = 0x%08x\n", p3);

    iowrite32((1 << 16) | p1,bar0 + 0x200);
    printk(KERN_INFO "nv_probe: TIMER_LOW = 0x%08x\n", ioread32(bar0 + 0x9400));
    printk(KERN_INFO "nv_probe: TIMER_LOW = 0x%08x\n", ioread32(bar0 + 0x9400));
    
    //pb_dma holds the physical device address of the allocated buffer may or may not be the direct physical address of the buffer, depending on the architecture and memory management(IOMMU).
    //while pb_cpu holds the virtual address that can be used by the CPU to access the buffer. 
    //The dma_alloc_coherent function ensures that the allocated memory is suitable for DMA operations, meaning it is contiguous in physical memory and not cached, which is important for device drivers that perform direct memory access.
    dma_addr_t pb_dma;
    void* pb_cpu ;
    pb_cpu = dma_alloc_coherent(&dev->dev,4096,&pb_dma,GFP_KERNEL);
    if(!pb_cpu){printk(KERN_ERR "nv_probe: dma_alloc_coherent failed\n"); err = -ENOMEM; goto unmap;}
    printk(KERN_INFO "nv_probe: Allocated DMA buffer address 0x%llx, CPU virtual address %p\n", pb_dma, pb_cpu);
    dma_free_coherent(&dev->dev,4096,pb_cpu,pb_dma);
    /*
[ 6269.538829] nv_probe: TIMER_LOW = 0xcef45540
[ 6269.538832] nv_probe: TIMER_LOW = 0xcef45de0
[ 6289.065425] nv_probe: removed
[ 6290.797139] nv_probe: PMC_ID = 0x138000a1
[ 6290.797143] nv_probe: PMC_ENABLE = 0x40002020
[ 6290.797144] nv_probe: INTR_HOST = 0x00000000
[ 6290.797145] nv_probe: PMC_NEW_ID = 0x138a1000
[ 6290.797147] nv_probe: TIMER_LOW = 0x617a7a00
[ 6290.797150] nv_probe: TIMER_LOW = 0x617a8280
[ 6290.797152] nv_probe: Allocated DMA buffer address 0000000051645bd8, CPU virtual address 0x1241000
    */

unmap:    
    pci_iounmap(dev, bar0);
release:
    pci_release_regions(dev);
disable:
    pci_disable_device(dev);
    return err;
}

static void remove(struct pci_dev *dev)
{
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