#pragma once

#include "home/calendar.hpp"
#include "home/events.hpp"
#include "home/result.hpp"
#include "home/weather.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

enum class WorldTone : std::uint8_t { Neutral = 0, Joyful, Melancholic, Tense, Haunted, Festive, Dreamlike };
enum class ThemeAnchor : std::uint8_t { None = 0, HauntedHalloweenRain };
enum class AnchorSource : std::uint8_t { None = 0, Derived, AuthorizedOverride };

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
    AnchorSource source{AnchorSource::None};
    std::string authority{};
    auto operator<=>(const WorldAnchorState&) const = default;
};

struct WorldAnchorOverride final {
    ThemeAnchor anchor{ThemeAnchor::None};
    unsigned strength_permille{};
    std::string authority{};
};

struct WorldContextState final {
    CalendarState calendar{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
};

Result<void> validate_world_affect_state(const WorldAffectState& state);
Result<void> validate_world_anchor_state(const WorldAnchorState& state);

Result<WorldContextState> resolve_world_context(
    CalendarState calendar,
    const std::vector<EventDefinition>& active_events,
    const WeatherState* weather,
    std::optional<WorldAnchorOverride> authorized_anchor = std::nullopt);

} // namespace home
