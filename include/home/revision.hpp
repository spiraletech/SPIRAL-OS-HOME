#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <optional>

namespace home {

class WorldRevision final {
public:
    using value_type = std::uint64_t;

    constexpr WorldRevision() noexcept = default;
    explicit constexpr WorldRevision(value_type value) noexcept : value_(value) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool is_initial() const noexcept { return value_ == 0; }

    [[nodiscard]] constexpr std::optional<WorldRevision> next() const noexcept {
        if (value_ == std::numeric_limits<value_type>::max()) {
            return std::nullopt;
        }
        return WorldRevision{value_ + 1};
    }

    auto operator<=>(const WorldRevision&) const = default;

private:
    value_type value_{0};
};

} // namespace home
