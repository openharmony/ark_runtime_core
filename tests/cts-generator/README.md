# Test generator and test runner tools


##

Tests are being generated during building.

* `CTS_TEST_SELECT_OPTION` - options, passed to test-runner.rb.
Useful for defining `--exclude-tag` and `--include-tag` options.
* `PANDA_CTS_JOBS_NUMBER` - amount of parallel jobs for test execution. Default is 8.

## Test generator

Options:

```
Usage: generate-cts.rb [options]
    -t, --template FILE              Path to template yaml file to generate tests (required)
    -s, --schema FILE                Path to json schema for template yaml (required)
    -k, --skip                       Skip yaml schema validation
    -o, --output DIR                 Path to directory where tests will be generated (required)
    -h, --help                       Prints this help
```

Usage example:

```
 ${PANDA_SRC_ROOT}/tests/cts-generator/generate-cts.rb \
   -t ${PANDA_SRC_ROOT}/tests/cts-generator/cts-template/template.yaml \
   -s ${PANDA_SRC_ROOT}/tests/cts-generator/cts-template/yaml-schema.json \
   -o cts-generated
```

This command will generate CTS tests using provided template file. Template is validated using schema file.

## Test runner

Options:

```
Usage: test-runner.rb [options]
    -p, --panda-build DIR            Path to panda build directory (required)
    -t, --test-dir DIR               Path to test directory to search tests recursively, or path to single test (required)
    -v, --verbose LEVEL              Set verbose level 1..5
        --timeout SECONDS            Set process timeout
        --global-timeout SECONDS     Set testing timeout, default is 0 (ulimited)
    -a, --run-all                    Run all tests, ignore "runner-option: ignore" tag in test definition
        --run-ignored                Run ignored tests, which have "runner-option: ignore" tag in test definition
    -e, --exclude-tag TAG            Exclude tags for tests
    -o, --panda-options OPTION       Panda options
    -i, --include-tag TAG            Include tags for tests
    -b, --bug_id BUGID               Include tests with specified bug ids
    -j, --jobs N                     Amount of concurrent jobs for test execution (default 8)
    --prlimit='OPTS'                 Run panda via prlimit with options
    --verifier-debug-config PATH     Path to verifier debug config file. By default, internal embedded verifier debug config is used.

    -H, --host-toolspath PATH        Directory with host-tools

    -h, --help                       Prints this help
```

Usage example:

```
${PANDA_SRC_ROOT}/tests/cts-generator/test-runner.rb
   -t cts-generated \
   -p ${PANDA_BUILD_ROOT} \
   -e release -e debug \
   -i clang_release_sanitizer,wrong-tag
```

This command will start all tests in `cts-generated` directory. Tests which have runner options `ignore` will be ignored.
Tests that have `release` and `clang_release_sanitizer` will be excluded.

To run all tests, add `-a` options.

To run only tests with `ignore` runner option, add `--run-ignored` options.

## Tips

### How to specify options for cmake cts-generator target?

`CTS_TEST_SELECT_OPTION` variable can be used.

```
cmake -DCTS_TEST_SELECT_OPTION="-b 1316 -a" ../panda
```

Run all tests marked with bugid 1316.

### How to generate all test not using cmake/make?

```
cd ${ROOT_PATH}/tests/cts-generator
./generate-cts.rb \
   -t ./cts-template/template.yaml \
   -s ./cts-template/yaml-schema.json \
   -o cts-generated
```

### How to run all tests?

All test can be executed using `make cts-generated` command, test with `ignore` runner options will be ignored by test runner.
If you want to run all tests, you can do the following:

```
cmake ${ROOT_PATH} -DCTS_TEST_SELECT_OPTION="--run-all"
make cts-generator
```

Also you can start test-runner.rb directly:

```
cd ${BUILD_DIR}/tests/cts-generator
./test-runner.rb
  -p ${BUILD_DIR} \
  -t ./cts-generated/ \
  -v 2 -j 8 \
  -i release \
  -e clang_release_sanitizer
```

Tests with `release` tag will be included to test execution, with `clang_release_sanitizer` will be excluded.

### How to run test with specified bug id runner-option?

Example:
```
test-runner.rb \
  -t ./cts-generated/ \
  -p ${BUILD_DIR} \
  -b 977 \
  -v 2 \
  -a
```

Please note that `-a` options (`--run-all`) is defined, otherwise tests will be excluded, if they have `ignore` runner option.

### How to run panda via prlimit?

Example:
```
test-runner.rb \
  -t ./cts-generated/ \
  -p ${BUILD_DIR} \
  --prlimit='--stack=8000000:8000000 --nproc=512'
```


### What should I do with failed/passed tests after bug-fixing?

1. Run all tests using regular build or using `tests` or `cts-geerator` targets.
2. Run tests related to bug.
    ```
    cmake -DCTS_TEST_SELECT_OPTION="-b 1316 -a" ../panda
    make cts-generator
    ```
3. If all test passed, congrats! Now you can enable tests for regular
execution. Update all tests that have `bugid: [number]` and `ignore: true` - remove bugid relation and `ignore` tag
to allow test runner to execute test.

4. If some tests failed, you can update them, if there are problems with tests. If tests are correct, continue with bugfixing and repeat all steps.
