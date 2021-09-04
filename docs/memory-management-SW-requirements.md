# Introduction

The main components of Panda memory management:
* Allocators
* Garbage Collector (GC)

Allocators have two purposes:
1. Allocations for the application
1. Allocations for the internal usage by Runtime (Allocations for compiler purposes, for GC internal structures etc)

Garbage Collector:
GC automatically recycles memory that will never be used again.
GC used to recycle memory allocated as result of application work (objects, compiled code etc).

# Overall Description

## Allocator

### Allocator Types
- Bump pointer allocator
- Arena Allocator (objects can be deallocated at once (list of arenas, almost at once - O (number of arenas in the list)))
- Freelist allocator
- TLAB
- Run of slots allocator
- Code allocator

### Spaces

- Code space (executable)
- Compiler Internal Space (linked list of arenas)
- Internal memory space for non-compiler part of runtime (including GC internals)
- Object space
- Non-moving space (space for non-movable objects)

## GC

- Concurrent generational GC (optional - we can disable generational mode)
- GC for classes (optional)
- GC for code cache (optional)
- Reference processor

High level requirements for GC:
- Acceptable latency (max pause) for good user experience
- Acceptable throughput
- Acceptable footprint size

# Key Features of the Memory Management System

You can flexibly configure the set of allocators and GCs.
You can choose MM configurations for applications using a profile (for example, you can choose non-compacting GCs with freelist allocators for some applications
if acceptable metrics is available for the applications).

# Interaction with Other Components

## Allocator

- Allocator API for runtime
- TLAB API for compiler and interpreter
- Interfaces for tools

## GC

- Safepoints
- Reference storage and additional interfaces for MM <-> JNI interaction
- Barriers APIs for compilers and interpreters
- Possibility to change VRegs for any frame in the stack
- Interfaces for tools

# Additional Requirements

- Memory management flexible enough to work with multiple languages.
- Provide various statistics data
