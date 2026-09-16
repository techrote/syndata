#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#include "engine/checked_math.hpp"
#include "engine/hash.hpp"
#include "engine/local_text.hpp"
#include "engine/prng.hpp"
#include "engine/result.hpp"

namespace {

int g_failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
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
    // Retained solely as immutable ancestry evidence from ArtMiner AM-001.
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

}  // namespace

int main() {
    test_splitmix64_vectors();
    test_seed_derivation_vectors();
    test_pcg32_vectors();
    test_hash_vectors();
    test_checked_math_and_text_bounds();

    if (g_failures != 0) {
        std::cerr << g_failures << " SynData engine primitive test(s) failed\n";
        return 1;
    }
    std::cout << "SynData deterministic primitive tests passed\n";
    return 0;
}
