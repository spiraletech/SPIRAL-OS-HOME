#pragma once

#include "home/calendar.hpp"
#include "home/entity.hpp"
#include "home/events.hpp"
#include "home/needs_mood_autonomy.hpp"
#include "home/player_life.hpp"
#include "home/relationships.hpp"
#include "home/result.hpp"
#include "home/revision.hpp"
#include "home/topology.hpp"
#include "home/weather.hpp"
#include "home/world_affect.hpp"
#include "home/world_clock.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace home {

struct SnapshotPlacement final {
    EntityId entity{};
    ZoneId zone{};
    auto operator<=>(const SnapshotPlacement&) const = default;
};

struct WorldSnapshot final {
    WorldId world{};
    WorldRevision revision{};
    WorldClockConfig clock_config{};
    WorldTime world_time{};
    std::uint64_t clock_remainder{};
    CalendarConfig calendar_config{};
    std::vector<EventDefinition> events{};
    std::vector<ClimateProfile> climates{};
    std::vector<WeatherState> weather{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
    std::vector<PlayerLifeState> player_life{};
    std::vector<PlayerDynamicsState> player_dynamics{};
    std::vector<RelationshipState> relationships{};
    std::vector<HouseholdState> households{};
    std::vector<EntityRecord> entities{};
    std::vector<ZoneRecord> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<SnapshotPlacement> placements{};
};

inline constexpr std::uint32_t kSnapshotFormatVersion = 9;

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot);
Result<WorldSnapshot> decode_snapshot(std::string_view encoded);
Result<void> save_snapshot_file(const WorldSnapshot& snapshot, const std::string& path);
Result<WorldSnapshot> load_snapshot_file(const std::string& path);

} // namespace home
