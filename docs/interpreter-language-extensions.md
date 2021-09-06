# Interpreter Language Specific Extension

[Builtins](../isa/builtins.yaml) mechanism allows for optimization of  bytecode for a particular language by introducing language-specific builtins. Builtins should be added to the interpreter main loop on build time.

## Interpreter main loop

The interpreter main loop is implemented using the computed goto approach:

```cpp
template <...>
void ExecuteImpl(ManagedThread* thread, const uint8_t *pc, Frame* frame) {
    static std::array<const void*, NUM_OPS> dispatch_table{
        &&HANDLE_OP1,
        &&HANDLE_OP2,
        ...
    }

    ...

    InstructionHandlerState<...> state(thread, pc, frame);

    DISPATCH(...);

HANDLE_OP1: {
        InstructionHandler<...> handler(&state);
        handler.template HandleOp1<Format1>();
        DISPATCH(...);
    }

HANDLE_OP2: {
        InstructionHandler<...> handler(&state);
        handler.template HandleOp2<Format2>();
        DISPATCH(...);
    }

    ...
}
```

It uses one dispatch table for public ISA opcodes and builtins.

The interpreter uses [`InstructionHandlerState`](../runtime/interpreter/instruction_handler_state.h) objects to encapsulate internal state that contains current thread, pc, frame, etc. It's used to construct an [`InstructionHandler`](../runtime/interpreter/interpreter-inl.h) instance that implements the `Handle<Opcode>` method with opcode implementation. We create a new instance for each opcode.

`InstructionHandler` contains implementation of all public ISA opcodes and common builtins. It extends the [`InstructionHandlerBase`](../runtime/interpreter/instruction_handler_base.h) class with helper methods that doesn't contains any opcode implementation.

## Language specific instructions

To add a language specific builtin, we need to add a new class that will extend `InstructionHandlerBase`. For example:

```cpp
template <...>
class ECMAInstructionHandler : public InstructionHandlerBase<...> {
public:
    ALWAYS_INLINE inline ECMAInstructionHandler(InstructionHandlerState<enable_profiling>* state)
        : InstructionHandlerBase<...>(state) {}

    template <Format format>
    ALWAYS_INLINE void HandleEcmaOp1() {
        ...
        this->template MoveToNextInst<format, false>();
    }
}
```

Use the added class in the interpreter main loop:

```cpp
template <...>
void ExecuteImpl(ManagedThread* thread, const uint8_t *pc, Frame* frame) {
    static std::array<const void*, NUM_OPS> dispatch_table{
        &&HANDLE_OP1,
        ...
        &&HANDLE_ECMA_OP1,
        ...
    }

    ...

    InstructionHandlerState<...> state(thread, pc, frame);

    DISPATCH(...);

HANDLE_OP1: {
        InstructionHandler<...> handler(&state);
        handler.template HandleOp1<Format1>();
        DISPATCH(...);
    }

    ...

HANDLE_ECMA_OP1: {
        ECMAInstructionHandler<...> handler(&state);
        handler.template HandleEcmaOp1<Format2>();
        DISPATCH(...);
    }

    ...
}
```

`ECMAInstructionHandler` should be located in `runtime/ecmascript/interpreter` directory and included to [`interpreter-inl.h`](../runtime/interpreter/interpreter-inl.h).
