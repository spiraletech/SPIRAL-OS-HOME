#pragma once
#include "home/versioned_world.hpp"
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

inline constexpr std::uint32_t kEntrokoiProjectionVersion = 1;

struct EntrokoiEventState final {
    EventId id{};
    std::string key{};
    std::string display_name{};
    EventKind kind{EventKind::Observance};
    int priority{};
    std::vector<std::string> affinities{};
    auto operator<=>(const EntrokoiEventState&) const = default;
};

struct EntrokoiZoneState final {
    ZoneId zone{};
    ZoneKind kind{ZoneKind::Region};
    std::string key{};
    std::string display_name{};
    std::optional<ZoneId> parent{};
    bool persistent{true};
    std::optional<ClimateProfile> climate{};
    std::optional<WeatherState> weather{};
    auto operator<=>(const EntrokoiZoneState&) const = default;
};

struct EntrokoiEntityState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    std::optional<ZoneId> zone{};
    bool persistent{true};
    std::optional<LifeStage> life_stage{};
    std::optional<LifePresence> life_presence{};
    std::optional<MoodState> mood{};
    std::optional<AuraState> aura{};
    std::optional<RelationshipState> relation{};
    auto operator<=>(const EntrokoiEntityState&) const = default;
};

struct EntrokoiItemState final {
    ItemId item{};
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    std::uint32_t durability{kItemDurabilityMaximum};
    std::optional<EntityId> owner{};
    std::optional<ZoneId> zone{};
    auto operator<=>(const EntrokoiItemState&) const = default;
};

struct EntrokoiWorldView final {
    std::uint32_t projection_version{kEntrokoiProjectionVersion};
    WorldId world{};
    WorldRevision revision{};
    WorldTime world_time{};
    CalendarState calendar{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
    EntrokoiEntityState observer{};
    std::vector<EntrokoiEventState> active_events{};
    std::vector<EntrokoiZoneState> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<EntrokoiEntityState> entities{};
    std::vector<EntrokoiItemState> items{};
    auto operator<=>(const EntrokoiWorldView&) const = default;
};

class EntrokoiAdapter final {
public:
    explicit EntrokoiAdapter(const VersionedWorld& world) noexcept : world_(world) {}
    [[nodiscard]] Result<EntrokoiWorldView> snapshot_for(EntityId observer) const;
private:
    const VersionedWorld& world_;
};

} // namespace home
