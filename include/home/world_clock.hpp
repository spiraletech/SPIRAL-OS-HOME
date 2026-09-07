#pragma once

#include "home/result.hpp"

#include <compare>
#include <cstdint>

namespace home {

struct WorldClockConfig final {
    std::uint64_t real_milliseconds_per_home_minute{30000};
    auto operator<=>(const WorldClockConfig&) const = default;
};

struct WorldTime final {
    std::uint64_t milliseconds{};
    auto operator<=>(const WorldTime&) const = default;
};

class WorldClock final {
public:
    explicit WorldClock(WorldClockConfig config = {}) noexcept : config_(config) {}

    [[nodiscard]] const WorldClockConfig& config() const noexcept { return config_; }
    [[nodiscard]] WorldTime now() const noexcept { return WorldTime{home_milliseconds_}; }
    [[nodiscard]] std::uint64_t remainder() const noexcept { return remainder_; }

    Result<WorldTime> advance_real_milliseconds(std::uint64_t real_milliseconds);
    Result<void> restore(WorldTime time, std::uint64_t remainder = 0);

private:
    WorldClockConfig config_{};
    std::uint64_t home_milliseconds_{};
    std::uint64_t remainder_{};
};

} // namespace home
