# Coding Style

Our Coding Style based on [google coding style](https://google.github.io/styleguide/cppguide.html) (you can get google config like this: `clang-format -dump-config -style=Google`).
But we have some modifications:

1. Indent: spaces 4. Line length: 120.
2. Delete spaces before public/private/protected.
3. All constants in UPPERCASE.
4. Enums in UPPERCASE:
   ```cpp
   enum ShootingHand { LEFT, RIGHT };
   ```
5. Unix/Linux line ending for all files.
6. Same parameter names in Method definitions and declarations.
7. No `k` prefix in constant names.
8. No one-line if-clauses:
   ```cpp
   if (x == kFoo) return new Foo();
   ```
9. Do not use special naming for getters/setters (google allows this:
   ```cpp
   int count() and void set_count(int count))
   ```
10. Always explicitly mark fall through in switch … case. Google uses its own macro, and we can agree on /* fallthrough */ :
    ```cpp
    switch (x) {
      case 41:  // No annotation needed here.
      case 43:
        if (dont_be_picky) {
          // Use this instead of or along with annotations in comments.
          /* fallthrough */
        } else {
          CloseButNoCigar();
          break;
        }
      case 42:
        DoSomethingSpecial();
        /* fallthrough */
      default:
        DoSomethingGeneric();
        break;
    }
    ```
11. When a return statement is unreachable, but the language syntax requires it, mark it with something like return nullptr; /* unreachable */, or define UNREACHABLE as assert(0 && "Unreachable") and insert it before the return statement.
12. Use standard flowerbox comments at the top of headers and translation units (agree on the format). Temporary you can use this:
    ```cpp
    /*
     * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
     * Licensed under the Apache License, Version 2.0 (the "License");
     * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
     *
     * http://www.apache.org/licenses/LICENSE-2.0
     *
     * Unless required by applicable law or agreed to in writing, software
     * distributed under the License is distributed on an "AS IS" BASIS,
     * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     * See the License for the specific language governing permissions and
     * limitations under the License.
     */
    ```
13. Always put `case` on the next level of `switch`:
    ```cpp
    switch (ch) {
        case ‘A’:
            ...
    }
    ```
14. Always put { } even if the body is one line:
    ```cpp
    if (foo) {
        return 5;
    }
    ```
15. Use `maybe_unused` attribute for unused vars/arguments:
    ```cpp
    int foo3([[maybe_unused]] int bar) {
        // ...
    }
    ```

We are using clang-format and clang-tidy to check code style.
