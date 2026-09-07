#pragma once

#include "home/world_clock.hpp"

#include <compare>
#include <cstdint>

namespace home {

enum class Season : std::uint8_t { Winter = 0, Spring, Summer, Autumn };

struct CalendarDate final {
    int year{2026};
    unsigned month{1};
    unsigned day{1};
    auto operator<=>(const CalendarDate&) const = default;
};

struct CalendarState final {
    CalendarDate date{};
    unsigned hour{};
    unsigned minute{};
    unsigned second{};
    unsigned weekday{}; // 0 Sunday ... 6 Saturday
    std::uint64_t day_index{};
    Season season{Season::Winter};
    auto operator<=>(const CalendarState&) const = default;
};

[[nodiscard]] CalendarState resolve_calendar(WorldTime time) noexcept;
[[nodiscard]] const char* season_name(Season season) noexcept;

} // namespace home
