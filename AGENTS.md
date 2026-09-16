# AGENTS.md — SynData repository instructions

These instructions apply to the entire repository.

## Authority

For implementation work, read and reconcile in this order:

1. the active GitHub issue;
2. `RAG.md`;
3. this file;
4. current repository documentation;
5. current `main` and accepted predecessor implementations/tests.

The active issue defines immediate scope. `RAG.md` defines durable architecture/product contracts. Existing tested behaviour on `main` is authoritative unless the issue intentionally changes it.

## Product constraints

SynData is a deterministic, provenance-first synthetic-data generation and calibration engine. Preserve these constraints unless an explicit accepted repository decision changes them:

- C++20/CMake baseline;
- Windows x64 first-class local target and Linux x64 first-class headless/CI target once SD-001 establishes the baseline;
- no telemetry, account or network runtime requirement;
- deterministic model-owned randomness only;
- random-access sample identity independent of worker scheduling/resume boundaries;
- canonical/versioned recipe/profile/job/manifest semantics;
- explicit latent truth, observations and annotations rather than treating rendered pixels as truth;
- domain-specific assumptions belong in domain modules/profiles, not generic validation/execution code;
- customer/proprietary process knowledge is configuration/profile data, not hard-coded engine folklore;
- caches are disposable; manifests/recipes/profiles and declared provenance are authoritative;
- malformed/unsupported versions fail explicitly rather than being partially reinterpreted;
- no claim of statistical representativeness without an explicit real-vs-synthetic comparison scope;
- no dynamic plugin ABI until a future accepted issue deliberately specifies one.

## ArtMiner ancestry

The initial deterministic source seam comes from `techrote/artminer` commit:

`57cb37526b708a9b88b9dbd82a88a1d7b394e152`

Use that immutable checkpoint when SD-001 or later provenance work needs to establish inherited contracts. The relevant upstream document is `docs/fork-readiness.md` at that commit. Do not silently track moving ArtMiner `main` as SynData ancestry.

## Engineering rules

- Prefer small, explicit semantic contracts over framework-heavy abstractions.
- Keep the deterministic engine independent of UI state and platform UI code.
- Generic graph validation must not contain customer/domain node-name special cases; express domain legality through registered metadata/constraints or domain-owned validation hooks explicitly permitted by the architecture.
- Stable ordering must not depend on worker completion order, pointer addresses, unordered-container iteration, locale, filesystem enumeration or UI timing.
- Treat every semantic change as a versioning/migration/test question.
- Validate bounds before expensive allocations or work.
- Transactional dataset/sample output must not advertise incomplete artifacts as complete.
- Do not make a GPU/accelerated path the sole semantic authority; preserve a canonical/reference path or documented reference equivalence contract.
- Do not introduce a third-party dependency without documenting licence, deterministic/semantic impact, deployment cost, portability, security/maintenance and why project-owned/platform code is insufficient.
- Avoid broad unrelated refactors in milestone PRs.

## Confidential data discipline

Profiles and calibration/reference datasets may contain proprietary information.

- Do not add sample customer data, real proprietary profiles or confidential imagery to the repository.
- Do not log full private profile contents by default.
- Shareable manifests should normally record fingerprints/safe metadata, not embed complete confidential profiles.
- Tests use synthetic/publicly-created fixtures committed specifically for testing.
- Do not add automatic uploads, analytics or network calls.

## Tests and CI

Every issue must add or update tests that prove its acceptance criteria. Preserve deterministic vectors/goldens unless the issue intentionally versions a semantic change and documents it.

Do not obtain green CI by:

- deleting relevant tests;
- weakening assertions/tolerances without technical justification;
- suppressing compiler errors/warnings globally;
- skipping deterministic/replay checks;
- disabling jobs or required checks;
- replacing expected outputs merely because implementation output changed.

## Branch / PR / merge protocol

For every implementation issue:

1. start from current `main` after inspecting accepted predecessors;
2. create a focused branch;
3. implement the complete issue, including tests and documentation reconciliation;
4. run relevant local checks where available;
5. open a focused PR linked to the issue;
6. inspect CI and repair failures rather than bypassing them;
7. merge the PR only when all required automated checks pass and acceptance criteria are actually met;
8. verify the merge/change is present on `main` after merging;
9. close the issue only after that verification.

The repository owner has authorized autonomous agents following issue prompts to merge their own PRs after required checks pass. If external infrastructure prevents required checks from completing, leave the PR/issue open with evidence rather than claiming completion.

## Documentation discipline

`RAG.md` is the authoritative architecture/product retrieval document. Update it when an implementation intentionally changes a durable architecture contract, milestone sequence or product behaviour.

Focused design/format documents may be added as implementation detail, but they must not silently contradict `RAG.md`.

A future `README.md` is user/developer orientation, not a competing architecture specification.
