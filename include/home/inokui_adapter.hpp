#pragma once

#include "home/versioned_world.hpp"

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace home {

struct InokuiEntityState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    std::string display_name{};
    Transform transform{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const InokuiEntityState&) const = default;
};

struct InokuiWorldView final {
    WorldId world{};
    WorldRevision revision{};
    CalendarState calendar{};
    std::vector<InokuiEntityState> entities{};

    auto operator<=>(const InokuiWorldView&) const = default;
};

class InokuiAdapter final {
public:
    explicit InokuiAdapter(const VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] InokuiWorldView snapshot() const;

private:
    const VersionedWorld& world_;
};

} // namespace home
