---
title: OS from Scratch
description: The spine project. A small operating system built milestone by milestone — bootloader, real→protected→long mode, a minimal kernel, interrupts, paging, a scheduler, a driver, and a shell. Target x86-64 via cross-compiler + QEMU.
tags: [os, kernel, bootloader, paging, x86-64, project]
order: 0
updated: 2026-06-21
---
# OS from Scratch

This is the branch that pulls the whole atlas together. Building even a tiny OS
forces you to *use* the machine model, C, pointers, assembly, the toolchain, and
systems concepts at the same time — there's no runtime to hide behind, because **you
are writing the runtime.** The project is structured as milestones from boot to a
working shell.

**Tooling decision (fixed):** target is **x86-64**, built with an `x86_64-elf`
cross-compiler and run under **QEMU**. This is host-agnostic — the same OS boots on
an Intel Mac today and on Apple Silicon tomorrow. Every tooling note assumes
cross-compiler + QEMU, not ARM bare metal.

## Planned milestones

- Setup: an `x86_64-elf` cross-compiler and QEMU
- The boot process: from BIOS/UEFI to your code
- A minimal bootloader
- Real mode → protected mode → long mode
- A freestanding kernel and the freestanding environment
- Writing to the screen (VGA text / framebuffer)
- The Global Descriptor Table (GDT)
- Interrupts and the Interrupt Descriptor Table (IDT)
- The PIC/APIC and timer interrupts
- Physical memory management
- Paging and virtual memory
- A simple scheduler and task switching
- A simple device driver (keyboard)
- A basic shell

## Core sources

- **OSDev Wiki** — the hub for everything here, including the *GCC Cross-Compiler* and *Bare Bones* guides. wiki.osdev.org
- **xv6 (MIT) + the xv6 book** — the most readable teaching kernel. pdos.csail.mit.edu/6.828/2023/xv6.html
- **OSTEP — *Operating Systems: Three Easy Pieces*** (free) — the best OS theory. ostep.org
- **Helin & Renberg — *The little book about OS development*** (free) and **\*Operating Systems: From 0 to 1\*** (free).
- **nanobyte OS** (YouTube) — a modern from-scratch series; **Intel SDM** for protected/long mode; **QEMU** docs.
- ⚠️ The **James Molloy** kernel tutorial has [known bugs](https://wiki.osdev.org/James_Molloy%27s_Known_Bugs) — cite it only with that warning.

**Connects to:** [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]
