#include "home/world_affect.hpp"

#include <algorithm>

namespace home {
namespace {
bool has_event(const std::vector<EventDefinition>& events, const char* key) {
    return std::any_of(events.begin(), events.end(), [&](const EventDefinition& event){ return event.key == key; });
}
unsigned clamp_permille(unsigned value) { return std::min(value, 1000u); }
} // namespace

Result<WorldContextState> resolve_world_context(CalendarState calendar,
    const std::vector<EventDefinition>& active_events,
    const WeatherState* weather,
    std::optional<WorldAnchorState> authorized_anchor) {
    if (weather != nullptr && !weather->zone.valid()) {
        return Result<WorldContextState>::failure(ErrorCode::ValidationFailed, "world affect received invalid weather state");
    }
    if (authorized_anchor.has_value() && authorized_anchor->strength_permille > 1000) {
        return Result<WorldContextState>::failure(ErrorCode::ValidationFailed, "theme anchor strength out of range");
    }

    WorldContextState state{};
    state.calendar = calendar;
    const bool halloween = has_event(active_events, "halloween");
    const bool raining = weather != nullptr && weather->intensity >= RainIntensity::Rain;
    const bool severe = weather != nullptr && weather->intensity >= RainIntensity::Storm;

    if (halloween && raining) {
        state.affect = WorldAffectState{WorldTone::Haunted, -180, severe ? 900u : 700u, 760u};
        state.anchor = WorldAnchorState{ThemeAnchor::HauntedHalloweenRain, severe ? 950u : 800u, false};
    } else if (!active_events.empty()) {
        state.affect = WorldAffectState{WorldTone::Festive, 280, 650, 800};
    } else if (severe) {
        state.affect = WorldAffectState{WorldTone::Tense, -260, 720, 690};
    } else if (raining) {
        state.affect = WorldAffectState{WorldTone::Melancholic, -90, 420, 840};
    } else {
        state.affect = WorldAffectState{WorldTone::Neutral, 0, 180, 950};
    }

    if (authorized_anchor.has_value()) {
        state.anchor = *authorized_anchor;
        state.anchor.authorized_override = true;
    }
    state.affect.intensity_permille = clamp_permille(state.affect.intensity_permille);
    state.affect.stability_permille = clamp_permille(state.affect.stability_permille);
    return Result<WorldContextState>::success(state);
}

} // namespace home
