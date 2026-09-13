#pragma once

#include "home/versioned_world.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

inline constexpr std::uint32_t kXenonProjectionVersion = 1;

struct XenonEventState final {
    EventId id{};
    std::string key{};
    std::string display_name{};
    EventKind kind{EventKind::Observance};
    int priority{};
    std::vector<std::string> affinities{};
    auto operator<=>(const XenonEventState&) const = default;
};

struct XenonZoneState final {
    ZoneId zone{};
    ZoneKind kind{ZoneKind::Region};
    std::string key{};
    std::string display_name{};
    std::optional<ZoneId> parent{};
    bool persistent{true};
    std::optional<ClimateProfile> climate{};
    std::optional<WeatherState> weather{};
    auto operator<=>(const XenonZoneState&) const = default;
};

struct XenonEntityState final {
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
    auto operator<=>(const XenonEntityState&) const = default;
};

struct XenonItemState final {
    ItemId item{};
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    std::uint32_t durability{kItemDurabilityMaximum};
    std::optional<EntityId> owner{};
    std::optional<ZoneId> zone{};
    auto operator<=>(const XenonItemState&) const = default;
};

struct XenonWorldView final {
    std::uint32_t projection_version{kXenonProjectionVersion};
    WorldId world{};
    WorldRevision revision{};
    WorldTime world_time{};
    CalendarState calendar{};
    WorldAffectState affect{};
    WorldAnchorState anchor{};
    std::vector<XenonEventState> active_events{};
    std::vector<XenonZoneState> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<XenonEntityState> entities{};
    std::vector<XenonItemState> items{};
    auto operator<=>(const XenonWorldView&) const = default;
};

class XenonAdapter final {
public:
    explicit XenonAdapter(const VersionedWorld& world) noexcept : world_(world) {}
    [[nodiscard]] Result<XenonWorldView> snapshot() const;
private:
    const VersionedWorld& world_;
};

} // namespace home
