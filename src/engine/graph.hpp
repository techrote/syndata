#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "engine/types.hpp"

namespace syndata::engine {

struct LogicalTypeRef final {
    std::string type_id;
    u32 semantic_version{1U};

    friend bool operator==(const LogicalTypeRef&, const LogicalTypeRef&) = default;
};

struct LogicalTypeMetadata final {
    LogicalTypeRef type;
    // An input expecting this logical type also accepts these explicitly
    // registered source types. Exact type/version identity is always accepted.
    std::vector<LogicalTypeRef> accepted_sources;
};

class TypeRegistry final {
public:
    explicit TypeRegistry(std::vector<LogicalTypeMetadata> types = {});

    [[nodiscard]] const LogicalTypeMetadata* find(std::string_view type_id, u32 semantic_version) const noexcept;
    [[nodiscard]] const LogicalTypeMetadata* find_any_version(std::string_view type_id) const noexcept;
    [[nodiscard]] bool is_compatible(const LogicalTypeRef& source, const LogicalTypeRef& target) const noexcept;
    [[nodiscard]] const std::vector<LogicalTypeMetadata>& types() const noexcept { return types_; }

private:
    std::vector<LogicalTypeMetadata> types_;
};

enum class ParameterKind {
    integer,
    real,
    boolean,
    enumeration,
};

[[nodiscard]] std::string_view to_string(ParameterKind kind) noexcept;

using ParameterValue = std::variant<i64, double, bool, std::string>;

struct ParameterDomain {
    std::optional<i64> integer_min;
    std::optional<i64> integer_max;
    std::optional<double> real_min;
    std::optional<double> real_max;
    std::vector<std::string> enum_values;
};

enum class MutationScale {
    none,
    linear,
    logarithmic,
    periodic,
    discrete,
};

struct MutationMetadata {
    bool mutable_parameter{true};
    MutationScale scale{MutationScale::linear};
    std::string group;
};

struct PortSpec {
    std::string name;
    LogicalTypeRef type;
    bool required{true};
    bool allow_multiple{false};
};

struct ParameterSpec {
    std::string name;
    ParameterKind kind{ParameterKind::real};
    ParameterValue default_value{0.0};
    ParameterDomain domain;
    MutationMetadata mutation;
};

enum class ParameterRelationKind {
    less_than,
};

struct ParameterRelation {
    std::string left;
    ParameterRelationKind relation{ParameterRelationKind::less_than};
    std::string right;
};

enum class NodeStateClass {
    stateless,
    stateful,
    state_boundary,
};

struct NodeMetadata {
    std::string type_id;
    u32 semantic_version{1U};
    std::vector<PortSpec> inputs;
    std::vector<PortSpec> outputs;
    std::vector<ParameterSpec> parameters;
    NodeStateClass state_class{NodeStateClass::stateless};
    std::vector<ParameterRelation> parameter_relations;
};

class NodeRegistry final {
public:
    explicit NodeRegistry(std::vector<NodeMetadata> nodes = {});

    [[nodiscard]] const NodeMetadata* find(std::string_view type_id, u32 semantic_version) const noexcept;
    [[nodiscard]] const NodeMetadata* find_any_version(std::string_view type_id) const noexcept;
    [[nodiscard]] const std::vector<NodeMetadata>& nodes() const noexcept { return nodes_; }

private:
    std::vector<NodeMetadata> nodes_;
};

struct EngineRegistry final {
    TypeRegistry types;
    NodeRegistry nodes;
};

enum class RegistryErrorCode {
    invalid_identifier,
    duplicate_logical_type,
    duplicate_node_type,
    unknown_compatibility_type,
    unknown_port_type,
    invalid_parameter_relation,
};

struct RegistryError final {
    RegistryErrorCode code{};
    std::string message;
};

[[nodiscard]] std::vector<RegistryError> validate_registry(const EngineRegistry& registry);

struct Recipe;

enum class ValidationErrorCode {
    unsupported_schema_version,
    unsupported_evaluator_version,
    resource_limit,
    invalid_identifier,
    duplicate_node_id,
    unknown_node_type,
    unsupported_node_version,
    unknown_logical_type,
    unsupported_logical_type_version,
    duplicate_parameter,
    unknown_parameter,
    missing_parameter,
    parameter_type_mismatch,
    parameter_out_of_domain,
    missing_node,
    unknown_port,
    incompatible_port_type,
    duplicate_edge,
    multiple_input_edges,
    missing_required_input,
    duplicate_output_name,
    cycle_detected,
    invalid_registry,
};

struct ValidationError {
    ValidationErrorCode code{};
    std::string message;
};

// Intrinsic validation checks product-native schema/resource/identifier/reference
// invariants that do not require a compiled domain catalog.
[[nodiscard]] std::vector<ValidationError> validate_recipe_intrinsic(const Recipe& recipe);

// Full validation additionally resolves node/port/logical-type semantics against
// a caller-supplied registry. The generic engine owns no built-in domain catalog.
[[nodiscard]] std::vector<ValidationError> validate_recipe(
    const Recipe& recipe,
    const EngineRegistry& registry);

}  // namespace syndata::engine
