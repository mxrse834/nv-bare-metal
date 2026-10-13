# GP108 Compute-Only Bring-Up: Initial Research Audit

**Status:** Initial source-backed audit; not a complete bring-up recipe.  
**Target:** NVIDIA GP108 / GeForce GT 1030, PCI ID `10de:1d01`.  
**Goal:** Bring the card up as a headless GPGPU compute device while another GPU drives the display.  
**Research principle:** Separate Linux infrastructure from GPU-specific hardware programming. Cite every register sequence and distinguish source-observed behavior from proven hardware facts.

## Executive summary

Nouveau already contains a GP108 device profile, graphics-engine implementation, secure-boot implementation, FIFO support inherited from related generations, and firmware declarations. This means the best starting point is to trace and understand the existing implementation rather than inventing a register sequence from scratch.

The highest-risk dependency is the GP108 graphics/compute engine's firmware and secure-boot path. The Nouveau GP108 GR implementation declares FECS/GPCCS firmware plus software context and initialization blobs. The GP108 secure-boot implementation creates an ACR instance configured for SEC2, FECS, and GPCCS and declares additional ACR and SEC2 firmware. Therefore, firmware availability, authenticity/loading path, and initialization ordering must be investigated before investing in command submission.

A separate known risk: Nouveau has a historical workaround for intermittent GP107/GP108 hangs during GR initialization. Hardware bring-up is not guaranteed to be a simple deterministic sequence of writes.

**Not established yet:** that a hand-written out-of-tree kernel module can initialize GP108 compute independently; that a minimal compute path can avoid the relevant GR firmware; or that a particular firmware file set is present on this machine. These require further source tracing and tests.

## Evidence labels

Use these labels in future notes:

- **[SOURCE]** Directly visible in source code or official documentation.
- **[OBSERVED]** Reproduced on the actual GT 1030, with logs or measurements.
- **[INFERENCE]** Reasonable interpretation of source, not independently verified.
- **[UNKNOWN]** Not answered by evidence currently collected.

## Initial source map

### 1. Device identification and engine profile

- Linux Nouveau device table / engine map: [device/base.c in Linux source](https://code.googlesource.com/linux/torvalds/linux/+/5a5409d90bd05f87fe5623a749ccfbf3f7c7d400/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c)
- In that source, the GP108 chipset profile maps several subsystems, including ACR, BAR, BIOS, bus, device initialization, fault handling, framebuffer, FIFO, GR, SEC2, and others. The exact set should be reviewed in the `nv138_chipset` profile and its surrounding helpers.
- Device ID cross-reference: [envytools GPU table](https://envytools.readthedocs.io/en/latest/hw/gpu.html) lists PCI ID `0x1d01` as GP108 / GeForce GT 1030.

**Next investigation:** locate the exact GP108/`nv138_chipset` profile in the kernel source and trace which subsystem constructors execute during device creation. Build a call graph rather than treating the profile as a literal initialization sequence.

### 2. GP108 graphics engine and firmware

- [GP108 GR implementation](https://git.zx2c4.com/linux-rng/tree/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c?id=88ff5d466c0250259818f3153dbdc4af1f8615dd)
- The implementation declares these firmware paths:
  - `nvidia/gp108/gr/fecs_bl.bin`
  - `nvidia/gp108/gr/fecs_inst.bin`
  - `nvidia/gp108/gr/fecs_data.bin`
  - `nvidia/gp108/gr/fecs_sig.bin`
  - `nvidia/gp108/gr/gpccs_bl.bin`
  - `nvidia/gp108/gr/gpccs_inst.bin`
  - `nvidia/gp108/gr/gpccs_data.bin`
  - `nvidia/gp108/gr/gpccs_sig.bin`
  - `nvidia/gp108/gr/sw_ctx.bin`
  - `nvidia/gp108/gr/sw_nonctx.bin`
  - `nvidia/gp108/gr/sw_bundle_init.bin`
  - `nvidia/gp108/gr/sw_method_init.bin`
- Its firmware interface uses the `gm200_gr_load` path with GP107-related GR support and GP108-specific ACR callbacks.

**Interpretation:** GR initialization depends on more than a pushbuffer and MMIO. The firmware declarations are strong evidence that these files are part of the supported Nouveau GR path, but declarations alone do not prove every file is mandatory for every possible minimal compute path.

**Next investigation:** trace `gf100_gr_new_`, `gm200_gr_load`, firmware request/error paths, and the GP107/GP108 GR initialization flow. Record which blobs are actually requested at runtime and in what order.

### 3. Secure boot / ACR / SEC2

- [GP108 secure-boot implementation](https://git.zx2c4.com/linux-rng/tree/drivers/gpu/drm/nouveau/nvkm/subdev/secboot/gp108.c?h=jd%2Fvdso-test-harness&id=af6c5d5e01ad9f2c9ca38cccaae6b5d67ddd241f)
- It constructs an ACR instance using the SEC2 falcon and flags for FECS, GPCCS, and SEC2.
- It declares firmware paths including:
  - `nvidia/gp108/acr/bl.bin`
  - `nvidia/gp108/acr/unload_bl.bin`
  - `nvidia/gp108/acr/ucode_load.bin`
  - `nvidia/gp108/acr/ucode_unload.bin`
  - `nvidia/gp108/sec2/desc.bin`
  - `nvidia/gp108/sec2/image.bin`
  - `nvidia/gp108/sec2/sig.bin`
  - and the GR files listed above.

**Interpretation:** secure firmware loading is an explicit part of Nouveau's GP108 setup. Do not assume a custom driver can simply DMA arbitrary FECS/GPCCS code into place and start it. Trace ACR/WPR/SEC2 semantics and firmware provenance first.

**Next investigation:** follow `gp108_secboot_new`, `acr_r370_new`, and the relevant `nvkm_secboot_ctor` / ACR loading code. Determine which stages require secure firmware and what resources/addresses are expected.

### 4. GP107/GP108 GR initialization hang workaround

- [Historical kernel patch discussion](https://lkml.rescloud.iu.edu/2004.2/08676.html)
- The patch describes some GP107/GP108 cards intermittently hanging during GR initialization, with a reported symptom like an MMIO read fault/timeout at offset `0x409800`.

**Interpretation:** initialization ordering and hardware quirks matter. Avoid copying only the obvious register writes from a source file while ignoring workarounds and timeout handling.

**Next investigation:** find the workaround in the current kernel source, identify its preconditions, and record the known failure symptom in the bring-up guide.

### 5. Envytools

- [Envytools hardware documentation](https://envytools.readthedocs.io/)
- [GPU identification tables](https://envytools.readthedocs.io/en/latest/hw/gpu.html)
- [Envytools repository](https://github.com/envytools/envytools)

Use envytools primarily for register/field definitions, hardware block descriptions, FIFO and method details, and reverse-engineered notes. A register definition is not automatically proof of the correct initialization order or all legal values.

### 6. Additional references to include

- [Linux kernel Nouveau driver documentation](https://docs.kernel.org/gpu/nouveau.html)
- [Mesa external hardware documentation guide](https://docs.mesa3d.org/drivers/nvk/external_hardware_docs.html)
- [NVIDIA open-gpu-doc](https://github.com/NVIDIA/open-gpu-doc)
- [Nouveau mailing-list archive](https://lists.freedesktop.org/archives/nouveau/)

NVIDIA's published open kernel modules target newer GPU generations and should not be treated as the primary implementation for Pascal GP108. They may still provide architectural context where appropriate.

## Bring-up dependency graph (current hypothesis)

This is a research graph, not a verified mandatory order for every subsystem:

1. **PCI/device ownership** — identify `10de:1d01`, enable the PCI function, claim resources, map BARs safely.
2. **Baseline hardware inspection** — record PCI config, BAR sizes, known-safe MMIO reads, and current kernel/Nouveau logs. Avoid speculative writes.
3. **Platform DMA setup** — establish the device's actual DMA address-width requirements from reliable hardware/source evidence; set Linux DMA masks accordingly; allocate/free a small coherent buffer correctly.
4. **Device initialization / memory subsystem** — trace BIOS/devinit, framebuffer, BAR, bus, and memory-management dependencies in Nouveau.
5. **Firmware/security infrastructure** — trace ACR, SEC2, WPR, and firmware loading. Confirm exact blob availability and runtime requests.
6. **GR engine initialization** — trace GP108/GP107 shared code, firmware startup, software init blobs, context setup, and known workarounds.
7. **FIFO/channel and GPU VA setup** — trace address-space/page-table, channel, runlist, DMA object, and pushbuffer setup in the matching generation's Nouveau path.
8. **Compute object/program submission** — identify the relevant compute class and how Nouveau userspace/Mesa builds methods and launches work.
9. **Completion and result validation** — establish fence/interrupt/polling path and verify a minimal workload (e.g. vector addition) against CPU output.

Important: stages 4–8 are not independent. Refine dependencies by tracing actual constructor/call relationships and logging the first failure. Do not implement stage 7 until stages 3–6 are understood.

## First work session: exact tasks

### Task A — pin down the source version

Use a full kernel source checkout (or the exact source tree matching the running kernel), and record the commit. Do not rely on a random mirror's current snapshot as the definitive version for your installed kernel.

### Task B — trace GP108 selection

Search for the GP108 chipset profile (`nv138_chipset`), its constructor, and the `0x1d01` PCI match. Draw the path from PCI match to `nvkm_device` creation to GR/SECBOOT/FIFO constructors.

### Task C — inventory firmware

Search the system for the declared `nvidia/gp108/...` files and check the kernel log for firmware requests/failures when Nouveau is used. Do not extract firmware from proprietary packages or redistribute blobs without checking licensing; first establish what is already installed and how Nouveau requests it.

### Task D — find the first viable compute path

Trace the GR engine and FIFO/channel paths into the userspace stack (Mesa Nouveau/NVK or another relevant path). Determine the smallest existing Nouveau compute operation that works on GP108. The goal is to reuse its proven sequence as a reference, not to immediately implement all of Mesa or Nouveau.

### Task E — create an evidence ledger

For each step, record:

| Field | Meaning |
|---|---|
| Step / prerequisite | What this operation establishes and what must precede it |
| Source location | Repository, commit, file, function, line range |
| Hardware effect | Register, firmware, memory, engine, or queue state |
| Evidence level | Source / observed / inference / unknown |
| Verification | Log message, register state, completion signal, test output |
| Failure behavior | Timeout, error code, MMIO fault, hang, missing firmware |

## Practical safety rules for `nv-bare-metal`

- Never assume a register offset is safe to read or write just because BAR0 is mapped.
- Avoid experimental engine-reset, clock, power, or interrupt writes until their semantics and recovery path are understood.
- Do not let the device access a DMA buffer after it has been freed or unmapped.
- Use Linux DMA API addresses for device programming; do not substitute CPU virtual or guessed physical addresses.
- Ensure every `probe()` failure unwinds all successfully acquired resources in reverse order.
- Keep the RTX 2070 Super as the display GPU, but verify that the GT 1030 is not bound to another driver before claiming its PCI resources.
- Capture `dmesg`, PCI config, source commit IDs, and test results for each experiment.

## Current conclusions

1. **[SOURCE]** GP108 is explicitly represented in Nouveau's device profile and has dedicated GR and secure-boot source files.
2. **[SOURCE]** Nouveau declares multiple GP108 GR, ACR, and SEC2 firmware files.
3. **[SOURCE]** A historical GP107/GP108 GR initialization hang workaround exists.
4. **[UNKNOWN]** The minimum firmware set and initialization sequence needed for a custom compute-only driver.
5. **[UNKNOWN]** Whether a standalone out-of-tree module can reasonably reproduce enough of Nouveau to launch compute work without porting large portions of NVKM and userspace support.
6. **[NEXT DECISION]** Decide whether `nv-bare-metal` remains an educational low-level driver that incrementally reproduces pieces, or whether the final working compute stack can legitimately reuse Nouveau kernel/userspace components while the project focuses on understanding and documenting the path.

## Sources

1. Linux kernel, Nouveau device map (`device/base.c`): https://code.googlesource.com/linux/torvalds/linux/+/5a5409d90bd05f87fe5623a749ccfbf3f7c7d400/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c
2. Nouveau GP108 GR implementation: https://git.zx2c4.com/linux-rng/tree/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c?id=88ff5d466c0250259818f3153dbdc4af1f8615dd
3. Nouveau GP108 secure boot: https://git.zx2c4.com/linux-rng/tree/drivers/gpu/drm/nouveau/nvkm/subdev/secboot/gp108.c?h=jd%2Fvdso-test-harness&id=af6c5d5e01ad9f2c9ca38cccaae6b5d67ddd241f
4. Envytools GPU documentation: https://envytools.readthedocs.io/en/latest/hw/gpu.html
5. GP107/GP108 GR init workaround patch discussion: https://lkml.rescloud.iu.edu/2004.2/08676.html
6. Linux Nouveau documentation: https://docs.kernel.org/gpu/nouveau.html
7. Mesa hardware documentation guide: https://docs.mesa3d.org/drivers/nvk/external_hardware_docs.html
8. NVIDIA open GPU documentation: https://github.com/NVIDIA/open-gpu-doc
9. Nouveau mailing-list archive: https://lists.freedesktop.org/archives/nouveau/

---

# Update 2026-10-09: traced mainline source and discovered a much shorter baseline route

This section supersedes earlier tentative wording where it differs. It was checked against the current upstream Linux `master` source tree and current Mesa documentation on 2026-10-09. The exact source version installed on the workstation still needs to be pinned before treating line numbers or the call graph as a match for that kernel.

## 7. GP108's actual subsystem profile

Current upstream file: [`nvkm/engine/device/base.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c), profile `nv138_chipset`.

The GP108 profile selects these particularly relevant implementations:

| Area | Constructor selected for GP108 | What this tells us |
|---|---|---|
| ACR / protected firmware loading | `gp108_acr_new` | Firmware loading/security infrastructure is part of this profile |
| Device initialization | `gm200_devinit_new` | GP108 reuses a later shared-generation devinit implementation |
| Framebuffer/memory controller | `gp102_fb_new`, `gp100_mc_new` | Memory setup spans generation-shared blocks; not just an allocated host buffer |
| GPU virtual memory | `gp100_mmu_new` | Device-side address spaces/page tables need to be understood |
| FIFO / scheduling | `gp100_fifo_new` | GP100-generation FIFO/channel and runlist implementation is reused |
| Graphics + compute engine | `gp108_gr_new` | GP108 has a dedicated GR setup path |
| Secure processor | `gp108_sec2_new` | SEC2 is explicitly part of this generation's device profile |
| Display | `gp102_disp_new` | Nouveau's general profile includes display support even if the card is used headlessly |

**Important correction to the mental model:** a headless compute device does not necessarily have a separate, self-contained “compute engine” that can be initialized without GR. In the Nouveau source, the compute command class is exposed through the GR implementation. Avoid assuming you can skip GR just because the GT 1030 will not drive a monitor. Conversely, “headless” does not mean a userspace stack must actively use the display engine.

## 8. GR initialization and compute class

Current upstream:

- [`nvkm/engine/gr/gp108.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c)
- [`nvkm/engine/gr/gp107.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp107.c)

`gp108_gr_new()` passes `gp108_gr_fwif` to the shared `gf100_gr_new_()` path. The GP108 firmware interface selects `gm200_gr_load`, with the shared `gp107_gr` engine description and GP108-specific FECS/GPCCS ACR descriptor callbacks. The shared `gp107_gr` class table advertises `PASCAL_B` and **`PASCAL_COMPUTE_B`**. This is direct source evidence of a Pascal compute class in the GR path; it is not by itself proof that firmware loading and a compute dispatch succeed on a given machine.

GR firmware declared by `gp108.c`:

- `nvidia/gp108/gr/fecs_bl.bin`
- `nvidia/gp108/gr/fecs_inst.bin`
- `nvidia/gp108/gr/fecs_data.bin`
- `nvidia/gp108/gr/fecs_sig.bin`
- `nvidia/gp108/gr/gpccs_bl.bin`
- `nvidia/gp108/gr/gpccs_inst.bin`
- `nvidia/gp108/gr/gpccs_data.bin`
- `nvidia/gp108/gr/gpccs_sig.bin`
- `nvidia/gp108/gr/sw_ctx.bin`
- `nvidia/gp108/gr/sw_nonctx.bin`
- `nvidia/gp108/gr/sw_bundle_init.bin`
- `nvidia/gp108/gr/sw_method_init.bin`

`MODULE_FIRMWARE()` declarations are an inventory of firmware paths associated with the driver; they don't alone prove that every blob is requested on every possible execution path. The definitive next step is to follow `gm200_gr_load` and its request/load/error paths, then compare those with runtime kernel logs.

## 9. ACR and SEC2 paths in current upstream

The source organization has changed across snapshots, so use paths from the same kernel revision when tracing symbols. In current upstream the relevant code is split into:

- [`nvkm/subdev/acr/gp108.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/acr/gp108.c) — defines the GP108 ACR firmware load/unload paths and WPR-related hooks.
- [`nvkm/engine/sec2/gp108.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/sec2/gp108.c) — declares `nvidia/gp108/sec2/desc.bin`, `image.bin`, and `sig.bin`, and selects `gp102_sec2_load` with `gp102_sec2`/ACR handling.

The current ACR file declares these GP108-specific firmware paths:

- `nvidia/gp108/acr/bl.bin`
- `nvidia/gp108/acr/ucode_load.bin`
- `nvidia/gp108/acr/unload_bl.bin`
- `nvidia/gp108/acr/ucode_unload.bin`

The SEC2 file declares:

- `nvidia/gp108/sec2/desc.bin`
- `nvidia/gp108/sec2/image.bin`
- `nvidia/gp108/sec2/sig.bin`

The ACR file shows why “load FECS/GPCCS firmware into a register block” is an unsafe oversimplification: firmware is described in terms of code/data addresses and entry points, while GPCCS has `NVKM_ACR_LSF_FORCE_PRIV_LOAD`. The existing driver uses a generation-specific authenticated/protected loading process. A custom implementation must first understand that mechanism rather than guessing at a direct Falcon start sequence.

## 10. FIFO / channel path

Current upstream: [`nvkm/engine/fifo/gp100.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/fifo/gp100.c).

The GP100 FIFO implementation selects, among other shared operations, the Pascal GPFIFO channel class `PASCAL_CHANNEL_GPFIFO_A` (class ID is defined in Nouveau's `include/nvif/class.h`). It reuses shared channel-ID/runlist constructors, initialization, interrupt handling, and runqueue code. This indicates the channel setup is not a tiny independent sequence; it is coupled to runlists, context scheduling, MMU faults, and the command-submission model.

Envytools remains useful for the *shape* of pushbuffer submission and command encoding, particularly its [DMA pusher documentation](https://github.com/envytools/envytools/blob/master/docs/hw/fifo/dma-pusher.rst). Treat that document as reverse-engineered architectural evidence, not as a complete GP108 initialization recipe; many pages describe older generations explicitly, and the details change by chipset generation.

## 11. GPU DMA address versus GPU virtual address

Do not collapse these addresses into one concept:

1. CPU virtual address: the pointer used by the CPU.
2. Linux DMA address (`dma_addr_t`): the address the device can use for a DMA mapping created by Linux's DMA API.
3. GPU virtual address: an address in a GPU-managed virtual address space, translated through GPU page tables by the GPU MMU.
4. Physical VRAM address or host RAM backing: the memory location ultimately backing the mapping.

The current [`gp100` virtual-memory implementation](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgp100.c) creates/removes page mappings and invokes Linux DMA map/unmap operations for system-memory pages. For GPU command submission, this implies that a buffer having a valid `dma_addr_t` does not automatically prove that its address is valid in the GPU's current virtual address space or in the address format expected by a particular command/class. Trace the channel VM, page-table construction, and object setup used by the real driver.

## 12. A practical compute-only path may already exist: establish a baseline first

Before reimplementing the entire stack, establish whether the installed Nouveau + Mesa stack can run a compute workload on this exact GT 1030. This is a *baseline/reference experiment*, not a replacement for the `nv-bare-metal` learning project.

### Option A: Vulkan compute via NVK

The current [Mesa NVK documentation](https://docs.mesa3d.org/drivers/nvk.html) says NVK supports Kepler and later GPUs, including Pascal, and requires Linux kernel 6.6 or later. It is a Vulkan driver, so it can execute compute shaders without displaying graphics. The GT 1030's Pascal generation is inside that broad supported family; actual success still depends on kernel/Mesa versions, firmware availability, and device initialization.

### Option B: OpenCL via Mesa Rusticl on Nouveau

Mesa's [Rusticl documentation](https://docs.mesa3d.org/rusticl.html) describes an OpenCL implementation over Gallium. Its current documentation lists `nouveau` among drivers that can be enabled, with support caveats. If the distro build includes this path, a small OpenCL vector-add program can act as a familiar compute test. Check the actual installed Mesa build; do not assume distro defaults have Rusticl or Nouveau enabled.

### Read-only baseline commands

Run these first; do not unbind either card, unload the display driver, or modify driver blacklists yet:

```bash
lspci -nnk -d 10de:1d01
lspci -nnvv -d 10de:1d01
lsmod | grep -E 'nouveau|nvidia|nvidia_drm|nvidia_modeset'
dmesg -T | grep -Ei 'nouveau|gp108|acr|sec2|firmware|gr:|fifo|mmu'
find /usr/lib/firmware /lib/firmware -path '*nvidia/gp108*' -type f 2>/dev/null
```

Then inspect compute-capable userspace availability without changing which driver owns the devices:

```bash
vulkaninfo --summary
clinfo
```

These commands may show both GPUs or fail if the relevant userspace components are missing. Record the exact PCI bus address of `10de:1d01` and the kernel driver bound to it. Do not assume the card index `0` or `1` maps to the GT 1030. If attempting runtime tests later, explicitly select the GT 1030 and verify the reported PCI device; otherwise it is easy to benchmark the RTX 2070 Super by accident.

**Success criterion for the baseline:** run a tiny vector-add or integer transform through an OpenCL kernel or Vulkan compute shader on the GT 1030, read back the result, and compare it against a CPU reference. This proves the existing stack can use the hardware; it does not prove the custom driver can.

## 13. Revised project strategy

Maintain two tracks in the guide:

- **Track A — known-working reference:** trace and, if possible, run a minimal compute program through Nouveau + Mesa. Capture kernel logs, firmware requests, selected PCI device, API, output, and software versions.
- **Track B — `nv-bare-metal`:** recreate the relevant initialization concepts incrementally, beginning with safe PCI/MMIO and resource ownership, then following memory/firmware/GR/FIFO/VM dependencies. Do not claim compute readiness until a command is dispatched and a result is independently validated.

Track A is not “cheating”. It is the control experiment that tells you what successful initialization looks like and gives you a source of known-good behavior. Track B remains the low-level engineering work.

## 14. Automated research workflow: where AI and your RTX 2070 Super fit

The research corpus itself is small enough that ordinary local tools should do most of the deterministic work. Use `git`, `rg`, `ctags`/clangd indexing, exact symbol and path search, and line-preserving code-aware chunks first. Those tasks do not need a GPU. Use embeddings for semantic lookup only after exact search is dependable.

A hybrid LLM workflow is likely most efficient:

- **Hosted/free LLM:** stronger synthesis across many source excerpts, complex call-graph explanations, and drafting the guide. It requires internet and can have usage/context limits; send targeted excerpts with repository links/commit IDs rather than a vague prompt or entire unfiltered repos.
- **RTX 2070 Super local LLM:** private/offline repeatable experiments, bulk summarization, local retrieval and code Q&A. `llama.cpp` documents CUDA acceleration and building with `-DGGML_CUDA=ON` ([build docs](https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md)). With 8 GB of VRAM, a quantized 7B/8B-class model may fit depending on model format, context length, and other GPU use; long context and desktop display usage reduce headroom. A local model is not automatically as capable as a larger hosted model.
- **Best split:** deterministic scripts collect symbols, files, snippets and line ranges; retrieval narrows the evidence; an LLM synthesizes a proposed explanation; source links and manual checks verify every hardware claim. Do not ask the model to guess missing registers or turn an uncertain inference into a fact.

This ChatGPT session cannot directly run inference on the RTX 2070 Super installed in your machine. We can, however, build a local retrieval/indexing pipeline and prompts/scripts for you to execute there.

## Updated source index

1. [`nv138_chipset` and GP108 subsystem profile — device/base.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c)
2. [GP108 GR firmware and constructor — gr/gp108.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c)
3. [Shared Pascal GR class table — gr/gp107.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp107.c)
4. [GP108 ACR path — subdev/acr/gp108.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/acr/gp108.c)
5. [GP108 SEC2 firmware — engine/sec2/gp108.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/sec2/gp108.c)
6. [Pascal FIFO/channel path — engine/fifo/gp100.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/fifo/gp100.c)
7. [GP100 GPU virtual memory — subdev/mmu/vmmgp100.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgp100.c)
8. [Envytools DMA pusher notes](https://github.com/envytools/envytools/blob/master/docs/hw/fifo/dma-pusher.rst)
9. [Mesa NVK](https://docs.mesa3d.org/drivers/nvk.html)
10. [Mesa Rusticl](https://docs.mesa3d.org/rusticl.html)
11. [llama.cpp CUDA backend build](https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md)

## 15. Important cross-check for the DMA-mask question

The current Nouveau source gives a concrete implementation reference that was missing from the initial pass:

- GP108 selects `gp100_mmu_new` in [`nv138_chipset`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c).
- [`nvkm/subdev/mmu/gp100.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gp100.c) defines the `gp100_mmu` functions with `.dma_bits = 47`.
- [`nvkm/engine/device/pci.c`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c), in `nvkm_device_pci_new()`, reads `device.mmu->dma_bits` and requests `dma_set_mask_and_coherent(..., DMA_BIT_MASK(bits))`. If that mask fails and is not 32 bits, it retries with a 32-bit mask and updates the MMU's configured `dma_bits`.

Therefore, **the current Nouveau implementation requests a 47-bit Linux DMA mask for the GP108 profile (with a 32-bit fallback)**. This is substantially better evidence for what an interoperable GP108 Linux driver does than guessing `DMA_BIT_MASK(40)` from a FIFO field.

Scope carefully: this is implementation evidence for the Linux DMA mask used by Nouveau's MMU path, not an independent silicon datasheet proving every address field in the GPU is 47 bits wide. Envytools' 40-bit `IB_ADDRESS` field describes a particular FIFO/pushbuffer interface field; it must not be assumed to be interchangeable with `struct nvkm_mmu_func::dma_bits`. Those values can describe different address domains or levels of translation.

For your driver, the safest research conclusion is: **use Nouveau's 47-bit mask choice as the reference to investigate, trace why GP100 MMU selects it, then verify compatibility with the exact kernel and device before adopting it. Do not treat 40 bits as established for the Linux DMA API mask just because a FIFO structure contains a 40-bit field.**

## 16. The actual high-level initialization call graph (now traced one layer deeper)

Current upstream source paths, traced across `nouveau_drm.c`, `nvkm/engine/device/pci.c`, and `nvkm/engine/device/base.c`:

```text
Linux PCI core binds Nouveau
  └─ nouveau_drm_probe(pdev)
       └─ nvkm_device_pci_new(pdev, ...)
            ├─ pci_enable_device(pdev)
            ├─ collect NVIDIA device/subsystem metadata
            └─ nvkm_device_ctor(...)
                 ├─ map primary MMIO / BAR0 resource
                 ├─ read PMC_BOOT0 and derive chipset/architecture
                 ├─ choose nv138_chipset for chipset 0x138 (GP108)
                 └─ create the profile's NVKM subdevices
            └─ obtain device.mmu->dma_bits
                 └─ dma_set_mask_and_coherent(DMA_BIT_MASK(bits))
                    └─ on a failed non-32-bit mask, retry 32-bit
       ├─ pci_set_master(pdev)
       ├─ create Nouveau DRM device
       ├─ pci_enable_device(pdev)
       └─ nouveau_drm_device_init(drm)
            └─ nvkm_device_init(device)
                 ├─ nvkm_device_preinit(device)
                 │    ├─ subdevice preinit loop
                 │    ├─ nvkm_devinit_post()
                 │    ├─ nvkm_top_parse()
                 │    └─ nvkm_fb_mem_unlock()
                 ├─ device/subdevice fini-to-poweroff stage
                 ├─ rearm interrupts
                 └─ subdevice init loop
```

This is the framework lifecycle, not a claim that every subdevice initializes independently or that the displayed order is the complete internal firmware dependency order. The source should be read at the matching kernel revision; device-profile constructors, preinit, init, and later engine/channel creation are separate layers.

Useful source entry points:

- [`nouveau_drm.c` — `nouveau_drm_probe()` and `nouveau_drm_device_init()`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nouveau_drm.c)
- [`device/pci.c` — `nvkm_device_pci_new()` and DMA-mask selection](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c)
- [`device/base.c` — `nvkm_device_ctor()`, `nvkm_device_init()`, and `nv138_chipset`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c)
- [`gr/gm200.c` — shared `gm200_gr_load()`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gm200.c)

The GR load function specifically calls `nvkm_acr_lsfw_load_bl_inst_data_sig()` for FECS and then GPCCS firmware, aborting on either error, and then calls `gk20a_gr_load_sw()` to load the software initialization/context blobs. This converts the firmware list from a simple declaration into a concrete loading sequence for the selected `gm200_gr_load` path.

## 17. Source-traced GR → compute-class and FIFO → channel path (2026-10-09)

This is the source-level trace established so far. It does **not** claim successful execution on the physical card.

### GR path

1. GP108's constructor in `nvkm/engine/gr/gp108.c` calls `gf100_gr_new_()` with `gp108_gr_fwif`.
2. The first `gp108_gr_fwif` entry selects `gm200_gr_load`, the shared `gp107_gr` function table, and GP108-specific FECS/GPCCS ACR descriptors.
3. `gp107_gr` advertises the `PASCAL_COMPUTE_B` class in its `.sclass` table. This proves a compute class is exposed by the Nouveau GR implementation, not that a custom implementation can dispatch work without the rest of GR initialization.
4. `gm200_gr_load()` loads FECS bootloader/instruction/data/signature material through `nvkm_acr_lsfw_load_bl_inst_data_sig()`. If that succeeds, it does the same for GPCCS. It then sets `gr->firmware = true` and calls `gk20a_gr_load_sw()` to load GR software/context initialization blobs. Each firmware load is checked and an error is returned on failure.

Sources: [GP108 GR wrapper and firmware list](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c), [GP107 GR function table and Pascal compute class](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gp107.c), [shared `gm200_gr_load()`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/gr/gm200.c).

### FIFO/channel path

The GP100 FIFO implementation used by the GP108 profile selects the `PASCAL_CHANNEL_GPFIFO_A` channel class. Its function table also selects shared channel-ID construction, runlist construction, FIFO/PBDMA initialization, runqueue, interrupt handling, and MMU-fault handling. Thus a minimal compute submitter needs more than the pushbuffer allocation: it needs a valid channel/VM and a valid scheduling/submission path. See [Nouveau GP100 FIFO implementation](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/fifo/gp100.c).

### DMA mask observation

The GP100 MMU function table sets `.dma_bits = 47`: [Nouveau GP100 MMU implementation](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gp100.c). The bring-up report's earlier device-PCl trace records Nouveau using the configured MMU DMA width to set the Linux DMA mask and retrying with 32 bits if needed. Treat this as **Nouveau implementation behavior** and trace it against the exact kernel version installed on the workstation before copying it. A particular FIFO command field's width and the Linux DMA API mask are not interchangeable concepts.

### Current dependency conclusion

The minimum known *software path* is:

```text
GP108 device profile
  ├─ MMU/VMM: DMA + GPU virtual address spaces
  ├─ ACR + SEC2: authenticated/protected firmware infrastructure
  ├─ GR: FECS → GPCCS → GR software/context blobs
  │    └─ exposes PASCAL_COMPUTE_B
  └─ FIFO: PASCAL_CHANNEL_GPFIFO_A
       └─ VM-bound channel + runlist + GPFIFO/pushbuffer submission
            └─ userspace must create program/code/data state and dispatch methods
                 └─ fence/completion + readback verification
```

The subsystems are not necessarily called in exactly this conceptual order; the graph shows dependencies to investigate. Establish the exact order by tracing NVKM's subdevice `preinit`/`init` loops and checking runtime logs.

## 18. Recommended next experiment: establish a working reference stack

Before writing a standalone GPU command submitter, test whether the current Nouveau + Mesa stack can run a compute workload on the GT 1030. This separates “the GPU/firmware/software stack can compute” from “my new kernel module can initialize it.”

Use read-only inventory first:

```bash
lspci -nnk -d 10de:1d01
lspci -nnvv -d 10de:1d01
dmesg -T | grep -Ei 'nouveau|gp108|acr|sec2|firmware|gr:|fifo|mmu'
find /usr/lib/firmware /lib/firmware -path '*nvidia/gp108*' -type f 2>/dev/null
vulkaninfo --summary 2>/dev/null
clinfo 2>/dev/null
```

Record the GT 1030's PCI bus address, bound driver, kernel version, Mesa version, available firmware files, and errors. If Vulkan compute is available through NVK, use a small compute shader; if OpenCL through Rusticl is available and enabled, use vector addition. In either case, explicitly select and verify the GT 1030 instead of assuming the first GPU returned by the API is the right one. Do not unbind a driver or change boot configuration until this baseline is recorded.

## 19. Local model vs free hosted model for this project

**Recommendation: use a hosted/free LLM plus web/source lookup for active research; add a local model only when you are ready to build and repeatedly query the repository corpus.**

A local 2070 Super can run quantized small coding models. For instance, the Ollama registry lists `qwen2.5-coder:7b` at about 4.7 GB with a 32K context configuration, which is a plausible starting point for an 8 GB GPU. That is model weight size, not total runtime VRAM: the KV cache/context, CUDA runtime, desktop use, and other allocations consume extra memory. Large long-context code models such as `qwen3-coder:30b` are listed at about 19 GB and are not a sensible fully GPU-resident choice on an 8 GB card.

References: [Ollama Qwen2.5-Coder model sizes](https://registry.ollama.com/library/qwen2.5-coder), [Ollama Qwen3-Coder sizes](https://registry.ollama.com/library/qwen3-coder), [llama.cpp CUDA build instructions](https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md).

Use the tools this way:

- **Hosted/free model:** cross-source synthesis, current web research, explaining complex call graphs, and drafting. It has usage limits, but you avoid local model setup and can use web retrieval directly.
- **Local RTX 2070 Super:** private/offline source Q&A, repeatable bulk summarization, and experimentation with a local RAG pipeline. Expect more limited reasoning and context than a larger hosted model; verify every generated hardware claim against retrieved source.
- **CPU scripts:** repository cloning (on a machine with internet), `rg`/exact symbol search, parsing file/function boundaries, indexing file paths and line ranges, and generating the evidence ledger. These tasks do not need an LLM or GPU.

Do not fine-tune or train an LLM for this project. Build hybrid retrieval first: exact/BM25 search for offsets and symbols, optional embeddings for semantic queries, code-aware chunks, and answers that cite exact source paths and commits. This will be more useful and more reproducible than feeding a model an unfiltered source dump.

## 20. Confidence ledger

| Claim | Status | Why |
|---|---|---|
| Nouveau GP108 path exposes `PASCAL_COMPUTE_B` | **Source-confirmed** | It appears in the `gp107_gr` class table used by the GP108 GR firmware interface. |
| GR initialization uses FECS/GPCCS firmware loading and software/context blobs | **Source-confirmed for Nouveau's selected firmware path** | `gm200_gr_load()` checks FECS and GPCCS load calls and then invokes `gk20a_gr_load_sw()`. |
| GP108 FIFO path uses Pascal GPFIFO channels | **Source-confirmed** | GP100 FIFO function table selects `PASCAL_CHANNEL_GPFIFO_A`. |
| The GT 1030 currently has a usable firmware set on this specific machine | **Unknown until checked locally** | Installed filesystem and logs have not been inspected. |
| NVK/Rusticl compute works on this specific GT 1030 and software stack | **Unknown until tested** | Broad architecture support is not a device-specific execution test. |
| A minimal standalone out-of-tree driver can bypass most of NVKM and dispatch compute | **Unproven** | The existing path depends on GR firmware/context, MMU/VMM, FIFO/channel, and userspace command construction. |
| The current Linux stack's relevant DMA mask is a 47-bit request | **Source-observed in Nouveau's MMU function table** | `gp100_mmu.dma_bits = 47`; independently verify the full mask setup in the exact kernel checkout. |
