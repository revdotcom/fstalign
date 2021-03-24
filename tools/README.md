# Tools
A collection of miscellaneous tools to support the fstalign project.

## generate_wer_test_data.pl
A simple perl script to generate synthetic transcripts with a targetted word error rate. Outputs will be written as plain text to `ref.out` and `hyp.out`.
The script contains settings to generate specific INS/DEL/SUB error frequencies, in addition to target reference transcript length. This is useful for testing the WER behavior of `fstalign` and also the performance of the algorithm when hit with edge case scenarios (e.g. 80% deletion rate).

Example usage:
`perl generate_wer_test_data.pl`

Example output:
```
writing to [ref.out]
writing to [hyp.out]
99 INS
117 DEL
88 SUB
expected WER 0.304
```