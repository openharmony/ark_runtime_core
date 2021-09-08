## Basic build

Panda libraries can be built using CMake:
```
$ mkdir build
$ cd build
$ cmake ${panda_dir}
$ make
```

Here, ${panda_dir} refers to  path to panda dirrectory.
After this step, create libraries and some debug-targets if you have installed additional libraries, like google-test, clang-format, clang-tidy, etc.

## Build directory structure

In the current build, create subdirectories for each project. For example, for the vixl library, create the `third_party/vixl` directory. Add the following command in root CMakeLists.txt:
```
add_subdirectory(third_party/vixl)
```
You may use built libraries in your component, e.g. target_link_libraries(tests compiler base vixl). However, for getting variables, use INTERFACE includes, e.g. target_include_directories(INTERFACE .) instead.

## Run tests

The tests-binary will also be generated during the build. To run tests, run the following command:
```
$ make test
Running tests...
Test project /home/igorban/src/panda_build
    Start 1: compiler_unit_tests
1/1 Test #1: compiler_unit_tests ...................   Passed    2.74 sec
..
```

## Calculate coverage

To calculate the test code coverage - just execute it:
```
$ make coverage
```
Location of the code coverage report: BUILD_DIR/compiler/coverage_report

## Check style

To check the style, perform the previous steps and build style-checker targets. Note that you must install clang-format and clang-tidy with libraries. For details, see scripts/bootstrap*.sh.
```
$ make clang_format
    Built target clang_format_opt_tests_graph_creation_test.cpp
    Built target clang_format_opt_opt.h
    ...

$ make clang_tidy
    Scanning dependencies of target copy_json
    Move compile commands to root directory
    ...
    Built target copy_json
    Scanning dependencies of target clang_tidy_opt_codegen_codegen.cpp
    ...
```

You may force fix clang-format issues using the `make clang_force_format` command.
Run `make help | grep clang` to see all possible clang-[format|style] targets.
For issues in opt.cpp, you may check the style through clang-format `make clang_format_opt_opt.cpp`, or through clang-tidy `make clang_tidy_opt_opt.cpp`. For the force clang-format code style, use `make clang_force_format_opt_opt.cpp`.
For code-style check through one check system, use `make clang_tidy` or `make clang_format`.

Generated files:
*  `compile_commands.json` - json nija-commands file to correct execution clang-tidy.
*  Standard cmake-files: `CMakeCache.txt`, `Makefile`, `cmake_install.cmake`  and folder `CMakeFiles`.


Discussion about format : rus-os-team/virtual-machines-and-tools/vm-investigation-and-design#24
*  Clang-tidy style file - `.clang-tidy`
*  Clang-format style file - `.clang-format`
*  Script to show diff through clang-format execution - `build/run-clang-format.py`
