#pragma once

#include "engine/types.hpp"

namespace syndata::engine {

[[nodiscard]] u64 splitmix64(u64 value) noexcept;
[[nodiscard]] u64 derive_seed(u64 root_seed, u64 domain) noexcept;

class Pcg32 final {
public:
    Pcg32(u64 seed, u64 sequence) noexcept;

    [[nodiscard]] u32 next_u32() noexcept;
    [[nodiscard]] u64 state() const noexcept { return state_; }
    [[nodiscard]] u64 increment() const noexcept { return increment_; }

private:
    u64 state_{0U};
    u64 increment_{1U};
};

}  // namespace syndata::engine
