<div align="left"><img src="https://i.imgur.com/CJpeJPa.png" width="550"/></div>

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
- [Advanced Usage](#Advanced-Usage)

## Overview
`fstalign` is a tool for creating alignment between two sequences of tokens (here out referred to as “reference” and “hypothesis”). It has two key functions: computing word error rate (WER) and aligning [NLP-formatted](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md) references with CTM hypotheses.

Due to its use of OpenFST and lazy algorithms for text-based alignment, `fstalign` is efficient for calculating WER while also providing significant flexibility for different measurement features and error analysis.

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

Additionally, we have dependencies outside of the third-party submodules:
- OpenFST - currently provided to the build system by settings the $OPENFST_ROOT environment variable or during the CMake command via `-DOPENFST_ROOT`.

### Build
The current build framework is CMake. Install CMake following the instructions here (https://cmake.org/install/).

To build fstalign, run:
```
    mkdir build && cd build
    cmake .. -DOPENFST_ROOT="<path to OpenFST>" -DDYNAMIC_OPENFST=ON
    make
```

Note: `-DDYNAMIC_OPENFST=ON` is needed if OpenFST at `OPENFST_ROOT` is compiled as shared libraries. Otherwise static libraries are assumed.

Finally, tests can be run using:
```
make test
```

### Docker

The fstalign docker image is hosted on Docker Hub and can be easily pulled and run:
```
docker pull revdotcom/fstalign
docker run --rm -it revdotcom/fstalign
```

See https://hub.docker.com/r/revdotcom/fstalign/tags for the available versions/tags to pull. If you desire to run the tool on local files you can mount local directories with the `-v` flag of the `docker run` command.

From inside the container:
```
/fstalign/build/fstalign --help
```

For development you can also build the docker image locally using:
```
docker build . -t fstalign-dev
```

## Quickstart
```
Rev FST Align
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
[+++] [20:37:10] [fstalign] done walking the graph
[+++] [20:37:10] [wer] best WER: 2/5 = 0.4000 (Total words in reference: 5)
[+++] [20:37:10] [wer] best WER: INS:0 DEL:0 SUB:2
[+++] [20:37:10] [wer] best WER: Precision:0.600000 Recall:0.600000
```

Note that in addition to general WER, the insertion/deletion/substitution breakdown is also printed. fstalign also has other useful outputs, including a JSON log for downstream machine parsing, and a side-by-side view of the alignment and errors generated. For more details, see the [Output](#Output) section below.

### Align Subcommand
Usage of the `align` subcommand is almost identical to the `wer` subcommand. The exception is that `align` can only be run if the provided reference is a NLP and the provided hypothesis is a CTM. This is because the core function of the subcommand is to align an NLP without timestamps to a CTM that has timestamps, producing an output of tokens from the reference with timings from the hypothesis.

## Advanced Usage
See [the advanced usage doc](https://github.com/revdotcom/fstalign/blob/develop/docs/Advanced-Usage.md) for more details.
