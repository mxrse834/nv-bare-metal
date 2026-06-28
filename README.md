# nv-bare-metal

## Bare-metal access to NVIDIA hardware

This is a purely educational project aimed at understanding the GPU pipeline in greater depth. Please follow along only on your own hardware and at your own risk.

Initially, I started this project in Rust, but due to current kernel limitations, I had to switch to C. In hindsight, I feel C is far more intuitive when working close to the hardware. It also makes for a better educational project, as we can focus on understanding the hardware concepts rather than the language semantics.

Please clone `envytools` before you start; it's a simply amazing resource made possible by the effort of the community.

```bash
git clone https://github.com/envytools/envytools.git
```

You are recommended to follow along starting with the README up to **P1**, after which each part **Pi** of the README corresponds to the code commit **Pi**.

Sorry for any inaccuracies, misinterpretations, or gaps in understanding on my part. You are encouraged to help me correct them. Thank you :))

---

# Hardware Setup

```text
Architecture : x86-64

GPU          : NVIDIA GT 1030 (GP108)

The GT 1030 has been blacklisted from initialization by the NVIDIA driver.
This is because driver version 595 (required by the RTX 2070 Super)
does not support GP108 and therefore leaves the card in a partially
initialized state.
```

---

# CUDA Software Stack

```text
CUDA runtime (libcudart)
        ↓
CUDA driver (libcuda)
        ↓
Kernel driver (nvidia.ko)
        ↓
Hardware command ring
```

---

# Locating the GPU

```bash
lspci | grep "GT 1030"
```

---

# MMIO Layout

```bash
lspci -s 03:00.0 -vv
```

```text
Region 0: Memory at f4000000 (32-bit, non-prefetchable) [size=16M]
          Registers

Region 1: Memory at e0000000 (64-bit, prefetchable) [size=256M]
          Memory aperture

Region 3: Memory at f0000000 (64-bit, prefetchable) [size=32M]
          Extra memory window

Region 5: I/O ports at d000 [size=128]
          Legacy x86 compatibility ports

Expansion ROM at f5000000 [disabled] [size=512K]
          GPU firmware (VBIOS / UEFI GOP)
```

---

# P1

## Software Stack Built

(PMC = Power Management Control. It provides the vendor ID, device ID, enabled engines, etc.)

```text
temp_c.ko (kernel module)
        ↓
Registered with the Linux PCI subsystem
        ↓
Kernel matched PCI ID 10de:1d01
        ↓
probe()
        ↓
Enabled device
        ↓
Reserved BAR0
        ↓
Mapped BAR0
        ↓
ioread32() crossed the PCIe bus
        ↓
GPU registers read
        ↓
Values printed to dmesg
```

---

## Registers Read

| Offset | Register   | Value      | Meaning                               |
| -----: | ---------- | ---------- | ------------------------------------- |
|  0x000 | PMC_ID     | 0x138000a1 | GP108, Pascal, TSMC fabrication       |
|  0x200 | PMC_ENABLE | 0x40002020 | Only PIBUS, PDAEMON, PDISPLAY enabled |
|  0x100 | INTR_HOST  | 0x00000000 | No pending interrupts                 |
|  0xa00 | PMC_NEW_ID | 0x138a1000 | Confirms GP108 identity               |

`envytools` contains documentation for many possible `PMC_ENABLE` combinations.

Extraction pattern used for each field:

```c
(raw >> lower_bit) & ((1 << width) - 1)
```
## PMC_ENABLE

### 1) PTIMER

```text
0x9400 : TIMER LOW
0x9410 : TIMER HIGH
```

---

### 2) PFIFO (bit 8 in PMC_ENABLE)

Multiple channels process multiple streams of data that are assigned by a PBDMA (Push Buffer DMA).

Ready channels are contained within a **runlist**. The channels themselves are stored in GPU-managed instance memory.

A **pushbuffer** is a ring buffer allocated in either system memory or VRAM.

Putting everything together:

* A small number of **PBDMAs** map pushbuffers to channels.
* There may be many pushbuffers, since they are essentially pointers to virtual addresses.
* A channel has a 1:1 relationship with a pushbuffer in terms of quantity, although not necessarily a fixed mapping.
* Channels are scheduled using the runlist.

All communication ultimately takes place through the **GMMU**, which maps GPU virtual addresses to physical addresses in VRAM or DRAM.

Scheduling behavior:

```text
Within one channel:
    Strict FIFO ordering

Across channels:
    Time-sliced scheduling via the runlist
```

---

# Linking with CUDA

1. `cudaMalloc()`

   * Allocates memory in VRAM.
   * Maps the allocation into the GMMU.
   * Stores the mapping in the GMMU page tables.

2. Kernel launch

   * Places commands into a pushbuffer located in either VRAM or system DRAM.
   * PFIFO eventually consumes commands from this pushbuffer through the assigned runlist channel.

---

# Understanding Address Translation

To truly understand the necessity and communication pipeline between the GPU, CPU, and memory, we must first understand addressing.

### 1. Address Spaces

Addressing can be thought of as the mechanism by which a device accesses memory.

Each device owns its own address space, and therefore multiple addressing schemes do **not** necessarily need to be coherent.

---

### 2. Common Address Spaces

The most common address spaces in a modern system are:

* Direct physical address space
* CPU virtual address space
* GPU virtual address space
* IOMMU virtual address space

General PCIe devices such as SATA controllers or NICs typically use either:

* IOMMU-translated addresses
* Direct physical addressing (unsafe)

because they generally do not implement their own virtual memory system.

---

### 3. Why does the IOMMU exist?

The purpose of the CPU MMU and GPU GMMU is relatively obvious:

* Process isolation
* Security
* The illusion of virtually unlimited address space

The role of the IOMMU is less obvious.

The IOMMU is a hardware unit located between the memory controller and the PCIe root complex.

Its purpose is to ensure kernel memory safety by translating DMA addresses generated by PCIe devices before they reach system memory.

The overall data path is:

```text
CPU
 │
 ▼
Memory Controller
 │
 ▼
RAM
 ▲
 │
IOMMU
 ▲
 │
Root Complex
 ▲
 │
PCIe Bus
 ▲
 │
GPU
```

---

# Ideal Memory Access Flow (after GMMU initialization)

```text
GPU engine (PGRAPH / PFIFO) wants address X
        │
        ▼
X is a GPU virtual address
        │
        ▼
GMMU translates X → Physical Address P
        │
        ├───────────────► Is P in VRAM?
        │
        ├── YES
        │       │
        │       ▼
        │   VRAM controller fetches data
        │   (never leaves GPU)
        │
        └── NO
                │
                ▼
            Address refers to system RAM
                │
                ▼
           Crosses the PCIe bus
                │
                ▼
      IOMMU performs DMA translation
                │
                ▼
        Memory controller accesses RAM
                │
                ▼
          Data returns across PCIe
```

---

# Current Limitation (before GMMU)

For now we are limited to the following design because the GMMU has not yet been initialized.

```text
CPU wants to send data to GPU
        │
        ▼
dma_alloc_coherent()
        │
        ├── CPU virtual address
        │
        └── DMA address
                │
                ▼
CPU writes commands using the virtual address
                │
                ▼
CPU tells PFIFO:
"Pushbuffer is located at DMA address P"
                │
                ▼
PFIFO places P on the PCIe bus
                │
                ▼
IOMMU translates DMA address
                │
                ▼
System RAM is accessed
                │
                ▼
Data arrives at the GPU
```

---

# P2

In **P1**, we performed:

* `pci_enable_device()`
* `pci_disable_device()`
* `ioremap()`
* `iounmap()`
* BAR reservation
* BAR release

all inside the same `probe()` function (refer to the P1 version).

However, this is not particularly useful for a real driver.

Any state allocated by the driver must remain valid until `remove()` is explicitly called.

Therefore, **P2** introduces **driver private data**, allowing all required mappings and data structures to persist for the lifetime of the device instead of only during the execution of `probe()`.
