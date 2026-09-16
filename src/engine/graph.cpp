#include "engine/graph.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

#include "engine/recipe.hpp"

namespace syndata::engine {
namespace {

[[nodiscard]] bool is_identifier_character(const char character) noexcept {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '_' || character == '-' || character == '.';
}

[[nodiscard]] bool is_valid_identifier(const std::string_view value) noexcept {
    return !value.empty() && value.size() <= 96U &&
           std::all_of(value.begin(), value.end(), is_identifier_character);
}

[[nodiscard]] const PortSpec* find_port(const std::vector<PortSpec>& ports, const std::string_view name) noexcept {
    const auto found = std::find_if(ports.begin(), ports.end(), [&](const PortSpec& port) { return port.name == name; });
    return found == ports.end() ? nullptr : &*found;
}

[[nodiscard]] const ParameterSpec* find_parameter(
    const std::vector<ParameterSpec>& parameters,
    const std::string_view name) noexcept {
    const auto found = std::find_if(
        parameters.begin(), parameters.end(), [&](const ParameterSpec& parameter) { return parameter.name == name; });
    return found == parameters.end() ? nullptr : &*found;
}

[[nodiscard]] const ParameterAssignment* find_assignment(
    const NodeInstance& node,
    const std::string_view name) noexcept {
    const auto found = std::find_if(
        node.parameters.begin(), node.parameters.end(),
        [name](const ParameterAssignment& assignment) { return assignment.name == name; });
    return found == node.parameters.end() ? nullptr : &*found;
}

[[nodiscard]] ParameterKind parameter_kind(const ParameterValue& value) noexcept {
    if (std::holds_alternative<i64>(value)) {
        return ParameterKind::integer;
    }
    if (std::holds_alternative<double>(value)) {
        return ParameterKind::real;
    }
    if (std::holds_alternative<bool>(value)) {
        return ParameterKind::boolean;
    }
    return ParameterKind::enumeration;
}

[[nodiscard]] bool parameter_in_domain(const ParameterValue& value, const ParameterSpec& spec) {
    switch (spec.kind) {
    case ParameterKind::integer: {
        const i64 integer = std::get<i64>(value);
        if (spec.domain.integer_min.has_value() && integer < *spec.domain.integer_min) {
            return false;
        }
        return !spec.domain.integer_max.has_value() || integer <= *spec.domain.integer_max;
    }
    case ParameterKind::real: {
        const double real = std::get<double>(value);
        if (!std::isfinite(real)) {
            return false;
        }
        if (spec.domain.real_min.has_value() && real < *spec.domain.real_min) {
            return false;
        }
        return !spec.domain.real_max.has_value() || real <= *spec.domain.real_max;
    }
    case ParameterKind::boolean:
        return true;
    case ParameterKind::enumeration: {
        const std::string& enumeration = std::get<std::string>(value);
        return std::find(spec.domain.enum_values.begin(), spec.domain.enum_values.end(), enumeration) !=
               spec.domain.enum_values.end();
    }
    }
    return false;
}

[[nodiscard]] std::optional<long double> numeric_value(const ParameterValue& value) noexcept {
    if (const auto* integer = std::get_if<i64>(&value)) {
        return static_cast<long double>(*integer);
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr && std::isfinite(*real)) {
        return static_cast<long double>(*real);
    }
    return std::nullopt;
}

void add_validation_error(
    std::vector<ValidationError>& errors,
    const ValidationErrorCode code,
    std::string message) {
    errors.push_back(ValidationError{code, std::move(message)});
}

void validate_parameter_relations(
    const NodeInstance& node,
    const NodeMetadata& metadata,
    std::vector<ValidationError>& errors) {
    for (const ParameterRelation& relation : metadata.parameter_relations) {
        const ParameterAssignment* left = find_assignment(node, relation.left);
        const ParameterAssignment* right = find_assignment(node, relation.right);
        if (left == nullptr || right == nullptr) {
            continue;
        }
        const auto left_value = numeric_value(left->value);
        const auto right_value = numeric_value(right->value);
        if (!left_value.has_value() || !right_value.has_value()) {
            continue;
        }

        bool satisfied = false;
        switch (relation.relation) {
        case ParameterRelationKind::less_than:
            satisfied = *left_value < *right_value;
            break;
        }
        if (!satisfied) {
            add_validation_error(
                errors,
                ValidationErrorCode::parameter_out_of_domain,
                "node '" + node.id + "' parameter relation requires '" + relation.left + "' < '" + relation.right + "'");
        }
    }
}

[[nodiscard]] bool numeric_parameter_kind(const ParameterKind kind) noexcept {
    return kind == ParameterKind::integer || kind == ParameterKind::real;
}

}  // namespace

TypeRegistry::TypeRegistry(std::vector<LogicalTypeMetadata> types) : types_(std::move(types)) {
    std::sort(types_.begin(), types_.end(), [](const LogicalTypeMetadata& left, const LogicalTypeMetadata& right) {
        return std::tie(left.type.type_id, left.type.semantic_version) <
               std::tie(right.type.type_id, right.type.semantic_version);
    });
}

const LogicalTypeMetadata* TypeRegistry::find(
    const std::string_view type_id,
    const u32 semantic_version) const noexcept {
    const auto found = std::lower_bound(
        types_.begin(), types_.end(), std::pair<std::string_view, u32>{type_id, semantic_version},
        [](const LogicalTypeMetadata& type, const std::pair<std::string_view, u32>& value) {
            return std::tie(type.type.type_id, type.type.semantic_version) < std::tie(value.first, value.second);
        });
    if (found == types_.end() || found->type.type_id != type_id || found->type.semantic_version != semantic_version) {
        return nullptr;
    }
    return &*found;
}

const LogicalTypeMetadata* TypeRegistry::find_any_version(const std::string_view type_id) const noexcept {
    const auto found = std::lower_bound(
        types_.begin(), types_.end(), type_id,
        [](const LogicalTypeMetadata& type, const std::string_view value) { return type.type.type_id < value; });
    if (found == types_.end() || found->type.type_id != type_id) {
        return nullptr;
    }
    return &*found;
}

bool TypeRegistry::is_compatible(const LogicalTypeRef& source, const LogicalTypeRef& target) const noexcept {
    if (source == target) {
        return true;
    }
    const LogicalTypeMetadata* target_metadata = find(target.type_id, target.semantic_version);
    if (target_metadata == nullptr) {
        return false;
    }
    return std::find(target_metadata->accepted_sources.begin(), target_metadata->accepted_sources.end(), source) !=
           target_metadata->accepted_sources.end();
}

std::string_view to_string(const ParameterKind kind) noexcept {
    switch (kind) {
    case ParameterKind::integer:
        return "i64";
    case ParameterKind::real:
        return "f64";
    case ParameterKind::boolean:
        return "bool";
    case ParameterKind::enumeration:
        return "enum";
    }
    return "unknown";
}

NodeRegistry::NodeRegistry(std::vector<NodeMetadata> nodes) : nodes_(std::move(nodes)) {
    std::sort(nodes_.begin(), nodes_.end(), [](const NodeMetadata& left, const NodeMetadata& right) {
        return std::tie(left.type_id, left.semantic_version) < std::tie(right.type_id, right.semantic_version);
    });
}

const NodeMetadata* NodeRegistry::find(const std::string_view type_id, const u32 semantic_version) const noexcept {
    const auto found = std::lower_bound(
        nodes_.begin(), nodes_.end(), std::pair<std::string_view, u32>{type_id, semantic_version},
        [](const NodeMetadata& node, const std::pair<std::string_view, u32>& value) {
            return std::tie(node.type_id, node.semantic_version) < std::tie(value.first, value.second);
        });
    if (found == nodes_.end() || found->type_id != type_id || found->semantic_version != semantic_version) {
        return nullptr;
    }
    return &*found;
}

const NodeMetadata* NodeRegistry::find_any_version(const std::string_view type_id) const noexcept {
    const auto found = std::lower_bound(
        nodes_.begin(), nodes_.end(), type_id,
        [](const NodeMetadata& node, const std::string_view value) { return node.type_id < value; });
    if (found == nodes_.end() || found->type_id != type_id) {
        return nullptr;
    }
    return &*found;
}

std::vector<RegistryError> validate_registry(const EngineRegistry& registry) {
    std::vector<RegistryError> errors;
    std::set<std::pair<std::string, u32>> seen_types;
    for (const LogicalTypeMetadata& type : registry.types.types()) {
        if (!is_valid_identifier(type.type.type_id)) {
            errors.push_back({RegistryErrorCode::invalid_identifier, "invalid logical type id: " + type.type.type_id});
        }
        if (!seen_types.emplace(type.type.type_id, type.type.semantic_version).second) {
            errors.push_back({RegistryErrorCode::duplicate_logical_type, "duplicate logical type registration: " + type.type.type_id});
        }
        for (const LogicalTypeRef& source : type.accepted_sources) {
            if (registry.types.find(source.type_id, source.semantic_version) == nullptr) {
                errors.push_back({
                    RegistryErrorCode::unknown_compatibility_type,
                    "logical type '" + type.type.type_id + "' accepts unregistered source type '" + source.type_id + "'",
                });
            }
        }
    }

    std::set<std::pair<std::string, u32>> seen_nodes;
    for (const NodeMetadata& node : registry.nodes.nodes()) {
        if (!is_valid_identifier(node.type_id)) {
            errors.push_back({RegistryErrorCode::invalid_identifier, "invalid node type id: " + node.type_id});
        }
        if (!seen_nodes.emplace(node.type_id, node.semantic_version).second) {
            errors.push_back({RegistryErrorCode::duplicate_node_type, "duplicate node type registration: " + node.type_id});
        }
        for (const PortSpec& port : node.inputs) {
            if (registry.types.find(port.type.type_id, port.type.semantic_version) == nullptr) {
                errors.push_back({
                    RegistryErrorCode::unknown_port_type,
                    "node '" + node.type_id + "' input '" + port.name + "' references unregistered logical type '" + port.type.type_id + "'",
                });
            }
        }
        for (const PortSpec& port : node.outputs) {
            if (registry.types.find(port.type.type_id, port.type.semantic_version) == nullptr) {
                errors.push_back({
                    RegistryErrorCode::unknown_port_type,
                    "node '" + node.type_id + "' output '" + port.name + "' references unregistered logical type '" + port.type.type_id + "'",
                });
            }
        }
        for (const ParameterRelation& relation : node.parameter_relations) {
            const ParameterSpec* left = find_parameter(node.parameters, relation.left);
            const ParameterSpec* right = find_parameter(node.parameters, relation.right);
            if (left == nullptr || right == nullptr || !numeric_parameter_kind(left->kind) || !numeric_parameter_kind(right->kind)) {
                errors.push_back({
                    RegistryErrorCode::invalid_parameter_relation,
                    "node '" + node.type_id + "' declares an invalid numeric parameter relation",
                });
            }
        }
    }
    return errors;
}

std::vector<ValidationError> validate_recipe_intrinsic(const Recipe& recipe) {
    std::vector<ValidationError> errors;

    std::size_t parameter_count = 0U;
    bool parameter_limit_exceeded = false;
    for (const NodeInstance& node : recipe.nodes) {
        if (node.parameters.size() > kMaximumRecipeParameters - (std::min)(parameter_count, kMaximumRecipeParameters)) {
            parameter_limit_exceeded = true;
            break;
        }
        parameter_count += node.parameters.size();
    }
    if (recipe.nodes.size() > kMaximumRecipeNodes || recipe.edges.size() > kMaximumRecipeEdges ||
        recipe.outputs.size() > kMaximumRecipeOutputs || recipe.metadata.size() > kMaximumRecipeMetadata ||
        parameter_limit_exceeded || parameter_count > kMaximumRecipeParameters) {
        add_validation_error(errors, ValidationErrorCode::resource_limit, "recipe exceeds configured graph/resource limits");
        return errors;
    }

    if (recipe.schema_version != kRecipeSchemaVersion) {
        add_validation_error(
            errors, ValidationErrorCode::unsupported_schema_version,
            "recipe schema version " + std::to_string(recipe.schema_version) + " is unsupported");
    }
    if (recipe.evaluator_version != kEvaluatorSemanticVersion) {
        add_validation_error(
            errors, ValidationErrorCode::unsupported_evaluator_version,
            "evaluator semantic version " + std::to_string(recipe.evaluator_version) + " is unsupported");
    }

    std::set<std::string, std::less<>> node_ids;
    for (const NodeInstance& node : recipe.nodes) {
        if (!is_valid_identifier(node.id) || !is_valid_identifier(node.type_id)) {
            add_validation_error(
                errors, ValidationErrorCode::invalid_identifier,
                "node ids/type ids must be 1..96 ASCII letters, digits, '.', '_' or '-': " + node.id);
        }
        if (!node_ids.insert(node.id).second) {
            add_validation_error(errors, ValidationErrorCode::duplicate_node_id, "duplicate node id: " + node.id);
        }
        std::set<std::string, std::less<>> parameter_names;
        for (const ParameterAssignment& parameter : node.parameters) {
            if (!is_valid_identifier(parameter.name)) {
                add_validation_error(
                    errors, ValidationErrorCode::invalid_identifier,
                    "invalid parameter identifier on node '" + node.id + "': " + parameter.name);
            }
            if (!parameter_names.insert(parameter.name).second) {
                add_validation_error(
                    errors, ValidationErrorCode::duplicate_parameter,
                    "node '" + node.id + "' has duplicate parameter '" + parameter.name + "'");
            }
        }
    }

    using EdgeKey = std::tuple<std::string, std::string, std::string, std::string>;
    std::set<EdgeKey> seen_edges;
    for (const Edge& edge : recipe.edges) {
        if (!is_valid_identifier(edge.from_node) || !is_valid_identifier(edge.from_port) ||
            !is_valid_identifier(edge.to_node) || !is_valid_identifier(edge.to_port)) {
            add_validation_error(errors, ValidationErrorCode::invalid_identifier, "edge contains an invalid identifier");
        }
        if (!seen_edges.emplace(edge.from_node, edge.from_port, edge.to_node, edge.to_port).second) {
            add_validation_error(errors, ValidationErrorCode::duplicate_edge, "duplicate edge in recipe");
        }
        if (!node_ids.contains(edge.from_node) || !node_ids.contains(edge.to_node)) {
            add_validation_error(
                errors, ValidationErrorCode::missing_node,
                "edge references a missing node: " + edge.from_node + " -> " + edge.to_node);
        }
    }

    std::set<std::string, std::less<>> output_names;
    for (const OutputBinding& output : recipe.outputs) {
        if (!is_valid_identifier(output.name) || !is_valid_identifier(output.node_id) || !is_valid_identifier(output.port)) {
            add_validation_error(errors, ValidationErrorCode::invalid_identifier, "output contains an invalid identifier");
        }
        if (!output_names.insert(output.name).second) {
            add_validation_error(errors, ValidationErrorCode::duplicate_output_name, "duplicate output name: " + output.name);
        }
        if (!node_ids.contains(output.node_id)) {
            add_validation_error(
                errors, ValidationErrorCode::missing_node,
                "output '" + output.name + "' references missing node '" + output.node_id + "'");
        }
    }

    return errors;
}

std::vector<ValidationError> validate_recipe(const Recipe& recipe, const EngineRegistry& registry) {
    std::vector<ValidationError> errors = validate_recipe_intrinsic(recipe);
    const std::vector<RegistryError> registry_errors = validate_registry(registry);
    if (!registry_errors.empty()) {
        for (const RegistryError& error : registry_errors) {
            add_validation_error(errors, ValidationErrorCode::invalid_registry, error.message);
        }
        return errors;
    }
    if (std::any_of(errors.begin(), errors.end(), [](const ValidationError& error) {
            return error.code == ValidationErrorCode::resource_limit;
        })) {
        return errors;
    }

    std::map<std::string, const NodeInstance*, std::less<>> nodes;
    std::map<std::string, const NodeMetadata*, std::less<>> metadata_by_node;
    for (const NodeInstance& node : recipe.nodes) {
        nodes.emplace(node.id, &node);
        const NodeMetadata* metadata = registry.nodes.find(node.type_id, node.semantic_version);
        if (metadata == nullptr) {
            if (registry.nodes.find_any_version(node.type_id) != nullptr) {
                add_validation_error(
                    errors, ValidationErrorCode::unsupported_node_version,
                    "node '" + node.id + "' requests unsupported semantic version " + std::to_string(node.semantic_version) +
                        " for type '" + node.type_id + "'");
            } else {
                add_validation_error(errors, ValidationErrorCode::unknown_node_type, "unknown node type: " + node.type_id);
            }
            metadata_by_node.emplace(node.id, nullptr);
            continue;
        }
        metadata_by_node.emplace(node.id, metadata);

        std::set<std::string, std::less<>> seen_parameters;
        for (const ParameterAssignment& assignment : node.parameters) {
            if (!seen_parameters.insert(assignment.name).second) {
                continue;  // intrinsic validation already reports duplicates.
            }
            const ParameterSpec* spec = find_parameter(metadata->parameters, assignment.name);
            if (spec == nullptr) {
                add_validation_error(
                    errors, ValidationErrorCode::unknown_parameter,
                    "node '" + node.id + "' has unknown parameter '" + assignment.name + "'");
                continue;
            }
            const ParameterKind actual_kind = parameter_kind(assignment.value);
            if (actual_kind != spec->kind) {
                add_validation_error(
                    errors, ValidationErrorCode::parameter_type_mismatch,
                    "node '" + node.id + "' parameter '" + assignment.name + "' expected " +
                        std::string(to_string(spec->kind)) + " but received " + std::string(to_string(actual_kind)));
                continue;
            }
            if (!parameter_in_domain(assignment.value, *spec)) {
                add_validation_error(
                    errors, ValidationErrorCode::parameter_out_of_domain,
                    "node '" + node.id + "' parameter '" + assignment.name + "' is outside its declared domain");
            }
        }
        for (const ParameterSpec& spec : metadata->parameters) {
            if (!seen_parameters.contains(spec.name)) {
                add_validation_error(
                    errors, ValidationErrorCode::missing_parameter,
                    "node '" + node.id + "' is missing explicit parameter '" + spec.name + "'");
            }
        }
        validate_parameter_relations(node, *metadata, errors);
    }

    std::map<std::pair<std::string, std::string>, std::size_t> input_edge_counts;
    std::map<std::string, std::size_t, std::less<>> indegree;
    std::map<std::string, std::vector<std::string>, std::less<>> adjacency;
    for (const NodeInstance& node : recipe.nodes) {
        indegree.emplace(node.id, 0U);
        adjacency.emplace(node.id, std::vector<std::string>{});
    }

    for (const Edge& edge : recipe.edges) {
        const auto from_node = metadata_by_node.find(edge.from_node);
        const auto to_node = metadata_by_node.find(edge.to_node);
        if (from_node == metadata_by_node.end() || to_node == metadata_by_node.end() ||
            from_node->second == nullptr || to_node->second == nullptr) {
            continue;
        }
        const PortSpec* from_port = find_port(from_node->second->outputs, edge.from_port);
        const PortSpec* to_port = find_port(to_node->second->inputs, edge.to_port);
        if (from_port == nullptr || to_port == nullptr) {
            add_validation_error(
                errors, ValidationErrorCode::unknown_port,
                "edge references an unknown output/input port: " + edge.from_node + "." + edge.from_port + " -> " +
                    edge.to_node + "." + edge.to_port);
            continue;
        }

        const LogicalTypeMetadata* source_type = registry.types.find(
            from_port->type.type_id, from_port->type.semantic_version);
        const LogicalTypeMetadata* target_type = registry.types.find(
            to_port->type.type_id, to_port->type.semantic_version);
        if (source_type == nullptr || target_type == nullptr) {
            const LogicalTypeRef& missing = source_type == nullptr ? from_port->type : to_port->type;
            const ValidationErrorCode code = registry.types.find_any_version(missing.type_id) == nullptr
                ? ValidationErrorCode::unknown_logical_type
                : ValidationErrorCode::unsupported_logical_type_version;
            add_validation_error(
                errors, code,
                "port references unavailable logical type '" + missing.type_id + "' version " +
                    std::to_string(missing.semantic_version));
            continue;
        }
        if (!registry.types.is_compatible(from_port->type, to_port->type)) {
            add_validation_error(
                errors, ValidationErrorCode::incompatible_port_type,
                "edge type mismatch: '" + from_port->type.type_id + "' -> '" + to_port->type.type_id + "'");
        }

        const auto input_key = std::make_pair(edge.to_node, edge.to_port);
        const std::size_t count = ++input_edge_counts[input_key];
        if (count > 1U && !to_port->allow_multiple) {
            add_validation_error(
                errors, ValidationErrorCode::multiple_input_edges,
                "input port accepts only one edge: " + edge.to_node + "." + edge.to_port);
        }

        if (to_node->second->state_class != NodeStateClass::state_boundary) {
            adjacency[edge.from_node].push_back(edge.to_node);
            ++indegree[edge.to_node];
        }
    }

    for (const auto& [node_id, metadata] : metadata_by_node) {
        if (metadata == nullptr) {
            continue;
        }
        for (const PortSpec& input : metadata->inputs) {
            if (input.required && input_edge_counts[{node_id, input.name}] == 0U) {
                add_validation_error(
                    errors, ValidationErrorCode::missing_required_input,
                    "node '" + node_id + "' is missing required input '" + input.name + "'");
            }
        }
    }

    for (const OutputBinding& output : recipe.outputs) {
        const auto metadata = metadata_by_node.find(output.node_id);
        if (metadata != metadata_by_node.end() && metadata->second != nullptr &&
            find_port(metadata->second->outputs, output.port) == nullptr) {
            add_validation_error(
                errors, ValidationErrorCode::unknown_port,
                "output '" + output.name + "' references unknown output port '" + output.node_id + "." + output.port + "'");
        }
    }

    std::set<std::string, std::less<>> ready;
    for (const auto& [node_id, count] : indegree) {
        if (count == 0U) {
            ready.insert(node_id);
        }
    }
    std::size_t visited = 0U;
    while (!ready.empty()) {
        const std::string node = *ready.begin();
        ready.erase(ready.begin());
        ++visited;
        for (const std::string& next : adjacency[node]) {
            std::size_t& count = indegree[next];
            --count;
            if (count == 0U) {
                ready.insert(next);
            }
        }
    }
    if (visited != recipe.nodes.size()) {
        add_validation_error(
            errors, ValidationErrorCode::cycle_detected,
            "ordinary graph cycles are invalid; feedback is legal only through an explicit state-boundary node");
    }

    return errors;
}

}  // namespace syndata::engine
