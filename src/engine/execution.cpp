#include "engine/execution.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace syndata::engine {

Result<ExecutionPlan, ExecutionPlanError> build_execution_plan(
    const Recipe& recipe,
    const EngineRegistry& registry,
    const ExecutionLimits& limits) {
    if (recipe.nodes.size() > limits.maximum_nodes || recipe.edges.size() > limits.maximum_edges ||
        recipe.outputs.size() > limits.maximum_outputs) {
        return Result<ExecutionPlan, ExecutionPlanError>::failure({
            ExecutionPlanErrorCode::resource_limit,
            "recipe exceeds execution preflight limits",
        });
    }

    const std::vector<ValidationError> validation_errors = validate_recipe(recipe, registry);
    if (!validation_errors.empty()) {
        return Result<ExecutionPlan, ExecutionPlanError>::failure({
            ExecutionPlanErrorCode::invalid_recipe,
            validation_errors.front().message,
        });
    }

    std::map<std::string, const NodeMetadata*, std::less<>> metadata;
    std::map<std::string, std::size_t, std::less<>> indegree;
    std::map<std::string, std::vector<std::string>, std::less<>> adjacency;
    std::map<std::string, std::vector<std::string>, std::less<>> dependencies;
    for (const NodeInstance& node : recipe.nodes) {
        const NodeMetadata* node_metadata = registry.nodes.find(node.type_id, node.semantic_version);
        if (node_metadata == nullptr) {
            return Result<ExecutionPlan, ExecutionPlanError>::failure({
                ExecutionPlanErrorCode::internal_error,
                "validated node metadata disappeared while building execution plan",
            });
        }
        metadata.emplace(node.id, node_metadata);
        indegree.emplace(node.id, 0U);
        adjacency.emplace(node.id, std::vector<std::string>{});
        dependencies.emplace(node.id, std::vector<std::string>{});
    }

    ExecutionPlan plan;
    plan.steps.reserve(recipe.nodes.size());
    for (const Edge& edge : recipe.edges) {
        const NodeMetadata* target = metadata.at(edge.to_node);
        if (target->state_class == NodeStateClass::state_boundary) {
            plan.state_boundary_edges.push_back(edge);
            continue;
        }
        adjacency[edge.from_node].push_back(edge.to_node);
        dependencies[edge.to_node].push_back(edge.from_node);
        ++indegree[edge.to_node];
    }

    for (auto& [node_id, values] : adjacency) {
        static_cast<void>(node_id);
        std::sort(values.begin(), values.end());
    }
    for (auto& [node_id, values] : dependencies) {
        static_cast<void>(node_id);
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }
    std::sort(
        plan.state_boundary_edges.begin(),
        plan.state_boundary_edges.end(),
        [](const Edge& left, const Edge& right) {
            return std::tie(left.from_node, left.from_port, left.to_node, left.to_port) <
                   std::tie(right.from_node, right.from_port, right.to_node, right.to_port);
        });

    std::set<std::string, std::less<>> ready;
    for (const auto& [node_id, count] : indegree) {
        if (count == 0U) {
            ready.insert(node_id);
        }
    }

    std::map<std::string, const NodeInstance*, std::less<>> instances;
    for (const NodeInstance& node : recipe.nodes) {
        instances.emplace(node.id, &node);
    }

    while (!ready.empty()) {
        const std::string node_id = *ready.begin();
        ready.erase(ready.begin());
        const NodeInstance& node = *instances.at(node_id);
        plan.steps.push_back(ExecutionStep{
            node.id,
            node.type_id,
            node.semantic_version,
            dependencies.at(node_id),
        });
        for (const std::string& next : adjacency.at(node_id)) {
            std::size_t& count = indegree[next];
            --count;
            if (count == 0U) {
                ready.insert(next);
            }
        }
    }

    if (plan.steps.size() != recipe.nodes.size()) {
        return Result<ExecutionPlan, ExecutionPlanError>::failure({
            ExecutionPlanErrorCode::internal_error,
            "validated graph did not produce a complete execution plan",
        });
    }
    return Result<ExecutionPlan, ExecutionPlanError>::success(std::move(plan));
}

EvaluatorRegistry::EvaluatorRegistry(std::vector<EvaluatorBinding> bindings) : bindings_(std::move(bindings)) {
    std::sort(bindings_.begin(), bindings_.end(), [](const EvaluatorBinding& left, const EvaluatorBinding& right) {
        return std::tie(left.node_type_id, left.node_semantic_version) <
               std::tie(right.node_type_id, right.node_semantic_version);
    });
}

const EvaluatorBinding* EvaluatorRegistry::find(
    const std::string_view node_type_id,
    const u32 node_semantic_version) const noexcept {
    const auto found = std::lower_bound(
        bindings_.begin(), bindings_.end(), std::pair<std::string_view, u32>{node_type_id, node_semantic_version},
        [](const EvaluatorBinding& binding, const std::pair<std::string_view, u32>& value) {
            if (binding.node_type_id != value.first) {
                return binding.node_type_id < value.first;
            }
            return binding.node_semantic_version < value.second;
        });
    if (found == bindings_.end() || found->node_type_id != node_type_id ||
        found->node_semantic_version != node_semantic_version) {
        return nullptr;
    }
    return &*found;
}

Result<void, EvaluationError> EvaluatorRegistry::dispatch(
    const NodeInstance& node,
    EvaluationContext& context) const {
    const EvaluatorBinding* binding = find(node.type_id, node.semantic_version);
    if (binding == nullptr || binding->function == nullptr) {
        return Result<void, EvaluationError>::failure({
            EvaluationErrorCode::missing_evaluator,
            "no evaluator registered for node type '" + node.type_id + "' version " +
                std::to_string(node.semantic_version),
        });
    }
    return binding->function(node, context);
}

}  // namespace syndata::engine
