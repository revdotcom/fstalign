# Documentation
## Table of Contents
* [Quickstart](#quickstart)
* [Subcommands](#subcommands)
  * [`wer`](#wer)
  * [`align`](#align)
* [Inputs](#inputs)
  * [CTM](#ctm)
  * [NLP](#nlp)
  * [FST](#fst)
  * [Synonyms](#synonyms)
  * [Normalizations](#normalizations)
  * [WER Sidecar](#wer-sidecar)
* [Text Transforms](#text-transforms)
  * [use-punctuation](#use-punctuation)
  * [use-case](#use-case)
* [Outputs](#outputs)
  * [Text Log](#text-log)
  * [Side-by-side](#sbs)
  * [JSON Log](#json-log)
  * [Aligned NLP](#nlp-1)
* [Advanced Usage](#advanced-usage)

In this document, we outline the functions of `fstalign` and the features that make this tool unique. Please feel free to start an issue if any of this documentation is lacking / needs further clarification.

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
## Subcommands
### `wer`

The wer subcommand is the most frequent usage of this tool. Required are two arguments traditional to WER calculation: a reference (`--ref <file_path>`) and a hypothesis (`--hyp <file_path>`) transcript. Currently the tool is configured to simply look at the file extension to determine the file format of the input transcripts and parse accordingly.

| File Extension | Reference Support | Hypothesis Supprt |
| ----------- | ----------- | ----------- |
| `.ctm`      | :white_check_mark: | :white_check_mark: |
| `.nlp`      | :white_check_mark: | :white_check_mark: |
| `.fst`      | :white_check_mark: | :white_check_mark: |
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

Note that in addition to general WER, the insertion/deletion/substitution breakdown is also printed. fstalign also has other useful outputs, including a JSON log for downstream machine parsing, and a side-by-side view of the alignment and errors generated. For more details, see the [Outputs](#outputs) section in this doc.

### `align`
Usage of the `align` subcommand is almost identical to the `wer` subcommand. The exception is that `align` can only be run if the provided reference is a NLP and the provided hypothesis is a CTM. This is because the core function of the subcommand is to align an NLP without timestamps to a CTM that has timestamps, producing an output of tokens from the reference with timings from the hypothesis.


## Inputs
### CTM
Time-marked conversations (CTM) are typical outputs for ASR systems. The format of CTMs that fstalign assumes is that each token is on a new line separated by spaces with the following fields.
```
<recording_id> <channel_id> <token_start_ts> <token_duration_ts> <token_value>
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
```

### NLP
[NLP Format](https://github.com/revdotcom/fstalign/blob/develop/docs/NLP-Format.md)

### FST
OpenFST FST files can only be passed to the `--hyp` parameter. fstalign will directly use this FST as the hypothesis during alignment. This is useful for something like oracle lattice analysis, where the reference is aligned to the most accurate path present in a lattice.

Please note that FST inputs require using the standard composition approach,
which can be set using `--composition-approach standard`. Approximate alignment
must also be disabled with `--disable-approx-alignment`.

### Synonyms
Synonyms allow for reference words to be equivalent to similar forms (determined by the user) for error counting. They are accepted for any input formats and passed into the tool via the `--syn <path_to_synonym_file>` flag. For details see [Synonyms Format](https://github.com/revdotcom/fstalign/blob/develop/docs/Synonyms-Format.md). A standard set of synonyms we use at Rev.ai is available in the repository under `sample_data/synonyms.rules.txt`.

In addition to allowing for custom synonyms to be passed in via CLI, fstalign also automatically generates synonyms based on the reference and hypothesis text. Currently, it does this for three cases: cutoff words (e.g. hello-), compound hyphenated words (e.g. long-term), and tags or codes that follow the regular expression: `<.*>` (e.g. <laugh>). In the first two cases, a synonym is dynamically generated with the hyphen removed. Both of these synonym types can be disabled through the CLI by passing in `--disable-cutoffs` and `--disable-hyphen-ignore`, respectively. For the last case of tags, we will automatically allow for `<unk>` to be a valid synonym -- currently, this feature cannot be turned off.

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

### WER Sidecar

CLI flag: `--wer-sidecar`

Only usable for NLP format reference files. This passes a [WER sidecar](https://github.com/revdotcom/fstalign/blob/develop/docs//NLP-Format.md#wer-tag-sidecar) file to
add extra information to some outputs. Optional.

## Text Transforms
In this section, we outline transforms that can be applied to input files. These will modify the handling of the files by `fstalign`.
### `use-punctuation`
Adding the `--use-punctuation` flag will treat punctuation from NLP files as individual tokens for `fstalign`. All other file formats that desire this format are expected to handle punctuation on their own and separating them into their own tokens.

The following files are equivalent with this flag set:

**example.nlp**
```
token|speaker|ts|endTs|punctuation|case|tags|wer_tags
Good|0||||UC|[]|[]
morning|0|||.|LC|['5:TIME']|['5']
Welcome|0|||!|LC|[]|[]
```

**example.txt**
```
good morning . welcome !
```

_Note that WER when this flag is set, measures errors in the words output by the ASR as well as punctuation._

### `use-case`
Adding the `--use-case` flag will take a word's letter case into consideration. In other words, the same word with different letters capitalized will now be considered a different word. For example consider the following:

**Ref:** `Hi this is an example`

**Hyp:** `hi THIS iS An ExAmPlE`

Without this flag, `fstalign` considers these two strings to be equivalent and result in 0 errors. With `--use-case` set, none of these words would be equivalent because they have different letter cases.

_Note that WER when this flag is set, measures errors in the words output by the ASR, taking into account letter casing._


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

## Advanced Usage
Much of the advanced features for fstalign come from providing [NLP file inputs](#NLP) to the references. Some of these features include:
- Entity category WER and normalization: based on labels in the NLP file, entities are grouped into classes in the WER output
  - For example: if the NLP has `2020|0||||CA|['0:YEAR']|` you will see
```s
[+++] [22:36:50] [approach1] class YEAR         WER: 0/8 = 0.0000
```

  - Another useful feature here is normalization, which allows tokens with entity labels to have multiple normalizations accepted as correct by fstalign. This functionality is enabled when the tool is invoked with `--ref-json <path_to_norm_sidecar>` (passed in addition to the `--ref`). This enables something like `2020` to be treated equivalent to `twenty twenty`. More details on the specification for this file are specified in the [Inputs](#Inputs) section below. Note that only reference-side normalization is currently supported.

- Speaker-wise WER: since the NLP file contains a speaker column, fstalign logs and output will provide a breakdown of WER by speaker ID if non-null

- Speaker-switch WER: similarly, fstalign will report the error rate of words around a speaker switch
  - The window size for the context of a speaker switch can be adjusted with the `--speaker-switch-context <int>` flag. By default this is set to 5.
