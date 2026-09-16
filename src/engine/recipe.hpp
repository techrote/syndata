#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "engine/graph.hpp"
#include "engine/result.hpp"
#include "engine/types.hpp"

namespace syndata::engine {

inline constexpr std::string_view kRecipeFileExtension = ".sdr";
inline constexpr std::string_view kRecipeMagic = "sdr";
inline constexpr u32 kRecipeSchemaVersion = 1U;
inline constexpr u32 kEvaluatorSemanticVersion = 1U;
inline constexpr std::size_t kMaximumRecipeNodes = 4096U;
inline constexpr std::size_t kMaximumRecipeParameters = 65536U;
inline constexpr std::size_t kMaximumRecipeEdges = 16384U;
inline constexpr std::size_t kMaximumRecipeOutputs = 1024U;
inline constexpr std::size_t kMaximumRecipeMetadata = 4096U;

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
    u32 schema_version{kRecipeSchemaVersion};
    u32 evaluator_version{kEvaluatorSemanticVersion};
    u64 root_seed{0U};
    std::vector<NodeInstance> nodes;
    std::vector<Edge> edges;
    std::vector<OutputBinding> outputs;
    // Metadata is descriptive/non-semantic. Domain/execution state that affects
    // generated results belongs in explicit nodes/parameters or later versioned
    // product contracts, never in metadata.
    std::vector<RecipeMetadata> metadata;
};

enum class RecipeErrorCode {
    malformed,
    resource_limit,
    unsupported_schema_version,
    unsupported_evaluator_version,
};

struct RecipeError {
    RecipeErrorCode code{RecipeErrorCode::malformed};
    std::size_t line{0U};
    std::string message;
};

[[nodiscard]] Result<Recipe, RecipeError> parse_recipe(std::string_view text);
[[nodiscard]] std::string serialize_recipe_canonical(const Recipe& recipe);
[[nodiscard]] std::string serialize_recipe_semantic(const Recipe& recipe);
[[nodiscard]] std::string semantic_fingerprint(const Recipe& recipe);

}  // namespace syndata::engine
