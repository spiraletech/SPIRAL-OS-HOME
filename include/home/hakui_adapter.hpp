#pragma once

#include "home/versioned_world.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

struct HakuiEntityState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    Transform transform{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const HakuiEntityState&) const = default;
};

struct HakuiWorldView final {
    WorldId world{};
    WorldRevision revision{};
    std::vector<HakuiEntityState> entities{};

    auto operator<=>(const HakuiWorldView&) const = default;
};

enum class HakuiConsequenceKind : std::uint8_t {
    TransformChanged = 0,
    EnteredZone,
    LeftZone
};

struct HakuiConsequence final {
    HakuiConsequenceKind kind{HakuiConsequenceKind::TransformChanged};
    WorldRevision expected_revision{};
    EntityId entity{};
    Transform transform{};
    ZoneId zone{};
};

class HakuiAdapter final {
public:
    explicit HakuiAdapter(VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] HakuiWorldView snapshot() const;
    [[nodiscard]] Result<WorldRevision> apply(const HakuiConsequence& consequence);

private:
    VersionedWorld& world_;
};

} // namespace home
