# SynData

SynData is a deterministic, provenance-first synthetic-data generation and calibration engine under active development.

**Current milestone: SD-002.** The repository now contains a portable deterministic engine, a SynData-native `.sdr` recipe schema, extensible logical-type/node registries, deterministic execution-plan construction and source-level evaluator dispatch contracts. Real domains, sampling, dataset production and observation/calibration layers arrive in later milestones.

The authoritative architecture and implementation order are in `RAG.md`. Repository implementation rules are in `AGENTS.md`. Source ancestry is documented in `docs/ancestry.md`; the native recipe/execution contract is documented in `docs/recipe-format.md`.

## Requirements

- CMake 3.24+
- C++20 compiler
- 64-bit Windows or Linux build

There are no third-party runtime dependencies in SD-002.

## Windows x64

```bat
scripts\0Build.cmd
scripts\0Test.cmd
scripts\0Run.cmd --self-check
```

Equivalent direct commands:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
build\Release\SynData.exe --self-check
```

## Linux x64

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/SynData --self-check
```

Or run `sh scripts/build-linux.sh`. Both CI platforms compile with warnings treated as errors.

## CLI

```text
SynData --help
SynData --version
SynData --self-check
SynData recipe validate <file.sdr>
SynData recipe inspect <file.sdr>
SynData recipe canonicalize <file.sdr>
SynData recipe fingerprint <file.sdr>
```

At SD-002 the standalone `recipe validate` command performs strict file/schema, resource, identifier and graph-reference validation. Full node/port/logical-type validation requires a caller-supplied compiled domain registry; the generic engine intentionally ships with no built-in domain catalog.

A minimal format example is `examples/sd002-minimal.sdr`.

## Native recipe identity

SynData recipes use the `.sdr` identity and `sdr 1` magic. ArtMiner `.amr` is explicitly rejected rather than silently reinterpreted. Canonical serialization and semantic fingerprints are versioned SynData contracts; descriptive `meta` records are retained in canonical files but excluded from semantic recipe identity.

Graph ports carry stable logical type ID + semantic-version references. Type compatibility is registered explicitly by compiled catalogs. Generic validation contains no domain node-name switch and evaluator dispatch is keyed by registered node type/version rather than hard-coded type checks.

## Determinism and ancestry

The initial deterministic seam comes from the immutable ArtMiner AM-016 checkpoint `57cb37526b708a9b88b9dbd82a88a1d7b394e152`. SynData preserves inherited SplitMix64, seed-derivation, PCG32 and FNV-1a regression vectors while using its own product-native recipe identity and semantics from SD-002 onward.

## Licence

No SynData software licence has been selected yet. Do not assume a licence from the absence of a `LICENSE` file.
