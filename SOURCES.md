# SOURCES.md — curated reference sources for Low-Level Atlas

Authoritative, canonical sources to ground notes per branch. Cite the relevant ones in
each branch `index.md` under `## Core sources` and in individual notes' `## Sources`.
Prefer primary/canonical sources over blog aggregators. Mark per note where each claim
comes from. Verify a URL before relying on it; treat these as starting points, not
gospel.

## Cross-cutting / general

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — the
  systems backbone (machine model, memory, linking, performance). csapp.cs.cmu.edu
- **Intel 64 and IA-32 Architectures Software Developer's Manual (SDM)** — x86-64 authority.
- **System V AMD64 ABI** — the x86-64 calling convention / object model.
- **cppreference** — C and C++ reference. en.cppreference.com
- **Compiler Explorer (godbolt.org)** — see what any compiler emits, live.

## machine-model
- **CS:APP** (Bryant & O'Hallaron) — the spine. csapp.cs.cmu.edu
- **Charles Petzold — *Code: The Hidden Language of Computer Hardware and Software*** — intuition from relays up.
- **Ben Eater — *Build an 8-bit computer from scratch*** — the deepest mental model. eater.net/8bit
- **Agner Fog — microarchitecture manuals** (advanced). agner.org/optimize

## c-from-the-metal
- **Jens Gustedt — *Modern C*** (free) — the spine. gustedt.gitlabpages.inria.fr/modern-c
- **Kernighan & Ritchie — *The C Programming Language* (K&R)** — the canonical original.
- **Robert Seacord — *Effective C*** — modern, security-minded C.
- **Peter van der Linden — *Expert C Programming: Deep C Secrets*** — quirks & UB gold.
- **Beej's Guide to C** (free) — beej.us/guide/bgc · **cppreference (C)** — en.cppreference.com/w/c

## pointers-and-memory
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — the memory paper. akkadia.org/drepper/cpumemory.pdf
- **CS:APP** — dynamic memory allocation (malloc) chapters.
- **Richard Reese — *Understanding and Using C Pointers*** (O'Reilly).
- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — rfleury.com
- **Doug Lea — dlmalloc** (gee.cs.oswego.edu/dl/html/malloc.html) and **Dan Luu** malloc writeups.

## assembly-and-compiler-output
- **Compiler Explorer (godbolt.org)** — the daily tool.
- **Jonathan Bartlett — *Programming from the Ground Up*** (free) — assembly from first principles.
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** (free PDF).
- **Felix Cloutier — x86/amd64 instruction reference** (felix.fr/x86) · **Intel SDM** (authority).
- **ARM64:** Stephen Smith — *Programming with 64-Bit ARM Assembly*; Apple ARM64 / Apple Silicon assembly docs.

## toolchain-and-linking
- **John Levine — *Linkers and Loaders*** — the spine.
- **Ian Lance Taylor — *Linkers* (20-part series)** — lwn.net/Articles/276782
- **Ulrich Drepper — *How To Write Shared Libraries*** — akkadia.org/drepper/dsohowto.pdf
- **David Drysdale — *Beginner's Guide to Linkers*** — lurklurk.org/linkers/linkers.html
- **ELF specification**; **Clang sanitizer docs** (clang.llvm.org/docs); **GDB manual** (sourceware.org/gdb/documentation).

## modern-cpp  *(optional branch)*
- **Bjarne Stroustrup — *A Tour of C++*** — the concise spine.
- **Scott Meyers — *Effective Modern C++***.
- **C++ Core Guidelines** (Stroustrup & Sutter) — isocpp.github.io/CppCoreGuidelines
- **Klaus Iglberger — *C++ Software Design***.
- **learncpp.com** (free) and **CppCon** talks (youtube.com/@CppCon).

## systems-programming
- **Michael Kerrisk — *The Linux Programming Interface* (TLPI)** — the syscall bible. man7.org/tlpi
- **Stevens & Rago — *Advanced Programming in the UNIX Environment* (APUE)**.
- **man7.org** — Kerrisk's man pages (authoritative API reference).
- Note **macOS/BSD vs Linux** syscall differences throughout.

## concurrency-and-performance
- **Anthony Williams — *C++ Concurrency in Action*** — threads, atomics, the memory model.
- **Jeff Preshing — blog** — memory ordering & lock-free. preshing.com
- **Herlihy & Shavit — *The Art of Multiprocessor Programming*** — theory of concurrent objects.
- **Agner Fog — optimization manuals** (agner.org/optimize) · **Brendan Gregg — *Systems Performance*** (brendangregg.com).
- **Mike Acton — *Data-Oriented Design* (CppCon 2014)** — youtube.com/watch?v=rX0ItVEVjHc

## os-from-scratch
- **OSDev Wiki** — the hub (incl. *GCC Cross-Compiler* + *Bare Bones* guides). wiki.osdev.org
- **xv6 (MIT) + the xv6 book** — the most readable teaching kernel. pdos.csail.mit.edu/6.828
- **OSTEP — *Operating Systems: Three Easy Pieces*** (free) — the best theory. ostep.org
- **Helin & Renberg — *The little book about OS development*** (free); **\*Operating Systems: From 0 to 1\*** (free).
- **nanobyte OS** (YouTube) — modern from-scratch series · **Intel SDM** (protected/long mode) · **QEMU** docs.
- ⚠️ **James Molloy** kernel tutorial — has [known bugs](https://wiki.osdev.org/James_Molloy%27s_Known_Bugs); cite only with the warning.

## craftsmanship-low-level
- **James Grenning — *Test-Driven Development for Embedded C*** — the spine.
- **GoogleTest** (google.github.io/googletest), **Catch2** (github.com/catchorg/Catch2), **Unity** (throwtheswitch.org) — testing frameworks.
- **Michael Feathers — *Working Effectively with Legacy Code*** — seams & safe refactoring.
- **libFuzzer** (llvm.org/docs/LibFuzzer.html) and **AFL++** (aflplus.plus) — coverage-guided fuzzing.
- **SerenityOS** (github.com/SerenityOS/serenity) — a disciplined from-scratch OS, a living case study.
