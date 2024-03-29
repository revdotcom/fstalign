# Tools
A collection of miscellaneous tools to support the fstalign project.

## generate_wer_test_data.pl
A simple perl script to generate synthetic transcripts with a targetted word error rate. Outputs will be written as plain text to `ref.out` and `hyp.out`.
The script contains settings to generate specific INS/DEL/SUB error frequencies, in addition to target reference transcript length. This is useful for testing the WER behavior of `fstalign` and also the performance of the algorithm when hit with edge case scenarios (e.g. 80% deletion rate).

Example usage:
`perl generate_wer_test_data.pl --ins_fract 0.2 --del_fract 0.3 --sub_fract 0.2 --ref_length 1000 --oref ref.out --ohyp hyp.out`

Example output:
```
writing to [ref.out]
writing to [hyp.out]
181 INS
316 DEL
205 SUB
expected WER 0.702
```

NOTE: this script provides an approximate WER, the algorithm could use some fine tuning to be exact.

## gather_runtime_metrics.sh
A simple bash script that is meant for benchmarking the resource (RAM and runtime) consumption of fstalign across different transcript settings (length, WER). It uses the `generate_wer_test_data.pl` to generate fake transcripts with a suite of hard-coded settings and runs them through fstalign, recording the resource usage to a CSV.

Example usage:
`bash gather_runtime_metrics.sh output_for_this_release.csv`
