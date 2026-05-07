# fstalign

`fstalign` is a tool for creating alignment between two sequences of tokens (here out referred to as "reference" and "hypothesis"). It has two key functions: computing word error rate (WER) and aligning [NLP-formatted](docs/NLP-Format.md) references with CTM hypotheses.

## Stack

- **C++** 14 — `CMakeLists.txt`
  Frameworks: CLI11
- **Docker** — `Dockerfile`

## Architecture

`fstalign` is a single-binary CLI driven by `src/main.cpp` (CLI11). Two subcommands are exposed (`wer`, `align`); both follow the same pipeline:

1. **Load** the reference and hypothesis into OpenFST acceptors. Loaders are polymorphic on input type — `NlpFstLoader` (NLP-formatted reference), `OneBestFstLoader` (plain text), `FstFileLoader` (pre-built `.fst`), `Ctm` (timed CTM hypothesis). Optional `SynonymEngine` expands the reference acceptor with allowed alternates.
2. **Compose** reference × hypothesis. Strategy is selected by `--composition-approach`: `StandardComposition` is OpenFST's stock composition; `AdaptedComposition` (default) is a lazy/streaming variant tuned for long sequences with many errors and is what the v2.0 speedup is about. Both implement `IComposition`.
3. **Traverse** the composed graph to extract the best alignment path. `Walker` drives the search using a `PathHeap`, and `AlignmentTraversor` walks the resulting path to materialise per-token decisions (match / sub / ins / del) along with class and entity tags from the NLP reference.
4. **Score & emit**. `wer.cpp` computes WER and class-broken-out stats; `fast-d.cpp` provides the lazy edit-distance kernel used during traversal. Outputs include side-by-side text (`sbs.txt`), a JSON log (schema: `docs/json_log_schema.json`), an optional NLP file with insertions, and stdout WER summaries.

```
ref (NLP / 1-best / .fst) ──┐
                            ├─► Loaders ─► [SynonymEngine] ─► IComposition ─► Walker ─► AlignmentTraversor ─► wer / json_logging ─► sbs.txt, json log, stdout
hyp (CTM / 1-best / .fst) ──┘                                  (Standard|Adapted)         (PathHeap, fast-d)
```

All long-running work happens in `fstaligner-common` (the library); `main.cpp` is just CLI parsing and dispatch into the top-level entry points in `fstalign.cpp`.

## External dependencies

- **OpenFST** (library) — finite-state transducer operations; provided via `OPENFST_ROOT` (env or `-DOPENFST_ROOT=` on the cmake line)
- **ICU** (library) — Unicode and locale support (`find_package(ICU)`)
- **Threads** (library) — system threading (`find_package(Threads)`)
- **CLI11** (library) — command-line argument parsing (vendored submodule)
- **spdlog** (library) — logging (vendored submodule)
- **jsoncpp** (library) — JSON output construction (vendored submodule)
- **csv** (library) — fast CTM and NLP CSV parsing (vendored submodule)
- **inih** (library) — INI file parsing (vendored submodule)
- **strtk** (library) — string utilities (vendored submodule)
- **catch2** (library) — unit testing (vendored submodule)
- **debian** (container) — Debian Bullseye base image (Dockerfile)

## Workspace

- Path: `fstalign`

**Build**

```bash
git submodule update --init --recursive
cmake -S . -B build -DOPENFST_ROOT="$OPENFST_ROOT" -DDYNAMIC_OPENFST=ON
cmake --build build
```

**Test**

```bash
ctest --test-dir build
```

**Lint**

```bash
clang-format --dry-run --Werror src/*.cpp src/*.h
```

**Run**

```bash
./build/fstalign --help
```

## Configuration

| Variable | Source | Default | Purpose |
| --- | --- | --- | --- |
| `OPENFST_ROOT` | env | — | Path to OpenFST install; required at build (CMake reads it) and embedded in Docker image at `/opt/openfst` |
| `DYNAMIC_OPENFST` | flag | `OFF` | Pass `-DDYNAMIC_OPENFST=ON` to cmake when OpenFST at `OPENFST_ROOT` is built as shared libraries |
| `--ref` | flag | — | Reference file (NLP format) |
| `--hyp` | flag | — | Hypothesis file (CTM) |
| `--composition-approach` | flag | `adapted` | Composition strategy: `adapted` or `standard` |
| `--use-case` | flag | `false` | Case-sensitive comparison |
| `--use-punctuation` | flag | `false` | Include punctuation in alignment |
| `--disable-strict-punctuation` | flag | `false` | Allow punctuation to align with words |
| `--disable-favored-subs` | flag | `false` | Disable preference for case-only substitutions |
| `--favored-sub-cost` | flag | `0.1` | Cost for favored (case-only) substitutions |

## Project layout

- `src/` — C++ implementation
- `test/` — Catch2 unit tests + test data
- `docs/` — NLP/Synonyms format specs, Usage guide, JSON log schema
- `sample_data/` — example reference and hypothesis inputs
- `tools/` — auxiliary scripts (`gather_runtime_metrics.sh`, `generate_wer_test_data.pl`, `sbs2fst.py`)
- `third-party/` — vendored dependencies (git submodules)
- `ext/` — external source tarballs (OpenFST) used by the Docker build

## Entry points

- `src/main.cpp` (executable) — `fstalign` (declared via `add_executable` in `CMakeLists.txt:85`)
- `Dockerfile` (CMD) — `PATH` puts `/fstalign/bin/fstalign` on the image path

## Conventions

- Third-party C++ dependencies are vendored as git submodules under `third-party/`; initialize with `git submodule update --init --recursive` before building
- Tests live in `test/` as Catch2 `*.cc` files
- OpenFST is intentionally NOT vendored — pulled at build time from `ext/openfst-<version>.tar.gz` (Docker) or pointed at via `OPENFST_ROOT` (host)

## Code style

- **Guide**: Google C++ Style Guide
- **Formatter**: `clang-format -style=file` (config: `.clang-format`, `BasedOnStyle: Google`)
- **Linters**: `clang-tidy`, `cppcheck`, `shellcheck` (for `tools/*.sh`)

## Current focus

_(last touched 2026-03-12 by Todd C. Parnell, Kirill Bykov, Jp)_

## Links

- Repository: git@github-work:revdotcom/fstalign.git
- Documentation: https://github.com/revdotcom/fstalign/blob/develop/docs/Usage.md
