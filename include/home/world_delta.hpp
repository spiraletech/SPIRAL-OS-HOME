#pragma once

#include "home/entity.hpp"
#include "home/inventory.hpp"
#include "home/needs_mood_autonomy.hpp"
#include "home/player_life.hpp"
#include "home/progression.hpp"
#include "home/relationships.hpp"
#include "home/revision.hpp"
#include "home/theorism.hpp"
#include "home/topology.hpp"
#include "home/weather.hpp"
#include "home/world_affect.hpp"
#include "home/world_clock.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace home {

enum class WorldChangeKind : std::uint8_t {
    EntityCreated = 0,
    EntityRemoved,
    EntityTransformUpdated,
    ZoneCreated,
    ZoneParentChanged,
    ZonesConnected,
    EntityZoneChanged,
    WorldTimeAdvanced,
    ClimateProfileChanged,
    WeatherStateChanged,
    WorldAffectChanged,
    WorldAnchorChanged,
    PlayerLifeStateChanged,
    PlayerDynamicsStateChanged,
    RelationshipStateChanged,
    HouseholdStateChanged,
    ItemStateChanged,
    SkillStateChanged,
    TaskStateChanged,
    QuestStateChanged,
    SubclassAffinityStateChanged,
    AuraStateChanged
};

struct EntityCreated final { EntityRecord entity{}; };
struct EntityRemoved final { EntityRecord entity{}; };
struct EntityTransformUpdated final { EntityId entity{}; Transform before{}; Transform after{}; };
struct ZoneCreated final { ZoneRecord zone{}; };
struct ZoneParentChanged final { ZoneId zone{}; std::optional<ZoneId> before{}; std::optional<ZoneId> after{}; };
struct ZonesConnected final { ZoneConnection connection{}; };
struct EntityZoneChanged final { EntityId entity{}; std::optional<ZoneId> before{}; std::optional<ZoneId> after{}; };
struct WorldTimeAdvanced final { WorldTime before{}; WorldTime after{}; std::uint64_t real_milliseconds{}; std::uint64_t before_remainder{}; std::uint64_t after_remainder{}; };
struct ClimateProfileChanged final { std::optional<ClimateProfile> before{}; ClimateProfile after{}; };
struct WeatherStateChanged final { std::optional<WeatherState> before{}; WeatherState after{}; Season season{Season::Winter}; std::uint64_t entropy{}; };
struct WorldAffectChanged final { WorldAffectState before{}; WorldAffectState after{}; };
struct WorldAnchorChanged final { WorldAnchorState before{}; WorldAnchorState after{}; };
struct PlayerLifeStateChanged final { std::optional<PlayerLifeState> before{}; std::optional<PlayerLifeState> after{}; };
struct PlayerDynamicsStateChanged final { std::optional<PlayerDynamicsState> before{}; std::optional<PlayerDynamicsState> after{}; };
struct RelationshipStateChanged final { std::optional<RelationshipState> before{}; std::optional<RelationshipState> after{}; };
struct HouseholdStateChanged final { std::optional<HouseholdState> before{}; std::optional<HouseholdState> after{}; };
struct ItemStateChanged final { std::optional<ItemState> before{}; std::optional<ItemState> after{}; };
struct SkillStateChanged final { std::optional<SkillState> before{}; std::optional<SkillState> after{}; };
struct TaskStateChanged final { std::optional<TaskState> before{}; std::optional<TaskState> after{}; };
struct QuestStateChanged final { std::optional<QuestState> before{}; std::optional<QuestState> after{}; };
struct SubclassAffinityStateChanged final { std::optional<SubclassAffinityState> before{}; std::optional<SubclassAffinityState> after{}; };
struct AuraStateChanged final { std::optional<AuraState> before{}; std::optional<AuraState> after{}; };

using WorldChangePayload = std::variant<
    EntityCreated, EntityRemoved, EntityTransformUpdated, ZoneCreated,
    ZoneParentChanged, ZonesConnected, EntityZoneChanged, WorldTimeAdvanced,
    ClimateProfileChanged, WeatherStateChanged, WorldAffectChanged, WorldAnchorChanged,
    PlayerLifeStateChanged, PlayerDynamicsStateChanged, RelationshipStateChanged,
    HouseholdStateChanged, ItemStateChanged, SkillStateChanged, TaskStateChanged,
    QuestStateChanged, SubclassAffinityStateChanged, AuraStateChanged
>;

struct WorldChange final {
    WorldChangeKind kind{WorldChangeKind::EntityCreated};
    WorldChangePayload payload{EntityCreated{}};
};

class WorldDelta final {
public:
    WorldDelta(WorldRevision from, WorldRevision to, std::vector<WorldChange> changes)
        : from_(from), to_(to), changes_(std::move(changes)) {}
    [[nodiscard]] WorldRevision from_revision() const noexcept { return from_; }
    [[nodiscard]] WorldRevision to_revision() const noexcept { return to_; }
    [[nodiscard]] const std::vector<WorldChange>& changes() const noexcept { return changes_; }
    [[nodiscard]] bool empty() const noexcept { return changes_.empty(); }
private:
    WorldRevision from_{};
    WorldRevision to_{};
    std::vector<WorldChange> changes_{};
};

} // namespace home
