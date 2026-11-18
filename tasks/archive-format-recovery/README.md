# archive-format-recovery

Failure category: hypothesis-driven investigation.

## Statement summary

The image ships `/app/bin/vpack`, a stripped archiver written for this task (C, MIT, about 850
lines, source in `environment/vpack/`, not present in the image). It only supports
`vpack pack DIR OUT`. The archive format has no documentation; `/app/README.txt` has one paragraph
on what the tool does, and `/app/samples` has five archives next to their source trees. The agent
must write `/app/unvpack ARCHIVE OUTDIR` that recreates the packed tree exactly (names as bytes,
file contents, empty directories, symlink targets, all 12 permission bits, nanosecond mtimes on
every entry including symlinks and `OUTDIR`) and that exits 1 with `corrupt` on stderr for damaged
archives.

## Environment

- `environment/Dockerfile`: stage 1 `gcc:12.5.0-bookworm` compiles and strips `vpack.c`; stage 2
  `python:3.12-slim-bookworm` with `pytest==8.3.4`, the binary, the README and the samples.
  Samples are generated during the build by `environment/mk_samples.py` (bind-mounted, not left in
  the image) and packed with the built binary, so they always match it.
- `tests/data/`: 68 valid archives, 48 damaged archives and `manifests.json` (expected tree per
  archive, from `lstat` of the real source trees). Produced once at authoring time by
  `authoring/gen_hidden.py` (seed 20261117) inside the built image.
- `tests/test.sh` deletes `/app/bin/vpack`, then runs `tests/test_unvpack.py` (117 tests). Each
  archive is copied to a fresh temp dir before `/app/unvpack` sees it. Reward 1 only if all pass.

## The format (private; for reviewers)

Header: `89 'V' 'P' 'K'`, version 3, flags (bit0 name table, bit1 compressed table), zigzag base
seconds, root mode XOR 0755, root time as `(sec-base)<<1 | has_ns` then ns. Data area: blobs of
non-empty, non-shared files in entry order. Table (optionally LZ-compressed with a raw-length
prefix): optional name table of names used 2+ times, entry count, entries
`kind(type|ns|lz|shared|nameref)`, parent (0 = root, k = entry k-1), name or name index, mode XOR
0644/0755 (not for links), seconds minus base, optional ns, then for files size + (shared entry
index | [clen if LZ] + CRC32 LE), for links target. Footer: u64 table offset, u32 table length,
CRC32 of header bytes followed by table bytes, `1a 'K' 'P' 'V'`. Varints are big-endian 7-bit
groups with a +1 bias per continuation byte (git offset style). LZ: 16 KiB blocks, each
`varint(clen<<1 | raw)`; window of 64 KiB carried across blocks; LSB-first control byte; short
match 2 bytes (len 3..10, dist 1..4096), long match 3 bytes (7-bit len code, 16-bit LE dist),
len code 127 adds a varint extension; overlapping matches allowed.

## Difficulty explanation

Independent things the agent has to get right, each found by building its own trees, packing
them and diffing archives (the binary is the only source of truth):

1. Biased big-endian varint (only visible for values >= 128; plain LEB128 reads samples wrongly).
2. Zigzag base time and per-entry offsets (negative mtimes only appear if the agent tries them).
3. Root mode and mtime live in the header with a different ns encoding than entries.
4. Modes are XORed with a type-dependent default (default modes encode as 0).
5. Nanosecond flag bit and the extra varint (all sample mtimes are whole seconds).
6. Optional shared name table, only when a basename repeats (never in samples).
7. Optional LZ-compressed directory table above 512 bytes (never in samples).
8. Content sharing for identical files: no data, no CRC, a reference instead, and the data area
   skips it (never in samples).
9. Block structure: 16 KiB blocks, raw blocks inside a compressed entry, window spanning blocks
   (all sample files are single-block, no raw blocks).
10. Two match encodings plus the extended-length varint (samples contain no matches longer than
    130 and no distances above 4096).
11. Overlapping copies (dist < len).
12. Symlinks: inline targets, no mode, mtime restored without following the link.
13. Directory mtimes restored after their children are written; `OUTDIR` attributes too.
14. Names and link targets are raw bytes (non-UTF-8, newlines, 255-byte names, ~3300-byte paths).
15. Corruption: per-entry CRC32, footer CRC over header plus table, footer magic and table
    location consistency; every failure path must exit 1 with `corrupt`, not a traceback.

The instruction states only the outcome contract and where the truth lives. The samples are the
easy regime (verified: flags 0, no name table, no compressed table, no shared data, no ns, no
negative times, no multi-block files, no extended lengths, no long distances).

Measured near-miss decoders (each is the reference with one change, run against the grader):

| Mutant | Tests failed of 117 |
|---|---|
| varint without the +1 bias | 68 |
| mode stored as absolute, not XOR | 66 |
| ignore the ns flag | 44 |
| no directory mtime pass | 49 |
| no root (`OUTDIR`) mode/mtime | 39 |
| symlink mtimes not set | 33 |
| no footer CRC check | 10 |
| no per-entry CRC check | 3 |
| no trailer magic check | 3 |

## Shortcuts and how the grader stops them

| Cheat (`cheats/`) | Idea | Result |
|---|---|---|
| `stored_only.sh` | Full container decoder, no LZ (rejects compressed entries/tables) | 8 of 68 valid archives pass (4 `empty_*`, 3 `incompressible_*`, `symlinks_03`); reward 0 |
| `shell_out.sh` | Call `vpack unpack/x/extract` | vpack is deleted and has no unpack mode; 0 of 68 valid; reward 0 |
| `sample_lookup.sh` | Hash the archive, copy the matching sample source tree | Hidden archives are different trees; 0 of 68 valid; reward 0 |

Cheats that always print `corrupt` pass the 48 damaged-archive tests, which is why reward is
all-or-nothing over the valid set as well.

## What it does not measure

Ownership, atime, xattrs, hard links, special files (vpack skips them), unpack speed beyond the 60 s
per-archive limit, and safety against malicious archives (path traversal is impossible by
construction; names containing `/` are rejected by the reference but not tested).

## Validation (real Docker runs, harbor 0.25.0, 2026-11-17)

| Command | Outcome |
|---|---|
| `harbor run -p archive-format-recovery -a oracle` (run 1) | reward 1.0, 117 passed |
| `harbor run -p archive-format-recovery -a oracle` (run 2) | reward 1.0 |
| `harbor run -p archive-format-recovery -a nop` | reward 0.0 |
| cheat `stored_only.sh` as solve.sh, `-a oracle` | reward 0.0 (60 failed, 57 passed) |
| cheat `shell_out.sh` as solve.sh, `-a oracle` | reward 0.0 (68 failed, 49 passed) |
| cheat `sample_lookup.sh` as solve.sh, `-a oracle` | reward 0.0 (68 failed, 49 passed) |
| `authoring/gen_hidden.py` | every valid archive round-trips through the reference decoder (strict and lenient) to the recorded manifest; every damaged archive is rejected by both |
| reference decoder on the 5 samples | each output equals its sample source tree |


