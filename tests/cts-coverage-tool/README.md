# Run Spec Coverage Tool

The tool reads isa.yaml spec file and the provided set of test files, computes the coverage and outputs a bunch of coverage reports.

```
spectrac -r path_to_summary_yaml_file \
    -d tests_directory \
    -g glob_to_select_tests_files_in_tests_directory \
   [-g more_glob_to_select_tests_files_in_the_same_tests_directory [-g ..] ] \
    -s path_to_isa_yaml_file \
    -n path_to_nontestable_yaml_file \
    -u path_to_output_notcovered_yaml_file \
    -U path_to_output_notcovered_markdown_file \
    -o path_to_output_yaml_file_with_irrelevant_tests \
    -O path_to_output_markdown_file_with_irrelevant_tests \
    -f path_to_output_yaml_file_with_coverage_data \
    -F path_to_output_markdown_file_with_coverage_data
```

Options:
```
--testdir (-d) - directory with tests, required
--testglob (-g) - glob for selecting test files in tests directory ("**/*.pa" for example), required
--spec (-s) - ISA spec file, required
--non-testable (-n) - yaml file with non-testable assertions
--full (-f) - output the full spec in yaml format with additional fields showing the coverage data (mark assertions as covered/not-covered/non-testable, provide coverage metric for each group)
--full_md (-F) - same as --full, but in markdown format
--uncovered (-u) - output yaml document listing the spec areas not covered by tests
--uncovered_md (-U) - same as --uncovered, but in markdown format
--orphaned (-o) - output list of test files that found not relevant to the current spec (the assertions in test file aren't found in the spec)
--orphaned_md (-O) - same as --orphaned, but in markdown format
--report (-r) - output the test coverage summary report in yaml
```

Example:
```
spectrac -r ~/panda/summary.yaml \
    -d ~/panda/tests \
    -g "cts-assembly/*.pa" \
    -g "cts-generator/**/*.pa" \
    -s ~/panda/isa/isa.yaml \
    -n ~/panda/isa/nontestable.yaml \
    -u notcovered.yaml \
    -o lost.txt \
    -f full_spec.yaml
```
