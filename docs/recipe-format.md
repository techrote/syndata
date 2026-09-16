# SynData recipe, logical-type and execution contracts

SD-002 establishes the first **SynData-native** recipe identity. These contracts replace the transitional ArtMiner-derived recipe identity used only as extraction evidence in SD-001.

## File identity

- extension: `.sdr`
- magic/schema record: `sdr 1`
- evaluator semantic record: `evaluator 1`
- required deterministic seed record: `seed <uint64>`
- UTF-8 text only; NULs, malformed UTF-8, oversized files and oversized source lines are rejected

ArtMiner `.amr` is not an alias. A file beginning with `amr` is explicitly rejected rather than being interpreted as SynData schema 1.

A future semantic change must increment the relevant schema/evaluator/node/type version or provide an explicit migration. Do not silently change the meaning of version 1.

## Records

The SD-002 grammar is intentionally small:

```text
sdr <schema-version>
evaluator <evaluator-semantic-version>
seed <uint64>
node <node-id> <node-type-id> <node-semantic-version>
param <node-id> <parameter-name> <i64|f64|bool|enum> <value>
edge <from-node> <from-port> <to-node> <to-port>
output <output-name> <node-id> <port>
meta <key> <value>
```

Tokens may be quoted. The canonical writer quotes identifiers/string values consistently, escapes control characters, sorts nodes/parameters/edges/outputs/metadata deterministically, and emits finite floating-point values with round-trip precision.

`meta` is descriptive and **non-semantic**. It is retained by canonical file serialization but excluded from `serialize_recipe_semantic()` and `semantic_fingerprint()`. Anything that can change generated data must therefore live in an explicit semantic contract such as node topology/parameters or a later versioned profile/job contract, not in metadata.

## Fingerprint

Schema-1 recipe semantic identity is:

1. canonical semantic SDR bytes with metadata omitted;
2. FNV-1a-64 of those bytes;
3. a second FNV-1a-64 over `SynData.Recipe.SemanticFingerprint.v1\n` followed by the same semantic bytes;
4. lowercase hexadecimal concatenation of the two 64-bit values.

The two-hash form is an identity/checksum contract inherited structurally from the fork seam; the domain separator is now explicitly SynData-native. It is not a cryptographic authenticity primitive.

## Intrinsic versus catalog validation

`validate_recipe_intrinsic()` checks invariants that do not require domain knowledge:

- schema/evaluator versions;
- resource bounds;
- identifiers;
- duplicate node/parameter/edge/output records;
- node references in edges/outputs.

`validate_recipe()` additionally consumes an explicit `EngineRegistry` and validates:

- node type ID + semantic version;
- explicit parameter schema/domain and declarative cross-parameter relations;
- input/output port existence;
- logical type ID + semantic version availability;
- registered directional type compatibility;
- single/multiple input cardinality;
- required inputs;
- ordinary-cycle rejection with explicit state-boundary handling.

The generic engine contains no built-in domain catalog.

## Logical type IDs

The closed inherited `DataKind` enum is removed. A port now references:

```text
LogicalTypeRef {
    type_id
    semantic_version
}
```

Compiled catalogs register `LogicalTypeMetadata`. Exact type/version identity is compatible automatically. A target type may explicitly list additional accepted source type/version references. Payload layout is intentionally absent from generic graph validation; SD-003 defines common value payloads and the source-level domain module contract.

Type and node IDs use the same bounded stable identifier syntax as recipe graph IDs. Registry validation rejects duplicate registrations, unresolved compatibility references, unregistered port types and invalid parameter relations before recipe execution.

## State boundaries and deterministic execution order

Ordinary same-tick graph cycles are invalid. A node registered as `NodeStateClass::state_boundary` gives edges **entering that node** previous-state semantics. Those edges are retained in `ExecutionPlan::state_boundary_edges` and excluded from the same-tick dependency DAG.

`build_execution_plan()` performs resource preflight before planning, requires a fully valid recipe/catalog pair, and topologically orders same-tick nodes with lexical node-ID tie-breaking. Therefore the execution order does not depend on recipe insertion order, unordered containers, pointer values or worker scheduling.

The plan records each step's stable node identity/version and sorted same-tick dependencies.

## Evaluator dispatch

`EvaluatorRegistry` maps `(node type ID, node semantic version)` to an evaluator binding/function pointer supplied above the generic engine. Dispatch does not contain `if node.type_id == ...` implementation branches.

SD-002 deliberately leaves domain payload storage opaque through `EvaluationContext::user_context`; SD-003 owns the first concrete common 2D value/lifetime contracts. This is a source-level C++ interface, not a binary plugin ABI promise.

## CLI scope in SD-002

The standalone CLI has no compiled real domain yet. Therefore:

```text
SynData recipe validate file.sdr
```

performs strict parse + intrinsic validation and reports the semantic fingerprint. Full catalog validation occurs when a compiled domain registry is supplied by later execution surfaces. `inspect`, `canonicalize` and `fingerprint` are likewise domain-independent.

## Resource limits

Schema 1 retains conservative inherited graph bounds:

- 4096 nodes;
- 65536 parameters;
- 16384 edges;
- 1024 outputs;
- 4096 metadata records;
- 8 MiB local text input and 64 KiB source lines.

Execution callers may impose tighter `ExecutionLimits`; preflight rejects those requests before plan construction.
