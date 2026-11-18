# Leakage note

## Ways to pass without solving, and what stops each

| Shortcut | Applies to | Guard |
|---|---|---|
| Run the original program at grade time: proxy to it, embed a copy (renamed, base64), or exec it from the replacement | queue server, tabula | The verifier deletes the original and every file with the same SHA-256, then runs the submission under a Python audit hook that refuses process creation, `ctypes` and native extension loading. `cheats/proxy_binary.sh` and `cheats/cheat_wrapper.sh` both score 0 |
| Call the archiver to do the work | archive | `vpack` only packs; there is no unpack mode to call. It is also deleted before grading. `cheats/shell_out.sh` scores 0 / 68 |
| Hardcode the visible examples | all three | Hidden cases are disjoint from every visible example. The hardcoding cheats score 0 / 446, 0 / 68 and 0 / 749 |
| Map an archive back to a known sample tree | archive | The hidden archives are built from different seeded trees. `cheats/sample_lookup.sh` scores 0 / 68 |
| Read the hidden suite during the run | all three | `tests/` is copied into the container only at verification time, after the agent has finished |
| Read the program instead of probing it (`strings`, disassembly) | all three | Forbidden in the statement but not enforceable. `strings` shows message texts, not rules, defaults, orderings or the format. This is the largest residual risk |
| Tamper with the verifier (patch Python or pytest inside the container) | all three | Not guarded: the verifiers share the agent's container so they can delete the binary in place. A separate verifier container that receives only the submission would close it |
| Training-data contamination: tasks, solutions and hidden suites are public in this repo | all three | Not guarded here. For real scoring I would keep the hidden suites and references private and regenerate the suites with new seeds per release (the `authoring/` scripts already do this) |

## The check added

**Delete-by-hash plus an audit hook** (queue server and tabula). Deleting `/app/bin/<tool>`
alone is not enough, because an agent can hide a copy anywhere, under any name, or in an
encoded form that it unpacks at run time. So the scrub hashes the original and removes every
byte-identical file on the filesystem. The replacement then runs in an interpreter where any
attempt to start a process or load native code raises immediately.

The queue-server cheat that hides a base64 copy inside the script gets past the scrub, because
the copy is not byte-identical. The hook then stops it (`blocked by grader: os.exec`) and it
scores 0 / 446.

## One thing these tasks do not measure

Code quality. A replacement can be one 2,000-line function and still score 1. The tasks
measure whether the agent can discover and reproduce behaviour exactly, not whether the result
is maintainable, fast beyond the stated limits, or well tested.
