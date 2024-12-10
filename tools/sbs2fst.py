#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (C) 2023
# Author: Miguel Ángel del Río Fernández <miguel.delrio@rev.com>
# All Rights Reserved

from argparse import ArgumentParser
from collections import OrderedDict
from dataclasses import dataclass, field
from itertools import takewhile
from pathlib import Path
from typing import Dict, Generator, List, Optional, Tuple


@dataclass
class SbsEntry:
    """ Represent a single SBS line."""
    ref_word: str
    hyp_word: str
    error: bool
    entity_class: str
    wer_tags: List[str] = field(default_factory=list)
    extra_columns: List[str] = field(default_factory=list)

    @classmethod
    def from_line(cls, line: str) -> 'SbsEntry':
        parts = line.strip(' \n').split('\t')
        if len(parts) == 4:
            # old format
            entry = SbsEntry(parts[0].strip(), parts[1].strip(),
                    parts[2] == 'ERR', parts[3])
        elif len(parts) == 5:
            # New format, wer_tags
            entry = SbsEntry(parts[0].strip(), parts[1].strip(),
                    parts[2] == 'ERR', parts[3],
                    [tag for tag in parts[4].split('|') if tag])
        elif len(parts) > 5:
            entry = SbsEntry(parts[0].strip(), parts[1].strip(),
                    parts[2] == 'ERR', parts[3],
                    [tag for tag in parts[4].split('|') if tag],
                    extra_columns=parts[5:])
        else:
            raise RuntimeError(f"Could not parse the line as SBS:\n{line}")
        return entry

    def __str__(self):
        if self.error:
            err_str = "ERR"
        else:
            err_str = ""
        wer_tags_str = "|".join(self.wer_tags)
        if wer_tags_str:
            wer_tags_str += "|"
        return '\t'.join([self.ref_word, self.hyp_word, err_str,
            self.entity_class, wer_tags_str]+self.extra_columns)


def load_from_file(fp: Path) -> Generator[SbsEntry, None, None]:
    with open(fp) as f:
        f.readline()
        lines = takewhile(lambda x: not x.startswith("--------"), f.readlines())
        for line in lines:
            yield SbsEntry.from_line(line)


class FSTState:
    def __init__(self):
        self.state: int = 0
        self.vocabulary: OrderedDict = OrderedDict({"<eps>": 0})
        self.unique_id: int = 0

    def update_vocabulary(self, word):
        """If `word` isn't in `self.vocabulary` add it
        with it's own unique id."""
        if word not in self.vocabulary:
            self.vocabulary[word] = len(self.vocabulary)

    def get_uid(self):
        """Return the `self.unique_id` and increment it
        by one."""
        current_uid = self.unique_id
        self.unique_id += 1
        return current_uid


def init_args():
    parser = ArgumentParser(description="SBS to FST")
    parser.add_argument("sbs_file", type=Path, help="The input SBS file")
    parser.add_argument("fst_file", type=Path, help="The output FST file")
    parser.add_argument(
        "--left",
        type=str,
        default="LEFT",
        help="Label for the left column. This label will be given to "
        "words that occur on the left (reference) side of the SBS "
        "during an ERR.",
    )
    parser.add_argument(
        "--right",
        type=str,
        default="RIGHT",
        help="Label for the right column. This label will be given to "
        "words that occur on the right (hypothesis) side of the SBS "
        "during an ERR.",
    )
    parser.add_argument(
        "--gold",
        type=str,
        default="GOLD",
        help="Label for the gold column. This is for words that both "
        "transcripts agree upon in the SBS.",
    )
    parser.add_argument(
        "--tag",
        action="store_true",
        help="If set, the script will add extra tagging information",
    )
    return parser.parse_args()


def prepare_IO(
    input: Path,
    output: Path,
):
    """Determines if the input is a directory or file and prepares output accordingly"""
    input_files = []
    output_files = []
    if input.is_dir():
        output.mkdir(parents=True, exist_ok=True)
        for file in input.glob("**/*.txt"):
            input_files.append(file)
            output_files.append(output / file.stem)
    else:
        input_files = [input]
        output_files = [output]
    return input_files, output_files


def _to_fst_line(state1, state2, arc, weight: float=0):
    return f"{state1} {state2} {arc} {arc} {weight}"


def flush_span(
    span: List[str], state: int, *, tag: Optional[str] = None, branch_factor: int = 0
) -> Tuple[List[str], int]:
    """Flush the span by generating the relevant fst lines. If `tag`
    is set add surrounding fst lines to correspond to the tag.
    `branch_factor` can also be set to increase the initial transition
    from the tag state to the first span state (SHOULD ONLY BE USED IN
    COMBINATION WITH `tag`).
    The primary use of the `branch_factor` is for the right side during
    a disagreement -- you want the first right-side arc to go from the same
    start as the left-side to a new state that isn't used by the left side
    at all. So by specifying the `branch_factor` you can "skip" states.
    In the context of a disagreement, the left-side will have 0 `branch_factor`
    while the right-side must have a `branch_factor` the size of left-side
    length.
    """
    if len(span) == 0:
        return [], state

    span_state = state + branch_factor + 1
    if tag:
        fst_lines = [_to_fst_line(state, span_state, tag)]
    else:
        fst_lines = [_to_fst_line(state, span_state, span[0])]
        span = span[1:]

    for token in span:
        fst_lines.append(_to_fst_line(span_state, span_state + 1, token))
        span_state += 1

    if tag:
        fst_lines.append(_to_fst_line(span_state, span_state + 1, tag))
        span_state += 1
    return fst_lines, span_state


def agreement_flush(
    gold_span: List[str], fst_state: FSTState, *, tag: bool = False, gold: Optional[str] = None
) -> List[str]:
    """Flush "gold" spans when both sides of the sbs agree and update the FSTState.
    If `tag` is True, adds a unique tag around the span using `gold` to label.
    """
    gold_tag = None
    if tag:
        gold_tag = f"___MULTIREF:{fst_state.get_uid()}_{gold}___"
        fst_state.update_vocabulary(gold_tag)

    gold_fst_lines, new_state = flush_span(gold_span, fst_state.state, tag=gold_tag)
    fst_state.state = new_state

    return gold_fst_lines


def disagreement_flush(
    left_span: List[str],
    right_span: List[str],
    fst_state: FSTState,
    *,
    tag: bool = False,
    left: Optional[str] = None,
    right: Optional[str] = None,
) -> List[str]:
    """Flush the left and right spans when transcripts disagree and update the FSTState.
    If `tag` is True, adds a unique tag around the left span using `left` to label and
    around the right span using `right` to label.
    """
    fst_lines = []

    left_tag = None
    if tag:
        left_tag = f"___MULTIREF:{fst_state.get_uid()}_{left}___"
        fst_state.update_vocabulary(left_tag)

    left_fst_lines, left_end_state = flush_span(left_span, fst_state.state, tag=left_tag)
    fst_lines.extend(left_fst_lines)

    right_tag = None
    if tag:
        right_tag = f"___MULTIREF:{fst_state.get_uid()}_{right}___"
        fst_state.update_vocabulary(right_tag)

    right_fst_lines, right_end_state = flush_span(
        right_span, fst_state.state, tag=right_tag, branch_factor=len(left_fst_lines)
    )
    fst_lines.extend(right_fst_lines)

    max_end_state = max(left_end_state, right_end_state)
    # We have to return both paths back to same state to progress
    fst_lines.append(_to_fst_line(left_end_state, max_end_state + 1, "<eps>"))
    fst_lines.append(_to_fst_line(right_end_state, max_end_state + 1, "<eps>"))

    fst_state.state = max_end_state + 1

    return fst_lines


def sbs2fst(
    sbs_file: Path,
    *,
    tag: bool = False,
    gold: Optional[str] = None,
    left: Optional[str] = None,
    right: Optional[str] = None,
) -> Tuple[List[str], Dict[str, int]]:
    """Given an `sbs_file` create the equivalent fst object.
    Optionally you can include tags by setting `tag` to true. These will be distinguished by the
    tag labels provided in `gold` (agreements), `left` (words on the reference side of the sbs not in hypothesis),
    and `right` (words on the hypothesis side of the sbs not in reference).
    """
    sbs = load_from_file(sbs_file)

    fst_state = FSTState()
    fst_lines = []

    left_span = []
    right_span = []
    gold_span = []
    for row_idx, row in enumerate(sbs):
        ref_word = "<eps>" if row.ref_word == "<ins>" else row.ref_word
        hyp_word = "<eps>" if row.hyp_word == "<del>" else row.hyp_word

        fst_state.update_vocabulary(ref_word)
        fst_state.update_vocabulary(hyp_word)

        if row.ref_word == row.hyp_word:
            # First flush the left & right spans to empty any disagreements
            if len(left_span) > 0 or len(right_span) > 0:
                disagreement_fst_lines = disagreement_flush(
                    left_span, right_span, fst_state, tag=tag, left=left, right=right
                )
                fst_lines.extend(disagreement_fst_lines)
                left_span = []
                right_span = []
            gold_span.append(row.ref_word)
        else:
            # First flush the gold span to empty any agreements
            if len(gold_span) > 0:
                gold_fst_lines = agreement_flush(gold_span, fst_state, tag=tag, gold=gold)
                fst_lines.extend(gold_fst_lines)
                gold_span = []

            if ref_word != "<eps>":
                left_span.append(ref_word)
            if hyp_word != "<eps>":
                right_span.append(hyp_word)

    # Flush the spans that have infomration. It'll only be a gold or a disagreement. Not both
    if len(gold_span) > 0:
        gold_fst_lines = agreement_flush(gold_span, fst_state, tag=tag, gold=gold)
        fst_lines.extend(gold_fst_lines)
    elif len(left_span) > 0 or len(right_span) > 0:
        disagreement_fst_lines = disagreement_flush(
            left_span, right_span, fst_state, tag=tag, left=left, right=right
        )
        fst_lines.extend(disagreement_fst_lines)

    fst_lines.append(f"{fst_state.state}")

    return fst_lines, fst_state.vocabulary


def main(
    sbs_file: Path,
    fst_file: Path,
    tag: bool = False,
    gold: Optional[str] = None,
    left: Optional[str] = None,
    right: Optional[str] = None,
):
    for inpath, outpath in zip(*prepare_IO(sbs_file, fst_file)):
        fst_lines, vocabulary = sbs2fst(inpath, tag=tag, gold=gold, left=left, right=right)

        with open(f"{outpath}.fst", "w") as fstfile:
            fstfile.write("\n".join(fst_lines))

        with open(f"{outpath}.txt", "w") as fstfile:
            for key, value in vocabulary.items():
                fstfile.write(f"{key} {value}\n")


if __name__ == "__main__":
    args = init_args()
    main(**vars(args))
