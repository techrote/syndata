# SynData

SynData is a deterministic, provenance-first synthetic-data generation and calibration engine under active development.

**Current milestone: SD-001.** This repository currently contains only the portable deterministic engine foundation and a minimal CLI sanity shell. It does not yet provide SynData-native recipes, domains, samplers, datasets, observation models or calibration workflows.

The authoritative architecture and implementation order are in `RAG.md`. Repository implementation rules are in `AGENTS.md`. Source ancestry is documented in `docs/ancestry.md`.

## Requirements

- CMake 3.24+
- C++20 compiler
- 64-bit Windows or Linux build

There are no third-party runtime dependencies in SD-001.

## Windows x64

From a Visual Studio developer command prompt or a shell with CMake/MSVC available:

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

Or run:

```sh
sh scripts/build-linux.sh
```

Both CI platforms compile with warnings treated as errors.

## CLI

```text
SynData --help
SynData --version
SynData --self-check
```

The SD-001 CLI deliberately has no dataset or recipe commands. Product-native recipe/type/execution contracts are SD-002 scope.

## Determinism and ancestry

The initial deterministic seam comes from the immutable ArtMiner AM-016 checkpoint `57cb37526b708a9b88b9dbd82a88a1d7b394e152`. SynData preserves the inherited SplitMix64, seed-derivation, PCG32 and FNV-1a regression vectors while removing the ArtMiner application/runtime layers and product namespace.

The transitional inherited recipe model exists only to prove that the generic registry/validation/canonical-fingerprint machinery survived the extraction. ArtMiner `.amr` is **not** a SynData file format.

## Licence

No SynData software licence has been selected yet. Do not assume a licence from the absence of a `LICENSE` file.
