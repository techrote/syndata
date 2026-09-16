#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "engine/graph.hpp"
#include "engine/types.hpp"

namespace syndata::engine {

// Transitional compatibility values inherited from the AM-016 engine seam.
// They are NOT SynData-native file/schema identifiers. SD-002 introduces the
// product-native recipe format before external SynData recipes are published.
inline constexpr u32 kInheritedRecipeSchemaVersion = 1U;
inline constexpr u32 kInheritedEvaluatorSemanticVersion = 1U;
inline constexpr std::size_t kMaximumRecipeNodes = 4096U;
inline constexpr std::size_t kMaximumRecipeParameters = 65536U;
inline constexpr std::size_t kMaximumRecipeEdges = 16384U;
inline constexpr std::size_t kMaximumRecipeOutputs = 1024U;
inline constexpr std::size_t kMaximumRecipeMetadata = 4096U;

struct RenderSettings {
    u32 width{512U};
    u32 height{512U};
    std::string quality{"reference"};
};

struct ParameterAssignment {
    std::string name;
    ParameterValue value;
};

struct NodeInstance {
    std::string id;
    std::string type_id;
    u32 semantic_version{1U};
    std::vector<ParameterAssignment> parameters;
};

struct Edge {
    std::string from_node;
    std::string from_port;
    std::string to_node;
    std::string to_port;
};

struct OutputBinding {
    std::string name;
    std::string node_id;
    std::string port;
};

struct RecipeMetadata {
    std::string key;
    std::string value;
};

struct Recipe {
    u32 schema_version{kInheritedRecipeSchemaVersion};
    u32 evaluator_version{kInheritedEvaluatorSemanticVersion};
    u64 root_seed{0U};
    RenderSettings render;
    std::vector<NodeInstance> nodes;
    std::vector<Edge> edges;
    std::vector<OutputBinding> outputs;
    std::vector<RecipeMetadata> metadata;
};

// These functions preserve the AM-016 canonical byte ordering for regression
// proof only. SynData SD-001 exposes no file parser and does not claim `.amr`
// as a SynData format. The domain separator is supplied by the ancestry test
// rather than hard-coded in the engine, keeping product identity out of core.
[[nodiscard]] std::string serialize_inherited_recipe_canonical(const Recipe& recipe);
[[nodiscard]] std::string serialize_inherited_recipe_semantic(const Recipe& recipe);
[[nodiscard]] std::string inherited_semantic_fingerprint(
    const Recipe& recipe,
    std::string_view domain_separator);

}  // namespace syndata::engine
