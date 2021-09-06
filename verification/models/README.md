## contexts_merge/

Using Alloy to prove that we may use simple set intersection of reg supertypes during context merge, instead of (more complex) set of least upper bounds.
And compatibility check operation is changed to:
```
bool check(Type typ, TypeSet tset) {
  for (const auto& t: tset) {
    if (t <= typ) {
      return true;
    }
  }
  return false;
}
```

### Files

#### contexts_merge/java_typing.als

This file models Java class hierarchy used during verification.

### contexts_merge/check_set_intersection_as_lub.als

This file contains two execution modes:

1. `example` - to show models of proper supertypes calculation during context merge and checking of types compatibility.

2. `verify` - to verify (search for counterexamples), where supertypes intersection calculation during context merge does not lead to correct checks of compatibility.
