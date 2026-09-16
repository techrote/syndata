#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "engine/graph.hpp"
#include "engine/hash.hpp"
#include "engine/local_text.hpp"
#include "engine/prng.hpp"
#include "engine/recipe.hpp"

namespace {

void print_help() {
    std::cout
        << "Usage: SynData [--help|--version|--self-check]\n"
        << "       SynData recipe validate <file.sdr>\n"
        << "       SynData recipe inspect <file.sdr>\n"
        << "       SynData recipe canonicalize <file.sdr>\n"
        << "       SynData recipe fingerprint <file.sdr>\n\n"
        << "SD-002 establishes the SynData-native .sdr schema and generic execution contracts.\n"
        << "The standalone recipe validate command checks schema, bounds, identifiers and graph references.\n"
        << "Port/type/node semantic validation additionally requires a compiled domain registry.\n";
}

int run_self_check() {
    using namespace syndata::engine;
    if (splitmix64(0ULL) != 0xe220a8397b1dcdafULL ||
        derive_seed(42ULL, 54ULL) != 0xbf411dba522b2d0cULL ||
        fnv1a64("deterministic") != 0x97f2ebf85d31152dULL) {
        std::cerr << "self-check FAILED\n";
        return 1;
    }
    Pcg32 generator(42ULL, 54ULL);
    if (generator.next_u32() != 0xa15c02b7U) {
        std::cerr << "self-check FAILED\n";
        return 1;
    }
    std::cout << "self-check OK\n";
    return 0;
}

[[nodiscard]] std::optional<syndata::engine::Recipe> load_recipe(const std::filesystem::path& path) {
    using namespace syndata::engine;
    auto text = read_local_text_file(path);
    if (text.is_error()) {
        std::cerr << "recipe read error: " << text.error().message << '\n';
        return std::nullopt;
    }
    auto parsed = parse_recipe(text.value());
    if (parsed.is_error()) {
        std::cerr << "recipe parse error";
        if (parsed.error().line != 0U) {
            std::cerr << " at line " << parsed.error().line;
        }
        std::cerr << ": " << parsed.error().message << '\n';
        return std::nullopt;
    }
    return std::move(parsed).value();
}

int run_recipe_command(const int argc, char* argv[]) {
    using namespace syndata::engine;
    if (argc != 4) {
        std::cerr << "usage: SynData recipe <validate|inspect|canonicalize|fingerprint> <file.sdr>\n";
        return 2;
    }
    const std::string_view action(argv[2]);
    if (action != "validate" && action != "inspect" && action != "canonicalize" && action != "fingerprint") {
        std::cerr << "recipe error: unknown action: " << action << '\n';
        return 2;
    }

    auto recipe = load_recipe(std::filesystem::path(argv[3]));
    if (!recipe.has_value()) {
        return 6;
    }
    const std::vector<ValidationError> intrinsic = validate_recipe_intrinsic(*recipe);
    if (!intrinsic.empty()) {
        for (const ValidationError& error : intrinsic) {
            std::cerr << "recipe validation error: " << error.message << '\n';
        }
        return 7;
    }

    if (action == "validate") {
        std::cout << "valid-sdr " << semantic_fingerprint(*recipe) << '\n';
        return 0;
    }
    if (action == "fingerprint") {
        std::cout << semantic_fingerprint(*recipe) << '\n';
        return 0;
    }
    if (action == "canonicalize") {
        std::cout << serialize_recipe_canonical(*recipe);
        return 0;
    }

    std::cout << "SynData recipe\n"
              << "schema: " << recipe->schema_version << '\n'
              << "evaluator: " << recipe->evaluator_version << '\n'
              << "seed: " << recipe->root_seed << '\n'
              << "nodes: " << recipe->nodes.size() << '\n'
              << "edges: " << recipe->edges.size() << '\n'
              << "outputs: " << recipe->outputs.size() << '\n'
              << "metadata: " << recipe->metadata.size() << '\n'
              << "fingerprint: " << semantic_fingerprint(*recipe) << '\n';
    return 0;
}

}  // namespace

int main(const int argc, char* argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }
    if (argc >= 2 && std::string_view(argv[1]) == "recipe") {
        return run_recipe_command(argc, argv);
    }
    if (argc != 2) {
        std::cerr << "error: invalid arguments\n";
        return 2;
    }

    const std::string_view argument(argv[1]);
    if (argument == "--help" || argument == "-h") {
        print_help();
        return 0;
    }
    if (argument == "--version") {
        std::cout << "SynData SD-002-dev\n";
        return 0;
    }
    if (argument == "--self-check") {
        return run_self_check();
    }

    std::cerr << "error: unknown option: " << argument << '\n';
    return 2;
}
