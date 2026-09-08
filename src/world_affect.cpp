#include "home/world_affect.hpp"

#include <algorithm>
#include <string_view>

namespace home {
namespace {
bool event_has_affinity(const EventDefinition& event, std::string_view affinity) {
    return std::find(event.affinities.begin(), event.affinities.end(), affinity) != event.affinities.end();
}

bool has_halloween_event(const std::vector<EventDefinition>& events) {
    return std::any_of(events.begin(), events.end(), [](const EventDefinition& event) {
        return event.key == "halloween" || event_has_affinity(event, "calendar.halloween");
    });
}

unsigned clamp_permille(unsigned value) { return std::min(value, 1000u); }
} // namespace

Result<void> validate_world_affect_state(const WorldAffectState& state) {
    if (state.valence_milli < -1000 || state.valence_milli > 1000
        || state.intensity_permille > 1000 || state.stability_permille > 1000) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "world affect state is out of range");
    }
    return Result<void>::success();
}

Result<void> validate_world_anchor_state(const WorldAnchorState& state) {
    if (state.strength_permille > 1000) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "theme anchor strength is out of range");
    }
    switch (state.source) {
        case AnchorSource::None:
            if (state.anchor != ThemeAnchor::None || state.strength_permille != 0 || !state.authority.empty()) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "empty anchor state is inconsistent");
            }
            break;
        case AnchorSource::Derived:
            if (state.anchor == ThemeAnchor::None || state.strength_permille == 0 || !state.authority.empty()) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "derived anchor state is inconsistent");
            }
            break;
        case AnchorSource::AuthorizedOverride:
            if (state.authority.empty()) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "authorized anchor requires authority provenance");
            }
            if (state.anchor == ThemeAnchor::None) {
                if (state.strength_permille != 0) {
                    return Result<void>::failure(ErrorCode::ValidationFailed, "cleared anchor override must have zero strength");
                }
            } else if (state.strength_permille == 0) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "active anchor override must have non-zero strength");
            }
            break;
    }
    return Result<void>::success();
}

Result<WorldContextState> resolve_world_context(CalendarState calendar,
    const std::vector<EventDefinition>& active_events,
    const WeatherState* weather,
    std::optional<WorldAnchorOverride> authorized_anchor) {
    if (weather != nullptr && !weather->zone.valid()) {
        return Result<WorldContextState>::failure(ErrorCode::ValidationFailed, "world affect received invalid weather state");
    }
    if (authorized_anchor.has_value()) {
        const WorldAnchorState candidate{
            authorized_anchor->anchor,
            authorized_anchor->strength_permille,
            AnchorSource::AuthorizedOverride,
            authorized_anchor->authority};
        const auto valid = validate_world_anchor_state(candidate);
        if (!valid) return Result<WorldContextState>::failure(valid.error().code, valid.error().message);
    }

    WorldContextState state{};
    state.calendar = calendar;
    const bool halloween = has_halloween_event(active_events);
    const bool raining = weather != nullptr && weather->intensity >= RainIntensity::Rain;
    const bool severe = weather != nullptr && weather->intensity >= RainIntensity::Storm;

    if (halloween && raining) {
        state.affect = WorldAffectState{WorldTone::Haunted, -180, severe ? 900u : 700u, 760u};
        state.anchor = WorldAnchorState{
            ThemeAnchor::HauntedHalloweenRain,
            severe ? 950u : 800u,
            AnchorSource::Derived,
            {}};
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
        state.anchor = WorldAnchorState{
            authorized_anchor->anchor,
            authorized_anchor->strength_permille,
            AnchorSource::AuthorizedOverride,
            authorized_anchor->authority};

        if (state.anchor.anchor == ThemeAnchor::HauntedHalloweenRain) {
            state.affect.tone = WorldTone::Haunted;
            state.affect.valence_milli = std::min(state.affect.valence_milli, -120);
            state.affect.intensity_permille = std::max(state.affect.intensity_permille, state.anchor.strength_permille);
            state.affect.stability_permille = std::min(state.affect.stability_permille, 800u);
        }
    }

    state.affect.intensity_permille = clamp_permille(state.affect.intensity_permille);
    state.affect.stability_permille = clamp_permille(state.affect.stability_permille);

    const auto affect_valid = validate_world_affect_state(state.affect);
    if (!affect_valid) return Result<WorldContextState>::failure(affect_valid.error().code, affect_valid.error().message);
    const auto anchor_valid = validate_world_anchor_state(state.anchor);
    if (!anchor_valid) return Result<WorldContextState>::failure(anchor_valid.error().code, anchor_valid.error().message);
    return Result<WorldContextState>::success(std::move(state));
}

} // namespace home
