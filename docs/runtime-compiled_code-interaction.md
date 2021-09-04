# Interaction of compiled code and the runtime

## Introduction

Compiled code and Panda runtime interact with each other during execution. This document describes the following interactions:
* Runtime structures
* Calling convention
* Structure of compiled code stack frames and stack traversing
* Transition from the interpreter to compiled code and vice versa
* Calling the runtime
* Deoptimization
* Stack unwinding during exception handling

## Panda runtime (the runtime)
Panda runtime is a set of functions used to execute managed code. The runtime consists of several modules.
The document describes the interpreter and the compiler modules.

The interpreter, as a part of runtime, executes bytecodes of managed functions and manages hotness
counter (see [Structure of `panda::Method`](#structure-of-pandamethod)) of managed functions.

The compiler translates bytecodes of managed functions to native code. The compiler provides
`panda::CompilerInterface::CompileMethodSync` for compilation. When a function is compiled, the compiler changes
its entrypoint to the native code generated. When the function is called next time, the native code will be executed.

## Calling convention
Panda runtime and managed code must call functions according to the target calling convention.
Compiled code of a managed function must accept one extra argument: the pointer to `panda::Method` which describes this function.
This argument must be the first argument.

Example:
Consider a function int max(int a, int b).
When the compiler generates native code for this function for ARM target it must consider that
the function accepts 3 arguments:
- a pointer to `panda::Method` in the register R0.
- `a` in the register R1
- `b` in the register R2

The function must return the result in the register R0.

## Structure of `panda::ManagedThread`
`panda::ManagedThread` has the following fields that compiled code may use:

| Field                | Type                  | Description |
| ---                  | ----                  | ----------- |
| sp_flag_             | bool*                 | Safepoint flag. See *Safepoints* in memory_management.md. |
| pending_exception_   | panda::ObjectHeader*  | A pointer to a thrown exception or 0 if there is no exception thrown. |
| runtime_entrypoints_ | void*[]               | A table of runtime entrypoints (See [Runtime entrypoints](#runtime_entrypoints)). |
| stack_frame_kind_    | StackFrameKind        | A kind of the current stack frame (compiled code or interpreter stack frame). |

## Access to `panda::ManagedThread` from compiled code
There is an allocated register for each target architecture to store a pointer to `panda::ManagedThread`. This register is called `thread register` and
must contain a valid pointer to `panda::ManagedThread` on entry to each compiled function.

## Runtime entrypoints
Runtime serves compiled code via runtime entrypoints. A runtime entrypoint is a function which conforms to the target calling convention.
A table of the entrypoints is located in `panda::ManagedThread::runtime_entrypoints_` which could be accessed via `thread register`.

## Structure of `panda::Method`
`panda::Method` describes a managed function in the runtime.
This document describes the following fields of `panda::Method`:

| Field | Description |
| ----- | ----------- |
| hotness_counter_ | A hotness counter of the managed function. |
| compiled_entry_point_ | Function entrypoint. |

### Hotness counter
The field `hotness_counter_` reflects hotness of a managed function. The interpreter increments it each time the function is called,
backward branch is taken and call instruction is handled.
When the hotness counter gets saturated (reaches the threshold), the interpreter triggers compilation of the function.
Panda runtime provides a command line option to tune the hotness counter threshold: `--compiler-hotness-threshold`.

### Entrypoint
Entrypoint is a pointer to native code which can execute the function. This code must conform to the target calling convention and must accept
one extra argument: a pointer to `panda::Method` ( See [Calling convention](#calling_convention)).
The managed function may have compiled code or be executed by the interpreter.
If the function has compiled code the `compiled_entry_point_` must point to compiled code.
If the function is executed by the interpreter, the `compiled_entry_point_` must point to a runtime function `CompiledCodeToInterpreterBridge` which calls the interpreter.

## Stack frame
A stack frame contains data necessary to execute the function the frame belongs to.
The runtime can create several kinds of stack frames. But all the frames of managed code must have the structure described in [Compiled code stack frame](#compiled-code-stack-frame).

### Interpreter stack frame
Interpreter stack frame is described by `panda::Frame` class. The class has fields to store virtual registers and a pointer to the previous stack frame.
All the consecutive interpreter stack frames are organized into a linked list. The field `panda::Frame::prev_` contains a pointer to the previous interpreter (or compiled bridge) frame.

### Compiled code stack frame
Each compiled function is responsible for reserving stack frame for its purpose and then release it when the function doesn't need it.
Generally compiled function builds the stack frame in prolog and releases it in epilog. If a compiled function doesn't require
the stack frame it can omit its creation.
When compiled code is being executed the `stack pointer` register must point to a valid stack frame (new stack frame created for the caller) and the
`frame pointer` register must point to correct place in the frame before the following operations:
* Managed objects access
* Safepoint flag access
* Call of managed functions or runtime entrypoints

Release of the stack frame could be done by restoring values of `stack pointer` and `frame pointer` registers to the value they have at the moment of function entry.

Compiled code stack frames of caller and callee must be continuous in the stack i.e. the callee's stack frame must immediately follow the caller's stack frame.

A compiled code stack frame must have the following structure:

```
(Stack grows in increasing order: higher slot has lower address)
-----+----------------------+ <- Stack pointer
     | Callee parameters    |
     +----------------------+
     | Spills               |
     +----------------------+
     | Caller saved fp regs |
  D  +----------------------+
  A  | Caller saved regs    |
  T  +----------------------+
  A  | Callee saved fp regs |
     +----------------------+
     | Callee saved regs    |
     +----------------------+
     | Locals               |
-----+----------------------+
  H  | Properties           |
  E  +----------------------+
  A  | panda::Method*       |
  D  +----------------------+ <- Frame pointer
  E  | Frame pointer        |
  R  +----------------------+
     | Return address       |
-----+----------------------+
```
Stack frame elements:
- data - arbitrary data necessary for function execution. It is optional.
- properties - define properties of the frame, i.e. whether it is OSR frame or not.
- `panda::Method*` - pointer to `panda::Method`, which describes the called function.
- frame pointer - pointer to the previous frame. The value of `frame pointer` is registered at the moment of function entry.
- return address - address to which control will be transferred after the function gets returned.

There are two special registers: `stack pointer` and `frame pointer`.
`stack pointer` register contains a pointer to the last stack element.
`frame pointer` register contains a pointer to the place in the stack where the return address is stored.

Panda has a special class for getting cframe layout: `class CFrameLayout`. Everything related to the cframe layout
should be processed via this class.

## Calling a function from compiled code
To call a managed function, the compiled code must resolve it (i.e. retrieve a pointer to callee's `panda::Method`),
prepare arguments in the registers and the stack (if necessary) and jump to callee's entrypoint.
Resolving of a function could be done by calling the corresponding runtime entrypoint.

Example:
Calling `int max(int a, int b)` function from compiled code on ARM architecture with arguments `2` and `3`
could be described by the following pseudocode:
```
// tr - thread register
// r0 contains a pointer to the current `panda::Method`
// Step 1: Resolve `int max(int, int)`
mov r1, MAX_INT_INT_ID // MAX_INT_INT_ID - identifier of int max(int, int) function
ldr lr, [tr, #RESOLVE_RUNTIME_ENTRYPOINT_OFFSET]
blx lr // call resolve(currentMethod, MAX_INT_INT_ID)
// r0 contains a pointer to `panda::Method` which describes `int max(int, int)` function.
// Step 2: Prepare arguments and entrypoint to call `int max(int, int)`
mov r1, #2
mov r2, #3
lr = ldr [r0, #entrypoint_offset]
// Step 3: Call the function
blx lr // call max('max_method', 2, 3)
// r0 contains the function result
```

## Calling a function from compiled code: Bridge function
The compiler has an entrypoints table. Each entrypoint contains a link to the bridge function.
The bridge function is auto-generated for each runtime function to be called using the macro assembly.
The bridge function sets up the Boundary Frame and performs the call to the actual runtime function.

To do a runtime call from compiled code the compiler generates:
* Puts callee saved (if need) and parameter holding (if any) register values to the stack.
* (callee saved registers goes to bridge function stack frame, and caller saved registers goes to the current stack frame).
* Setups parameter holding register values.
* Loads bridge function address and branch instruction.
* Restores register values.

The bridge function does the following:
* Setups the bridge function stack frame.
* Pushes the caller saved registers (except of registers holding function parameters) to the caller's stack frame.
* Adjusts the Stack Pointer, and pass execution to the runtime function.
* Restores the Stack Pointer and caller saved registers.

Bridge function stack frame:
```
--------+------------------------------------------+
        | Return address                           |
        +------------------------------------------+
 HEADER | Frame pointer                            |
        +------------------------------------------+
        | COMPILED_CODE_TO_INTERPRETER_BRIDGE flag |
        +------------------------------------------+
        | - unused -                               |
--------+------------------------------------------+
        |                                          |
        | Callee saved regs                        |
        |                                          |
 DATA   +------------------------------------------+
        |                                          |
        | Callee saved fp regs                     |
        |                                          |
--------+------------------------------------------+
        +  16-byte alignment pad to the next frame +
```

## Transition from the interpreter to compiled code
When the interpreter handles a call instruction, it first resolves the callee method.
The procedure varies depending on the callee's entrypoint:
If the entrypoint points to `CompiledCodeToInterpreterBridge`, the interpreter executes the callee. In this case the interpreter calls itself directly.
In other cases, the interpreter calls the function `InterpreterToCompiledCodeBridge` to pass the resolved callee function,
the call instruction, the interpreter's frame and the pointer to `panda::ManagedThread`.

`InterpreterToCompiledCodeBridge` function does the following:
* Builds a boundary stack frame.
* Sets the pointer to `panda::ManagedThread` to the thread register.
* Changes stack frame kind in `panda::ManagedThread::stack_frame_kind_` to compiled code stack frame.
* Prepares the arguments according to the target calling convention. The function uses the bytecode instruction (which must be a variant of `call` instruction)
    and interpreter's frame to retrieve the function's arguments.
* Jumps to the callee's entrypoint.
* Saves the returned result to the interpreter stack frame.
* Changes stack frame kind in `panda::ManagedThread::stack_frame_kind_` back to interpreter stack frame.
* Drops the boundary stack frame.

`InterpreterToCompiledCodeBridge`'s boundary stack frame is necessary to link the interpreter's frame with the compiled code's frame.
Its structure is depicted below:
```
---- +----------------+ <- stack pointer
b s  | INTERPRETER_   |
o t  | TO_COMPILED_   |
u a  | CODE_BRIDGE    |
n c  +----------------+ <- frame pointer
d k  | pointer to the |
a    | interpreter    |
r f  | frame          |
y r  |                |
  a  +----------------+
  m  | return address |
  e  |                |
---- +----------------+
```

The structure of boundary frame is the same as a stack frame of compiled code.
Instead of pointer to `panda::Method` the frame contains constant `INTERPRETER_TO_COMPILED_CODE_BRIDGE`.
Frame pointer points to the previous interpreter frame.

## Transition from compiled code to the interpreter
If a function needs be executed by the interpreter, it must have `CompiledCodeToInterpreterBridge` as an entrypoint.
`CompiledCodeToInterpreterBridge` does the following:
* Changes stack frame kind in `panda::ManagedThread::stack_frame_kind_` to interpreter stack frame.
* Creates a boundary stack frame which contains room for interpreter frame.
* Fills in the interpreter frame by the arguments passed to `CompiledCodeToInterpreterBridge` in the registers or via the stack.
* Calls the interpreter.
* Stores the result in registers or in the stack according to the target calling convention.
* Drops the boundary stack frame.
* Changes stack frame kind in `panda::ManagedThread::stack_frame_kind_` back to compiled code stack frame.

`CompiledCodeToInterpreterBridge`'s boundary stack frame is necessary to link the compiled code's frame with the interpreter's frame.
Its structure is depicted below:
```
---- +----------------+ <-+ stack pointer
  s  | interpreter's  | -+ `panda::Frame::prev_`
b t  | frame          |  |
o a  +----------------+ <+ frame pointer
u c  | frame pointer  |
n k  +----------------+
d    | COMPILED_CODE_ |
a f  | TO_            |
r r  | INTERPRETER_   |
y a  | BRIDGE         |
  m  +----------------+
  e  | return address |
---- +----------------+
     |     ...        |
```

The structure of boundary frame is the same as a stack frame of compiled code.
Instead of a pointer to `panda::Method` the frame contains constant `COMPILED_CODE_TO_INTERPRETER_BRIDGE`.
Frame pointer points to the previous frame in compiled code stack frame.
The field `panda::Frame::prev_` must point to the boundary frame pointer.

## Stack traversing
Stack traversing is performed by the runtime. When the runtime examines a managed thread's stack, the thread cannot execute any managed code.
Stack unwinding always starts from the top frame, the kind of which can be determined from `panda::ManagedThread::stak_frame_kind_` field. The pointer
to the top frame can be determined depending on the kind of the top stack frame:
* If the top stack frame is an interpreter stack frame, the address of the interpreter's frame can be retrieved from `panda::ManagedThread::GetCurrentFrame()`.
* If the top stack frame is a compiled code stack frame, `frame pointer` register contains the address of the top stack frame.

Having a pointer to the top stack frame, its kind and structure the runtime can move to the next frame.
Moving to the next frame is done according to the table below:

| Kind of the current stack frame | How to get a pointer to the next stack frame | Kind of the previous stack frame |
| ------------------------------- | -------------------------------------------- | -------------------------------- |
| Interpreter stack frame         | Read `panda::Frame::prev_` field             | Interpreter stack frame or COMPILED_CODE_TO_INTERPRETER boundary frame |
| INTERPRETER_TO_COMPILED_CODE_BRIDGE boundary stack frame | Read `pointer to the interpreter frame` from the stack | Interpreter stack frame |
| COMPILED_CODE_TO_INTERPRETER_BRIDGE boundary stack frame | Read `frame pointer` from the stack | Compiled code stack frame |
| Compiled code stack frame | Read `frame pointer` | Compiled code stack frame or INTERPRETER_TO_COMPILED_CODE_BRIDGE boundary frame|

Thus the runtime can traverse all the managed stack frames moving from one frame to the previous frame and changing frame type
crossing the boundary frames.

Unwinding of stack frames has specifics.
* Compiled code could be combined from several managed functions (inlined functions). If the runtime needs to get information about inlined functions
during handling a compiled code stack frame, it uses meta information generated by the compiler.
* Compiled code may save any callee-saved registers on the stack. Before moving to the next stack frame the runtime must restore values of these registers.
To do that the runtime uses information about callee-saved registers stored on the stack. This information is generated by the compiler.
* Values of virtual registers could be changed during stack unwinding. For example, when GC moves an object, it must update all the references to the object.
The runtime should provide an internal API for changing values of virtual registers.

Example:
Consider the following call sequence:
```
         calls        calls
    foo --------> bar ------> baz
(interpreted)  (compiled)  (interpreted)
```
Functions `foo` and `baz` are executed by the interpreter and the function `bar` has compiled code.
In this situation the stack might look as follow:
```
---- +----------------+ <- stack pointer
E    | native frame   |
x u  | of             |
e t  | interpreter    |
c e  |                |
---- +----------------+ <--- `panda::ManagedThread::GetCurrentFrame()`
b    | baz's          | -+
o s  | interperer     |  |
u t  | stack frame    |  |
n a  +----------------+<-+
d c  | frame pointer  | -+
a k  +----------------+  |
r    | COMPILED_CODE_ |  |
y f  | TO_            |  |
  r  | INTERPRETER_   |  |
  a  | BRIDGE         |  |
  m  +----------------+  |
  e  | return address |  |
---- +----------------+  |
     |      data      |  |
     +----------------+  |
 b   | panda::Method* |  |
 a   +----------------+ <+
 r   | frame pointer  | -+
     +----------------+  |
     | return address |  |
---- +----------------+  |
b s  | INTERPRETER_   |  |
o t  | TO_COMPILED_   |  |
u a  | CODE_BRIDGE    |  |
n c  +----------------+ <+
d k  | pointer to the | -+
a    | interpreter    |  |
r f  | frame          |  |
y r  |                |  |
  a  +----------------+  |
  m  | return address |  |
  e  |                |  |
---- +----------------+  |
E    | native frame   |  |
x u  | of             |  |
e t  | interpreter    |  |
c e  |                |  |
---- +----------------+  |
     |      ...       |  |
     +----------------+ <+
     | foo's          |
     | interpreter    |
     | frame          |
     +----------------+
     |       ...      |
```

The runtime determines kind of the top stack frame by reading `panda::ManagedThread::stack_frame_kind_` (the top stack frame kind must be interpreter stack frame).
`panda::ManagedThread::GetCurrentFrame()` method must return the pointer to `baz`'s interpreter stack frame.
To go to the previous frame the runtime reads the field `panda::Frame::prev_` which must point to `COMPILED_CODE_TO_INTERPRETER_BRIDGE` boundary stack frame.
It means that to get `bar`'s stack frame the runtime must read `frame pointer` and the kind of the next frame will be compiled code's frame.
At this step the runtime has a pointer to `bar`'s compiled code stack frame. To go to the next frame runtime reads `frame pointer` again and gets
`INTERPRETER_TO_COMPILED_CODE_BRIDGE` boundary stack frame. To reach `foo`'s interpreter stack frame the runtime reads `pointer to the interpreter's frame` field.

## Deoptimization
There may be a situation when compiled code cannot continue execution for some reason.
For such cases compiled code must call `void Deoptimize()` runtime entrypoint to continue execution of the method in the interpreter from the point where compiled code gets stopped.
The function reconstructs the interpreter stack frame and calls the interpreter.
When compiled code is combined from several managed functions (inlined functions) `Deoptimize` reconstructs interpreter stack frame and calls the interpreter for each inlined function too.

Details in [deoptimization documentation](deoptimization.md)
## Throwing an exception
Throwing an exception from compiled code is performed by calling the runtime entrypoint `void ThrowException(panda::ObjectHeader* exception)`.
The function `ThrowException` does the following:
* Saves all the callee-saved registers to the stack
* Stores the pointer to the exception object to `panda::ManagedThread::pending_exception_`
* Unwinds compiled code stack frames to find the corresponding exception handler by going from one stack frame to the previous and making checks.

If the corresponding catch handler is found in the current stack frame, the runtime jumps to the handler.

If an INTERPRETER_TO_COMPILED_CODE_BRIDGE boundary stack frame is reached, the runtime returns to the interpreter, letting it handle the exception.
Returning to the interpreter is performed as follow:
1. Determine the return address to the boundary frame. The return address is stored in the following compiled code stack frame.
2. Set the pointer to the boundary frame into stack pointer, and assign the return address determined at the previous step to program counter.

If there is no catch handler in the current frame then the runtime restores values of callee-saved registers and moves to the previous stack frame.

Details of stack traversing are described in [Stack traversing](#stack_traversing)

Finding a catch handler in a compiled code stack frame is performed according meta information generated by the compiler.

The interpreter must ignore the returned value if `panda::ManagedThread::pending_exception_` is not 0.
