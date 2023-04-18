# NLP Format
NLP files are `.csv` inspired, pipe-separated text files that contain token and metadata information of a transcript. Each line of a file represents a single transcript token and the metadata associated with it. 

| Column | Description |
| ----------- | ----------- |
| Column 1: token | A single token in the transcript. These are typically single words or multiple words with hyphens in between. |
| Column 2: speaker | A unique ID that associates this token to a specific speaker in an audio. |
| Column 3: ts | A float representing the time in seconds that starts of the token’s utterance. |
| Column 4: endTs | A float representing the time in seconds that ends of the token’s utterance. |
| Column 5: punctuation | A punctuation character that is included at the end of a token that is used when reconstructing the transcript. Example punctuation: `",", ";", ".", "!"`. These will be ignored from WER token matching. |
| Column 6: case | A two letter code to denominate the which of four possible casings for this token: <br />UC - Denotes a token that has the first character in uppercase and every other character lowercase<br />LC - Denotes a token that has every character in lowercase<br />CA - Denotes a token that has every character in uppercase<br />MC - Denotes a token that doesn’t follow the previous rules. This is the case when upper- and lowercase characters are mixed throughout the token |
| Column 7: tags | Displays one of the several entity tags that are listed in wer_tags in long form - such that the displayed entity here is in the form `ID:ENTITY_CLASS`. If normalization is used, only entities in this column can be normalized. |
| Column 8: wer_tags | A list of entity tags that are associated with this token. In this field, only entity IDs should be present. The specific ENTITY_CLASS for each ID can be extracted from an accompanying wer_tags sidecar json. |
 
Example:
```
token|speaker|ts|endTs|punctuation|case|tags|wer_tags
Good|0||||UC|[]|[]
morning|0||||LC|['5:TIME']|['5']
and|0||||LC|[]|[]
welcome|0||||LC|[]|[]
to|0||||LC|[]|[]
the|0||||LC|['6:DATE']|['6']
first|0||||LC|['6:DATE']|['6']
quarter|0||||LC|['6:DATE']|['6']
2020|0||||CA|['0:YEAR']|['0', '1', '6']
NexGEn|0||||MC|['7:ORG']|['7']
```

## WER tag sidecar

WER tag sidecar files contain accompanying info for tokens in an NLP file. The
keys are IDs corresponding to tokens in the NLP file `wer_tags` column. The
objects under the keys are information about the token.

Example:
```
{
 '0': {'entity_type': 'YEAR'},
 '1': {'entity_type': 'CARDINAL'},
 '6': {'entity_type': 'SPACY>TIME'},
}
```
