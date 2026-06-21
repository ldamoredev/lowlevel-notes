---
title: Must Know
description: The load-bearing ideas of low-level programming — the handful of truths that, once internalized, make the rest of the atlas click.
tags: [orientation, fundamentals]
order: 2
updated: 2026-06-21
---
# Must Know

If you remember nothing else from this atlas, remember these. Each one is expanded
across the branches; together they're the spine of the low-level mindset.

## The ideas

- **You are the runtime now.** No GC, no bounds checks, no integer normalization. The
  machine executes the bytes exactly — including the wrong thing.
- **A pointer is an address with a type.** Everything powerful and everything dangerous
  about C follows from that one sentence.
- **Memory is a hierarchy, and distance is time.** A cache miss can cost ~100× a hit.
  Performance is mostly about access patterns, not clever code.
- **Undefined behavior is a contract you signed.** The compiler is allowed to assume it
  never happens — and will optimize as if it doesn't.
- **The stack and the heap are different worlds.** One is automatic and fast; the other
  is manual and yours to manage. Knowing which is which prevents most memory bugs.
- **The compiler is not a black box.** Read its assembly and "what is this doing / is it
  fast?" stop being guesses.
- **Linking is real.** Separate compilation, symbols, and the loader are where half of
  the confusing errors live.
- **Discipline scales down.** TDD, contracts, and sanitizers matter *more* in C, not
  less — because the failure mode is corruption, not an exception.

## Where they live

- [[lowlevel/machine-model/index|Machine Model]] — the runtime reset, the memory hierarchy.
- [[lowlevel/pointers-and-memory/index|Pointers & Memory]] — pointers, stack vs heap, bugs.
- [[lowlevel/c-from-the-metal/index|C from the Metal]] — undefined behavior, the type system.
- [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] — reading the compiler.
- [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] — linking and the build pipeline.
- [[lowlevel/craftsmanship-low-level/index|Craftsmanship Low-Level]] — discipline at the metal.

See also: [[lowlevel/start-here|Start Here]] · [[lowlevel/index|Full Index]]
