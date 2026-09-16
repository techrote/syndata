#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/execution.hpp"
#include "engine/graph.hpp"
#include "engine/recipe.hpp"

namespace {

int g_failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

[[nodiscard]] syndata::engine::ParameterSpec integer_parameter(
    std::string name,
    const syndata::engine::i64 default_value,
    const syndata::engine::i64 minimum,
    const syndata::engine::i64 maximum) {
    using namespace syndata::engine;
    ParameterDomain domain;
    domain.integer_min = minimum;
    domain.integer_max = maximum;
    return ParameterSpec{
        std::move(name),
        ParameterKind::integer,
        default_value,
        std::move(domain),
        MutationMetadata{true, MutationScale::discrete, "window"},
    };
}

[[nodiscard]] syndata::engine::EngineRegistry make_registry() {
    using namespace syndata::engine;
    std::vector<LogicalTypeMetadata> types;
    types.push_back(LogicalTypeMetadata{LogicalTypeRef{"example.scalar", 1U}, {}});
    types.push_back(LogicalTypeMetadata{
        LogicalTypeRef{"example.mask", 1U},
        {LogicalTypeRef{"example.scalar", 1U}},
    });

    std::vector<NodeMetadata> nodes;
    nodes.push_back(NodeMetadata{
        "example.source",
        1U,
        {},
        {PortSpec{"value", LogicalTypeRef{"example.scalar", 1U}, false, false}},
        {},
        NodeStateClass::stateless,
        {},
    });
    nodes.push_back(NodeMetadata{
        "example.filter",
        1U,
        {PortSpec{"input", LogicalTypeRef{"example.mask", 1U}, true, false}},
        {PortSpec{"value", LogicalTypeRef{"example.mask", 1U}, false, false}},
        {
            integer_parameter("low", 1, 0, 10),
            integer_parameter("high", 2, 0, 10),
        },
        NodeStateClass::stateless,
        {ParameterRelation{"low", ParameterRelationKind::less_than, "high"}},
    });
    nodes.push_back(NodeMetadata{
        "example.delay",
        1U,
        {PortSpec{"next", LogicalTypeRef{"example.mask", 1U}, true, false}},
        {PortSpec{"previous", LogicalTypeRef{"example.mask", 1U}, false, false}},
        {},
        NodeStateClass::state_boundary,
        {},
    });
    return EngineRegistry{TypeRegistry(std::move(types)), NodeRegistry(std::move(nodes))};
}

[[nodiscard]] std::string_view fixture_text() {
    return R"SDR(# deliberately non-canonical order
sdr 1
evaluator 1
seed 424242
node z_source example.source 1
node m_filter example.filter 1
param m_filter low i64 1
param m_filter high i64 2
edge z_source value m_filter input
output main m_filter value
meta purpose sd002
)SDR";
}

[[nodiscard]] syndata::engine::Recipe parse_fixture() {
    auto parsed = syndata::engine::parse_recipe(fixture_text());
    expect(parsed.is_ok(), "SDR fixture parses");
    if (parsed.is_error()) {
        return {};
    }
    return std::move(parsed).value();
}

[[nodiscard]] bool has_error(
    const std::vector<syndata::engine::ValidationError>& errors,
    const syndata::engine::ValidationErrorCode code) {
    return std::any_of(errors.begin(), errors.end(), [code](const syndata::engine::ValidationError& error) {
        return error.code == code;
    });
}

void test_native_format_round_trip_and_vector() {
    using namespace syndata::engine;
    const EngineRegistry registry = make_registry();
    Recipe recipe = parse_fixture();
    expect(validate_registry(registry).empty(), "custom registry is valid");
    expect(validate_recipe_intrinsic(recipe).empty(), "fixture passes intrinsic validation");
    expect(validate_recipe(recipe, registry).empty(), "fixture passes full custom-domain validation");

    const std::string fingerprint = semantic_fingerprint(recipe);
    expect(
        fingerprint == "2bc0a0a5eca802b702c6290a34190cba",
        "SynData SDR semantic fingerprint reference vector");

    const std::string canonical = serialize_recipe_canonical(recipe);
    expect(canonical.rfind("sdr 1\n", 0U) == 0U, "canonical recipe has SynData SDR identity");
    expect(canonical.find("amr ") == std::string::npos, "canonical recipe never emits ArtMiner AMR identity");
    auto reparsed = parse_recipe(canonical);
    expect(reparsed.is_ok(), "canonical SDR reparses");
    if (reparsed.is_ok()) {
        expect(semantic_fingerprint(reparsed.value()) == fingerprint, "canonical round trip preserves fingerprint");
        expect(serialize_recipe_canonical(reparsed.value()) == canonical, "canonical round trip is byte-stable");
    }

    recipe.metadata.push_back(RecipeMetadata{"note", "non-semantic"});
    expect(semantic_fingerprint(recipe) == fingerprint, "descriptive metadata does not alter semantic identity");
}

void test_artminer_identity_and_malformed_inputs_are_rejected() {
    using namespace syndata::engine;
    expect(parse_recipe("amr 1\nevaluator 1\nseed 1\n").is_error(), "ArtMiner AMR is not reinterpreted as SDR");
    expect(parse_recipe("sdr 2\nevaluator 1\nseed 1\n").is_error(), "unsupported schema version is rejected");
    expect(parse_recipe("sdr 1\nevaluator 9\nseed 1\n").is_error(), "unsupported evaluator version is rejected");
    expect(parse_recipe("sdr 1\nevaluator 1\nseed 1\nwat 2\n").is_error(), "unknown semantic record is rejected");
}

void test_types_versions_relations_and_limits() {
    using namespace syndata::engine;
    const EngineRegistry registry = make_registry();
    Recipe recipe = parse_fixture();

    recipe.nodes.front().semantic_version = 9U;
    expect(
        has_error(validate_recipe(recipe, registry), ValidationErrorCode::unsupported_node_version),
        "unsupported node semantic version fails explicitly");

    recipe = parse_fixture();
    recipe.nodes.front().type_id = "example.unknown";
    expect(
        has_error(validate_recipe(recipe, registry), ValidationErrorCode::unknown_node_type),
        "unknown node type fails explicitly");

    recipe = parse_fixture();
    for (ParameterAssignment& parameter : recipe.nodes[1].parameters) {
        if (parameter.name == "low") {
            parameter.value = static_cast<i64>(3);
        } else if (parameter.name == "high") {
            parameter.value = static_cast<i64>(2);
        }
    }
    expect(
        has_error(validate_recipe(recipe, registry), ValidationErrorCode::parameter_out_of_domain),
        "catalog-declared cross-parameter relation is enforced generically");

    recipe = parse_fixture();
    recipe.nodes.resize(kMaximumRecipeNodes + 1U);
    const auto oversized = validate_recipe_intrinsic(recipe);
    expect(
        !oversized.empty() && oversized.front().code == ValidationErrorCode::resource_limit,
        "programmatic graph resource overflow fails before expensive validation");

    std::vector<LogicalTypeMetadata> bad_types;
    bad_types.push_back(LogicalTypeMetadata{LogicalTypeRef{"example.scalar", 1U}, {}});
    std::vector<NodeMetadata> bad_nodes;
    bad_nodes.push_back(NodeMetadata{
        "example.bad",
        1U,
        {},
        {PortSpec{"value", LogicalTypeRef{"example.scalar", 2U}, false, false}},
        {},
        NodeStateClass::stateless,
        {},
    });
    const EngineRegistry bad_registry{TypeRegistry(std::move(bad_types)), NodeRegistry(std::move(bad_nodes))};
    expect(!validate_registry(bad_registry).empty(), "unregistered logical type version fails registry validation");
}

void test_execution_order_is_insertion_independent() {
    using namespace syndata::engine;
    const EngineRegistry registry = make_registry();

    Recipe first;
    first.root_seed = 1U;
    first.nodes.push_back(NodeInstance{"z_source", "example.source", 1U, {}});
    first.nodes.push_back(NodeInstance{"a_source", "example.source", 1U, {}});

    Recipe second = first;
    std::reverse(second.nodes.begin(), second.nodes.end());

    auto first_plan = build_execution_plan(first, registry);
    auto second_plan = build_execution_plan(second, registry);
    expect(first_plan.is_ok() && second_plan.is_ok(), "independent-node execution plans build");
    if (first_plan.is_ok() && second_plan.is_ok()) {
        expect(first_plan.value().steps.size() == 2U, "execution plan has two steps");
        expect(first_plan.value().steps[0].node_id == "a_source", "ready nodes use stable lexical tie-break");
        expect(first_plan.value().steps[1].node_id == "z_source", "stable order includes second source");
        expect(
            first_plan.value().steps[0].node_id == second_plan.value().steps[0].node_id &&
                first_plan.value().steps[1].node_id == second_plan.value().steps[1].node_id,
            "execution order is independent of recipe insertion order");
    }

    ExecutionLimits tiny;
    tiny.maximum_nodes = 1U;
    expect(
        build_execution_plan(first, registry, tiny).is_error(),
        "execution resource preflight rejects oversized request before planning");
}

void test_state_boundary_breaks_same_tick_cycle() {
    using namespace syndata::engine;
    const EngineRegistry registry = make_registry();
    Recipe recipe;
    recipe.root_seed = 8U;
    recipe.nodes.push_back(NodeInstance{"delay", "example.delay", 1U, {}});
    recipe.nodes.push_back(NodeInstance{
        "filter",
        "example.filter",
        1U,
        {ParameterAssignment{"low", static_cast<i64>(1)}, ParameterAssignment{"high", static_cast<i64>(2)}},
    });
    recipe.edges.push_back(Edge{"delay", "previous", "filter", "input"});
    recipe.edges.push_back(Edge{"filter", "value", "delay", "next"});
    recipe.outputs.push_back(OutputBinding{"main", "filter", "value"});

    expect(validate_recipe(recipe, registry).empty(), "explicit state boundary makes feedback legal");
    auto plan = build_execution_plan(recipe, registry);
    expect(plan.is_ok(), "feedback recipe builds execution plan");
    if (plan.is_ok()) {
        expect(plan.value().steps.size() == 2U, "feedback plan has both nodes");
        expect(plan.value().steps[0].node_id == "delay", "state boundary can execute before same-tick consumer");
        expect(plan.value().steps[1].node_id == "filter", "consumer follows state-boundary output");
        expect(plan.value().state_boundary_edges.size() == 1U, "previous-state edge is recorded separately");
    }
}

syndata::engine::Result<void, syndata::engine::EvaluationError> counting_evaluator(
    const syndata::engine::NodeInstance&,
    syndata::engine::EvaluationContext& context) {
    if (context.user_context == nullptr) {
        return syndata::engine::Result<void, syndata::engine::EvaluationError>::failure({
            syndata::engine::EvaluationErrorCode::evaluator_failed,
            "missing test context",
        });
    }
    ++*static_cast<int*>(context.user_context);
    return syndata::engine::Result<void, syndata::engine::EvaluationError>::success();
}

void test_domain_owned_evaluator_dispatch() {
    using namespace syndata::engine;
    EvaluatorRegistry evaluators({
        EvaluatorBinding{"example.source", 1U, "test.source.cpu", 1U, &counting_evaluator},
        EvaluatorBinding{"example.filter", 1U, "test.filter.cpu", 1U, &counting_evaluator},
    });
    int calls = 0;
    EvaluationContext context{&calls};
    NodeInstance source{"source", "example.source", 1U, {}};
    expect(evaluators.dispatch(source, context).is_ok(), "registered domain evaluator dispatches without type switch");
    expect(calls == 1, "domain evaluator callback ran");
    source.semantic_version = 2U;
    auto missing = evaluators.dispatch(source, context);
    expect(
        missing.is_error() && missing.error().code == EvaluationErrorCode::missing_evaluator,
        "unregistered evaluator version fails explicitly");
}

}  // namespace

int main() {
    test_native_format_round_trip_and_vector();
    test_artminer_identity_and_malformed_inputs_are_rejected();
    test_types_versions_relations_and_limits();
    test_execution_order_is_insertion_independent();
    test_state_boundary_breaks_same_tick_cycle();
    test_domain_owned_evaluator_dispatch();

    if (g_failures != 0) {
        std::cerr << g_failures << " SD-002 recipe/execution test(s) failed\n";
        return 1;
    }
    std::cout << "SD-002 recipe/type/execution tests passed\n";
    return 0;
}
