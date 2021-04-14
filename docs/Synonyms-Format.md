# Synonyms File Format
Synonyms allow for reference words to be equivalent to similar forms (determined by the user) for error counting. They are accepted for any input formats and passed into the tool via the `--syn <path_to_synonym_file>` flag.

The file structure is a simple text file where each line is a synonym and each synonym is separated by a pipe where the left hand side is the reference version of the term and the right hand side is the accepted hypothesis alternative.

```
format : LHS<pipe>RHS
where:
  LHS : space-delimited words to match in the original reference text
  RHS : semi-colon-delimited list of space-delimited words to consider as equivalent expressions to the LHS
```

Note that there is no built in symmetry, so synonyms must be doubly specified for symmetrical equivalence (example below illustrates this). Empty lines or lines starting with '#' are ignored.

Example:
```
i am     | i'm
i'm      | i am
okay     | ok
ok       | okay
```

A full example of a synonyms file is available in the repository under `sample_data/synonyms.rules.txt`.
