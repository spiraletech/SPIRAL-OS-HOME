#include "home/versioned_world.hpp"

#include <utility>

namespace home {

Result<WorldContextState> VersionedWorld::refresh_world_context(
    std::optional<ZoneId> weather_zone,
    std::optional<WorldAnchorOverride> anchor_override) {
    const WeatherState* weather_state = nullptr;
    if (weather_zone.has_value()) {
        if (!topology_.contains(*weather_zone)) {
            return Result<WorldContextState>::failure(ErrorCode::NotFound, "world affect weather zone not found");
        }
        weather_state = weather_.find(*weather_zone);
        if (!weather_state) {
            return Result<WorldContextState>::failure(ErrorCode::NotFound, "world affect weather state not found");
        }
    }

    const auto events = active_events();
    if (!events) return Result<WorldContextState>::failure(events.error().code, events.error().message);

    const auto resolved = resolve_world_context(calendar(), events.value().events, weather_state, std::move(anchor_override));
    if (!resolved) return Result<WorldContextState>::failure(resolved.error().code, resolved.error().message);

    const WorldAffectState before_affect = affect_;
    const WorldAnchorState before_anchor = anchor_;
    const WorldAffectState after_affect = resolved.value().affect;
    const WorldAnchorState after_anchor = resolved.value().anchor;

    std::vector<WorldChange> changes;
    if (before_affect != after_affect) {
        changes.push_back(WorldChange{WorldChangeKind::WorldAffectChanged, WorldAffectChanged{before_affect, after_affect}});
    }
    if (before_anchor != after_anchor) {
        changes.push_back(WorldChange{WorldChangeKind::WorldAnchorChanged, WorldAnchorChanged{before_anchor, after_anchor}});
    }
    if (changes.empty()) {
        return Result<WorldContextState>::failure(ErrorCode::ValidationFailed, "world context refresh produced no state change");
    }

    affect_ = after_affect;
    anchor_ = after_anchor;
    const auto committed = commit(std::move(changes));
    if (!committed) {
        affect_ = before_affect;
        anchor_ = before_anchor;
        return Result<WorldContextState>::failure(committed.error().code, committed.error().message);
    }

    return Result<WorldContextState>::success(resolved.value());
}

} // namespace home
