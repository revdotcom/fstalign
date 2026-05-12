# Repo Readiness Report

**Repo:** fstalign (`revdotcom/fstalign`)
**Audited:** 2026-05-12
**Based on:** AI in EPD Module 3 — Repo Readiness Checklist (Claire Vo & Zach Davis)
**Audit version:** v2.1 (local + external GitHub checks; non-GitHub CI captured only)
**External validation:** GitHub Actions via gh

## Summary

- **Overall readiness:** Early
- **Score:** 3 ✅ / 7 🟡 / 9 ❌ / 4 🔗 / 3 ➖ (of 26)
- **Stack detected:** C++14 (CMake build, Catch2 tests), Docker, Perl/Python/Shell tools
- **External validation verdict:** CI (push-triggered, default branch) median 6m32s with 7/7 success on last 7 develop runs; no agentic code-review tool configured locally — GitHub Copilot's PR reviewer bot found on 1/20 PRs; CodeQL `Analyze (python)` / `Analyze (actions)` running on 4–5/20 PRs from an org-level install; stale-branch and stale-PR workflows firing daily; 10/20 PRs reference a Jira-style `FSTALIGN-*` ticket.

*Scale: small (41 source files). Items 9–12 (skill creator, hooks, deny-hooks, telemetry) and 20/24 (skills marketplace, deterministic PR guardrails) are reasonable defers at this scale — read the band as "Tier 1 + Tier 2 readiness for a repo of this scale." Item-level PASS/FAIL is unchanged; this is reading context, not a scoring override.*

### Top 3 Next Actions
1. **Add a per-language style/practices doc and an operational-surface section.** CLAUDE.md's "Code style" is four lines; the architecture diagram is good but never says what runtime the binary serves (CLI distributed via Docker Hub `revdotcom/fstalign`), where to monitor it in production at Rev, or where the Docker publish target lives. Both gaps are cheap to close in the existing CLAUDE.md.
2. **Wire `clang-format --dry-run --Werror` into CI as a fast gate.** Config exists (`.clang-format`, Google C++ style) and is the documented lint in CLAUDE.md, but CI (`.github/workflows/CI.yml`) only builds the Docker image and runs `make test`. A 5-second clang-format step is the highest-leverage Tier 2 fix.
3. **Even a thin `.claude/` (skills dir + a committed `settings.json` with a curated `permissions.allow` for the C++/CMake/git workflow) would move items 7–8 from FAIL to PASS.** The current state is zero agent tooling — every Tier 1 tooling-cluster item fails (7–12).

## Tier 1 — Foundation

| # | Item | Status | Evidence / Notes |
|---|------|--------|------------------|
| 1 | AGENTS.md (top-level + nested) | ✅ | `CLAUDE.md` (121 lines), substantive: stack, architecture diagram, deps, build/test/lint/run commands, configuration table, project layout, entry points, conventions. No nested files needed at this scale. |
| 2 | Best practices per-language, checked into repo | 🟡 | `CLAUDE.md` "Code style" (4 lines) names Google C++ Style Guide + `clang-format` + `clang-tidy`/`cppcheck`/`shellcheck`. No dedicated style doc, no `.claude/tips/`. Tools/* shell scripts (`gather_runtime_metrics.sh`) and a Perl/Python helper lack any documented conventions. C++ guidance is thin but present; shell/Perl/Python are uncovered. |
| 3 | Architecture + operational surface | 🟡 | Architecture is well-covered: `CLAUDE.md` has a component diagram (Loaders → IComposition → Walker → AlignmentTraversor) and `README.md` covers the v2.0 algorithm changes. Operational surface is largely missing: no environments (dev/prod/feature), no runtime hosting (this is a CLI distributed as a Docker image to `revdotcom/fstalign` on Docker Hub — never named in CLAUDE.md), no observability (the binary writes a JSON log per run but nothing about where logs go in production use), no build/deploy ownership pointer for the GitHub Actions `deploy.yml` workflow. |
| 4 | Database schema reference | ➖ | No relational data store; CLI tool operates on file inputs. N/A. |
| 5 | CONTRIBUTING.md | ✅ | `CONTRIBUTING.md` (42 lines) covers Gitflow workflow (`develop` base), forking, PR steps, code-of-conduct, licensing. Commit-message conventions are implicit. |
| 6 | .env.example | ➖ | No runtime env config. Only `OPENFST_ROOT` (build-time CMake input) and Docker `DEBIAN_BASE` build-arg, both documented in `CLAUDE.md` configuration table. N/A. |
| 7 | settings.json with pre-approved permissions | ❌ | No `.claude/` directory. No `.cursor/` config. No tracked or example `settings.json`. |
| 8 | Skills directory | ❌ | No `.claude/skills/`, no `skills/`, no plugin marketplace config. |
| 9 | Skill-creator skill | ❌ | None present and no skill infrastructure to host one. |
| 10 | Hooks for deterministic checks | ❌ | No `.githooks/`, `husky`, `.pre-commit-config.yaml`, or `.claude/settings.json` PostToolUse hooks. `clang-format` is run on-demand only. |
| 11 | Hooks for agent friction | ❌ | No PreToolUse / deny / validate hooks. |
| 12 | Tool-call telemetry | ❌ | No `CLAUDE_CODE_ENABLE_TELEMETRY` references, no OTLP config, no hook-driven log destination. |

## Tier 2 — DX & Speed

| # | Item | Status | Evidence / Notes |
|---|------|--------|------------------|
| 13 | Single-command local dev setup | 🟡 | Documented as 3 commands in `CLAUDE.md` (submodule init + cmake configure + cmake build) and ~4 commands in `README.md`. No `Makefile`/`justfile`/`Taskfile.yml`/`docker-compose.yml`. The host build also requires an out-of-tree OpenFST install referenced by `OPENFST_ROOT`. Docker path is `docker pull revdotcom/fstalign` — single command, but for *running*, not local dev. |
| 14 | Fast linting (<30s) | 🟡 | `.clang-format` config exists (Google base, 88-line config). `CLAUDE.md` documents `clang-format --dry-run --Werror src/*.cpp src/*.h` and references `clang-tidy`/`cppcheck`/`shellcheck`. **Not invoked in CI** (`.github/workflows/CI.yml` only builds Docker and runs tests). |
| 15 | Fast formatting (<20s) | 🟡 | `.clang-format` config exists. No pre-commit hook, no PostToolUse, no CI step enforcing it. |
| 16 | Fast compilation / type checking (<45s) | 🟡 | CMake build runs in CI inside the Docker image (median 6m32s for full pipeline including OpenFST build + tests across `bullseye` + `bookworm` matrix). Not a separable typecheck step, and full CI well exceeds 45s — but the C++ compile is the typecheck. |
| 17 | Reliable tests | ✅ | Catch2 framework, 3 test files (`fast-d-tests.cc`, `fstalign_Test.cc`, `compose-tests.cc`) + shared utilities in `test/`. Run via `ctest --test-dir build` or `make test`. CI runs them on every push on both Debian bases. No SKIP / xfail blocks visible. |
| 18 | Fast CI (parallelize, cache, smart ordering) | 🔗 | GitHub Actions. `.github/workflows/CI.yml` triggers on `push` (matrix bullseye/bookworm). See External Validation. |
| 19 | Branch previews | ➖ | CLI tool with no UI surface. N/A. |

## Tier 3 — Advanced

| # | Item | Status | Evidence / Notes |
|---|------|--------|------------------|
| 20 | Skills marketplace | ❌ | No `.claude/settings.json`, no marketplace, no plugin enablement. |
| 21 | Specs as code | 🟡 | `docs/json_log_schema.json` (98 lines) is a real machine-readable JSON Schema for the JSON output log. NLP/CTM/Synonyms input formats are documented as prose only (`docs/NLP-Format.md`, `docs/Synonyms-Format.md`, `docs/Usage.md`). No codegen wired to the JSON schema. No OpenAPI/Protobuf/GraphQL (correctly — CLI tool, no external API). |
| 22 | MCP server configs | ❌ | No `.mcp.json`, no `.claude/settings.json` `mcpServers`, no MCP-usage doc. |
| 23 | Agentic code review | 🔗 | Nothing configured locally (no `.cursor/`, `.coderabbit.yaml`, `.greptile*`, no `claude.yml` workflow). Phase 2 found Copilot's PR reviewer on 1/20 PRs — see External Validation. |
| 24 | Other deterministic PR guardrails | 🔗 | Locally: stale-branch and stale-PR cleanup workflows (`stale-branches.yml`, `stale-prs.yml`) + Lychee link checker (`links_fail_fast.yml`) + copyright auto-bumper (`copyright-update.yml`). No semgrep, no dependabot, no gitleaks, no Chromatic, no secret scanner. Phase 2 also found CodeQL running on a subset of PRs from an org-level install — see External Validation. |
| 25 | Agents in the cloud | ❌ | No `claude.yml` / `claude-code-review.yml` / Devin / Replit / Modal config. |
| 26 | Slack + issue tracker integration | 🔗 | `FSTALIGN-*` Jira-style ticket prefix is used heavily — see External Validation. No Slack MCP config, no PR template referencing ticket IDs. |

## External Validation (Phase 2)

### CI Timing (item 18)

| Workflow | Recent runs (develop) | Median | p95 | Success rate | Last run |
|---|---|---|---|---|---|
| `CI.yml` | 7 (push) | 6m32s | 6m54s | 7/7 (100%) | 2026-05-08 |
| `copyright-update.yml` | 1 (schedule) | 0m13s | 0m13s | 1/1 | 2026-01-01 |
| `deploy.yml` | 0 | — | — | — | — (release-triggered only) |
| `links_fail_fast.yml` | 7 (push) | 0m18s | 0m25s | 3/7 (43%) | 2026-05-08 |
| `stale-branches.yml` | 20 (schedule) | 0m10s | 0m20s | 20/20 (100%) | 2026-05-12 |
| `stale-prs.yml` | 20 (schedule) | 0m11s | 0m18s | 20/20 (100%) | 2026-05-12 |

CI total pipeline is 6m32s median — **OK** band (5–10 min). No caching of OpenFST build observed; rebuilt from scratch on every run.

**Side findings:**
- `deploy.yml` is healthy (release-triggered, fires only on `release: published`). Its zero recent runs reflect a 16-month gap since the last tagged release, not a broken workflow.
- `links_fail_fast.yml` (Lychee broken-link checker) failed 4 of the last 7 runs on develop. This is a guardrail with a high failure rate that is presumably non-blocking (it would otherwise block all merges to develop). Belongs in Residual Unknowns.
- `copyright-update.yml` is `schedule`-only (annual cron), one run in the last 30 days — operating normally.
- `CI.yml` triggers only on `push` and `workflow_dispatch`, not `pull_request`. Check-runs therefore land on the post-merge push to develop rather than on PR head SHAs, which depresses the per-PR check-run rate (see item 24 below).

### Agentic Code Review Activity (item 23)

| Tool | Status | Evidence |
|---|---|---|
| (none configured locally) | — | No `.cursor/`, `.coderabbit.yaml`, `.greptile*`, `claude.yml` workflow. |

**Undiscovered-locally:**

| Tool | Status | Evidence |
|---|---|---|
| `copilot-pull-request-reviewer[bot]` | Sporadic | Found on PR #64 reviews API (1/20 PRs = 5%). Likely an org-level GitHub Copilot Code Review install enabled on this repo. Not configured in `.github/`. |

### PR Guardrail Firing (item 24)

| Guardrail | Status | Hit rate on last 20 PRs (develop) |
|---|---|---|
| `linkChecker` (Lychee) — local config | Sporadic | 4/20 check-run appearances |
| Stale-branches / stale-PRs (org hygiene) | Verified firing (scheduled) | 20/20 scheduled runs in last 30 days |
| `CodeQL` `Analyze (python)` | Sporadic (org-level) | 5/20 PRs |
| `CodeQL` `Analyze (actions)` | Sporadic (org-level) | 4/20 PRs |
| `Upload Results` (CodeQL companion) | Sporadic | 2/20 PRs |

Hit rates are depressed by the `push`-only CI trigger and a 20-PR sample that spans 2023–2026 (~3 years), during which various org-level guardrails were rolled out at different times. Recent PR #64 (2026-05-08) has the cleanest check-run set; rates against just the last 5 PRs (2026 only) would be much higher. **No commit statuses observed on any sampled PR** — all guardrails report via check-runs.

**Undiscovered-locally:**
- CodeQL is firing from an org-level install with no `.github/codeql/` config in this repo.

### Cloud Agent Usage (item 25)

| Workflow | 30d runs | 7d runs | Success rate | Last invoked |
|---|---|---|---|---|
| (none configured) | — | — | — | — |

### Issue Tracker Linkage (item 26)

- Ticket linkage rate: **10/20 recent PRs** reference a ticket
- Tracker inferred: **Jira** (`FSTALIGN-*` prefix on 16 matches across 8 PRs; `NERD-3467` on PR #64 — secondary prefix from a cross-team contributor)
- PR template references ticket ID: **no** (no `.github/PULL_REQUEST_TEMPLATE.md`)
- Suffixed/rework IDs observed: 1 (`FSTALIGN-37-1` on PR #52 — a rebase of `FSTALIGN-37`)

10/20 is below the typical Verified threshold (≥70%). Several merged PRs are auto-PRs (`Update license copyright year(s)` × 3) or maintenance ones that have no ticket and reasonably should not. Filtering those out lifts the rate to roughly 10/15 = 67%.

## Known External Systems (captured, not validated in this version)

| Category | System | Config location | Notes |
|---|---|---|---|
| Container registry | Docker Hub (`revdotcom/fstalign`) | `.github/workflows/deploy.yml`, README | Release-triggered Docker image publish to Docker Hub. Last release per `deploy.yml` history is from before 2026. |
| Issue tracker | Jira (`FSTALIGN-*`) | PR titles + branch names | No webhook/GitHub app traces in this repo's config; assumed an org-level Jira ↔ GitHub install. |
| External library | OpenFST 1.7.2 | `ext/openfst-1.7.2.tar.gz`, `Dockerfile`, `CMakeLists.txt` | Bundled tarball, not a submodule. Pinned. |

## Residual Unknowns

- **`links_fail_fast.yml` failing 4/7 recent runs on develop** — failing-but-non-blocking guardrail (would otherwise block all develop merges, but PRs continue to merge). Worth a quick triage: either a broken link in the tracked Markdown is being ignored, or the workflow's `fail: true` is being honored only on PR runs.
- **`CI.yml` triggers only on `push`** (not `pull_request`) — PR head SHAs never receive `build` check-runs, only the post-merge develop push does. This is the root cause of the low check-run hit rate on the PR sample, and is also why merge-blocking signal is weaker than it could be. Easy fix: add `pull_request: { branches: [develop] }` to the workflow trigger list.
- **No CI cache for OpenFST** — the OpenFST build runs from scratch on every CI run (it's a Docker stage and not cacheable across runners without explicit configuration). This is the dominant cost in the 6m32s CI median.
- **Tools/ scripts (Perl, Python, Shell) have no documented style or test coverage.** `tools/gather_runtime_metrics.sh` and `tools/sbs2fst.py` could break silently — `shellcheck` is mentioned in `CLAUDE.md` but isn't wired anywhere.
- **`CLAUDE.md` "Current focus" section is empty** — has the touched-by line but no narrative. Drift risk: the file says `last touched 2026-03-12` but the actual most-recent CLAUDE.md edit (in the May 8 doc-bootstrap PR #64) is more recent.
- **No PR template** — combined with the 50% raw ticket linkage rate, this is the obvious leverage point if Jira-linkage is meant to be enforced.

## Detailed Findings

### Tier-1 cluster profile

Knowledge (items 1–6, applicable: 1, 2, 3, 5): 2 PASS / 2 PARTIAL / 0 FAIL → `pass_pct` = 50%, `fails` = 0.
Tooling (items 7–12): 0 PASS / 0 PARTIAL / 6 FAIL → `pass_pct` = 0%, `fails` = 6.

The split is sharp — the tooling cluster is uniformly empty (no `.claude/`, no hooks, no telemetry, no MCP) and the knowledge cluster is uniformly thin-but-present. But the knowledge `pass_pct` of 50% is below the 80% threshold for "knowledge-strong", so the named-profile annotation does NOT fire. The honest read is **"both clusters have room to grow, and the tooling cluster is the easier lift at this scale."**

### Multi-author, low-velocity context

PR #64 (4 days ago) is the first PR in 4 months; before that, the cadence is roughly one PR every 2–4 months going back to 2023. The repo is a stable C++ CLI more than an actively-developed codebase. Several Tier 3 items (item 25 cloud agents, item 22 MCP) score FAIL but are realistically optional at this scale and cadence. The Top 3 Next Actions reflect that — they target the highest-leverage cheap wins (clang-format-in-CI, operational-surface in CLAUDE.md, a minimal `.claude/`) rather than mandating the full Tier 3 surface.
