#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/checked_math.hpp"
#include "engine/graph.hpp"
#include "engine/hash.hpp"
#include "engine/local_text.hpp"
#include "engine/prng.hpp"
#include "engine/recipe.hpp"
#include "engine/result.hpp"

namespace {

int g_failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

[[nodiscard]] syndata::engine::ParameterSpec real_parameter(
    std::string name,
    const double default_value,
    const double minimum,
    const double maximum) {
    using namespace syndata::engine;
    ParameterDomain domain;
    domain.real_min = minimum;
    domain.real_max = maximum;
    return ParameterSpec{
        std::move(name),
        ParameterKind::real,
        default_value,
        std::move(domain),
        MutationMetadata{true, MutationScale::linear, "sample"},
    };
}

[[nodiscard]] syndata::engine::NodeRegistry make_custom_registry() {
    using namespace syndata::engine;
    NodeMetadata sample{
        "example.measure.window",
        1U,
        {},
        {PortSpec{"value", DataKind::scalar_field, false, false}},
        {
            real_parameter("low", 0.2, 0.0, 1.0),
            real_parameter("high", 0.8, 0.0, 1.0),
        },
        NodeStateClass::stateless,
        EvaluatorCapabilities{false, false},
        {ParameterRelation{"low", ParameterRelationKind::less_than, "high"}},
    };
    return NodeRegistry({std::move(sample)});
}

[[nodiscard]] syndata::engine::Recipe make_custom_recipe() {
    using namespace syndata::engine;
    Recipe recipe;
    recipe.schema_version = kInheritedRecipeSchemaVersion;
    recipe.evaluator_version = kInheritedEvaluatorSemanticVersion;
    recipe.root_seed = 424242ULL;
    recipe.render = RenderSettings{32U, 24U, "reference"};
    recipe.nodes.push_back(NodeInstance{
        "sample",
        "example.measure.window",
        1U,
        {
            ParameterAssignment{"low", 0.2},
            ParameterAssignment{"high", 0.8},
        },
    });
    recipe.outputs.push_back(OutputBinding{"main", "sample", "value"});
    recipe.metadata.push_back(RecipeMetadata{"purpose", "engine-only-custom-catalog"});
    return recipe;
}

void test_splitmix64_vectors() {
    using syndata::engine::splitmix64;
    expect(splitmix64(0x0000000000000000ULL) == 0xe220a8397b1dcdafULL, "SplitMix64 vector 0");
    expect(splitmix64(0x0000000000000001ULL) == 0x910a2dec89025cc1ULL, "SplitMix64 vector 1");
    expect(splitmix64(0x0123456789abcdefULL) == 0x157a3807a48faa9dULL, "SplitMix64 vector 2");
    expect(splitmix64(0xffffffffffffffffULL) == 0xe4d971771b652c20ULL, "SplitMix64 vector 3");
}

void test_seed_derivation_vectors() {
    using syndata::engine::derive_seed;
    expect(derive_seed(0ULL, 0ULL) == 0xa706dd2f4d197e6fULL, "derive_seed vector 0");
    expect(derive_seed(1ULL, 2ULL) == 0xe06dd043328bd285ULL, "derive_seed vector 1");
    expect(
        derive_seed(0x0123456789abcdefULL, 0x0fedcba987654321ULL) == 0xab0d666b1c2a7065ULL,
        "derive_seed vector 2");
    expect(derive_seed(42ULL, 54ULL) == 0xbf411dba522b2d0cULL, "derive_seed vector 3");
}

void test_pcg32_vectors() {
    using syndata::engine::Pcg32;
    using syndata::engine::u32;
    constexpr std::array<u32, 10> expected{
        0xa15c02b7U, 0x7b47f409U, 0xba1d3330U, 0x83d2f293U, 0xbfa4784bU,
        0xcbed606eU, 0xbfc6a3adU, 0x812fff6dU, 0xe61f305aU, 0xf9384b90U,
    };
    Pcg32 generator(42ULL, 54ULL);
    for (const u32 expected_value : expected) {
        expect(generator.next_u32() == expected_value, "PCG32 reference vector");
    }
}

void test_hash_vectors() {
    using syndata::engine::fnv1a64;
    using syndata::engine::hex_u64;
    expect(fnv1a64(std::string_view{}) == 0xcbf29ce484222325ULL, "FNV-1a empty vector");
    expect(fnv1a64("a") == 0xaf63dc4c8601ec8cULL, "FNV-1a a vector");
    // This literal is retained only as an immutable ancestry vector from AM-001.
    expect(fnv1a64("ArtMiner") == 0x1d19c8c4094e4d49ULL, "upstream FNV-1a product-name vector");
    expect(fnv1a64("deterministic") == 0x97f2ebf85d31152dULL, "FNV-1a deterministic vector");
    expect(hex_u64(0x0123456789abcdefULL) == "0123456789abcdef", "stable u64 hex formatting");
}

void test_checked_math_and_text_bounds() {
    using namespace syndata::engine;
    auto bytes = checked_image_byte_count(1920U, 1080U, 4U);
    expect(bytes.is_ok() && bytes.value() == 8294400ULL, "checked image byte count");
    expect(checked_image_byte_count(0U, 1080U, 4U).is_error(), "zero image dimension rejected");
    expect(checked_multiply_u64((std::numeric_limits<u64>::max)(), 2ULL).is_error(), "multiply overflow rejected");

    expect(is_valid_utf8("SynData \xe2\x9c\xa8"), "valid UTF-8 accepted");
    expect(!is_valid_utf8(std::string_view("\xc0\xaf", 2U)), "overlong UTF-8 rejected");
    expect(!is_valid_utf8(std::string_view("a\0b", 3U)), "embedded NUL rejected");
    expect(validate_local_text("one\ntwo\n", 64U, 8U).is_ok(), "bounded local text accepted");
    expect(validate_local_text(std::string(9U, 'x'), 64U, 8U).is_error(), "overlong local line rejected");
}

void test_custom_catalog_and_inherited_fingerprint() {
    using namespace syndata::engine;
    const NodeRegistry registry = make_custom_registry();
    expect(registry.find("example.measure.window") != nullptr, "custom node is registered");
    expect(registry.find("core.scalar.constant") == nullptr, "engine contains no inherited built-in node catalog");

    Recipe recipe = make_custom_recipe();
    expect(validate_recipe(recipe, registry).empty(), "custom-catalog recipe validates");

    // Metadata is deliberately non-semantic in the inherited canonical contract.
    constexpr std::string_view kUpstreamFingerprintDomain = "ArtMiner.SemanticFingerprint.v1\n";
    const std::string before = inherited_semantic_fingerprint(recipe, kUpstreamFingerprintDomain);
    recipe.metadata.push_back(RecipeMetadata{"note", "non-semantic"});
    const std::string after = inherited_semantic_fingerprint(recipe, kUpstreamFingerprintDomain);
    expect(before == after, "non-semantic metadata does not change inherited semantic fingerprint");

    // Parameter insertion order must not affect the canonical byte stream/fingerprint.
    std::swap(recipe.nodes.front().parameters[0], recipe.nodes.front().parameters[1]);
    expect(
        inherited_semantic_fingerprint(recipe, kUpstreamFingerprintDomain) == before,
        "canonical fingerprint is independent of parameter insertion order");

    const std::string canonical = serialize_inherited_recipe_canonical(recipe);
    expect(canonical.find("meta \"note\" \"non-semantic\"") != std::string::npos, "canonical form retains sorted metadata");
    expect(serialize_inherited_recipe_semantic(recipe).find("meta ") == std::string::npos, "semantic form excludes metadata");

    recipe.nodes.front().parameters[0].value = 0.1;
    recipe.nodes.front().parameters[1].value = 0.2;
    // Locate by name because the order was intentionally swapped above.
    for (auto& parameter : recipe.nodes.front().parameters) {
        if (parameter.name == "high") {
            parameter.value = 0.1;
        } else if (parameter.name == "low") {
            parameter.value = 0.2;
        }
    }
    const auto errors = validate_recipe(recipe, registry);
    bool found_relation_error = false;
    for (const ValidationError& error : errors) {
        if (error.code == ValidationErrorCode::parameter_out_of_domain &&
            error.message.find("'low' < 'high'") != std::string::npos) {
            found_relation_error = true;
            break;
        }
    }
    expect(found_relation_error, "catalog-declared cross-parameter relation is enforced generically");
}

void test_graph_resource_limit() {
    using namespace syndata::engine;
    const NodeRegistry registry = make_custom_registry();
    Recipe recipe = make_custom_recipe();
    recipe.nodes.resize(kMaximumRecipeNodes + 1U);
    const auto errors = validate_recipe(recipe, registry);
    expect(
        !errors.empty() && errors.front().code == ValidationErrorCode::resource_limit,
        "oversized programmatic graph is rejected before graph work");
}

}  // namespace

int main() {
    test_splitmix64_vectors();
    test_seed_derivation_vectors();
    test_pcg32_vectors();
    test_hash_vectors();
    test_checked_math_and_text_bounds();
    test_custom_catalog_and_inherited_fingerprint();
    test_graph_resource_limit();

    if (g_failures != 0) {
        std::cerr << g_failures << " SynData engine test(s) failed\n";
        return 1;
    }
    std::cout << "SD-001 SynData engine tests passed\n";
    return 0;
}
