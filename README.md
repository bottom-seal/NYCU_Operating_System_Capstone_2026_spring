<div align="center">

# 🐧 OSC 2026 — A RISC-V Kernel from Scratch

**A bare-metal RISC-V operating system for the Orange Pi RV2, built one lab at a time**

*NYCU IOC5226 Operating System Capstone · Spring 2026*

![Arch](https://img.shields.io/badge/arch-RISC--V%2064-283272?style=flat-square)
![Board](https://img.shields.io/badge/board-Orange%20Pi%20RV2-f47920?style=flat-square)
![Lang](https://img.shields.io/badge/lang-C%20%2B%20Assembly-555?style=flat-square)
![Firmware](https://img.shields.io/badge/firmware-OpenSBI%20%2B%20U--Boot-6a5acd?style=flat-square)
![Labs](https://img.shields.io/badge/labs-7%20%2F%207-2ea44f?style=flat-square)

[Course website](https://nycu-caslab.github.io/OSC2026/) ·
[Demo video](#-demo) ·
[Lab overview](#-roadmap)

</div>

---

This repository contains my work for the [Operating System Capstone (OSC 2026)](https://nycu-caslab.github.io/OSC2026/) course at NYCU. Over seven labs I built a small RISC-V (RV64) kernel with no standard library and no existing OS underneath. It starts as a UART "hello world", gains a bootloader, a memory allocator, interrupts, a scheduler, user processes, Sv39 virtual memory and finally a virtual file system. It runs on a real **Orange Pi RV2** board (SpacemiT K1/X1 SoC) and is booted by OpenSBI and U-Boot.

Each `labN/` directory is a **self-contained snapshot** of the kernel at the end of that lab. Each lab builds on the one before it, so `lab7/` is the most complete kernel.

## 🎬 Demo

The final kernel running several user processes at once: the course's video player program draws to the HDMI framebuffer while the interactive shell keeps responding over UART. Fork, the preemptive scheduler, timer-driven `usleep` and the `display` syscall all work together here.

<!-- Tip: drag-and-drop asset/multi-process_demo.mp4 into this README in GitHub's web editor
     to get a github.com/user-attachments URL. Pasting that URL alone on a line makes GitHub play the video inline. -->

▶️ **[Watch `asset/multi-process_demo.mp4`](asset/multi-process_demo.mp4)**

## 🗺 Roadmap

| Lab | Topic | What the kernel gained | Code |
|:---:|-------|------------------------|------|
| 1 | [Hello World](https://nycu-caslab.github.io/OSC2026/labs/lab1.html) | Boot stub, polling UART driver, shell, SBI `ecall` wrapper | [`lab1/`](lab1) |
| 2 | [Booting](https://nycu-caslab.github.io/OSC2026/labs/lab2.html) | UART bootloader, FDT parser, cpio initramfs, **self-relocating bootloader** | [`lab2/`](lab2) |
| 3 | [Memory Allocator](https://nycu-caslab.github.io/OSC2026/labs/lab3.html) | Buddy page allocator, slab-style chunk pools, DTB-driven reserved memory | [`lab3/`](lab3) |
| 4 | [Exception & Interrupt](https://nycu-caslab.github.io/OSC2026/labs/lab4.html) | Trap vector, U-mode programs, timer queue, PLIC + async UART, priority task queue | [`lab4/`](lab4) |
| 5 | [Thread & User Process](https://nycu-caslab.github.io/OSC2026/labs/lab5.html) | Kernel threads, round-robin scheduler, `fork`/`exec`/`wait`, framebuffer, **POSIX signals** | [`lab5/`](lab5) |
| 6 | [Virtual Memory](https://nycu-caslab.github.io/OSC2026/labs/lab6.html) | Sv39 higher-half kernel, per-process page tables, **`mmap`, demand paging, copy-on-write** | [`lab6/`](lab6) |
| 7 | [Virtual File System](https://nycu-caslab.github.io/OSC2026/labs/lab7.html) | VFS + tmpfs root, mounts, per-process fd/cwd, read-only `/ramfs`, **`/dev/uart`, `/dev/fb`** | [`lab7/`](lab7) |

**Bold** marks the advanced (bonus) exercises.

---

## Lab 1 — Hello World

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab1.html) · 📁 [`lab1/`](lab1) (board) · [`lab1/qemu/`](lab1/qemu) (QEMU `virt` variant)

**Goal:** get C code running on bare metal, talk to the outside world through the UART, and query the firmware.

| Exercise | Implementation |
|----------|----------------|
| Basic initialization | [`boot/start.S`](lab1/boot/start.S) zeroes `.bss` between `__bss_start` and `__bss_stop`, points `sp` at the end of the image and tail-calls `start_kernel` |
| UART setup | [`src/uart.c`](lab1/src/uart.c) is a polling driver over MMIO at `0xD4017000`. It spins on `LSR.DR`/`LSR.TDRQ` and translates `\r` ↔ `\n` |
| Simple shell | Line editor with backspace handling; supports `help` and `hello` |
| System information | [`src/sbi.c`](lab1/src/sbi.c) wraps `ecall` in inline asm (`a0`–`a7`). The `info` command prints the SBI spec version, implementation ID and implementation version |

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab1_3.png" width="45%" alt="Simple shell">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab1_4.png" width="45%" alt="info command">
  <br><sub>Expected shell and <code>info</code> output. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab1.html">OSC 2026 Lab 1</a></sub>
</p>

---

## Lab 2 — Booting

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab2.html) · 📁 [`lab2/bootloader_OrangePi/`](lab2/bootloader_OrangePi) · [`lab2/kernel/`](lab2/kernel) · [`lab2/bonus/`](lab2/bonus)

**Goal:** stop copying the SD card for every build by loading kernels over serial, and discover hardware from the devicetree instead of hard-coding it.

- **UART bootloader.** [`upload_kernel.py`](lab2/upload_kernel.py) sends a small header (`magic = "BOOT"`, `size`) followed by the raw kernel. [`loader.c`](lab2/bootloader_OrangePi/src/loader.c) checks the magic, streams the bytes to a separate load address, then jumps to the new kernel and passes `hartid` and the DTB pointer through.
- **Devicetree.** [`dt_parse.c`](lab2/kernel/src/dt_parse.c) is a hand-written FDT walker (`fdt_path_offset`, `fdt_getprop`, big-endian swaps). It finds the UART base, the initrd range (`/chosen`) and the memory size at runtime.
- **Initial ramdisk.** [`initrd_parse.c`](lab2/kernel/src/initrd_parse.c) parses *New ASCII Format* cpio archives. The `ls` and `cat <file>` shell commands use it.
- **⭐ Advanced: bootloader self-relocation.** [`lab2/bonus/`](lab2/bonus) uses the DTB to find a free spot in DRAM. It scans down from the top of memory and skips its own image, the DTB and the ramdisk. It copies itself there ([`relocate.c`](lab2/bonus/src/relocate.c)), then switches stack and PC with [`new_sp.S`](lab2/bonus/src/new_sp.S). The real kernel can then be loaded at the standard `0x00200000` entry point, where the bootloader originally ran.

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/devicetree.png" width="55%" alt="Flattened devicetree layout">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/ls_cat.png" width="38%" alt="ls and cat on the initramfs">
  <br><sub>FDT blob layout, and <code>ls</code>/<code>cat</code> on the cpio initramfs. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab2.html">OSC 2026 Lab 2</a></sub>
</p>

---

## Lab 3 — Memory Allocator

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab3.html) · 📁 [`lab3/kernel/`](lab3/kernel) (final) · [`lab3/kernel_base/`](lab3/kernel_base) (basic version, fixed memory range)

**Goal:** a page frame allocator for 4 KiB pages, a small-object allocator on top of it, and boot-time logic to bootstrap both.

- **Buddy system.** [`page_alloc.c`](lab3/kernel/src/page_alloc.c) keeps a `struct page` frame array and `free_area[0..MAX_ORDER]` free lists, built on an intrusive Linux-style [`list.h`](lab3/kernel/include/list.h). Allocation splits larger blocks. Freeing merges a block with its buddy (`idx ^ (1 << order)`) for as long as the buddy is free. Every split and merge is logged over UART for debugging.
- **Dynamic allocator.** `allocate(size)` and `free(ptr)` serve requests from 8 chunk pools (16 B to 2 KiB). The pools carve up pages from the buddy system. A page goes back to the buddy system when its last chunk is freed. Larger requests go to the buddy system directly.
- **⭐ Efficient page allocation.** Converting between a frame and its index or physical address is O(1) pointer arithmetic, so allocate and free stay O(log n).
- **⭐ Reserved memory.** The usable DRAM range comes from the DTB. The kernel image, the DTB, the initramfs, every `/reserved-memory` child node and the frame array itself are all reserved before the allocator hands anything out.
- **⭐ Startup allocation.** The frame array is not a fixed-size static. `find_mem_map_region()` sizes it from the detected memory and places it in a free hole in DRAM before the buddy system exists. This breaks the chicken-and-egg dependency between the frame array and the allocator.

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/buddy_frame_array.svg" width="48%" alt="Buddy frame array">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/buddy.svg" width="48%" alt="Buddy system">
  <br><sub>Frame array and buddy free lists. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab3.html">OSC 2026 Lab 3</a></sub>
</p>

---

## Lab 4 — Exception and Interrupt

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab4.html) · 📁 [`lab4/kernel/`](lab4/kernel)

**Goal:** handle traps, drop into U-mode, and make I/O and timers asynchronous.

- **Exception handling.** The trap entry in [`start.S`](lab4/kernel/boot/start.S) saves a full `pt_regs` frame. [`trap.c`](lab4/kernel/src/trap.c) decodes `scause`. `exec <file>` ([`exec.c`](lab4/kernel/src/exec.c)) loads a program from the initramfs and `sret`s into it in U-mode. Each `ecall` traps back and prints `scause`/`sepc`/`stval`, then `sepc += 4` resumes the program.
- **Core timer.** The timer uses the SBI timer extension with `rdtime`, and prints the time since boot every 2 seconds.
- **UART interrupt.** [`plic.c`](lab4/kernel/src/plic.c) enables the UART IRQ on the PLIC. [`uart.c`](lab4/kernel/src/uart.c) is now interrupt-driven, with RX and TX ring buffers: the ISR fills RX, and `putc` queues bytes and turns on the TX interrupt.
- **⭐ Timer multiplexing.** [`timer.c`](lab4/kernel/src/timer.c) keeps a list of one-shot timers sorted by deadline, and always programs the hardware for the earliest one. `setTimeout <sec> <msg>` demonstrates it.
- **⭐ Concurrent I/O handling.** ISRs stay short (top half). Deferred work goes into a priority-sorted queue ([`task.c`](lab4/kernel/src/task.c)), which runs with interrupts re-enabled before returning from the trap. A higher-priority task can **preempt** a running lower-priority one. The previous priority is saved and restored, so nested interrupts unwind correctly.

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/RISC_privilege.png" width="70%" alt="RISC-V privilege levels">
  <br><sub>RISC-V privilege modes: the kernel runs in S-mode on top of OpenSBI (M-mode). Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab4.html">OSC 2026 Lab 4</a></sub>
</p>

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab4_b1.png" width="32%" alt="Exception handling output">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab4_b2.png" width="32%" alt="Timer interrupt output">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab4_adv1.png" width="32%" alt="setTimeout output">
  <br><sub>Expected output for U-mode <code>ecall</code>, the 2-second timer, and <code>setTimeout</code>. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab4.html">OSC 2026 Lab 4</a></sub>
</p>

---

## Lab 5 — Thread and User Process

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab5.html) · 📁 [`lab5/kernel/`](lab5/kernel)

**Goal:** turn the kernel into a multitasking system that runs real user processes.

- **Threads.** [`thread.c`](lab5/kernel/src/thread.c) defines a `task_struct` with saved `ra`, `sp` and `s0`–`s11`. The assembly `switch_to()` swaps them and keeps `current` in `tp`. The kernel has a round-robin run queue, an idle thread, and reaping of zombie threads.
- **User processes and syscalls.** [`syscall.c`](lab5/kernel/src/syscall.c) dispatches on `a7`:

  | # | Syscall | # | Syscall |
  |---|---------|---|---------|
  | 0 | `getpid` | 5 | `waitpid` |
  | 1 | `uart_read` | 6 | `exit` |
  | 2 | `uart_write` | 7 | `stop` |
  | 3 | `exec` | 8 | `display` |
  | 4 | `fork` | 9 | `usleep` |

  `fork` duplicates the trap frame and user image, so the parent gets the child's PID and the child gets 0. The timer forces a reschedule, which makes the kernel **preemptive**.
- **Video player.** The scheduler tick runs at 1/32 s. `usleep` puts the caller to sleep and sets a timer to wake it. [`framebuffer.c`](lab5/kernel/src/framebuffer.c) writes frames to the Orange Pi RV2's HDMI framebuffer and flushes the D-cache with `cbo.flush` so each frame actually reaches the screen. The provided player runs alongside the shell (see the [demo](#-demo)).
- **⭐ POSIX signals.** [`signal.c`](lab5/kernel/src/signal.c) implements `signal` (10), `sigreturn` (11) and `kill` (12). Pending signals are kept as a bitmask and checked on the way back to user mode. Handlers run **in U-mode** on a dedicated signal stack. A `sigreturn` trampoline is copied onto that stack (followed by `fence.i`), and it restores the saved user context when the handler returns.

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab5_fork_test.png" width="48%" alt="fork test output">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab5_help.png" width="40%" alt="user program shell">
  <br><sub>Expected fork test output and the user-program shell. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab5.html">OSC 2026 Lab 5</a></sub>
</p>

---

## Lab 6 — Virtual Memory

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab6.html) · 📁 [`lab6/kernel/`](lab6/kernel)

**Goal:** give each process its own address space with RISC-V **Sv39** paging.

- **Kernel space.** The kernel is now linked at `0xffffffc000200000` ([`link.ld`](lab6/kernel/boot/link.ld)). `setup_vm()` in [`vm.c`](lab6/kernel/src/vm.c) builds an identity map plus a higher-half linear map of 4 GiB using 2 MiB pages. It splits pages down to 4 KiB where MMIO needs device attributes (UART, PLIC, framebuffer). It then writes `satp` (mode 8), runs `sfence.vma`, and drops the identity map once execution has moved to the high addresses.
- **User space.** Each process gets its own 3-level page table (`create_user_pgd`), with 4 KiB mappings for code at `0x0` and a stack just below `0x4000000000`. `fork` and `exec` were rewritten around page tables, and the context switch loads the next process's `satp`. The video player still runs smoothly.
- **⭐ `mmap` (syscall 13).** [`mmap.c`](lab6/kernel/src/mmap.c) tracks per-process `vm_region`s. It supports `PROT_READ|WRITE|EXEC`, `MAP_ANONYMOUS` and `MAP_POPULATE`, address hints, and overlap checks.
- **⭐ Page faults and demand paging.** Pages in a region are mapped only on first touch. Accesses outside any region, or that violate its protection, print `[Segmentation fault]` and kill the process.
- **⭐ Copy-on-write.** On `fork`, writable pages are shared read-only and tagged with a software `PTE_COW` bit, and each frame gets a reference count. A store fault on a COW page copies the frame, or just restores write access if this process is the frame's last user.

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/Riscv_SV39_Memory_Layout.png" width="46%" alt="Sv39 memory layout">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab6_sv39.png" width="50%" alt="Sv39 page table walk">
  <br><sub>Sv39 address space split and the 3-level page-table walk. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab6.html">OSC 2026 Lab 6</a></sub>
</p>

---

## Lab 7 — Virtual File System

> 📘 [Lab spec](https://nycu-caslab.github.io/OSC2026/labs/lab7.html) · 📁 [`lab7/kernel/`](lab7/kernel)

**Goal:** a Unix-style VFS layer with multiple file systems, mount points and device files.

- **Root file system.** [`tmpfs.c`](lab7/kernel/src/tmpfs.c) is an in-memory file system implementing the vnode and file operations (`lookup`, `create`, `mkdir`, `open`, `read`, `write`, `close`). [`vfs.c`](lab7/kernel/src/vfs.c) mounts it as `/`.
- **Multi-level VFS.** Path resolution walks the path one component at a time. It handles `.` and `..`, and crosses into a mounted file system's root whenever it reaches a mount point.
- **Multitask VFS.** Each `task_struct` has its own `cwd` and a 16-entry `fd_table`. New syscalls: `open` (14), `close` (15), `read` (16), `write` (17), `mkdir` (18), `mount` (19), `chdir` (20). They accept both absolute and relative paths.
- **`/ramfs`.** [`ramfs.c`](lab7/kernel/src/ramfs.c) builds a read-only tree from the cpio initramfs and mounts it at `/ramfs`. `write`, `create` and `mkdir` on it fail.
- **⭐ `/dev/uart`.** [`uartdev.c`](lab7/kernel/src/uartdev.c) registers a character device and creates its node with `mknod`. Every process starts with fd 0, 1 and 2 opened on it.
- **⭐ `/dev/fb`.** [`framebuffer.c`](lab7/kernel/src/framebuffer.c) exposes the HDMI framebuffer as a write-only device. It supports `lseek64` (21) and `ioctl` (22) for querying framebuffer info, and flushes the cache after each write.

The VFS sets itself up at boot like this:

```text
/                 tmpfs  (root)
├── dev/
│   ├── uart      char device → UART console (fd 0/1/2)
│   └── fb        char device → HDMI framebuffer
└── ramfs/        ramfs  (read-only, populated from initramfs.cpio)
```

<p align="center">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab7_vfs_ex.png" width="40%" alt="Example VFS tree">
  <img src="https://nycu-caslab.github.io/OSC2026/_images/lab7_impl_vis.png" width="56%" alt="VFS implementation overview">
  <br><sub>An example mount tree and how the VFS objects connect. Image source: <a href="https://nycu-caslab.github.io/OSC2026/labs/lab7.html">OSC 2026 Lab 7</a></sub>
</p>

---

## 🛠 Building & Running

**Toolchain:** `riscv64-unknown-elf-gcc`, `mkimage` (U-Boot tools), and optionally `qemu-system-riscv64`. Python 3 with `pyserial` is needed for the helper scripts.

```bash
cd lab7/kernel
make            # produces build/kernel.bin and the FIT image build/kernel.fit
```

**On the Orange Pi RV2**, there are two ways to boot:

1. Copy `build/kernel.fit` to the SD card's boot partition (Lab 2 includes a [`copy.sh`](lab2/bonus/build/copy.sh) helper for this).
2. Boot the Lab 2 UART bootloader once, type `load` in its shell, then push the kernel over serial from the lab's root directory (e.g. `lab7/`):

   ```bash
   python3 flush_uart.py /dev/ttyUSB0
   ```

   ```bash
   python3 upload_kernel.py /dev/ttyUSB0
   ```

**On QEMU**, Lab 1 has a `virt`-machine variant:

```bash
cd lab1/qemu && make run
```

The later labs are tuned for the Orange Pi RV2's UART, PLIC and framebuffer, so they are meant to run on the real board.

## 📂 Repository Layout

```text
.
├── asset/                 demo media
├── lab1/                  Hello World (board) + qemu/ variant
├── lab2/
│   ├── bootloader_OrangePi/   UART bootloader
│   ├── kernel/                kernel with FDT + initramfs
│   └── bonus/                 self-relocating bootloader
├── lab3/kernel{,_base}/   buddy + chunk allocator
├── lab4/kernel/           traps, timers, PLIC, task queue
├── lab5/kernel/           threads, processes, signals, framebuffer
├── lab6/kernel/           Sv39 VM, mmap, demand paging, COW
└── lab7/kernel/           VFS, tmpfs, ramfs, /dev/uart, /dev/fb
```

## 🙏 Acknowledgements

- Lab specifications, starter code and all diagrams/expected-output screenshots embedded above come from the **[NYCU OSC 2026 course website](https://nycu-caslab.github.io/OSC2026/)** by the NYCU CAS Lab. They are used here for reference, and each image links back to its source page.
- [RISC-V Privileged Specification](https://github.com/riscv/riscv-isa-manual), [RISC-V SBI Specification](https://github.com/riscv-non-isa/riscv-sbi-doc) and the [Devicetree Specification](https://www.devicetree.org/specifications/).
- The Linux kernel, which inspired the `list.h`, `container_of`, buddy allocator and VFS designs.
