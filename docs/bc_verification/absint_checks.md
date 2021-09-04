## Checks performed on abstract interpretation stage

### Physical compatibility of arguments to instructions and actual parameters to methods

This type of checks eliminate runtime problems with undefined bits in integers, truncation issues, etc.

From security point of view, this checks guarantee expected ranges of values in code and absence of handling
undefined information.

### Access checks

This type of checks ensure private/protected/public access rights.

This type of checks prevent unintended/unexpected access from one method to another.
Or access to wrong fields of object.

### Checks of subtyping

This type of checks ensure compatibily of objects in arguments to instructions and actual parameters to methos.

The checks eliminate calls of methods with incorrect this, wrong access to arrays, etc.

### Checks of exception handlers

This type of checks ensure correctness of context on exception handler entry.

The checks can help to detect usage of inconsistent information in registers in exception handlers.

### Checks of exceptions that may be thrown in runtime

Some code may exhibit behavior of permanent throwing of exceptions, for example, NPE.

This is definitely not normal mode of control-flow in code, so verifier should be able to detect such situations in which an exception is always thrown.

### Check of return values from methods

This type of checks eliminate inconsistency between the method signature and the type of actual return value.
