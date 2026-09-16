#include "engine/recipe.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <tuple>

#include "engine/hash.hpp"

namespace syndata::engine {
namespace {

[[nodiscard]] std::string quote_token(const std::string_view token) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string output;
    output.reserve(token.size() + 2U);
    output.push_back('"');
    for (const unsigned char byte : token) {
        switch (byte) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (byte < 0x20U) {
                output += "\\x";
                output.push_back(kHex[(byte >> 4U) & 0x0fU]);
                output.push_back(kHex[byte & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(byte));
            }
            break;
        }
    }
    output.push_back('"');
    return output;
}

[[nodiscard]] std::string canonical_real(const double value) {
    if (value == 0.0) {
        return "0";
    }
    char buffer[64]{};
    const auto converted = std::to_chars(
        buffer,
        buffer + sizeof(buffer),
        value,
        std::chars_format::general,
        std::numeric_limits<double>::max_digits10);
    if (converted.ec != std::errc{}) {
        return "0";
    }
    return std::string(buffer, converted.ptr);
}

[[nodiscard]] std::string parameter_type_tag(const ParameterValue& value) {
    if (std::holds_alternative<i64>(value)) {
        return "i64";
    }
    if (std::holds_alternative<double>(value)) {
        return "f64";
    }
    if (std::holds_alternative<bool>(value)) {
        return "bool";
    }
    return "enum";
}

[[nodiscard]] std::string parameter_value_text(const ParameterValue& value) {
    if (const auto* integer = std::get_if<i64>(&value)) {
        return std::to_string(*integer);
    }
    if (const auto* real = std::get_if<double>(&value)) {
        return canonical_real(*real);
    }
    if (const auto* boolean = std::get_if<bool>(&value)) {
        return *boolean ? "true" : "false";
    }
    return quote_token(std::get<std::string>(value));
}

[[nodiscard]] std::string serialize_impl(const Recipe& recipe, const bool include_metadata) {
    std::string output;
    // This literal preserves the upstream AM-016 regression byte stream only.
    // SD-001 does not expose a parser or file extension for it.
    output += "amr " + std::to_string(recipe.schema_version) + "\n";
    output += "evaluator " + std::to_string(recipe.evaluator_version) + "\n";
    output += "seed " + std::to_string(recipe.root_seed) + "\n";
    output += "render " + std::to_string(recipe.render.width) + " " + std::to_string(recipe.render.height) + " " +
              quote_token(recipe.render.quality) + "\n";

    std::vector<const NodeInstance*> nodes;
    nodes.reserve(recipe.nodes.size());
    for (const auto& node : recipe.nodes) {
        nodes.push_back(&node);
    }
    std::sort(nodes.begin(), nodes.end(), [](const NodeInstance* left, const NodeInstance* right) {
        return std::tie(left->id, left->type_id, left->semantic_version) <
               std::tie(right->id, right->type_id, right->semantic_version);
    });

    for (const NodeInstance* node : nodes) {
        output += "node " + quote_token(node->id) + " " + quote_token(node->type_id) + " " +
                  std::to_string(node->semantic_version) + "\n";

        std::vector<const ParameterAssignment*> parameters;
        parameters.reserve(node->parameters.size());
        for (const auto& parameter : node->parameters) {
            parameters.push_back(&parameter);
        }
        std::sort(parameters.begin(), parameters.end(), [](const ParameterAssignment* left, const ParameterAssignment* right) {
            if (left->name != right->name) {
                return left->name < right->name;
            }
            if (left->value.index() != right->value.index()) {
                return left->value.index() < right->value.index();
            }
            return parameter_value_text(left->value) < parameter_value_text(right->value);
        });

        for (const ParameterAssignment* parameter : parameters) {
            output += "param " + quote_token(node->id) + " " + quote_token(parameter->name) + " " +
                      parameter_type_tag(parameter->value) + " " + parameter_value_text(parameter->value) + "\n";
        }
    }

    std::vector<Edge> edges = recipe.edges;
    std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) {
        return std::tie(left.from_node, left.from_port, left.to_node, left.to_port) <
               std::tie(right.from_node, right.from_port, right.to_node, right.to_port);
    });
    for (const auto& edge : edges) {
        output += "edge " + quote_token(edge.from_node) + " " + quote_token(edge.from_port) + " " +
                  quote_token(edge.to_node) + " " + quote_token(edge.to_port) + "\n";
    }

    std::vector<OutputBinding> outputs = recipe.outputs;
    std::sort(outputs.begin(), outputs.end(), [](const OutputBinding& left, const OutputBinding& right) {
        return std::tie(left.name, left.node_id, left.port) < std::tie(right.name, right.node_id, right.port);
    });
    for (const auto& binding : outputs) {
        output += "output " + quote_token(binding.name) + " " + quote_token(binding.node_id) + " " +
                  quote_token(binding.port) + "\n";
    }

    if (include_metadata) {
        std::vector<RecipeMetadata> metadata = recipe.metadata;
        std::sort(metadata.begin(), metadata.end(), [](const RecipeMetadata& left, const RecipeMetadata& right) {
            return std::tie(left.key, left.value) < std::tie(right.key, right.value);
        });
        for (const auto& item : metadata) {
            output += "meta " + quote_token(item.key) + " " + quote_token(item.value) + "\n";
        }
    }

    return output;
}

}  // namespace

std::string serialize_inherited_recipe_canonical(const Recipe& recipe) {
    return serialize_impl(recipe, true);
}

std::string serialize_inherited_recipe_semantic(const Recipe& recipe) {
    return serialize_impl(recipe, false);
}

std::string inherited_semantic_fingerprint(
    const Recipe& recipe,
    const std::string_view domain_separator) {
    const std::string semantic = serialize_inherited_recipe_semantic(recipe);
    const u64 primary = fnv1a64(semantic);
    std::string domain_separated(domain_separator);
    domain_separated += semantic;
    const u64 secondary = fnv1a64(domain_separated);
    return hex_u64(primary) + hex_u64(secondary);
}

}  // namespace syndata::engine
