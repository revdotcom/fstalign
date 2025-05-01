![CI](https://github.com/revdotcom/fstalign/workflows/CI/badge.svg)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

# fstalign
- [Overview](#Overview)
- [What's new in 2.0](#What's-new-in-2.0)
- [Installation](#Installation)
  * [Dependencies](#Dependencies)
  * [Build](#Build)
  * [Docker](#Docker)
- [Documentation](#Documentation)

## Overview
`fstalign` is a tool for creating alignment between two sequences of tokens (here out referred to as “reference” and “hypothesis”). It has two key functions: computing word error rate (WER) and aligning [NLP-formatted](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md) references with CTM hypotheses.

Due to its use of OpenFST and lazy algorithms for text-based alignment, `fstalign` is efficient for calculating WER while also providing significant flexibility for different measurement features and error analysis.

## What's new in 2.0

Version 2.0 introduces two major changes: 
1. A new method to traverse the composition graph, which dramatically improves the overall speed, especially when the sequences are long contain many errors.
We have files that took 25 minutes to align before that can now take about 7 seconds. This is especially noticeable with the adapted composition (the default).
1. Some smarts were introduced when --use-case and --use-punctuation are enabled.
Now, by default, punctuation symbols can only be substituted by other punctuation symbols (or deleted/inserted).
Also, words that differ only by the first letter case will be preffered for substitution.


Here's an example of the 1.x behavior and the 2.0 version
```
==> v1.x sbs.txt <==
           ref_token	hyp_token           	IsErr	Class	Wer_Tag_Entities
             Welcome	Welcome             			###322_###|
                back	back                			
                  to	to                  			
             another	another             			
             episode	episode             			###323_###|
                  of	of                  			
            Podcasts	Podcast             	ERR		###324_###|
                  in	and                 	ERR		
               Color	Color               			###167_###|###325_###|
                   :	of                  	ERR		
                 The	the                 	ERR		
             Podcast	Podcast             			###168_###|###326_###|
                   .	.                   			
                   I	I                   			

==> v2.0 sbs.txt <==
           ref_token	hyp_token           	IsErr	Class	Wer_Tag_Entities
             Welcome	Welcome             			###322_###|
                back	back                			
                  to	to                  			
             another	another             			
             episode	episode             			###323_###|
                  of	of                  			
            Podcasts	Podcast             	ERR		###324_###|
                  in	and                 	ERR		
               Color	Color               			###167_###|###325_###|
               <ins>	of                  	ERR		
                   :	<del>               	ERR		
                 The	the                 	ERR		
             Podcast	Podcast             			###168_###|###326_###|
```
The confusion between `:` and `of` is not longer allowed.

Also, here's how favoring or not the substitution based on case-insensitive comparison, while still counting it as an error, looks like:
```
==> v1.x sbs.txt <==
           ref_token	hyp_token           	IsErr	Class	Wer_Tag_Entities
             shorten    shorten                         ###801_###|
                It's    it's                    ERR     
               Berry    Barry                   ERR     ###785_###|###788_###|###802_###|
                   .    .        
                Just    Just   
                Yeah    like                    ERR     ###805_###|                                                                                                             
                   .    <del>                   ERR
                Like    <del>                   ERR     
                   ,    <del>                   ERR     
                   I    I                               ###809_###|
                have    have   
                   a    a         
            nickname    nickname 

==> v2.0 sbs.txt <==
           ref_token	hyp_token           	IsErr	Class	Wer_Tag_Entities
                It's    it's                    ERR     
               Berry    Barry                   ERR     ###785_###|###788_###|###802_###|
                   .    .     
                Just    Just     
                Yeah    <del>                   ERR     ###805_###|
                   .    <del>                   ERR     
                Like    like                    ERR     
                   ,    <del>                   ERR     
                   I    I                               ###809_###|
                have    have     
                   a    a        
            nickname    nickname  
```
Here, `Like <-> like` substitution is favored.  While this generally won't change the WER value itself (although it can), it will improve the timing alignments.  


These behavior, as well as the beam size (that has a default value of 50.0) can be controlled with the following new parameters:
```
  --disable-strict-punctuation
                              Disable strict punctuation alignment (which prevents punctuation aligning with words).
  --disable-favored-subs      Disable favored substitutions (which makes alignment favor substitutions between words which differ only by case).
  --favored-sub-cost FLOAT    Cost for favored substitutions (e.g., case diff). Default: 0.1
```

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
