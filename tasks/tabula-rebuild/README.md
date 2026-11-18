# tabula-rebuild

## Statement summary

The agent gets an execute-only, stripped, static binary `/app/bin/tabula` (a
CSV/TSV table processor with 26 options), a user-level man page
`/app/docs/tabula.1.md`, and `tabula --help`. It must write a clean-room Python
3 (stdlib only) reimplementation at `/app/rebuild/tabula` that matches the
original's stdout, stderr and exit code byte for byte for any invocation. The
man page covers the common regime; the edge rules are only observable by
running the binary.

Failure category: long-tail exactness under black-box discovery.

## Environment

- `environment/src/tabula.c`: the original tool (about 1,900 lines of C, MIT, written
  for this task). It is compiled in a separate build stage
  (`gcc:12-bookworm`, pinned by digest) with `-O2 -static -s`. Only the
  binary is copied into the agent image, as `/app/bin/tabula`, mode 0111,
  owned by root. The source never reaches the agent image.
- Agent image: `python:3.12-slim-bookworm` (pinned by digest), `pytest==8.3.3`,
  the man page, an empty `/app/rebuild/`.
- The agent runs as root, so file permissions cannot stop it from reading the
  binary. The instruction forbids reading, copying, decompiling or
  disassembling it, and the grader enforces what it can (see below).
- `authoring/gen_cases.py` (seeded, `random.Random(20261117)`) generates the
  case specs and `authoring/run_cases.py` records them against the binary or
  compares a candidate. Neither is part of the Harbor task.

## Difficulty explanation

The hidden suite has 749 invocations in 13 categories (cli 72, parse 52,
header 33, select 51, where 102, where_err 60, sort 63, group 96, numfmt 48,
quote 39, pretty 44, pipeline 29, mixed 60). Reward is all-or-nothing.
These are independent rules the agent has to find by probing, because the man page
either says nothing about them or only names the feature:

1. Option parsing: GNU-style permutation, unique-prefix abbreviation (exact
   match wins), the exact ambiguity message listing every candidate, `=` on a
   flag, attached short values, clusters, `--`, an extra operand reported the
   moment it is seen, and value errors reported in argv order before conflict
   checks.
2. Value validation: a digit-only grammar with at most 9 digits (`+2` and
   `-1` are rejected), precision 0 to 9, `--max-width` at least 2, delimiter
   words (`tab`, `\t` only), and the fixed order of five conflict checks.
3. Numeric literal grammar: optional sign, at most one dot, ASCII digits only,
   **at most 15 digits**. So `1e3`, `0x10`, ` 7` and a 16-digit integer are
   text, while `5.`, `.5`, `+3` and `-0` are numbers. This affects filters,
   sorting, aggregates, formatting, quoting and alignment.
4. Input reader: BOM stripping, skipping lines that are completely empty (but
   not `""`), keeping a lone CR as data, multi-line quoted fields with
   line numbers taken from the start of the record or field, an error for any
   character after a closing quote, NUL rejection, and padding or cutting
   ragged rows unless `--strict` is given.
5. Headers: an empty header name becomes `c<N>`, the first duplicate name
   wins, `-H` suppresses header output, and an empty input is an error unless
   `-H` is given.
6. Column lists: a numeric item is always an index even if a column has that
   name, `N-`, `-M` and reversed ranges, error precedence, and the error
   message echoes the number as it was written (`007`).
7. Filter semantics: a comparison is numeric only if both sides parse as
   numbers (a quoted `'10'` counts), otherwise it is a bytewise compare. A
   bare operand is true when it is non-empty and not numerically zero. An unknown bare
   identifier becomes a string literal, but an unknown backquoted name is an
   error. Keywords are case-insensitive, and multiple `-w` are combined with AND.
8. Filter diagnostics: the whole expression is tokenized before parsing,
   byte offsets are reported, and there are messages for each failure class
   (unterminated string or name, invalid character in printable and hex forms,
   unexpected token text, unexpected end, expected `)`, invalid `$`
   reference).
9. Sorting: stable. Under `n`, non-numbers always go after numbers and keep
   input order, even with `r`. Natural order compares digit runs by value and
   puts digits before non-digits, with leading-zero ties left in input order.
   `i` is an ASCII-only fold. Grouped output is sorted by its own column names.
10. Aggregates: sums are exact decimals that keep the largest input scale
    (`0.10+0.2-0.30` gives `0.00`). Averages are exact, rounded half-even to 6
    places, with zeros stripped. min and max compare numerically only when
    every non-empty value is numeric, and return the original text.
    count/distinct ignore empty values, while `count(*)` counts rows. Groups appear
    in first-appearance order. `-a` without `-g` returns one row even for an empty
    input, and `-g` without `-a` adds an implicit `count`.
11. Number formatting: four rounding modes on exact decimals, default
    half-even, no negative zero, thousands grouping, re-rendering of every
    numeric data cell (group keys and counts included, header excluded), and
    formatting before `--empty` substitution.
12. Output quoting: minimal quoting triggers on the delimiter, a quote, CR, LF,
    or leading or trailing blank, and on an empty field in a single-column table.
    `nonnumeric` and `none` have their own rules (`none` errors with the output
    line and column).
13. Pretty table: alignment (right only if every non-empty cell is numeric),
    minimum width 1, ` | ` and `-+-` separators, trailing blanks
    stripped, caret notation for control characters (width 2), a specific
    wide and zero-width code point table, byte-level handling of invalid
    UTF-8, and `--max-width` truncation with `…` that never splits a wide
    character.
14. Pipeline order: filter, group, sort, select, unique, skip, limit, count,
    format, empty substitution, write.

Cost to begin: the source is not available and nothing in the environment
says which edge cases matter. A run that matches the man page is not enough;
the agent has to design its own differential probes against the binary. The
basics-only rebuild below passes 39% of the suite.

## Shortcuts and how the grader stops them

| Shortcut | Grader defence | Observed |
|---|---|---|
| Wrapper that execs `/app/bin/tabula`, with a zlib-compressed copy stashed in `/opt` as a fallback (`cheats/cheat_wrapper.sh`) | Before any case runs, the verifier deletes `/app/bin/tabula` and every file on the filesystem that contains the binary's embedded identification string or matches its sha256. Each case runs as `python3 -I -S sandbox_run.py`, which installs an audit hook that blocks `os.exec*`, `subprocess.Popen`, `os.posix_spawn`, `os.spawn*`, `os.system`, `os.fork`, ctypes, and imports of `_posixsubprocess`, `_ctypes` and `cffi`. | reward 0, 0/749 (the compressed copy survives deletion, but its exec is blocked) |
| Byte copy of the binary shipped in `/app/rebuild` with a launcher (`cheats/cheat_copied_binary.sh`) | ELF, `.so` and binary-copy scan of `/app/rebuild`, plus deletion and the audit hook | reward 0 (forbidden-file test fails; 0/749) |
| Rebuild written from the man page and `--help` only, with exact help and version text (`cheats/cheat_basics_only.sh`) | All-or-nothing over 749 tail-heavy cases | reward 0, **295/749 pass** (cli 19/72, parse 22/52, header 20/33, select 18/51, where 55/102, where_err 2/60, sort 31/63, group 35/96, numfmt 6/48, quote 26/39, pretty 11/44, pipeline 20/29, mixed 30/60) |

Residual risk: the verifier shares the agent's container, so a determined
agent with root could tamper with the Python standard library or build its own
x86/arm emulator. Running with `-I -S` removes `sitecustomize`, `.pth` and user-site hooks, but the
grader does not verify the integrity of the standard library. Reading the binary's bytes is forbidden by the
instruction but cannot be technically prevented for root.

## What this task does not measure

- Code quality, speed or structure of the rebuild. The grader only checks
  bytes.
- Behaviour outside the recorded cases, such as numeric overflow at the
  extremes of the 15-digit grammar, inputs larger than a few KB, and
  permission-denied errors (the agent runs as root).
- Reverse-engineering skill in the disassembly sense. That is forbidden here.

## Validation

All runs are real Docker runs on Harbor 0.25 (darwin/arm64 host, Docker 29.8.0).

| Command | Outcome |
|---|---|
| `harbor run -p tasks-hard/tabula-rebuild -a oracle` (run 1) | reward 1.0, 749/749, verifier 65 s, total 1m39s |
| `harbor run -p tasks-hard/tabula-rebuild -a oracle` (run 2) | reward 1.0, 749/749 |
| `harbor run -p tasks-hard/tabula-rebuild -a nop` | reward 0.0 (rebuild missing, 0/749) |
| scratch copy with `solution/solve.sh` = `cheats/cheat_wrapper.sh`, `-a oracle` | reward 0.0, 0/749 |
| scratch copy with `solution/solve.sh` = `cheats/cheat_copied_binary.sh`, `-a oracle` | reward 0.0, forbidden-file test failed, 0/749 |
| scratch copy with `solution/solve.sh` = `cheats/cheat_basics_only.sh`, `-a oracle` | reward 0.0, 295/749 |
| `authoring/run_cases.py compare tests/cases.json /app/bin/tabula` inside the built task image | 749/749: the shipped binary reproduces the fixture exactly |


