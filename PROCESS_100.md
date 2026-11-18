# Producing 100 tasks at steady quality

**Goal:** about 100 Harbor tasks built from real agent failures, each fair, verified and
discriminating, with quality that does not drift as authors and volume grow.

## 1. Intake (from failures, not ideas)
- Every task starts from a logged failure: a trajectory, the failing behaviour, and a one-line
  hypothesis for the category (for example "verification limited to own hypotheses").
- Keep a category ledger. Cap any one category at about 15% of the set, and require a new task
  to differ from existing ones in domain and mechanism.

## 2. Template (every task has the same shape)
- `instruction.md` states the outcome contract, tolerances and grading mechanics, never the
  defects or the plan.
- `task.toml` carries difficulty, solution and verification explanations, timeouts and the author.
- `environment/` is multi-stage; no answer material reaches the agent image.
- `solution/`, `tests/` (seeded or recorded hidden suite), `cheats/` (at least 3: proxy, hardcode,
  docs-only baseline), `authoring/` (scripts that regenerate the suite) and a README with a
  validation table.

## 3. Automated gates (CI, before any human looks)
1. **Lint and layout:** required files, no secrets, no stray build output, pinned image digests
   and package versions.
2. **Leak scan:** the built agent image contains no reference source, answer files or generators.
3. **Docker runs:** oracle = 1 twice, nop = 0, every cheat = 0, and the docs-only baseline's
   pass count recorded.
4. **Mutation run:** one-behaviour mutants of the reference; at least 90% must be killed.
5. **Determinism:** recordings repeated three times; any case that differs is dropped.
6. **Similarity check** against every existing task (instruction, solution and tests embeddings);
   near duplicates are rejected.

## 4. Calibration
- 5 cold runs each from a mid-tier and a frontier agent.
- Accept if the frontier agent's pass rate is in the target band and failures land on the
  intended behaviour families.
- A case that every agent fails goes to an ambiguity audit.
- Tasks solved in very few tool calls go back for redesign; the usual cause is that the
  statement or the docs carry the plan.

## 5. Human review (two reviewers, fixed rubric)
- **Rubric:** fairness (every graded behaviour observable or stated), grader correctness,
  leakage, difficulty locus (the hard part is the intended capability), and statement clarity.
- **Blocking findings** go back to the author with one concrete fix each. Two rounds maximum,
  then the task is dropped.
- One reviewer per batch re-runs a random task end to end, to audit the gates themselves.

## 6. Metrics (weekly)
- **Throughput:** accepted tasks per author per week, and rework rounds per task.
- **Gate failures by type:** which automated check fails most is where the template or
  training needs work.
- **Reviewer agreement:** the rate at which the two reviewers agree; drift prompts a
  calibration session.
- **Post-release defect rate:** tasks later found ambiguous or broken; target under 3%.
- **Set balance:** pass-rate distribution across the set, and the category mix.

## 7. Release
- Keep the hidden suites and references private.
- Rotate seeds per release.
- Retire tasks that every agent saturates.
