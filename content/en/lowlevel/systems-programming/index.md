---
title: Systems Programming
description: Talking to the operating system — syscalls, processes, threads, mmap, low-level I/O, signals, and the POSIX layer. The bridge between "your program" and the OS, and the on-ramp to the OS-from-scratch project.
tags: [systems, syscalls, posix, processes, signals]
order: 0
updated: 2026-06-21
---
# Systems Programming

Above the machine but below the framework sits the operating system, and you reach
it through **syscalls**. This branch is about that boundary: how a `read()` becomes a
trap into the kernel, how processes are created with `fork`/`exec`, what a file
descriptor really is, how `mmap` maps memory and files, and how signals interrupt
your control flow. It's the **POSIX layer** — the contract between your program and
the OS — and it sets up everything you'll build by hand in the OS project.

> A syscall is the one place your user-space program is allowed to ask the kernel to
> do something it can't do itself. Almost all "talking to the system" reduces to that.

## Planned notes

- The syscall: crossing the boundary into the kernel
- Processes: `fork`, `exec`, and `wait`
- File descriptors and low-level I/O (`open`/`read`/`write`)
- The process address space and `mmap`
- Memory-mapping files and anonymous mappings
- Signals and signal handling
- Pipes and inter-process communication
- Sockets: the networking syscalls (intro)
- Threads (pthreads) and the OS view of them
- The POSIX layer and what it standardizes
- `errno` and error-handling discipline
- macOS/BSD vs Linux syscall differences

## Core sources

- **Michael Kerrisk — *The Linux Programming Interface* (TLPI)** — the syscall bible. man7.org/tlpi
- **Stevens & Rago — *Advanced Programming in the UNIX Environment* (APUE)** — the classic.
- **man7.org** — Kerrisk's man pages, the authoritative syscall/API reference. man7.org
- Note the **macOS/BSD vs Linux** differences throughout (different syscall numbers, some APIs absent).

**Connects to:** [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/concurrency-and-performance/index|Concurrency & Performance]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]
