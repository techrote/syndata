# SynData source ancestry — SD-001

## Immutable source checkpoint

SynData's initial deterministic substrate was selectively transplanted from:

- repository: `techrote/artminer`
- commit: `57cb37526b708a9b88b9dbd82a88a1d7b394e152`
- upstream milestone: merged AM-016 fork-readiness checkpoint
- upstream architecture note: `docs/fork-readiness.md` at that exact commit
- upstream reusable target: `artminer_engine`

This SHA is the ancestry record. SynData does **not** track moving ArtMiner `main` for inherited semantics.

## Why this checkpoint

AM-016 deliberately separated ArtMiner's domain-neutral deterministic mechanics from its built-in art catalog and application layers. It also removed concrete ArtMiner node-type checks from generic cross-parameter validation by making relations catalog-declared. That made the checkpoint suitable for selective transplantation without dragging the procedural-art product into SynData.

## Contracts preserved

SD-001 ports or adapts these generic contracts into the `syndata::engine` namespace:

- fixed-width primitive types and `Result` helper;
- SplitMix64 seed transform and `derive_seed` semantics;
- PCG32 stream semantics;
- FNV-1a 64-bit hashing and stable hexadecimal formatting;
- checked allocation-size arithmetic;
- bounded local UTF-8 text validation/read helpers;
- generic node registry mechanics;
- graph validation, typed ports, parameter domains and catalog-declared parameter relations;
- explicit state-boundary treatment for ordinary-cycle validation;
- the inherited in-memory recipe model, canonical ordering and semantic fingerprint machinery used by the AM-016 engine-only proof.

The deterministic PRNG/hash/seed vectors from the upstream tests are copied as regression evidence. A test-only upstream fingerprint domain string is retained solely to prove the inherited canonical fingerprint path; the SynData engine does not hard-code ArtMiner product identity.

## Deliberate adaptations

These changes are intentional and do not alter the inherited PRNG/hash/seed behavior:

1. namespaces and build targets are renamed immediately to `syndata::engine` / `syndata_engine`;
2. the Windows-only build gate is removed; Windows x64 and Linux x64 are both first-class headless build/test targets;
3. no built-in node catalog exists in `syndata_engine`; callers must supply an explicit registry;
4. no ArtMiner UI/platform/render/export/Quarry layer is transplanted;
5. SD-001 does **not** expose an ArtMiner `.amr` parser, file command or file extension as a SynData interface;
6. the inherited canonical serializer exists only as a regression bridge for the engine-only proof. Its old `amr 1` byte prefix is not a SynData schema declaration;
7. the old product-specific secondary fingerprint domain is caller-supplied by the ancestry test rather than embedded in engine code.

SD-002 is responsible for replacing the transitional closed data-kind enum and inherited recipe model with SynData-native logical type IDs, schema identity and execution contracts. Therefore no external SynData recipe format is declared by SD-001.

## Explicitly excluded ArtMiner layers

The following were deliberately not copied:

- Win32 application/browser/playback/material/lineage windows;
- D3D11/DXGI preview implementation;
- Windows WIC image/export stack;
- Quarry batch/art-search, novelty/diversity and thumbnail policy;
- ArtMiner built-in node catalog;
- static/growth/motion/material/glyph evaluators;
- specimen browser, breeding, topology mutation and art lineage logic;
- ArtMiner workspace/session persistence and packaging layout;
- example `.amr` art recipes and product UI resources.

No semiconductor/process model, dataset layer, sampler/profile system, plugin ABI, GPU backend or GUI is introduced in SD-001.

## Licence status

The source repository has not selected a SynData software licence. SD-001 does not invent an SPDX identifier, licence header or distribution grant. The repository owner must make that decision explicitly in a later accepted change.
