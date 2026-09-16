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
- graph validation, parameter domains and catalog-declared parameter relations;
- explicit state-boundary treatment for ordinary-cycle validation;
- deterministic canonical ordering/fingerprint mechanics used to prove the engine extraction.

The deterministic PRNG/hash/seed vectors from the upstream tests remain regression evidence.

## Deliberate adaptations

These changes were intentional and do not alter the inherited PRNG/hash/seed behavior:

1. namespaces and build targets were renamed immediately to `syndata::engine` / `syndata_engine`;
2. the Windows-only build gate was removed; Windows x64 and Linux x64 are first-class headless build/test targets;
3. no built-in node catalog exists in `syndata_engine`; callers supply explicit registries;
4. no ArtMiner UI/platform/render/export/Quarry layer was transplanted;
5. SD-001 did **not** expose an ArtMiner `.amr` parser, file command or file extension as a SynData interface.

### SD-002 resolution of transitional contracts

SD-002 removes the remaining transitional inherited recipe/data-kind surface from the active engine contract:

- the closed ArtMiner-derived `DataKind` enum is replaced by registered logical type IDs plus semantic versions;
- the inherited in-memory `amr 1` canonical serializer/fingerprint bridge is replaced by the product-native `.sdr` / `sdr 1` schema;
- the secondary fingerprint domain is now `SynData.Recipe.SemanticFingerprint.v1`;
- ArtMiner `amr` input is explicitly rejected rather than reinterpreted;
- execution planning and evaluator dispatch are SynData-native generic contracts.

The old AM-016 canonical byte stream is therefore historical ancestry evidence, not an active SynData format/API. The immutable upstream commit remains the provenance point for the low-level deterministic mechanics.

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

No semiconductor/process model, dataset layer, sampler/profile system, plugin ABI, GPU backend or GUI is introduced by SD-001/SD-002.

## Licence status

The source repository has not selected a SynData software licence. The project does not invent an SPDX identifier, licence header or distribution grant. The repository owner must make that decision explicitly in a later accepted change.
