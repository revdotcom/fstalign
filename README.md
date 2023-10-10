<div align="left"><img src="docs/fstalign_logo.png" width="550"/></div>

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

## Documentation
For more information on how to use `fstalign` see our [documentation](https://github.com/revdotcom/fstalign/blob/develop/docs/Usage.md) for more details.
