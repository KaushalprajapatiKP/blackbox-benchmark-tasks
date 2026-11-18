# Checking difficulty with many runs

What I have now is one cold run per task. That shows the tasks are long (67 to 235 tool calls)
and that they can be failed for the intended reasons; it is not a difficulty estimate. With
many runs available I would do the following.

1. **Runs.** At least 5 runs per task for each of 3 to 4 agents spanning capability, for
   example a small open model, a mid-tier model and two frontier models. Fixed agent timeout,
   same harness.
2. **Pass rate with uncertainty.** Report pass@1 and pass@5 with Wilson 95% intervals. With 5
   runs an observed 1 / 5 covers roughly 4% to 62%, so single tasks are only binned (never,
   sometimes, usually, always solved), not ranked.
3. **Target band.** A useful task is solved sometimes by the strongest agent and rarely by
   weaker ones. A task solved 0 / N by every agent is suspect until a human or the reference
   proves it fair. A task solved N / N by the weakest agent is retired or hardened.
4. **Per-case failure histogram.** For every failed run, record which hidden cases failed.
   - Failures spread across the intended behaviour families point to real difficulty.
   - The same single case failing for every agent, including strong ones, is a red flag for
     ambiguity or a wrong answer key, so audit it before counting it.
5. **Effort profile.** Tool calls, tokens and wall time per run, and the point where runs stall.
   This separates "hard to discover" from "slow to type".
6. **Stability.** Rerun the reference twice and the verifier on the same submission twice, so
   grading itself contributes no variance. Each task here already records two identical oracle
   runs.
7. **Sensitivity.** Mutation testing of the reference (one behaviour changed at a time) shows
   which behaviours the suite can detect at all. The COBOL and Fortran tasks in the companion
   repo do this (54 and 26 mutants, all caught).
8. **Human anchor.** Time one or two engineers on a subset, to map turn counts onto human
   effort.
