---
title: "The process address space & virtual memory"
description: Why every process sees its own clean, private memory from address 0 upward — the illusion the OS and MMU build on top of physical RAM. Pages, virtual-to-physical translation, isolation, and a fork demo where the same address holds two different values.
tags: [machine-model, virtual-memory, address-space, paging, mmu]
order: 12
updated: 2026-06-22
---
# The process address space & virtual memory

Every note so far printed addresses as if your program owned all of memory. It doesn't —
and yet the illusion is airtight. Each process gets its **own private virtual address
space**: a flat range from 0 upward that *looks* like dedicated RAM but is a fiction the OS
and the CPU's **MMU** (memory management unit) maintain on top of shared physical memory.
The address `0x1040` in your process and `0x1040` in another are *different* bytes of
physical RAM — or one may be in RAM and the other swapped to disk. This indirection buys
three things at once: **isolation** (a process can't see or corrupt another's memory),
**the illusion of contiguous space**, and **more address space than you have RAM**. This
note is the first look; the full mechanism is a kernel topic the OS project will build.

> The reset: a pointer is a *virtual* address, not a physical location. Two processes can
> hold the identical pointer value and mean completely different memory. The flat
> address-space map from earlier notes is per-process and entirely virtual.

## The illusion: virtual vs physical

Programs only ever use **virtual addresses**. On every memory access, the MMU translates
the virtual address to a **physical address** in RAM, using per-process tables the kernel
maintains. Because the mapping is per-process:

- **Isolation is automatic.** Process A simply has no mapping for process B's pages, so it
  *cannot* name them. A wild pointer crashes your own process, not the system.
- **Layout can be simple.** Each process sees the clean text/data/heap/…/stack map from the
  [[lowlevel/machine-model/stack-vs-heap|stack-vs-heap]] note, starting near 0, regardless
  of where its pages physically landed or how fragmented RAM is.
- **Virtual can exceed physical.** Rarely-used pages live on disk (swap) and are pulled in
  on demand, so total virtual memory across processes can exceed installed RAM.

## Pages: the unit of mapping

Virtual memory isn't mapped byte-by-byte — that table would be enormous. Memory is divided
into fixed-size **pages** (4 KB on x86-64 and Apple Silicon; larger "huge pages" exist).
The kernel's **page tables** map each virtual page to a physical **frame**, and the MMU
walks them on every access, caching recent translations in the **TLB** (translation
lookaside buffer) so the common case is fast.

- A virtual address splits into a **page number** (which page) and an **offset** (where in
  the page). Only the page number is translated; the offset is copied through.
- A reference to a page that isn't currently in RAM triggers a **page fault** — the CPU
  traps to the kernel, which fetches the page (from disk, or zero-fills a fresh one) and
  resumes your instruction as if nothing happened.
- Each page carries **permissions** (read/write/execute). Writing a read-only page (like
  code) or an unmapped page is what produces a **segmentation fault**.

## See the isolation: same address, two values

`fork()` duplicates a process. Parent and child then have the *same* virtual address for a
global — but writing it in the child does not affect the parent, because each has its own
physical page (copied on write). Build and run:

```c
// vmem.c — one virtual address, two independent values across processes.
// gcc -O0 -Wall -Wextra vmem.c -o vmem && ./vmem
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int shared_global = 100;       // same virtual address in parent and child

int main(void) {
    printf("page size: %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("&shared_global = %p (value %d)\n\n",
           (void*)&shared_global, shared_global);
    fflush(stdout);            // flush before fork, or the child inherits our buffer

    pid_t pid = fork();
    if (pid == 0) {            // child: same address, its own physical page (COW)
        shared_global = 999;
        printf("[child  pid=%d] &g = %p  value = %d\n",
               getpid(), (void*)&shared_global, shared_global);
        return 0;
    }
    wait(NULL);                // parent: unchanged by the child's write
    printf("[parent pid=%d] &g = %p  value = %d\n",
           getpid(), (void*)&shared_global, shared_global);
    return 0;
}
```

```
page size: 4096 bytes
&shared_global = 0x105960000 (value 100)

[child  pid=68695] &g = 0x105960000  value = 999
[parent pid=68694] &g = 0x105960000  value = 100
```

Same pointer `0x105960000` in both processes, yet the child reads `999` and the parent still
reads `100`. That's virtual memory: the address is an *index into each process's private
map*, not a physical location. The **copy-on-write** that makes `fork` cheap — pages are
shared read-only until one side writes, then transparently duplicated — is the same paging
machinery at work.

## Where this connects (and what's deferred)

This is the foundation under a lot of the atlas, but the deep mechanism is kernel work:

- **`mmap`** asks the kernel to add mappings to your address space directly — anonymous
  memory (what `malloc` uses under the hood for big requests) or a file mapped into memory.
  That's a [[lowlevel/systems-programming/index|systems programming]] topic.
- **Page tables, the TLB, page-fault handling, and paging policy** are built from scratch in
  the [[lowlevel/os-from-scratch/index|OS project]] — when you write the paging code, this
  illusion becomes something *you* implement.
- **Caches sit physically between MMU and RAM**, so virtual memory and the
  [[lowlevel/machine-model/the-memory-hierarchy|memory hierarchy]] interact: a TLB miss and
  a cache miss are different stalls on the same access path.

## Failure modes & trade-offs

- **Segfault = bad virtual access.** Dereferencing NULL, a dangling pointer, or running off
  an array touches an unmapped or wrong-permission page; the MMU traps and the kernel kills
  the process. It's the hardware protecting other processes from your bug.
- **Translation has a cost.** Every access is translated; a TLB miss forces a page-table
  walk. Poor locality thrashes the TLB just as it thrashes caches — another reason
  access patterns matter.
- **Swapping (thrashing) is a cliff.** When the working set exceeds RAM, the kernel pages to
  disk constantly and throughput collapses — the nanosecond world falling to milliseconds,
  as in the hierarchy note.
- **Addresses aren't stable or comparable across processes.** A virtual address is
  meaningful only within its process; never share raw pointers between processes (use shared
  memory with its own mapping). ASLR also randomizes them per run.

## In practice

- **Think "my pointers are private virtual addresses."** That single reframing explains
  isolation, why segfaults are local, and why two processes' pointers don't relate.
- **Reason about memory in pages.** Allocation, protection (`mprotect`), file mapping, and
  fault behavior are all page-granular; the 4 KB page is the real quantum of OS memory.
- **A segfault is a *diagnosis*, not a mystery.** It means "you named a virtual address you
  weren't allowed to touch" — almost always a bad pointer; reach for ASan/the debugger.
- **This closes the machine model.** You now have CPU, registers, the memory hierarchy, data
  representation, how source becomes execution, and the address space those bytes live in —
  the substrate the C layer builds on next.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/the-memory-hierarchy|The memory hierarchy]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Sources

- **Arpaci-Dusseau — *Operating Systems: Three Easy Pieces*, "Virtualizing Memory"** — the clearest free treatment of address spaces, paging, TLBs, and page faults; the spine for this note. https://pages.cs.wisc.edu/~remzi/OSTEP/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 9 — virtual memory from the programmer's view, with the x86-64 mechanics. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — how virtual memory, the TLB, and caches interact in real hardware. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 3A — the authoritative paging and MMU specification for x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **man pages — `mmap(2)`, `fork(2)`, `mprotect(2)`** — the syscalls that manipulate a process's address space. https://man7.org/linux/man-pages/man2/mmap.2.html
