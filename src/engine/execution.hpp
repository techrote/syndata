#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "engine/graph.hpp"
#include "engine/recipe.hpp"
#include "engine/result.hpp"

namespace syndata::engine {

struct ExecutionLimits final {
    std::size_t maximum_nodes{kMaximumRecipeNodes};
    std::size_t maximum_edges{kMaximumRecipeEdges};
    std::size_t maximum_outputs{kMaximumRecipeOutputs};
};

struct ExecutionStep final {
    std::string node_id;
    std::string node_type_id;
    u32 node_semantic_version{1U};
    std::vector<std::string> same_tick_dependencies;
};

struct ExecutionPlan final {
    std::vector<ExecutionStep> steps;
    // Edges entering a state-boundary node carry previous-state semantics and
    // therefore are excluded from same-tick topological dependency ordering.
    std::vector<Edge> state_boundary_edges;
};

enum class ExecutionPlanErrorCode {
    resource_limit,
    invalid_recipe,
    internal_error,
};

struct ExecutionPlanError final {
    ExecutionPlanErrorCode code{ExecutionPlanErrorCode::internal_error};
    std::string message;
};

[[nodiscard]] Result<ExecutionPlan, ExecutionPlanError> build_execution_plan(
    const Recipe& recipe,
    const EngineRegistry& registry,
    const ExecutionLimits& limits = {});

struct EvaluationContext final {
    // The generic engine deliberately does not define domain payload layout.
    // Compiled domain layers own the pointed-to execution/value context.
    void* user_context{nullptr};
};

enum class EvaluationErrorCode {
    missing_evaluator,
    evaluator_failed,
};

struct EvaluationError final {
    EvaluationErrorCode code{EvaluationErrorCode::evaluator_failed};
    std::string message;
};

using EvaluatorFunction = Result<void, EvaluationError> (*)(
    const NodeInstance& node,
    EvaluationContext& context);

struct EvaluatorBinding final {
    std::string node_type_id;
    u32 node_semantic_version{1U};
    std::string evaluator_id;
    u32 evaluator_semantic_version{1U};
    EvaluatorFunction function{nullptr};
};

class EvaluatorRegistry final {
public:
    explicit EvaluatorRegistry(std::vector<EvaluatorBinding> bindings = {});

    [[nodiscard]] const EvaluatorBinding* find(std::string_view node_type_id, u32 node_semantic_version) const noexcept;
    [[nodiscard]] Result<void, EvaluationError> dispatch(
        const NodeInstance& node,
        EvaluationContext& context) const;

private:
    std::vector<EvaluatorBinding> bindings_;
};

}  // namespace syndata::engine
