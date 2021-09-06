## Type system implementation

Type system is simple: there are only parametric type families, named as `Sort`s, and their instances, named as `Type`s.

For simplicity, literals without parens are Sorts and with parens are Types.

### Special types

  - Bot() - subtype of all types, subtyping relation is made implicitly upon type creation.
  - Top() - supertype of all types, subtyping relation is created implicitly.

## Sorts

Sorts are denoted by indices (just numbers) internally, and there is a separate class to hold their names.
For type_system description technical details can be ommitted and Sorts will be understood in this
text just as literals.

## Types

Internally they are just indices.

A sort, accompanied with a list of parameters, becomes a type.

Each parameter consists of: a sign of variance for calculating subtyping relation and a type.

Variances are:
- `~` invariant, which means that corresponding parameter in subtyping relation must be in subtype and supertype relation
      simultaneously.
- `+` covariant, which means that the suptyping relation of parameters is in the direction of subtyping relation of type.
- `-` contravariant, direction of suptyping of parameter is in opposite to such of type.

## Examples

Sorts:

- `i8`
- `Math`
- `function`

Types:

- `i8()`
- `i16()`
- `i32()`

- `function(-i32(), -i16(), +i8())`
- `function(-function(-i8(), +i16()), +i16())`

Subtyping relations (`subtype <: supertype`):

- `i8() <: i16() <: i32()`
- `function(-i32(), -i16(), +i8()) <: function(-i16(), -i8(), +i32())`
- `function(-function(-i8(), +i16()), +i16()) <: function(function(-16(), +8()), +i32())`

## Closure

After defining base types and initial subtyping relation, a closure of subtyping relation is computed.
This helps to speed up subtyping checking during verification.

Loops in subtyping relation are treated as classes of equivalence, for instance:
`a() <: b() <: c() <: a()` leads to indistinguishability of a(), b() and c() in type system.
