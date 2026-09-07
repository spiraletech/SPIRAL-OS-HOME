#pragma once

#include "home/versioned_world.hpp"

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace home {

struct EntrokoiEntityState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const EntrokoiEntityState&) const = default;
};

struct EntrokoiWorldView final {
    WorldId world{};
    WorldRevision revision{};
    CalendarState calendar{};
    std::vector<ZoneRecord> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<EntrokoiEntityState> entities{};

    auto operator<=>(const EntrokoiWorldView&) const = default;
};

class EntrokoiAdapter final {
public:
    explicit EntrokoiAdapter(const VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] EntrokoiWorldView snapshot() const;

private:
    const VersionedWorld& world_;
};

} // namespace home
