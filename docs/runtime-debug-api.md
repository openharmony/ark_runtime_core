# Runtime Debug API

### Requirements

1. Runtime should support debugging on the platforms from low-end IoT devices to high-end mobile phones.

### Key Design Decisions

1. Runtime doesn't patch apps' bytecode on the fly. Instead, it notifies the listeners when the PC of bytecode is changed.

1. Runtime and debugger work in the same process. Debugger functionality is provided via shared library, the runtime loads when working in debugg mode. Debugger works in its own thread and is responsible for thread management rely on it. Runtime doesn't create/destroy threads.

### Rationale

1. As some low-end targets can store bytecode in ROM, runtime cannot patch apps' bytecode on the fly. So it uses slower approach with interpreter instrumentation.

1. To simplify communication between debugger and runtime (especially on microcontrollers) they are work in the same process. Debugger is loaded as shared library when it's necessary.

### Specification / Implementation

To start runtime in debug mode, thef `Runtime::StartDebugger()` method is used. This method loads the debug library and calls the `StartDebugger` function from it. It takes pointer to [`debug::Debugger`](../runtime/tooling/debugger.h) object that implements [debug interface](../runtime/include/tooling/debug_interface.h) - point of interaction with the runtime.
Also it takes a TCP port number and a reserved argument.

Runtime provides [`RuntimeNotificationManager`](../runtime/include/runtime_notification.h) class that allows to subscribe to different events:
* `LoadModule` - occurs when the Panda file is loaded by the runtime
* `StartProcess` - occurs when the managed process is started or failed to start
* `BytecodePcChanged` - occurs when the PC of bytecode is changed during interpretation (only if runtime works in debug mode)

[`debug::Debugger`](../runtime/tooling/debugger.h) subscribes to these events and notifies debugger via hooks.
