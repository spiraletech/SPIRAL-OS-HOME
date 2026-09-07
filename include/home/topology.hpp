#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

enum class ZoneKind : std::uint8_t {
    Region = 0,
    District,
    Parcel,
    Interior,
    Room,
    Threshold,
    Water,
    Restricted
};

struct ZoneRecord final {
    ZoneId id{};
    WorldId world{};
    ZoneKind kind{ZoneKind::Region};
    std::string key{};
    std::string display_name{};
    std::optional<ZoneId> parent{};
    bool persistent{true};

    auto operator<=>(const ZoneRecord&) const = default;
};

struct ZoneCreateInfo final {
    ZoneKind kind{ZoneKind::Region};
    std::string key{};
    std::string display_name{};
    std::optional<ZoneId> parent{};
    bool persistent{true};
};

struct ZoneConnection final {
    ZoneId from{};
    ZoneId to{};
    bool bidirectional{true};
    bool traversable{true};
    std::string tag{};

    auto operator<=>(const ZoneConnection&) const = default;
};

class TopologyRegistry final {
public:
    explicit TopologyRegistry(WorldId world, std::uint64_t first_zone_value = 1) noexcept;

    [[nodiscard]] WorldId world() const noexcept { return world_; }
    [[nodiscard]] std::size_t size() const noexcept { return zones_.size(); }
    [[nodiscard]] bool contains(ZoneId id) const noexcept;

    Result<ZoneId> create_zone(ZoneCreateInfo info);
    Result<void> restore_zone(ZoneRecord zone);
    Result<void> set_parent(ZoneId child, std::optional<ZoneId> parent);
    Result<void> connect(ZoneConnection connection);
    Result<void> disconnect(ZoneId from, ZoneId to);

    Result<void> place_entity(EntityId entity, ZoneId zone);
    Result<void> clear_entity(EntityId entity);

    [[nodiscard]] const ZoneRecord* find(ZoneId id) const noexcept;
    [[nodiscard]] std::optional<ZoneId> zone_of(EntityId entity) const noexcept;
    [[nodiscard]] std::vector<ZoneId> children_of(ZoneId parent) const;
    [[nodiscard]] std::vector<ZoneId> neighbors_of(ZoneId zone, bool traversable_only = true) const;
    [[nodiscard]] bool directly_traversable(ZoneId from, ZoneId to) const noexcept;
    [[nodiscard]] std::vector<ZoneRecord> snapshot_zones() const;
    [[nodiscard]] std::vector<ZoneConnection> snapshot_connections() const;

private:
    struct Placement final {
        EntityId entity{};
        ZoneId zone{};
    };

    [[nodiscard]] Result<ZoneId> allocate_zone_id();
    [[nodiscard]] bool would_create_cycle(ZoneId child, ZoneId parent) const noexcept;
    void advance_allocator_past(ZoneId id) noexcept;

    WorldId world_{};
    std::uint64_t next_zone_value_{1};
    std::vector<ZoneRecord> zones_{};
    std::vector<ZoneConnection> connections_{};
    std::vector<Placement> placements_{};
};

} // namespace home
