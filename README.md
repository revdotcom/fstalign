![CI](https://github.com/revdotcom/fstalign/workflows/CI/badge.svg)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

# fstalign
- [Overview](#Overview)
- [Installation](#Installation)
  * [Dependencies](#Dependencies)
  * [Build](#Build)
  * [Docker](#Docker)
- [Quickstart](#Quickstart)
  * [WER Subcommand](#WER-Subcommand)
  * [Align Subcommand](#Align-Subcommand)
- [Inputs](#Inputs)
- [Outputs](#Outputs)

## Overview
`fstalign` is a tool for creating alignment between two sequences of tokens (here out referred to as “reference” and “hypothesis”). It has two key functions: computing word error rate (WER) and aligning [NLP-formatted](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md) references with CTM hypotheses.

Due to its use of OpenFST and lazy algorithms for text-based edit-distance alignment, `fstalign` is one of the fastest and most efficient tools for calculating WER. Furthermore, the tool offers additional features to augment error analysis, which will be covered more in depth below.

## Installation

### Dependencies

We use git submodules to manage third-party dependencies. Initialize and update submodules before proceeding to the main build steps.
```
git submodule update --init --recursive
```

This will pull the current dependencies:
- catch2 - for unit testing
- spdlog - for logging
- CLI11 - for CLI construction
- csv - for CTM and NLP input parsing
- jsoncpp - for JSON output construction
- strtk - for various string utilities

Additionally, we have dependencies outside of the third-party submodules.
- OpenFST - currently provided to the build system via the $KALDI_ROOT ($KALDI_ROOT/tools/openfst)
- Kaldi - used for kaldi-io archive processing

These are passed into the CMake command using `-DFST_KALDI_ROOT`.

### Build

The current build framework is CMake. Install CMake following the instructions here (https://cmake.org/install/).

To build fstalign, run:
```
    mkdir build && cd build
    cmake .. -DFST_KALDI_ROOT=<path to kaldi> -DDYNAMIC_KALDI=ON
    make
```

Note: `-DDYNAMIC_KALDI=ON` is important if the `KALDI_ROOT` specified is compiled into shared libraries. Otherwise static libraries are assumed.

Finally, tests can be run using:
```
make test
```

### Docker

The fstalign docker image is hosted on Github packages and can be pulled through:
```
docker pull docker.pkg.github.com/revdotcom/fstalign/fstalign:1.0.0
```

This docker image contains the source code, built dependencies, and the built binary. A container can be started using:
```
docker run --rm -it docker.pkg.github.com/revdotcom/fstalign/fstalign:1.0.0
```

Additionally, if you desire to run the tool on local files you can mount local directories with the `-v` flag of the `docker run` command. Once the container is running, the binary is located at `/fstalign/build/fstalign`.

For development you can also build the docker image locally using:
```
docker build . -t fstalign-dev
```

from the repository root. From there you can start a container from `fstalign-dev` using the procedure above.

## Quickstart
```
Rev FST Aligner
Usage: ./fstalign [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --version                   Show fstalign version.

Subcommands:
  wer                         Get the WER between a reference and an hypothesis.
  align                       Produce an alignment between an NLP file and a CTM-like input.
```

### WER Subcommand

The wer subcommand is the most frequent usage of this tool. Required are two arguments traditional to WER calculation: a reference (`--ref <file_path>`) and a hypothesis (`--hyp <file_path>`) transcript. Currently the tool is configured to simply look at the file extension to determine the file format of the input transcripts and parse accordingly.

| File Extension | Reference Support | Hypothesis Supprt |
| ----------- | ----------- | ----------- |
| `.ctm`      | :white_check_mark: | :white_check_mark: |
| `.nlp`      | :white_check_mark: | :white_check_mark: |
| `.fst`      | :x: | :white_check_mark: |
| All other file extensions, assumed to be plain text | :white_check_mark: | :white_check_mark: |

Basic Example:
```
ref.txt
this is the best sentence

hyp.txt
this is a test sentence

./bin/fstalign wer --ref ref.txt --hyp hyp.txt
```

When run, fstalign will dump a log to STDOUT with summary WER information at the bottom. For the above example:
```
[+++] [20:37:10] [approach1] performing lazy composition
[+++] [20:37:10] [walker] starting a walk in the park
[+++] [20:37:10] [walker] we reached a final node with a wer of 0.4
[+++] [20:37:10] [walker] we reached a final node with a wer of 0.6
[+++] [20:37:10] [walker] we have 2 candidates after 39 loops
[+++] [20:37:10] [walker] getting details for candidate 0
[+++] [20:37:10] [walker] approx WER was 0.4, real WER is 0.4
[+++] [20:37:10] [walker] getting details for candidate 1
[+++] [20:37:10] [walker] approx WER was 0.6, real WER is 0.4
[+++] [20:37:10] [approach1] done walking the graph
[+++] [20:37:10] [approach1] best WER: 2/5 = 0.4000 (Total words in reference: 5)
[+++] [20:37:10] [approach1] best WER: INS:0 DEL:0 SUB:2
[+++] [20:37:10] [approach1] best WER: Precision:0.600000 Recall:0.600000
[+++] [20:37:10] [console] done
```

Note that in addition to general WER, the insertion/deletion/substitution breakdown is also printed. fstalign also has other useful outputs, including a JSON log for downstream machine parsing, and a side-by-side view of the alignment and errors generated. For more details, see the [Output](#Output) section below.

Much of the advanced usage and features for fstalign come from providing [NLP file inputs](#NLP) to the references. Some of these features include:
- Entity category WER and normalization: based on labels in the NLP file, entities are grouped into classes in the WER output
  - For example: if the NLP has `2020|0||||CA|['0:YEAR']|` you will see
```s
[+++] [22:36:50] [approach1] class YEAR         WER: 0/8 = 0.0000
```

  - Another useful feature here is normalization, which allows tokens with entity labels to have multiple normalizations accepted as correct by fstalign. This functionality is enabled when the tool is invoked with `--ref-json <path_to_norm_sidecar>`. This enables something like `2020` to be treated equivalent to `twenty twenty`. More details on the specification for this file are specified in the [Inputs](#Inputs) section below. Note that only reference-side normalization is currently supported.

- Speaker-wise WER: since the NLP file contains a speaker column, fstalign logs and output will provide a breakdown of WER by speaker ID if non-null

- Speaker-switch WER: similarly, fstalign will report the error rate of words around a speaker switch
  - The window size for the context of a speaker switch can be adjusted with the `--speaker-switch-context <int>` flag. By default this is set to 5.


### Align Subcommand
Usage of the `align` subcommand is almost identical to the `wer` subcommand. The exception is that `align` can only be run if the provided reference is a NLP and the provided hypothesis is a CTM. This is because the core function of the subcommand is to align an NLP without timestamps to a CTM that has timestamps, producing an output of tokens from the reference with timings from the hypothesis.

## Inputs
### CTM
Time-marked conversations (CTM) are typical outputs for ASR systems. The format of CTMs that fstalign assumes is that each token is on a new line separated by spaces with the following fields.
```
<recording_id> <channel_id> <token_start_ts> <token_end_ts> <token_value>
```
Moreover, there is an optional sixth field `<confidence_score>` that is read in if provided. The field does not affect the WER calculation and is primarily there just to support the parsing the common alteration to the basic CTM format.

Example (no confidence scores):
```
test.wav 1 1.0 1.0 a
test.wav 1 3.0 1.0 b
test.wav 1 5.0 1.0 c
test.wav 1 7.0 1.0 d
test.wav 1 9.0 1.0 <unk>
test.wav 1 11.0 1.0 e
test.wav 1 13.0 1.0 f
test.wav 1 15.0 1.0 g
test.wav 1 17.0 1.0 h
test.wav 1 21.0 1.0 i
test.wav 1 23.0 1.0 j
```

### NLP
[NLP Format](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md)

### FST
FST archive files can only be passed to the `--hyp` parameter, and are formatted as a Kaldi archive of utterance FSTs. For more details see https://kaldi-asr.org/doc/io.html. fstalign will read in each FST in the archive and, in sorted order by archive key, concatenate each FST until a single FST is created for the entire hypothesis.

This is useful for something like oracle lattice analysis, where the reference is aligned to the most accurate path present in a lattice.

### Synonyms
Synonyms allow for reference words to be equivalent to similar forms (determined by the user) for error counting. They are accepted for any input formats and passed into the tool via the `--syn <path_to_synonym_file>` flag. The file structure is a simple text file where each line is a synonym and each synonym is separated by a pipe where the left hand side is the reference version of the term and the right hand side is the accepted hypothesis alternative. Note that there is no built in symmetry, so synonyms must be doubly specified for symmetrical equivalence (example below illustrates this).

Example:
```
i am     | i'm
i'm      | i am
okay     | ok
ok       | okay
```

A standard set of synonyms we use at Rev.ai is available in the repository under `sample_data/synonyms.rules.txt`.

### Normalizations

Normalizations are a similar concept to synonyms. They allow a token or group of tokens to be represented by alternatives when calculating the WER alignment. Unlike synonyms, they are only accepted for NLP file inputs where the tokens are tagged with a unique ID. The normalizations are specified in a JSON format, with the unique ID as keys. Example to illustrate the schema:
```
{
    "0": {
        "candidates": [
            {
                "probability": 0.5,   // Optional and currently unused field
                "verbalization": [
                    "twenty",
                    "twenty"
                ]
            },
            {
                "probability": 0.5,
                "verbalization": [
                    "two",
                    "thousand",
                    "and",
                    "twenty"
                ]
            }
        ],
        "class": "YEAR"
    }
}
```

## Outputs

### Text Log
CLI flag: `--log`

Saves stdout messages to a log file.

### SBS
CLI flag: `--output-sbs`

Writes a side-by-side alignment of the reference and hypothesis to a file. Useful for debugging and error analysis.

Example:
```
           ref_token    hyp_token               IsErr   Class
                   i    i                               
                 was    was                             
                just    just
               going    going                           
                  to    to                              
                 say    say                             
                 one    one
               thing    thing
               <ins>    me                      ERR
                 i'm    i                       ERR     ___0_CONTRACTION___
              really    really
        appreciating    appreciated             ERR
```

In this example, "i'm" was labeled as `___0_CONTRACTION___` in the reference, so the error will be added when computing the WER specific for `CONTRACTION` entities.

### JSON Log
CLI flag: `--json-log`

Writes all WER statistics and precision/recall information to a machine-parseable JSON file.

Schema: [json_log_schema.json](https://github.com/revdotcom/fstalign/blob/develop/docs/json_log_schema.json)

Example snippet:
```
{
        "wer" : 
        {
                "bestWER" :
                {
                        "deletions" : 93,
                        "insertions" : 47,
                        "meta" : {},
                        "numErrors" : 228,
                        "numWordsInReference" : 1312,
                        "precision" : 0.89336490631103516,
                        "recall" : 0.86204266548156738,
                        "substitutions" : 88,
                        "wer" : 0.17378048598766327
                },
                "classWER" :
                {
                        "CARDINAL" :
                        {
                                "deletions" : 0,
                                "insertions" : 0,
                                "meta" : {},
                                "numErrors" : 0,
                                "numWordsInReference" : 7,
                                "substitutions" : 0,
                                "wer" : 0.0
                        }
                },
                "bigrams" :
                {
                        "amount of" :
                        {
                                "correct" : 0,
                                "deletions" : 1,
                                "insertions" : 0,
                                "precision" : 0.0,
                                "recall" : 0.0,
                                "substitutions" : 0
                        },
```
The “bigrams” and “unigrams” fields are only populated with unigrams and bigrams that surpass the minimum frequency specified by the `--pr_threshold` flag, which is set to 0 by default.

### NLP

CLI flag: `--output-nlp`

Writes out the reference [NLP](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md), but with timings provided by a hypothesis CTM. Mostly relevant for the `align` subcommand.
