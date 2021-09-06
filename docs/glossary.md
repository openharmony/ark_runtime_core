# Glossary

## Introduction

During the development of Panda Runtime, we faced the fact that terminology related to the
development of compilers and interpreters is confusing in some cases. This document describes what
the terms used mean.

## Core terminology

* **AOT** stands for **Ahead-Of-Time**. In compilers, terms "ahead-of-time compilation" and "AOT
  compilation" are used to indicate that the source code or bytecode is compiled before actual
  execution. In case of Panda Runtime, AOT compilation is used to compile Panda Bytecode into
  native machine code to reduce runtime overhead from reading and compiling bytecode.
  See https://en.wikipedia.org/wiki/Ahead-of-time_compilation.
* **Bytecode**. See **Panda Bytecode**.
* **Compiler** is a tool that performs source code or bytecode translation, optimization and
  native code generation.
* **IR** stands for **Intermediate Representation**. IR is an internal compiler data structure
  used to represent input program and usually designed for analysis and optimization.
  See https://en.wikipedia.org/wiki/Intermediate_representation.
* **ISA** stands for **Instruction Set Architecture**. See **Panda Bytecode** and
  https://en.wikipedia.org/wiki/Instruction_set_architecture.
* **JIT** stands for **Just-In-Time**. In compilers, terms "just-in-time compilation" and "JIT
  compilation" are used to indicate that the source code or bytecode is compiled during program
  execution. In case of Panda Runtime, JIT compilation is used to compile Panda Bytecode into
  native machine code to reduce overhead from interpreting bytecode.
  See https://en.wikipedia.org/wiki/Just-in-time_compilation.
* **Panda Assembler** is a tool that translates **Panda Assembly Language**
  to **Panda Binary File**.
* **Panda Assembly Language** is a low-level programming language retaining very strong
  correspondence between the instructions in the language and **Panda Bytecode** instructions.
  See [Assembly Format](assembly_format.md).
* **Panda Binary File** or **Panda File** or **PF** is a binary representation of Panda Bytecode.
  See [File Format](file_format.md).
* **Panda Bytecode** or **PBC** is a portable hardware-independent instruction set designed for
  compactness and efficient execution by Panda Runtime. See https://en.wikipedia.org/wiki/Bytecode.
* **Runtime** is a runtime system, also called runtime environment.

## Memory management terms

* **Allocator** is a part of the system servicing allocation and deallocation requests.
* **Card** is a division of memory with some fixed size. A card is usually smaller than a page in size.
* **Card table** is used for marking cards. In general, it has one bit or byte (a byte used to
  improve performance of GC barrier) for one card.
    It can be used by both generational and concurrent collectors.
    It can be used for tracking references from the tenured generation to the young generation and
    for tracking modified references during the concurrent phase of GC.
* **Compacting GC** is a GC which compacts live objects to reduce fragmentation.
* **Conservative GC** or non-precise GC works with ambiguous references,
  i.e. it treats anything inside object boundaries like a reference. Opposite term: **Precise GC**.
* **Full GC** is cleaning the entire Heap.
* **Garbage collection** is also known as automatic memory management,
  which means automatic recycling of dynamically allocated memory.
* **Garbage collector** performs garbage collection. The garbage collector recycles memory
  that it can prove will never be used again.
* **GC** stands for Garbage collector or sometimes for Garbage collection.
* **Minor GC** in general is garbage collection performed over the young generation space
  (which includes survivor Eden and Survivor spaces).
* **Major GC** in general is garbage collection performed over the tenured or old generation space.
  Sometimes it is triggered by Minor GC, so it is impossible to separate them (see **Full GC**).
* **Safepoint** is a range of execution where the state of the executing thread is well described.
    Safepoint is used as a point at which we can safely stop the thread, and at this point, all
    references on the stack are mapped (i.e., it is known when we have an object on the stack or not).
    Mutator is at a known point in its interaction with the heap.
    It means that all objects which are currently in use are known and we know how to get a value
    for each of their fields.
* **Precise GC** deals only with exact/sure references, i.e. it knows the object layout and can
  extract references to the objects. Opposite term: **Conservative GC**.
* **Semi-space compaction** is a compaction algorithm when memory is split to the two equal spaces
  (one is empty and one used for allocation) and we just copy objects from one space to another
  using the bump pointer allocator.
* **STW** stands for **stop the world**, used in context of stop the world pauses forced by GC -
  GC force to stop execution of code (interpreted, AOTed or JITed) to make exclusively some
  operations.
* **TLAB** stands for **Thread Local Allocation Buffer** - buffer used exclusively for allocations
  in the thread (no lock required).
* **White object** is an object non visited by GC (at the end of GC white objects will be reclaimed).
* **Gray object** is reachable/alive object, but still should be scanned by GC (to process all fields with reference type).
* **Black object** is reachable/alive object and scanned by GC.
* **Throughput** is % of time not spent in GC over a long period of time (sometimes `GC throughput` is used as % time spent in GC over a long period of time).
