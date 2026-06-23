# Low-Level Atlas — Content Plan

Living plan for filling the atlas with rich, source-backed notes (target **10–15 per
branch**, atomic and dense, no fluff). Written EN-first; ES overlays are a second pass
(the build falls back to EN with a banner meanwhile). This is the document we iterate
on before writing atomic notes.

## Throughline

From *"I can program"* to *"I understand the machine."* The reader is a senior
engineer fluent in managed languages (TypeScript, Kotlin/Java) with little low-level
depth. The atlas descends from the machine model through C, pointers, assembly, and
the toolchain, climbs back up into systems and performance, and converges on an OS
built from scratch. Software-craftsmanship discipline (TDD, clean code, XP) runs
through all of it.

## Fixed decisions (do not revisit)

- **OS target = x86-64**, built with an `x86_64-elf` cross-compiler, run under **QEMU**.
  Host-agnostic — same OS on Intel today and Apple Silicon tomorrow. Tooling notes
  assume cross-compiler + QEMU, **not** ARM bare metal.
- **Assembly = x86-64 primary** (matches the OS, the literature, Compiler Explorer
  defaults) with an **ARM64 appendix** per asm note (daily future hardware).
- **C++ is optional.** `modern-cpp` is a deferred branch (flagged `optional` in
  `build.py`); expand after the C layer is solid. The OS is C-first.
- **Topic slug = `lowlevel`** (paths and wikilinks). Repo: `lowlevel-notes`.

## Taxonomy (phase → branch)

- **00 Orientation** — entry notes only (`start-here`, `must-know`, `reference-registry`)
- **01 Foundations** — `machine-model`
- **02 The C Layer** — `c-from-the-metal` · `pointers-and-memory`
- **03 Down to the Metal** — `assembly-and-compiler-output` · `toolchain-and-linking`
- **04 C++ (optional)** — `modern-cpp` *(deferred)*
- **05 Systems** — `systems-programming` · `concurrency-and-performance`
- **06 The Project** — `os-from-scratch`
- **★ Always Active** — `craftsmanship-low-level`

## Progress

Scaffold pass: every branch has a written `index.md` with a planned-note roadmap,
sources, and cross-links; taxonomy wired in `build.py`; site builds with 0 unresolved
links. **`machine-model` is the first fully written branch (12/12 notes, EN+ES, each
with a runnable demo under `examples/`).** Remaining branches still to be written.

- [x] **machine-model** — index ✓ · **12/12 notes DONE** (✓ you-are-the-runtime-now · ✓ the-cpu-fetch-decode-execute · ✓ registers-and-the-isa · ✓ the-memory-hierarchy · ✓ cache-lines-and-locality · ✓ stack-vs-heap · ✓ bits-bytes-words-and-addresses · ✓ twos-complement-and-integer-representation · ✓ endianness · ✓ ieee-754-floating-point · ✓ how-source-becomes-execution · ✓ process-address-space-and-virtual-memory · EN+ES · runnable demo per note under `examples/machine-model/`)
- [ ] **c-from-the-metal** — index ✓ · 0/13 notes
- [ ] **pointers-and-memory** — index ✓ · 0/15 notes  *(flagship; set the quality bar here)*
- [ ] **assembly-and-compiler-output** — index ✓ · 0/12 notes
- [ ] **toolchain-and-linking** — index ✓ · 0/13 notes
- [ ] **modern-cpp** — index ✓ · 0/12 notes  *(OPTIONAL — expand after the C layer)*
- [ ] **systems-programming** — index ✓ · 0/12 notes
- [ ] **concurrency-and-performance** — index ✓ · 0/13 notes
- [ ] **os-from-scratch** — index ✓ · 0/14 milestones
- [ ] **craftsmanship-low-level** — index ✓ · 0/12 playbooks
- [ ] **ES overlay pass** (after EN notes land)

Suggested write order: `machine-model` → `c-from-the-metal` → `pointers-and-memory`
→ `assembly-and-compiler-output` → `toolchain-and-linking` → `systems-programming`
→ `concurrency-and-performance` → `os-from-scratch` → `craftsmanship-low-level`,
with `modern-cpp` optional/last.

## Per-branch note outlines + core sources

### machine-model  *(01 Foundations)*
*Sources: CS:APP (Bryant & O'Hallaron) ← spine; "Code" (Petzold) for intuition; Ben Eater (8-bit CPU) for the deep model; Agner Fog microarch (advanced).*
You are the runtime now · The CPU: fetch–decode–execute & the clock · Registers & the
ISA · The memory hierarchy (registers→L1/L2/L3→RAM→disk) · Cache lines & locality ·
Stack vs heap (the two regions) · Bits, bytes, words & addresses · Two's complement &
integer representation · Endianness · IEEE 754 floating point · How source becomes
execution · The process address space & virtual memory (intro)

### c-from-the-metal  *(02 The C Layer)*
*Sources: "Modern C" (Gustedt, free) ← spine; K&R; "Effective C" (Seacord); "Expert C Programming: Deep C Secrets" (van der Linden); Beej's Guide to C; cppreference.*
Why C still matters · The weak type system · Undefined behavior (the contract) · More
UB: signed overflow, aliasing, sequencing · The preprocessor · Translation units,
declarations vs definitions, linkage · Arrays & array-to-pointer decay · Structs,
unions & bitfields · Strings as `char*` · Integer promotions & conversions · The
minimal standard library · Build & run a C program from zero · Reading the C standard
& cppreference

### pointers-and-memory  *(02 The C Layer — flagship)*
*Sources: Drepper "What Every Programmer Should Know About Memory" ← the memory paper; CS:APP malloc chapters; Reese "Understanding and Using C Pointers"; Ryan Fleury "Untangling Lifetimes: The Arena Allocator"; Doug Lea / Dan Luu malloc writeups.*
What a pointer really is · Pointer arithmetic & stride · The stack: frames & automatic
storage · The heap: malloc/free & the allocator · Pointers to pointers · `void*` &
type erasure · const-correctness · Function pointers · Struct layout: alignment &
padding · Data layout & cache-friendliness · Strict aliasing & type punning ·
Use-after-free & double-free · Buffer overflow & out-of-bounds · Memory leaks &
ownership · Custom allocators: arena, pool, bump

### assembly-and-compiler-output  *(03 Down to the Metal)*
*Sources: Compiler Explorer (godbolt) ← daily tool; "Programming from the Ground Up" (Bartlett, free); "x86-64 Assembly Language Programming with Ubuntu" (Jorgensen, free); Felix Cloutier x86 reference; Intel SDM. ARM64: "Programming with 64-Bit ARM Assembly" (Smith); Apple ARM64 docs.*
Why read assembly (Godbolt) · x86-64 registers · Core instruction set (mov, arithmetic,
lea) · Addressing modes & memory operands · Control flow, jumps & flags · The stack at
asm level · Stack frames (prologue/epilogue) · System V AMD64 calling convention · How
C constructs compile · What `-O2` does · Inline assembly · ARM64 appendix: AArch64
registers & calling convention

### toolchain-and-linking  *(03 Down to the Metal)*
*Sources: "Linkers and Loaders" (John Levine) ← spine; Ian Lance Taylor's 20-part linker series; "How To Write Shared Libraries" (Drepper); "Beginner's Guide to Linkers" (Drysdale); ELF spec; Clang sanitizer docs; GDB manual.*
The pipeline (preprocess→compile→assemble→link) · Object files · The ELF format ·
Symbols: definition, reference, resolution · Static linking & archives · Dynamic
linking & shared libraries · The dynamic loader, PLT/GOT, relocation · gcc vs clang &
the driver · make & dependency graphs · cmake · gdb/lldb essentials · Sanitizers
(ASan/UBSan/TSan) · valgrind & profilers

### modern-cpp  *(04 C++ — OPTIONAL, deferred)*
*Sources: "A Tour of C++" (Stroustrup) ← concise spine; "Effective Modern C++" (Meyers); C++ Core Guidelines; "C++ Software Design" (Iglberger); learncpp.com (free); CppCon talks.*
C vs C++: what you gain · RAII (the idea that changes everything) · Smart pointers ·
Move semantics & value categories · References, const & overloading · Templates &
generic programming · The STL · Zero-cost abstractions (and where they leak) · Modern
C++ (11→23) highlights · Error handling: exceptions vs expected · Which subset to use ·
SerenityOS: an OS written in C++

### systems-programming  *(05 Systems)*
*Sources: "The Linux Programming Interface" (Kerrisk) ← syscall bible; APUE (Stevens); man7.org. Note macOS/BSD vs Linux differences.*
The syscall (crossing into the kernel) · Processes: fork/exec/wait · File descriptors &
low-level I/O · The address space & mmap · Memory-mapping files · Signals · Pipes & IPC
· Sockets (intro) · Threads (pthreads) & the OS view · The POSIX layer · errno &
error-handling discipline · macOS/BSD vs Linux syscall differences

### concurrency-and-performance  *(05 Systems)*
*Sources: "C++ Concurrency in Action" (Williams); Jeff Preshing blog (memory ordering/lock-free); "The Art of Multiprocessor Programming" (Herlihy & Shavit); Agner Fog optimization manuals; "Systems Performance" (Gregg); Mike Acton "Data-Oriented Design" (CppCon).*
Concurrency vs parallelism · Threads & shared state · Data races · Mutexes & condition
variables (their cost) · Atomics & the memory model · Memory ordering
(acquire/release/seq-cst) · Lock-free programming · False sharing & cache coherence ·
Cache-aware & cache-oblivious code · SIMD & vectorization · Data-oriented design ·
Benchmarking seriously · Profiling

### os-from-scratch  *(06 The Project — milestones)*
*Sources: OSDev Wiki ← the hub; xv6 + book (MIT) ← most readable teaching kernel; OSTEP (free, best theory); "The little book about OS development" (Helin & Renberg, free); "Operating Systems: From 0 to 1" (free); nanobyte OS (YouTube); Intel SDM (protected/long mode); QEMU + OSDev "GCC Cross-Compiler" guide. ⚠️ James Molloy tutorial has known bugs — cite with the warning.*
Setup: x86_64-elf cross-compiler & QEMU · The boot process (BIOS/UEFI→your code) · A
minimal bootloader · Real→protected→long mode · A freestanding kernel · Writing to the
screen (VGA/framebuffer) · The GDT · Interrupts & the IDT · PIC/APIC & timer
interrupts · Physical memory management · Paging & virtual memory · A simple scheduler
& task switching · A simple driver (keyboard) · A basic shell

### craftsmanship-low-level  *(★ Always Active — playbooks)*
*Sources: "Test-Driven Development for Embedded C" (Grenning) ← spine; GoogleTest/Catch2/Unity docs; "Working Effectively with Legacy Code" (Feathers); libFuzzer/AFL++ docs; SerenityOS as a living case study.*
TDD in C/C++ · Testing frameworks: Unity, Catch2, GoogleTest · Test doubles & seams in
C · Contracts: assert, preconditions, invariants · Defensive programming · Fuzzing
(libFuzzer & AFL++) · Sanitizers in the test loop · Code review for memory-unsafe code
· Refactoring legacy C safely · Clean code at the bare metal · Build hygiene
(warnings-as-errors, CI) · SerenityOS as a disciplined-OS case study

## Conventions

- Atomic notes: H1 + short framing (mental model first) + 2–4 sections (bullets/tables)
  + a **Pitfall**/**In practice** angle + at least one runnable artifact (C preferred,
  x86-64 asm via Godbolt) + a `## Sources` section + a **Connects to:** line of
  `[[wikilinks]]`.
- Assembly notes: x86-64 primary, **ARM64 appendix** when it adds value.
- Cite real, authoritative, current sources; put them in the branch index "Core
  sources" and `SOURCES.md`. No marketing summaries; never fabricate a citation.
- `featured: true` on one standout note across the whole atlas; `draft: true` to stage WIP.
- Internal links use `[[lowlevel/<branch>/<slug>|Label]]`; never leave unresolved links.
  When a planned note becomes real, swap its plain-text roadmap entry in the branch
  `index.md` for a `[[wikilink]]`.
