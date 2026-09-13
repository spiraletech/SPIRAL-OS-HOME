#pragma once

#include "home/versioned_world.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

inline constexpr std::uint32_t kInokuiProjectionVersion = 1;

struct InokuiEventState final {
    EventId id{};
    std::string key{};
    std::string display_name{};
    EventKind kind{EventKind::Observance};
    int priority{};
    std::vector<std::string> affinities{};

    auto operator<=>(const InokuiEventState&) const = default;
};

struct InokuiZoneState final {
    ZoneId zone{};
    ZoneKind kind{ZoneKind::Region};
    std::string key{};
    std::string display_name{};
    std::optional<ZoneId> parent{};
    bool persistent{true};
    std::optional<ClimateProfile> climate{};
    std::optional<WeatherState> weather{};

    auto operator<=>(const InokuiZoneState&) const = default;
};

struct InokuiEntityState final {
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

    auto operator<=>(const InokuiEntityState&) const = default;
};

struct InokuiItemState final {
    ItemId item{};
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    std::uint32_t durability{kItemDurabilityMaximum};
    std::optional<EntityId> owner{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const InokuiItemState&) const = default;
};

struct InokuiWorldView final {
    std::uint32_t projection_version{kInokuiProjectionVersion};
    WorldId world{};
    WorldRevision revision{};
    WorldTime world_time{};
    CalendarState calendar{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
    std::vector<InokuiEventState> active_events{};
    std::vector<InokuiZoneState> zones{};
    std::vector<InokuiEntityState> entities{};
    std::vector<InokuiItemState> items{};

    auto operator<=>(const InokuiWorldView&) const = default;
};

class InokuiAdapter final {
public:
    explicit InokuiAdapter(const VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] Result<InokuiWorldView> snapshot() const;

private:
    const VersionedWorld& world_;
};

} // namespace home
