# GP108 Compute Bring-Up — Plain-English Guide

**Goal:** Use the NVIDIA GT 1030 (GP108) as a headless compute device while the RTX 2070 Super continues to drive the display.

**Important:** “Headless” means the GT 1030 does not drive a monitor. It does *not* automatically mean we can ignore the GPU's graphics/GR initialization code. On this NVIDIA generation, Nouveau exposes the compute command class through the GR engine path.

This guide is deliberately conceptual. It is the map to read before diving into register definitions and source code.

## The big picture

Imagine the GPU as a factory that is powered off and has no instructions loaded. Linux can see the factory from outside, but that does not mean the machinery inside is ready to perform work.

Our job is to move through these stages:

1. **Find the GPU.** Linux detects the PCI device.
2. **Open a control path.** The driver maps BAR0 so it can access the GPU's control registers.
3. **Give it usable memory.** The driver allocates memory and obtains the address the GPU must use to access it.
4. **Start the GPU's internal machinery.** The appropriate hardware blocks, firmware, and memory-management machinery must be initialized.
5. **Create a work queue.** The driver sets up a channel/queue through which commands can be submitted.
6. **Submit compute work.** A program and its inputs are made available and a command tells the GPU to execute it.
7. **Check the result.** The CPU waits for completion and verifies the output.

Passing one stage does not imply the next stage works.

## The terms in normal language

| Term | Plain-English meaning |
|---|---|
| **PCI / PCIe** | The connection through which the computer discovers and communicates with the card. |
| **BAR0** | A window into some of the GPU's control registers. Mapping it gives the driver an address to use; it does not initialize the GPU. |
| **MMIO** | Reading or writing a device register through that mapped window. A register write can change hardware state, so offsets and values must be verified before use. |
| **DMA** | The GPU or another device reads/writes memory without the CPU copying every byte itself. |
| **DMA address** | The address the device must use for a DMA operation. Do not assume it is the same as a CPU pointer or a physical address. |
| **Coherent buffer** | A buffer allocated through Linux's DMA API with coherent visibility rules. It is useful for shared control data, but allocating one does not make the GPU consume it. |
| **Firmware** | Small programs/data needed by internal GPU control processors. Nouveau names specific files for GP108's GR, ACR, and SEC2 paths. |
| **GR** | Nouveau's graphics-engine subsystem. On this generation, its setup also exposes the compute command class; it is not simply “display graphics.” |
| **ACR / SEC2** | Parts of Nouveau's secure firmware-loading machinery. They help load protected GPU firmware; they are not interchangeable with an ordinary CPU-side file copy. |
| **GPU virtual memory / MMU** | The GPU-side address translation machinery. A GPU command may use a GPU virtual address, which must be backed by a correctly configured mapping. |
| **FIFO / channel / runlist** | The command scheduling system: a channel describes a source of work, and scheduling machinery gives it time on the GPU. Details depend on the GPU generation. |
| **Pushbuffer / GPFIFO** | A mechanism for supplying command words to the GPU and telling the scheduling system where that work is. A buffer existing in RAM is not the same as a functioning command queue. |
| **Compute class** | The hardware command interface used to request compute operations. Nouveau's GP107-family GR table includes `PASCAL_COMPUTE_B`; that is source evidence of a supported interface, not proof our custom driver can launch work yet. |
| **Fence / completion** | A way to tell when submitted work has finished so the CPU can safely consume the result. |

## What the existing research report actually says

The technical report is a set of clues from Nouveau's implementation, not a recipe to copy line by line.

- Nouveau has a GP108-specific GR setup file.
- That file declares firmware files for FECS, GPCCS, and GR software/context initialization.
- GP108 also has ACR/SEC2 code involved in protected firmware loading.
- The FIFO implementation it uses is shared with the Pascal/GP100 family and includes channel/runlist scheduling.
- A historical GP107/GP108 GR initialization hang is documented, so ordering and hardware-specific workarounds matter.

**What this does not prove:** that we have all required firmware installed; that all declared blobs are needed on every path; that our out-of-tree module can safely reproduce Nouveau's initialization; or that any compute launch has succeeded.

## Where our project is now

Based on the code discussed so far:

- **Linux PCI-driver structure:** you have started implementing `probe()`/`remove()` and learned how `module_init()` registers the PCI driver.
- **BAR0 and coherent memory:** your code has started mapping/storing BAR0 state and allocating a 4 KiB coherent pushbuffer buffer. That is resource setup, not command submission.
- **Hardware bring-up:** not yet demonstrated. We have not established from a successful test that GP108 firmware and its internal engines are initialized by your own module.
- **Compute execution:** not yet demonstrated. We have not submitted a compute program and verified its output.

That is normal. The hard step is not allocating a buffer; it is learning the initialization dependencies and command protocol safely.

## The next task: observe before writing more registers

Do a read-only baseline audit on the Linux machine first:

```bash
# Identify the card and see which driver currently owns it
lspci -nnk -d 10de:1d01

# Search the usual Linux firmware tree for GP108 files
find /usr/lib/firmware/nvidia -path '*gp108*' -type f 2>/dev/null

# Look for relevant kernel messages (if Nouveau has run for this device)
dmesg | grep -Ei 'nouveau|gp108|fecs|gpccs|sec2|firmware|acr'
```

These commands only inspect state; they do not initialize the GPU. Empty results do not necessarily prove firmware is unavailable, because firmware may be stored elsewhere, compressed differently, or Nouveau may not have been loaded for this card.

Save the output with the kernel version and the source commit you are researching. That gives us a real baseline rather than guessing what is installed or which driver is active.

## How we will use the source code

For each stage, answer only four questions:

1. **What are we trying to make happen?**
2. **Which existing Nouveau function does it?**
3. **What must already be true before that function can work?**
4. **How would we know it worked?**

Keep a confidence label:

- **Source says:** visible in code or official documentation.
- **Observed:** reproduced on your GT 1030 with logs or a test.
- **Inferred:** plausible interpretation but not proven.
- **Unknown:** not established yet.

Don't copy experimental MMIO writes from a source snippet until the relevant register meaning, prerequisites, and failure mode are understood. Keep the RTX 2070 Super as the display GPU and avoid experiments that could hang the host without a recovery plan.

## The real finish line

The project is not finished when BAR0 maps, a firmware filename is found, or a pushbuffer gets allocated.

The meaningful milestone is: **submit a simple compute workload to GP108, observe completion, copy the output back, and match it against a CPU result.** Vector addition is a good first target. Only after that works should we build a small userspace interface or turn the research corpus into a more capable LLM assistant.
