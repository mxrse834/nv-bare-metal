struct nvkm_device
{
    const struct nvkm_device_func *func;
    const struct nvkm_device_quirk *quirk;
    struct device *dev;
    enum nvkm_device_type type;
    u64 handle;
    const char *name;
    const char *cfgopt;
    const char *dbgopt;

    struct list_head head;
    struct mutex mutex;
    int refcount;

    void __iomem *pri;

    u32 debug;

    const struct nvkm_device_chip *chip;
    enum
    {
        NV_04 = 0x04,
        NV_10 = 0x10,
        NV_11 = 0x11,
        NV_20 = 0x20,
        NV_30 = 0x30,
        NV_40 = 0x40,
        NV_50 = 0x50,
        NV_C0 = 0xc0,
        NV_E0 = 0xe0,
        GM100 = 0x110,
        GP100 = 0x130,
        GV100 = 0x140,
        TU100 = 0x160,
        GA100 = 0x170,
        GH100 = 0x180,
        AD100 = 0x190,
        GB10x = 0x1a0,
        GB20x = 0x1b0,
    } card_type;
    u32 chipset;
    u8 chiprev;
    u32 crystal;

    struct
    {
        struct notifier_block nb;
    } acpi;

#define NVKM_LAYOUT_ONCE(type, data, ptr) data *ptr;
#define NVKM_LAYOUT_INST(type, data, ptr, cnt) data *ptr[cnt];
#include <core/layout.h>
#undef NVKM_LAYOUT_INST
#undef NVKM_LAYOUT_ONCE
    struct list_head subdev;

    struct
    {
        struct list_head intr;
        struct list_head prio[NVKM_INTR_PRIO_NR];
        spinlock_t lock;
        int irq;
        bool alloc;
        bool armed;
        bool legacy_done;
    } intr;
};