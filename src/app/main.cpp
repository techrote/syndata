#include <iostream>
#include <string_view>

#include "engine/hash.hpp"
#include "engine/prng.hpp"

namespace {

void print_help() {
    std::cout
        << "Usage: SynData [--help|--version|--self-check]\n\n"
        << "SD-001 provides only the portable deterministic engine foundation.\n"
        << "SynData-native recipe, domain and dataset commands arrive in later milestones.\n";
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

}  // namespace

int main(const int argc, char* argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }
    if (argc != 2) {
        std::cerr << "error: expected one option\n";
        return 2;
    }

    const std::string_view argument(argv[1]);
    if (argument == "--help" || argument == "-h") {
        print_help();
        return 0;
    }
    if (argument == "--version") {
        std::cout << "SynData SD-001-dev\n";
        return 0;
    }
    if (argument == "--self-check") {
        return run_self_check();
    }

    std::cerr << "error: unknown option: " << argument << '\n';
    return 2;
}
