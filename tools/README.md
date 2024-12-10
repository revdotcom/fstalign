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

## sbs2fst.py
A python interface to simplify the conversion of a side-by-side file, generated from fstalign's `--output-sbs` flag, into [files that can be used to produce an FST using OpenFST](https://www.openfst.org/twiki/bin/view/FST/FstQuickTour).

Example usage:

`python sbs2fst.py sbs_file.txt fst_file_name`

The output will be two files: `fst_file_name.fst` which will describe the FST in the AT&T FSM format used by OpenFST, and `fst_file_name.txt` which contains the complete list of symbols in the FST.

The additional flags can be passed into the python script to add metadata that fstalign uses for tracking performance. These are useful to understand when fstalign picks tokens that are: only in the side-by-side's `ref_token` column (labeled by the `--left` flag), only in the side-by-side's `hyp_token` column (labeled by the `--right` flag), or in both columns because the `ref_token` and `hyp_token` agree (labeled by the `--gold` flag). 

Example usage:

`python sbs2fst.py --tag --left VERBATIM --right NONVERBATIM --gold AGREEMENT sbs_file.txt fst_file_name`

The output will produce an FST with tags indicating tokens that were only in the `ref_token` with `VERBATIM`, tokens that were only in the `hyp_token` with `NONVERBATIM`, and tokens that were in both columns with `AGREEMENT`. 

### Compiling the FST
Once you have used `sbs2fst.py` to produce the `.txt` and `.fst` files, you *must* then compile the FST before passing it into fstalign. An example command can be found below:

`fstcompile --isymbols=${SYMBOLS} --osymbols=${SYMBOLS} ${TXT_FST} ${COMPILED_FST}`

where `SYMBOLS` is the `.txt` file produced by `sbs2fst.py`, `TXT_FST` is the `.fst` file, and `COMPILED_FST` is a new `.fst` file that produces the binary FST usable by fstalign.

Example usage:
```bash
python sbs2fst.py --tag --left VERBATIM --right NONVERBATIM --gold AGREEMENT sbs_file.txt fst_file_name
fstcompile --isymbols=fst_file_name.txt --osymbols=fst_file_name.txt fst_file_name.fst fst_file_name.compiled.fst
```
You can now use `fst_file_name.compiled.fst` in fstalign with the corresponding symbols file as follows:
```bash
fstalign --ref fst_file_name.complied.fst --symbols fst_file_name.txt ...
```

Note that when you `sbs2fst.py` to produce a "tagged" FST with the `--tag` flag, fstalign will aggregate WER metrics for each of the specified tags (`--left`, `--right`, and `--gold`) in the JSON log file specified by fstalign's `--json-log` flag.

