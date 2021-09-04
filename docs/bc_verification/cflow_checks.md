## Checks for the method layout

```
=========
   ...
  code
   ...
---------
   ...
exception
handler
   ...
---------
   ...
  code
   ...
---------
   ...
exception
handler
   ...
 -------
 inner
 exc.
 handler
 -------
   ...
---------
   ...
  code
   ...
=========

```

The layout of exception handlers is quite flexible, even nesting of a handler in another handler is allowed.

## Checks for cflow transitions

### Execution beyond method body

```
=========
   ...
  code
   ...
---------
   ...
exception
handler
   ...
---------
   ...
  code
   ...
  ldai 0 ---\
=========   |
       <----/
```

```
=========
   ...
  code
   ...
---------
   ...
exception
handler
   ...
   jmp -----\
   ...      |
=========   |
       <----/
```

Mis-jump or improper termination of cflow at the end of the method body is prohibited.

```
=========
   ...
  code
   ...
---------
   ...
exception
handler
   ...
---------
   ...
lbl:  <-----\
   ...      |
  code      |
   ...      |
  jeqz lbl -+
=========   |
       <----/
```

Conditional jumps are in grey zone. If they are proven as always jumping into code, then they are considered normal. Currently,
due to limitations of the verifier, conditional jumps at the end of the method are prohibited.

### Code to exception handler

direct jumps:

```
=========
   ...
  code
   ...
   jmp catch1--\
   ...         |
---------      |
catch1: <------/
   ...
exception
handler
   ...
---------
   ...
```

fallthrough:

```
=========
   ...
  code
   ...
   ldai 3 --\
---------   |
catch1: <---/
   ...
exception
handler
   ...
---------
   ...
```

By default, only `throw` transition is allowed. Neither `jmp` nor fallthrough at the beginning of the exception handler is allowed.
This behavior can be altered by the option `C-TO-H`.

### Checks for jumps from code to the exception handler

```
=========
   ...
  code
   ...
   jmp lbl1  --\
   ...         |
---------      |
catch:         |
   ...         |
lbl1:     <----/
   ldai 3
   ...
exception
handler
   ...
---------
   ...
```

Jumps into body of exception handler from code are prohibited by default.

### Checks for cflow transitions between exception handlers

direct jumps:

```
=========
   ...
  code
   ...
---------
catch1:
   ...
exception
handler
   ...
   jmp catch2--\
   ...         |
---------      |
catch2: <------/
   ...
exception
handler
   ...
---------
   ...
```

fallthrough:

```
=========
   ...
  code
   ...
---------
catch1:
   ...
exception
handler
   ...
   ldai 3 --\
---------   |
catch2: <---/
   ...
exception
handler
   ...
---------
   ...
```

By default, such cflow transitions are prohibited.

### Checks for cflow transitions from an exception handler into the inner handler

direct jumps:

```
=========
   ...
  code
   ...
---------
catch1:
   ...
exception
handler
   ...
   jmp lbl  ---\
   ...         |
---------      |
catch2:        |
   ...         |
lbl:    <------/
   ldai 3
   ...
exception
handler
   ...
---------
   ...
```

fallthrough from inner handler:

```
=========
   ...
  code
   ...
---------
catch1:
   ...
outer
exception
handler
   ...
 -------
catch2:
   ...
lbl:
   ldai 3
   ...
 inner
 exc.
 handler
   ...
  ldai 0  --\
 -------    |
   ...   <--/
outer
exc.
handler
   ...
---------
   ...
```

By default such cflow transitions are prohibited.

### Checks for jumps from the exception handler into code

```
=========
   ...
  code
   ...
lbl:   <-------\
   ...         |
---------      |
   ...         |
exception      |
handler        |
   ...         |
   jmp lbl  ---/
   ...
---------
   ...
```

By default, such jumps are prohibited.
