#pragma once

#include "home/versioned_world.hpp"

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace home {

struct XenonEntityState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const XenonEntityState&) const = default;
};

struct XenonWorldView final {
    WorldId world{};
    WorldRevision revision{};
    CalendarState calendar{};
    std::vector<ZoneRecord> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<XenonEntityState> entities{};

    auto operator<=>(const XenonWorldView&) const = default;
};

class XenonAdapter final {
public:
    explicit XenonAdapter(const VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] XenonWorldView snapshot() const;

private:
    const VersionedWorld& world_;
};

} // namespace home
