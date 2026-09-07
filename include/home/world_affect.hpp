#pragma once

#include "home/calendar.hpp"
#include "home/events.hpp"
#include "home/result.hpp"
#include "home/weather.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

namespace home {

enum class WorldTone : std::uint8_t { Neutral = 0, Joyful, Melancholic, Tense, Haunted, Festive, Dreamlike };
enum class ThemeAnchor : std::uint8_t { None = 0, HauntedHalloweenRain };

struct WorldAffectState final {
    WorldTone tone{WorldTone::Neutral};
    int valence_milli{};
    unsigned intensity_permille{};
    unsigned stability_permille{1000};
    auto operator<=>(const WorldAffectState&) const = default;
};

struct WorldAnchorState final {
    ThemeAnchor anchor{ThemeAnchor::None};
    unsigned strength_permille{};
    bool authorized_override{false};
    auto operator<=>(const WorldAnchorState&) const = default;
};

struct WorldContextState final {
    CalendarState calendar{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
};

Result<WorldContextState> resolve_world_context(
    CalendarState calendar,
    const std::vector<EventDefinition>& active_events,
    const WeatherState* weather,
    std::optional<WorldAnchorState> authorized_anchor = std::nullopt);

} // namespace home
