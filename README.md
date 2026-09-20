# nv-bare-metal
##Experimental Linux PCI driver for exploring GP108 MMIO, DMA, PFIFO, and GPU command submission.

## Learning NVIDIA hardware from the other side of CUDA

This is a purely educational project aimed at understanding the NVIDIA GPU pipeline in greater depth by interacting with a spare GPU directly from a Linux PCI driver.

It is **not** a replacement NVIDIA driver, and it cannot submit work to the GPU yet. The current driver can bind to my GP108, map BAR0, inspect selected registers, enable a couple of engine gates experimentally, and keep a coherent DMA buffer alive for the lifetime of the device.

Please follow along only on hardware you are willing to experiment with and entirely at your own risk. A wrong MMIO write can hang the GPU or the whole machine :))

I initially started this project in Rust, but switched to C because of the current state of Rust support in my kernel setup. In hindsight, C also feels more direct for this project: I can focus on PCI, MMIO, DMA, and the GPU itself instead of fighting language or kernel-support details.

The biggest reference for this work has been [envytools](https://github.com/envytools/envytools), an amazing community effort for documenting NVIDIA hardware:

```bash
git clone https://github.com/envytools/envytools.git
```

This is still an ongoing learning project. There may be inaccuracies or gaps in my understanding, and corrections are very welcome.

---

## Current status

### P1 — first contact with the GPU

- Registered a Linux PCI driver for NVIDIA device `10de:1d01`
- Enabled the PCI device and reserved its BAR regions
- Mapped BAR0 into kernel virtual memory
- Read PMC, interrupt, PFIFO, and PTIMER registers through MMIO
- Allocated coherent DMA memory and inspected the returned CPU and DMA addresses

### P2 — persistent driver state

- Added per-device private state using `struct pvt_dev`
- Stored the BAR0 mapping and DMA allocation in that state
- Attached it to the PCI device using `pci_set_drvdata()`
- Kept the resources alive after `probe()` returned
- Recovered and released them from `remove()`

### Not implemented yet

- VBIOS parsing and firmware loading
- Complete engine initialization
- GMMU page-table construction
- PFIFO channels, runlists, and PBDMA setup
- Pushbuffer submission
- Interrupt handling
- PGRAPH or compute execution

In particular, setting the PFIFO bit in `PMC_ENABLE` only opens/enables that engine gate. It does **not** mean PFIFO has been initialized or that it can consume commands.

---

## Hardware setup

```text
CPU architecture : x86-64
Target GPU       : NVIDIA GT 1030 (GP108, PCI ID 10de:1d01)
Main GPU         : NVIDIA RTX 2070 Super
Operating system : Linux
```

On my machine, the GT 1030 is left unclaimed by the installed NVIDIA driver stack. The newer driver is needed for the RTX 2070 Super but does not initialize this GP108 in my particular setup, leaving it available for experimentation.

That detail is specific to my machine and driver configuration. Do not assume another GPU is safe to bind to an experimental module merely because it has the same architecture.

---

## Where this sits below CUDA

When we normally launch a CUDA kernel, several software layers hide the hardware-facing work:

```text
CUDA application
        ↓
CUDA runtime (libcudart)
        ↓
CUDA user-mode driver (libcuda)
        ↓
NVIDIA kernel driver
        ↓
GPU command submission and hardware engines
```

This project starts near the bottom of that stack. Instead of asking CUDA to manage the GPU, the kernel module binds to the PCI device and interacts with registers exposed through its Base Address Registers.

---

## Locating the GPU

The target GPU can be located on the PCI bus with:

```bash
lspci | grep "GT 1030"
```

On my system it appears at `03:00.0`. Its PCI resources can then be inspected with:

```bash
lspci -s 03:00.0 -vv
```

The relevant layout reported on my machine is:

```text
Region 0: Memory at f4000000 (32-bit, non-prefetchable) [size=16M]
          GPU registers

Region 1: Memory at e0000000 (64-bit, prefetchable) [size=256M]
          Memory aperture

Region 3: Memory at f0000000 (64-bit, prefetchable) [size=32M]
          Additional memory window

Region 5: I/O ports at d000 [size=128]
          Legacy I/O-port space

Expansion ROM at f5000000 [disabled] [size=512K]
          GPU VBIOS / UEFI GOP image
```

These addresses are assigned by the platform and are not portable constants. The driver asks the PCI subsystem for the resources instead of hard-coding the physical addresses.

---

# P1 — mapping BAR0 and reading the GPU

P1 established the smallest useful hardware-access path:

```text
temp_c.ko
        ↓
registered with the Linux PCI subsystem
        ↓
matched PCI ID 10de:1d01
        ↓
probe()
        ↓
enabled the PCI device
        ↓
reserved its BAR regions
        ↓
mapped BAR0 with pci_iomap()
        ↓
ioread32() performed MMIO reads
        ↓
register values appeared in dmesg
```

At this point the interesting part was not merely printing four integers. A read such as:

```c
p0 = ioread32(bar0 + 0x000);
```

travels through a kernel virtual mapping backed by a PCI BAR and reaches a hardware register on the GPU.

## Registers observed

The following values were observed on my GP108. They are measurements from this machine, not values that should be expected from every GP108:

| Offset | Register | Observed value | Interpretation |
| ---: | --- | ---: | --- |
| `0x000` | `PMC_ID` | `0x138000a1` | Identifies the GPU/chip configuration |
| `0x100` | `INTR_HOST` | `0x00000000` | No host interrupt was pending when read |
| `0x200` | `PMC_ENABLE` | `0x40002020` | Shows which top-level engine gates were enabled |
| `0xa00` | `PMC_NEW_ID` | `0x138a1000` | Additional chipset-identification information |

Here PMC refers to NVIDIA's top-level **master-control** block. `envytools` contains the register and bitfield definitions used to interpret these raw values.

A typical bitfield extraction looks like:

```c
(raw >> lower_bit) & ((1U << width) - 1U)
```

## PTIMER

The timer registers explored so far are:

```text
0x9400 : timer low
0x9410 : timer high
```

Reading the low register twice produced two increasing values, which was a simple way of confirming that the timer was live after its PMC gate was enabled.

## PFIFO

PFIFO is part of NVIDIA's command-submission machinery. At a high level, the working model I am using is:

- User-mode or kernel-mode software constructs GPU commands in a pushbuffer.
- A channel provides the GPU execution context associated with submitted work.
- Runnable channels are represented through runlists.
- PBDMAs fetch and process command streams on behalf of scheduled channels.
- GPU virtual addresses used by those engines are translated through the GMMU.

There can be many channels and only a smaller number of hardware PBDMAs, so the GPU schedules channels onto the available command-fetch machinery.

This is deliberately a simplified mental model. The exact relationship among channels, pushbuffers, runlists, PBDMAs, engines, and synchronization is more nuanced and is one of the things this project is meant to investigate.

---

## Connecting this back to CUDA

From the CUDA side, operations such as allocation and kernel launch look simple:

```cpp
cudaMalloc(&ptr, size);
kernel<<<grid, block>>>(ptr);
```

Below that interface, the driver must manage GPU address spaces, memory residency, channels, synchronization, and command submission. Conceptually:

1. A CUDA allocation creates device-accessible storage and the mappings needed for the GPU to address it.
2. A launch eventually becomes commands written into driver-managed command buffers.
3. The GPU command-processing machinery fetches and dispatches that work.

The exact placement of an allocation and exact command path depend on the CUDA and driver memory-management configuration. The point here is not that `cudaMalloc()` always corresponds to one fixed hardware action; it is that a large amount of driver and GPU setup exists beneath that one call.

---

## Address spaces: where things finally started making sense

To understand why DMA, the IOMMU, and the GMMU all exist, I first had to stop thinking of "an address" as one universal number.

A modern system can involve several address spaces:

- CPU virtual addresses
- system physical addresses
- device-visible DMA addresses
- GPU virtual addresses
- VRAM locations

Those addresses do not have to be numerically identical.

### CPU virtual memory

Normal CPU code accesses virtual addresses. The CPU MMU translates them according to the current process or kernel page tables.

### DMA and the IOMMU

A PCIe device performing DMA uses a device-visible DMA address. When an IOMMU is enabled, that address may be translated before reaching system RAM.

The IOMMU provides isolation and prevents a device from freely accessing arbitrary system memory outside the mappings created for it.

A simplified path is:

```text
PCIe device
      ↓
PCIe root complex
      ↓
IOMMU translation, when enabled
      ↓
memory controller
      ↓
system RAM
```

### GPU virtual memory and the GMMU

GPU engines generally operate on GPU virtual addresses. The GPU's own MMU translates those addresses according to GPU page tables and the selected memory aperture.

A simplified future data path, once the necessary GPU state exists, is:

```text
GPU engine requests GPU virtual address X
        ↓
GMMU translates X
        ↓
mapping selects VRAM or system memory
        ├── VRAM: handled by the GPU memory subsystem
        └── system memory: transaction crosses PCIe
```

This project has **not** initialized the GMMU yet. The diagram describes the machinery I am working toward, not current functionality.

---

## Coherent DMA allocation

Before building GPU page tables, I experimented with Linux's DMA API:

```c
pb_cpu = dma_alloc_coherent(&dev->dev, 4096, &pb_dma, GFP_KERNEL);
```

This returns two views of the allocation:

```text
pb_cpu : kernel virtual address used by the CPU
pb_dma : DMA address suitable for this PCI device
```

The two values represent the same allocation from different address spaces. If an IOMMU is active, `pb_dma` does not necessarily equal the underlying system physical address.

`dma_alloc_coherent()` provides a CPU/device-coherent mapping, meaning the CPU and device can observe each other's writes according to the DMA API's coherence rules without explicit streaming-DMA synchronization calls.

At this stage, the buffer is only allocated and kept alive. The driver does **not** yet configure PFIFO to treat it as a valid pushbuffer.

---

# P2 — making the resources actually persist

P1 successfully mapped BAR0, read registers, and allocated a DMA buffer—but then cleaned everything up before `probe()` returned.

That was useful as a hardware-access experiment, but not as the lifetime model of a real driver. Once `probe()` succeeds, the resources belonging to the bound device must remain available until the driver is removed.

P2 introduces driver-private state:

```c
struct pvt_dev {
    void __iomem *bar0;
    dma_addr_t pb_dma;
    void *pb_cpu;
};
```

After BAR0 and the coherent DMA buffer are acquired, their handles are saved:

```c
pvt->bar0 = bar0;
pvt->pb_dma = pb_dma;
pvt->pb_cpu = pb_cpu;

pci_set_drvdata(dev, pvt);
```

`probe()` can now return successfully without destroying the state:

```text
probe()
  ├── enable device
  ├── reserve PCI regions
  ├── map BAR0
  ├── allocate private state
  ├── allocate coherent DMA memory
  ├── attach state with pci_set_drvdata()
  └── return 0

          resources remain alive

remove()
  ├── recover state with pci_get_drvdata()
  ├── free coherent DMA memory
  ├── unmap BAR0
  ├── free private state
  ├── release PCI regions
  └── disable device
```

This is a small change in terms of lines of code, but it is the point where the experiment starts following the lifecycle of an actual Linux PCI driver.

---

## Building and loading

You need a Linux system with the matching kernel headers installed. The target GPU must not already be owned by another driver.

Build the module from `mod_gp108`:

```bash
make
```

Load it:

```bash
sudo insmod temp_P2.ko
```

Inspect the kernel log:

```bash
sudo dmesg | tail -n 30
```

Unload it:

```bash
sudo rmmod temp_P2
```

The module filename depends on the object selected by the Makefile. For the P2 source, the Makefile should contain:

```make
obj-m += temp_P2.o
```

---

## Repository progression

Each `Pi` section corresponds to the matching `Pi` commit so the driver can be followed as it develops:

| Part | Main idea |
| --- | --- |
| P1 | Bind to GP108, map BAR0, read registers, and experiment with DMA allocation |
| P2 | Preserve BAR0 and DMA state for the complete PCI-device lifetime |

Future parts will be added only as the corresponding hardware behavior is implemented and observed.

---

## Planned next steps

- Separate raw register offsets and bit definitions from the driver logic
- Inspect the GP108 initialization sequence in envytools/Nouveau
- Understand the required firmware and falcon-managed engines
- Investigate instance memory and GPU page-table formats
- Build toward a minimal PFIFO channel and valid pushbuffer
- Add clearer captured logs for each verified milestone

The long-term goal is to understand the route from a userspace launch all the way down to GPU-visible commands—not merely to copy an initialization sequence without understanding it.

---

## References and acknowledgements

- [envytools](https://github.com/envytools/envytools)
- The Linux PCI driver and DMA API documentation
- Nouveau and NVIDIA open GPU kernel-module source where applicable

This README was written with AI assistance using my implementation, hardware observations, and original project notes. Any mistakes in the code or technical interpretation are still my responsibility.

