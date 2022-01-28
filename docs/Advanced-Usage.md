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

### Synonyms
Synonyms allow for reference words to be equivalent to similar forms (determined by the user) for error counting. They are accepted for any input formats and passed into the tool via the `--syn <path_to_synonym_file>` flag. For details see [Synonyms Format](https://github.com/revdotcom/fstalign/blob/develop/docs/Synonyms-Format.md). A standard set of synonyms we use at Rev.ai is available in the repository under `sample_data/synonyms.rules.txt`.

In addition to allowing for custom synonyms to be passed in via CLI, fstalign also automatically generates synonyms based on the reference and hypothesis text. Currently, it does this for two cases: cutoff words (hello-) and compound hyphenated words (long-term). In both cases, a synonym is dynamically generated with the hyphen removed. Both of these synonym types can be disabled through the CLI by passing in `--disable-cutoffs` and `--disable-hyphen-ignore`, respectively.

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
