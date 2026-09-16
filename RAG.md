# SynData — RAG / authoritative implementation plan

> **Status:** authoritative architecture and ordered implementation plan.  
> **Repository:** `techrote/syndata`.  
> **Product/engine name:** **SynData**.  
> **Purpose:** a deterministic, provenance-first synthetic-data generation and calibration engine that can serve multiple image/data domains without embedding one customer's process assumptions into the core.  
> **Source ancestry checkpoint:** `techrote/artminer` commit `57cb37526b708a9b88b9dbd82a88a1d7b394e152` (merged AM-016 fork-readiness checkpoint).

This file is intended to be sufficient retrieval context for autonomous implementation agents. `AGENTS.md`, the active GitHub issue, current `main`, accepted predecessor implementations/tests, and this file are authoritative. If they conflict, reconcile the conflict explicitly in the implementation PR rather than silently choosing an interpretation.

## Contents

1. **Product definition** — what SynData is and is not.
2. **Ancestry and fork contract** — what is inherited from ArtMiner and what must diverge.
3. **Core design principles** — deterministic, domain-neutral, local-first contracts.
4. **Architecture** — engine, domain, sampling, dataset, calibration and interface layers.
5. **Domain and type model** — extensible domain modules without an early plugin ABI.
6. **Sampling and profiles** — distributions, correlations, constraints and confidential profiles.
7. **Latent truth, observations and annotations** — the causal separation that makes synthetic data useful.
8. **Dataset identity, provenance and manifests** — reproducible sample/job/dataset identity.
9. **Batch execution, cohorts and splits** — bounded parallel generation and leakage-safe partitioning.
10. **Calibration and representativeness** — comparing and fitting synthetic populations to real data.
11. **Interoperability and outputs** — native artifacts plus ML-friendly adapters.
12. **Interfaces and workflow** — CLI first, local inspection UI later.
13. **Platform, performance and dependency policy** — portable headless core with measured acceleration.
14. **Security and confidential-domain handling** — keep proprietary profile knowledge local and explicit.
15. **Testing and CI** — semantic regression, replay, robustness and cross-platform gates.
16. **Plan review and improvements** — risks found in the initial concept and corrections made.
17. **Ordered implementation series** — serial SD-001…SD-016 milestones.
18. **Autonomous implementation protocol** — branch/PR/check/merge rules for every issue.
19. **Decision log and deferred scope** — choices agents must not invent.

---

## 1. Product definition

SynData is a deterministic engine for turning **explicit latent state + explicit parameters + explicit observation models** into training/evaluation artifacts with exact ground truth and complete provenance.

The central workflow is:

```text
Domain/profile + generation recipe + job seed
                    ↓
              parameter sampler
                    ↓
               latent state
              ↙      ↓       ↘
      annotations  observers  measurements
           ↓          ↓           ↓
       masks/etc.   images      scalar truth
              \       |        /
                 sample bundle
                    ↓
          dataset manifest/splits
                    ↓
      QA / comparison / calibration
```

SynData should support varied domains by keeping domain knowledge outside the generic engine. Semiconductor-process imagery is a plausible future domain, but it is **not** the initial core model and no semiconductor-specific process assumptions belong in the engine plan.

### Product character

SynData should behave like an engineering instrument rather than an image-effects program:

- reproducible;
- inspectable;
- batch-capable;
- provenance-rich;
- explicit about what is simulated versus measured;
- able to preserve exact latent truth;
- able to distinguish visual plausibility from statistical representativeness.

### Initial non-goals

- no semiconductor-specific physics, recipe values, defect frequencies or tool assumptions in the core;
- no claim that generic synthetic imagery represents a particular fab/process/customer distribution;
- no dynamic plugin ABI or arbitrary DLL loading in the initial series;
- no cloud requirement, account system, telemetry or network runtime dependency;
- no learned image model required by the engine;
- no universal tensor/volume/mesh/time-series abstraction before a real domain requires it;
- no attempt to solve every dataset format in the native schema;
- no hidden RNG, schedule-dependent sample identity, or untraceable output;
- no automatic ingestion/exfiltration of confidential reference data.

---

## 2. Ancestry and fork contract

SynData starts from the reusable deterministic seam deliberately created in ArtMiner AM-016.

Immutable source checkpoint:

- repository: `https://github.com/techrote/artminer`
- commit: `57cb37526b708a9b88b9dbd82a88a1d7b394e152`
- fork-readiness document at that commit: `docs/fork-readiness.md`
- reusable source target there: `artminer_engine`

AM-016 identified these contracts as worth preserving:

- explicit root seeds and model-owned deterministic randomness;
- stable seed derivation and PRNG reference vectors;
- versioned recipe/node semantics;
- canonical serialization and semantic fingerprints;
- typed graph validation and explicit state boundaries;
- bounded resource validation;
- deterministic ordering under concurrency;
- canonical/reference execution for reproducibility claims;
- provenance sufficient to identify generating semantics;
- regression fixtures that expose semantic changes.

### What SynData should inherit

SD-001 should selectively transplant the generic engine seam, its tests and provenance record — **not** the ArtMiner UI, Quarry art-search policy, art node catalog, D3D preview, WIC export stack, glyph/material workflows or ArtMiner product identity.

The new repository should rename product/source namespaces and targets immediately while preserving inherited deterministic test vectors. Because SynData is a new product with new semantics, public ArtMiner `.amr` recipe identity must not be silently repurposed.

### What SynData should deliberately change

SynData needs capabilities the ArtMiner engine intentionally deferred:

- stable logical data-type IDs extensible by compiled domain modules;
- domain-owned evaluator dispatch rather than a built-in art evaluator switch;
- product-native recipe/profile/job/dataset identity;
- latent-state and ground-truth contracts;
- deterministic parameter distributions, correlations and constraints;
- dataset manifests, cohorts, partitions and coverage analysis;
- observation-model composition;
- real-vs-synthetic comparison and calibration.

Source ancestry must remain documented. Do not claim an external software licence or add licence headers unless the repository owner explicitly chooses one.

---

## 3. Core design principles

### 3.1 Determinism is semantic, not cosmetic

A sample must be reproducible from explicit semantic state. At minimum this includes:

- engine schema/evaluator semantic versions;
- domain module ID/version;
- generation recipe fingerprint;
- profile fingerprint;
- job fingerprint;
- job root seed;
- sample index and deterministic sample identity;
- sampled parameter values or a deterministic derivation sufficient to recover them;
- observer/annotator versions;
- explicit output dimensions/quality settings;
- cohort/split identity when applicable.

Do not use time, process-global RNG, thread scheduling, pointer values, hash-map iteration order, locale or UI timing as semantic inputs.

### 3.2 Random access beats sequential RNG coupling

Dataset generation must be reproducible independent of worker count and resume boundaries. Prefer seed derivation such as:

```text
sample_seed = derive(job_seed, sample_index)
parameter/group seed = derive(sample_seed, stable_parameter_or_group_id)
observer seed = derive(sample_seed, observer_id)
```

A worker should be able to generate sample 4,000,123 without generating samples 0…4,000,122 first. Correlated groups may own a deterministic stream, but stream consumption must remain versioned and local to that group.

### 3.3 Domain knowledge is input/layering, not core folklore

The engine should not contain checks such as `if domain == semiconductor` or `if node_type == customer_tool_X`. Domain-specific legality, parameters, operators and observers belong to domain metadata/evaluators or customer profile data.

### 3.4 Truth and observation are distinct

Do not collapse latent truth into a rendered image. A sample can have one latent state and multiple observations/annotations. Clean truth, noisy observation, segmentation masks and scalar measurements should be correlated outputs from the same sample identity.

### 3.5 Plausibility is not representativeness

SynData may generate plausible examples without enough evidence to claim deployment-distribution fidelity. Representativeness requires comparison against appropriate real data and an explicit calibration/evaluation record.

### 3.6 Local-first confidential operation

Profiles and calibration sets may contain valuable proprietary information. The baseline engine operates fully locally, performs no telemetry/network access, and does not require uploading reference data.

---

## 4. Architecture

Target dependency direction:

```text
syndata_engine
  deterministic PRNG/hash
  canonical recipe/schema primitives
  graph/type metadata and generic validation
  execution-plan contracts
          ↓
syndata_domain
  compiled domain registration contract
  evaluator/observer/annotator dispatch interfaces
  stable logical type IDs
          ↓
syndata_sampling
  parameter paths
  distributions
  correlations/constraints
  profiles
          ↓
syndata_dataset
  sample bundles
  manifests/provenance
  deterministic batch jobs
  cohorts/splits/coverage
          ↓
syndata_calibration
  descriptors
  real-vs-synthetic comparison
  deterministic fitting/search
          ↓
syndata_io / syndata_cli / later syndata_app
  artifact codecs/adapters
  CLI workflows
  local inspection UI
```

A reference domain lives above `syndata_domain` and exercises the whole stack without contaminating generic layers.

### 4.1 Core engine responsibilities

The generic engine owns semantics such as:

- canonical versioned recipe parsing/serialization;
- recipe fingerprinting;
- node/port/parameter schema metadata;
- stable logical type-ID comparison;
- graph validation and cycle/state-boundary rules;
- generic parameter-relation constraints;
- deterministic seed derivation;
- resource-limit preflight;
- execution-plan ordering.

It does **not** own concrete image-generation algorithms or customer distributions.

### 4.2 Domain layer responsibilities

A compiled domain module owns:

- domain ID and semantic version;
- supported logical data types;
- operator/node metadata;
- evaluator implementations;
- observer and annotator implementations;
- domain-specific relation/constraint metadata;
- optional domain-level safe defaults.

The initial module contract is a **source/compile-time API**, not a stable binary ABI. ABI promises come later only if a real deployment requirement justifies them.

### 4.3 Reference data scope

The initial built-in reference domain should focus on 2D raster synthetic-data workflows:

- scalar/float planes;
- binary masks;
- integer label maps;
- RGBA/greyscale images;
- instance/measurement records;
- scalar/string metadata.

The type system must be extensible without editing generic graph-validation logic, but SD-002 must not invent universal N-D tensor/mesh/volume semantics prematurely.

---

## 5. Domain and type model

### Stable logical type identity

Graph validation should reason over stable logical type identifiers rather than a closed ArtMiner-style enum hard-wired into the validator. Payload layout and execution implementation stay outside generic validation.

A domain can therefore introduce a type such as `inspection2d.instance_set.v1` without changing the generic validator, while common types can live under a SynData namespace such as `syndata.image.rgba8.v1`.

### Operator roles

Roles are metadata, not separate graph languages. Useful roles include:

- `generator` — creates initial latent state;
- `transform` — changes latent or intermediate state;
- `observer` — converts latent/intermediate state to a simulated measurement;
- `annotator` — derives exact labels/ground truth;
- `measurement` — derives scalar/vector truth metadata.

A node may expose more than one output but its role must not change graph semantics implicitly.

### Domain package versioning

Every domain module/profile combination must be version-identifiable. A semantic change that changes generated meaning requires a version bump and regression evidence. Old dataset manifests must never be silently interpreted under new semantics.

---

## 6. Sampling and profiles

### 6.1 Profiles

A **profile** supplies domain/process/customer configuration without baking those assumptions into code. It may define:

- parameter distributions;
- fixed values;
- correlations;
- conditional rules;
- validity constraints;
- observer settings;
- safe public labels/metadata;
- private calibration-derived values.

Profiles are versioned and fingerprinted.

### 6.2 Initial deterministic distributions

The initial sampler library should support transparent distributions with explicit parameters:

- fixed;
- uniform;
- integer uniform;
- log-uniform;
- normal/truncated normal;
- categorical/weighted categorical;
- bounded empirical table/histogram when supplied locally.

Sampling semantics and edge cases must be versioned and have deterministic vectors.

### 6.3 Correlation and constraints

Independent parameters are often unrealistic. The engine needs:

- named correlated parameter groups;
- conditional parameters;
- bounded rejection/repair rules whose behaviour is deterministic;
- declared cross-parameter constraints;
- explicit failure when a profile cannot produce a valid sample within its bounded policy.

Do not hide constraint failures by silently clamping arbitrary values after sampling.

### 6.4 Confidential-profile handling

A shareable dataset manifest should normally record a **profile fingerprint and safe metadata**, not automatically embed confidential profile contents. A separate internal/full provenance bundle may preserve the complete profile when explicitly requested. Reproducibility claims must state which provenance tier is available.

---

## 7. Latent truth, observations and annotations

A `SampleBundle` is the central product concept.

Conceptually:

```text
SampleBundle
  identity
  sampled parameters
  latent outputs
  observations[]
  annotations[]
  measurements[]
  provenance
  artifact hashes
```

### Latent truth

Latent state is authoritative generated truth. It may include geometry, material/region IDs, fields, object instances or other domain state.

### Observations

Observers produce what a model might actually see. Multiple observations can derive from the same latent state, for example:

- clean/reference raster;
- noisy sensor raster;
- alternate modality;
- low-resolution and high-resolution pair.

Observer randomness must derive from the sample identity and observer ID, so changing only the observer can preserve latent truth exactly.

### Annotations

Because the generator owns the truth, annotation extraction should be exact where semantics allow. Initial contracts should support:

- semantic masks;
- instance-ID maps/records;
- boxes derived from instances;
- class labels;
- scalar regression targets;
- parameter truth;
- validity/visibility flags.

Annotations must identify their source latent semantics rather than infer labels from the rendered observation unless the task explicitly tests an inference/measurement algorithm.

---

## 8. Dataset identity, provenance and manifests

### 8.1 Identity hierarchy

Use stable deterministic identities for:

- engine build/semantic version;
- domain module;
- recipe;
- profile;
- job;
- cohort/family;
- sample;
- observer/annotation artifact.

A recommended semantic relationship is:

```text
job_fingerprint = hash(recipe_fp, profile_fp, domain_version, sampler_version,
                       output contract, root seed, requested population)
sample_id       = hash(job_fingerprint, sample_index)
artifact_id     = hash(sample_id, artifact_role, producer_semver)
```

The exact canonical fields become contractual once implemented.

### 8.2 Dataset manifest

The dataset manifest must be deterministic and streamable for large populations. It records at least:

- dataset/job identity;
- semantic versions;
- sample IDs and indexes;
- cohort/group/split IDs;
- parameter/provenance references;
- artifact relative paths, media/type descriptors and hashes;
- annotation roles;
- generation status/failure reason if explicitly retained;
- redaction/provenance tier.

Do not make directory enumeration order authoritative.

### 8.3 Transactionality

Never advertise a sample/dataset as complete before its declared artifacts and manifest records are durably committed. Interrupted runs must be resumable without accepting partial stale files as valid output.

---

## 9. Batch execution, cohorts and splits

### 9.1 Batch engine

Large jobs require:

- deterministic sample enumeration;
- bounded worker pools;
- bounded memory;
- cancellation;
- checkpoint/resume;
- stable output ordering independent of worker completion order;
- random-access regeneration of a sample by ID/index;
- transactional sample writes;
- explicit job manifests;
- cache keys that include all semantic inputs.

Caches are disposable and never authoritative.

### 9.2 Counterfactual cohorts

SynData should make controlled datasets easy. A cohort may share latent base state while varying exactly one declared factor, or share all process factors while varying observer noise. Cohort identity and the controlled differences must be explicit in provenance.

### 9.3 Leakage-safe partitioning

Randomly splitting nearly identical generated images can give misleading validation results. Splits therefore support grouping by:

- latent family/cohort;
- source template/layout identity;
- generator topology/family;
- customer-defined group key;
- other stable provenance dimensions.

Train/validation/test assignment must be deterministic, reproducible and checked for forbidden group overlap.

### 9.4 Coverage

Dataset generation should expose coverage rather than just sample count:

- per-parameter ranges/histograms;
- class/defect balance;
- correlation summaries;
- invalid/rejected sample reasons;
- cohort/split population;
- duplicate/near-duplicate indicators when available.

---

## 10. Calibration and representativeness

Calibration is a first-class future product capability, but it must be built on transparent deterministic measurements before learned models.

### 10.1 Comparison pipeline

```text
real reference set ─→ deterministic descriptors ─┐
                                                 ├→ comparison report
synthetic set      ─→ deterministic descriptors ─┘
```

Initial descriptors may include:

- intensity/colour histograms;
- edge magnitude/orientation statistics;
- connected-component/morphology summaries;
- spatial-frequency/power-spectrum summaries;
- noise/autocorrelation summaries;
- size/spacing/shape distributions;
- task/domain-provided descriptors.

Every distance must name the descriptor, normalization and aggregation. Do not collapse the report into an unexplained single realism score.

### 10.2 Calibration search

Later deterministic fitting may search profile parameters to reduce selected real-vs-synthetic descriptor differences. Requirements:

- bounded deterministic search;
- explicit objective terms/weights;
- calibration train/holdout separation;
- parameter bounds and priors;
- checkpoint/resume;
- full provenance of the fitted profile;
- no assertion of physical truth merely because image statistics match.

### 10.3 Real data remains authoritative for transfer claims

A calibrated profile can be called representative only with a documented comparison population and scope. SynData must make it easy to record those limits.

---

## 11. Interoperability and outputs

### Native artifacts

The core dataset format should prefer simple, inspectable, deterministic files and relative paths. Initial useful outputs include:

- raw/typed raster planes;
- PNG where a deterministic codec path is available;
- masks/label maps with explicit bit depth;
- metadata/measurement records;
- canonical recipe/profile/job manifests;
- checksum files or embedded hashes.

### Adapters

Interoperability belongs above the native semantic model. Later adapters can emit common training forms such as:

- image + mask folders;
- CSV/NDJSON metadata;
- COCO-style detection/segmentation JSON;
- YOLO-style boxes/labels;
- NumPy-friendly raw arrays/descriptors;
- paired clean/noisy or HR/LR directory structures.

An adapter must not silently discard information; lossy mapping must be explicit in its report.

Do not make a heavyweight ML framework a runtime dependency merely to write a dataset.

---

## 12. Interfaces and workflow

### CLI first

The initial product surface is headless and scriptable. Illustrative commands (exact syntax becomes contractual only when implemented):

```text
syndata recipe validate recipe.sdr
syndata profile validate profile.sdp
syndata sample render job.sdj --index 42 --output out/sample-42
syndata dataset build job.sdj --output dataset/
syndata dataset verify dataset/
syndata compare real/ synthetic/ --descriptor-set inspection2d-v1
syndata calibrate calibration.sdc --output fitted-profile.sdp
```

The GUI must not be required for deterministic execution or CI.

### Local inspection UI later

A later local application may inspect:

- latent layers;
- observation layers;
- masks/instances;
- sampled parameters;
- provenance;
- cohorts/counterfactual neighbours;
- distribution/coverage/calibration summaries.

It should use the same engine and manifests as the CLI rather than introducing UI-only semantics.

---

## 13. Platform, performance and dependency policy

### Platform

- C++20 and CMake are the baseline inherited implementation technology.
- Windows 10/11 x64 remains a first-class local target.
- The **headless generic engine and CLI should also build/test on Linux x64 early**, because synthetic-data generation is likely to run on workstations, CI and servers.
- GUI/accelerator backends may remain platform-specific; do not force a cross-platform GUI abstraction into the early core.

### Performance

Correctness and provenance come first, but the architecture must support high-volume generation.

Priorities:

1. deterministic random-access sample generation;
2. bounded parallel CPU execution;
3. profiling with representative workloads;
4. vectorization/batching where semantics remain unchanged;
5. GPU/accelerator backends only for measured bottlenecks with canonical CPU/reference equivalence tests.

Do not create a GPU-only semantic path that cannot reproduce/reference its output.

### Dependencies

Keep the deterministic core small. New dependencies require documented justification covering:

- licence;
- reproducibility/semantic stability;
- binary/runtime deployment cost;
- cross-platform availability;
- security/maintenance burden;
- whether a narrow in-project implementation would be safer.

No network runtime dependency is required by the initial product.

---

## 14. Security and confidential-domain handling

SynData may eventually be deployed with proprietary process profiles and private calibration imagery. Treat this as an architectural requirement, not an afterthought.

Baseline rules:

- no telemetry;
- no implicit network access;
- profile/reference paths stay local unless an explicit future feature says otherwise;
- logs must not dump full confidential profile contents by default;
- shareable manifests record fingerprints/safe labels rather than automatically embedding private profiles;
- path traversal and output-root escape are rejected;
- input file sizes, dimensions, graph sizes, manifest sizes and sample counts are bounded/preflighted;
- malformed/unsupported semantic versions fail clearly rather than being partially reinterpreted;
- caches and temporary files are non-authoritative;
- reference datasets are read-only inputs during calibration unless a command explicitly creates a derived output elsewhere.

A future server/cloud mode would require a separate threat model and authorization design; it is not implicit in this plan.

---

## 15. Testing and CI

Every milestone must add tests that prove its acceptance criteria. The repository should accumulate:

- inherited PRNG/hash deterministic vectors from the ArtMiner ancestry point;
- recipe/profile/job canonical round-trip tests;
- malformed and bounded-input tests;
- graph/type/parameter-relation validation tests;
- domain registration/evaluator dispatch tests;
- sampler deterministic vectors and statistical sanity tests using fixed fixtures;
- schedule/worker-count independence tests;
- sample/job/dataset identity vectors;
- latent/annotation exactness goldens;
- observer goldens;
- resume/cancellation/transactionality tests;
- cohort and split leakage tests;
- manifest/artifact hash verification tests;
- real-vs-synthetic descriptor vectors;
- calibration search replay tests;
- export-adapter consistency tests;
- reference-domain end-to-end dataset fixtures.

CI should include strict warning-as-error Release builds and tests on Windows x64 and Linux x64 once SD-001 establishes the portable headless baseline. Hardware/GPU qualification remains a separate measured test surface.

Never obtain green CI by deleting relevant tests, weakening deterministic assertions without justification, replacing golden outputs casually, disabling required checks or silently changing version semantics.

---

## 16. Plan review and improvements

The initial idea — fork ArtMiner and make a semiconductor synthetic image generator — was reviewed before issue decomposition. The following corrections are part of the plan.

1. **Do not make semiconductor the core domain.** Semiconductor imagery is a demanding future application, not the architecture. The first proof domain is deliberately generic 2D inspection imagery.
2. **Do not equate convincing images with useful training data.** The design separates plausibility from representativeness and reserves calibrated claims for measured real-vs-synthetic comparison.
3. **Do not bake proprietary process knowledge into source.** Customer/process knowledge is profile data, fingerprinted and kept local.
4. **Do not assume independent parameters.** Correlated groups, conditional rules and constraints arrive before serious domain modelling.
5. **Do not let concurrency determine randomness.** Random-access seed derivation makes sample identity independent of worker count, scheduling and resume points.
6. **Do not over-generalize the payload model.** Start with 2D raster/label/record workflows but make logical type IDs extensible so future domains can add volumes/series without rewriting generic validation.
7. **Do not design a binary plugin ABI before the contracts are proven.** Compile-time domain modules are sufficient for the initial series and much easier to version safely.
8. **Do not let train/test leakage hide generator weakness.** Cohort/family-aware deterministic partitioning is a first-class dataset contract.
9. **Do not make one opaque realism score.** Comparison/calibration exposes descriptor terms, normalizations and distances individually.
10. **Do not optimize GPU paths before measuring CPU/reference workloads.** A canonical reference path remains the semantic authority.
11. **Do not leak confidential profiles through provenance.** Separate reproducibility identity (fingerprints) from optional full/private provenance bundles.
12. **Do not drag ArtMiner product identity indefinitely.** SD-001 records ancestry; SD-002 establishes SynData-native schemas/IDs before public SynData files exist.
13. **Do not postpone data QA until export.** Manifest integrity, label consistency, replay and coverage become explicit product functions.
14. **Do not make the first useful result depend on a GUI.** The CLI/reference domain/batch builder deliver useful datasets before UI work.

### Improved development sequence

The reviewed sequence is therefore:

```text
inherit deterministic seam
        ↓
SynData-native schema + domain dispatch
        ↓
truth/provenance + sampler/profile contracts
        ↓
generic 2D primitives + observation models
        ↓
reference inspection domain
        ↓
batch datasets + leakage/coverage + QA
        ↓
interoperability
        ↓
comparison/calibration
        ↓
local inspection UI
        ↓
profiling/acceleration/release hardening
```

This sequence front-loads trustworthy semantics and a useful end-to-end dataset before product polish or domain specialization.

---

## 17. Ordered implementation series

Issues are intended to be implemented serially unless an issue explicitly says otherwise. Each issue must leave `main` coherent and usable.

| ID | Milestone | Primary result |
|---|---|---|
| SD-001 | Ancestry + portable foundation | Selectively transplant ArtMiner AM-016 `artminer_engine`, rename to SynData, preserve deterministic vectors, Windows+Linux headless CI |
| SD-002 | SynData-native schema + execution contracts | Product-native recipe identity, extensible logical type IDs, generic execution plan and domain-owned evaluator dispatch |
| SD-003 | Domain module contract | Versioned compile-time domain registration, reference common 2D value/record types, no dynamic ABI |
| SD-004 | Sample bundle + provenance | Latent/observation/annotation/measurement bundle, stable sample/artifact IDs, provenance tiers |
| SD-005 | Deterministic sampler + profiles | Versioned distributions, correlations, conditional rules, constraints, confidential profile fingerprinting |
| SD-006 | Generic 2D latent/annotation primitives | Shapes/regions/transforms/perturbations with exact semantic/instance/measurement annotations |
| SD-007 | Observation-model toolkit | PSF/blur, gain/offset, noise, distortion, resampling, quantization and paired observations with reference goldens |
| SD-008 | Reference Inspection2D domain | End-to-end generic industrial/inspection-style domain proving generation → observation → exact labels |
| SD-009 | Deterministic dataset builder | CLI jobs, bounded workers, random-access samples, transactionality, cancellation/checkpoint/resume, streamable manifests |
| SD-010 | Cohorts, splits + coverage | Counterfactual cohorts, leakage-safe deterministic partitioning, balancing/coverage and duplicate indicators |
| SD-011 | Dataset QA + replay audit | Dataset verify/replay commands, manifest/hash/label consistency, corruption/partial-output detection |
| SD-012 | Interoperability adapters | Explicit deterministic adapters for common image/mask/metadata/detection training layouts |
| SD-013 | Real-vs-synthetic comparison | Deterministic descriptor framework, reference-set ingestion and transparent comparison reports |
| SD-014 | Calibration search | Deterministic bounded fitting of profile parameters with holdout evaluation and replayable fitted profiles |
| SD-015 | Local inspector + counterfactual explorer | Native/local UI for sample layers, provenance, cohorts, coverage and calibration inspection using the same core |
| SD-016 | Profiling + release hardening | Representative benchmarks, measured acceleration seam, security/resource audit, portable packaging and v0.x readiness |

Domain-specific production packs — semiconductor, PCB, microscopy, metallurgy, weld inspection, etc. — begin **after** the generic/reference stack proves the needed contracts. A future domain issue may extend generic contracts only when it provides a concrete requirement and regression test.

---

## 18. Autonomous implementation protocol

Every implementation issue must be executable by an autonomous agent using the repository and GitHub issue as context.

Unless an issue explicitly overrides a point, the agent must:

1. read the active issue in full;
2. read `RAG.md`, `AGENTS.md`, current `README.md` if present, current `main`, and every document/test explicitly referenced by the issue;
3. inspect accepted predecessor implementations rather than assuming exact code shape from this plan;
4. start from current `main` and create a focused branch;
5. reconcile ambiguity in favour of existing tested contracts and record material interpretations in the PR;
6. implement the complete issue, not just scaffolding;
7. add/update tests and documentation required to keep the repository internally consistent;
8. run relevant local checks where available;
9. open a focused PR linked to the issue;
10. inspect automated checks and repair failures rather than bypassing them;
11. merge only after all required automated checks pass and acceptance criteria are genuinely satisfied;
12. verify the merged commit/change is present on `main` after merge;
13. close the issue only after that verification;
14. leave explicit follow-up issues for genuinely deferred work rather than silently omitting acceptance criteria.

The user grants autonomous implementation issues permission to create branches, modify repository content, open PRs and merge the agent's own PR after required automated checks pass.

### Prohibited completion shortcuts

- do not weaken/delete tests to get green CI;
- do not change deterministic vectors/goldens without explaining and versioning an intentional semantic change;
- do not close issues with known unmet acceptance criteria;
- do not merge while relevant required checks are failing or pending;
- do not add domain-specific assumptions to generic layers merely because they simplify one reference implementation;
- do not add telemetry/network/cloud dependencies implicitly;
- do not choose a software licence on the user's behalf;
- do not claim representativeness or physical validity without the evidence required by this plan.

---

## 19. Decision log and deferred scope

The following are intentionally not guessed by the plan:

- repository/software licence;
- final commercial branding beyond the working name SynData;
- stable public binary plugin ABI;
- cloud/service deployment model;
- macOS support;
- exact GPU API/backend strategy;
- heavyweight dataset container formats such as HDF5/Parquet/WebDataset;
- learned similarity/realism models;
- a specific production semiconductor process model;
- customer-specific confidential distributions or calibration data.

These are not blockers for the initial implementation series. Agents must not invent values, licences, customer process assumptions or external-service dependencies to fill them.
