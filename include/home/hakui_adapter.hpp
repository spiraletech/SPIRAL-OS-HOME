#pragma once

#include "home/player_life.hpp"
#include "home/result.hpp"
#include "home/transaction.hpp"
#include "home/versioned_world.hpp"
#include "home/world_clock.hpp"

#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace home {

struct HakuiBodyState final {
    EntityId entity{};
    EntityKind kind{EntityKind::Unknown};
    std::string archetype{};
    Transform transform{};
    std::optional<ZoneId> zone{};
    std::optional<LifeStage> life_stage{};
    std::optional<LifePresence> life_presence{};
    bool persistent{true};

    auto operator<=>(const HakuiBodyState&) const = default;
};

struct HakuiFrame final {
    WorldId world{};
    WorldRevision revision{};
    WorldTime world_time{};
    std::vector<HakuiBodyState> bodies{};

    auto operator<=>(const HakuiFrame&) const = default;
};

struct HakuiTransformConsequence final {
    EntityId entity{};
    Transform transform{};

    auto operator<=>(const HakuiTransformConsequence&) const = default;
};

struct HakuiZoneConsequence final {
    EntityId entity{};
    std::optional<ZoneId> zone{};

    auto operator<=>(const HakuiZoneConsequence&) const = default;
};

using HakuiConsequence = std::variant<HakuiTransformConsequence, HakuiZoneConsequence>;

struct HakuiConsequenceBatch final {
    WorldTransactionId id{};
    WorldRevision expected_revision{};
    std::string authority{};
    std::vector<HakuiConsequence> consequences{};
};

struct HakuiApplyReceipt final {
    WorldTransactionId id{};
    WorldRevision from_revision{};
    WorldRevision to_revision{};
    std::size_t consequence_count{};

    auto operator<=>(const HakuiApplyReceipt&) const = default;
};

class HakuiAdapter final {
public:
    explicit HakuiAdapter(VersionedWorld& world) noexcept : world_(world) {}

    [[nodiscard]] HakuiFrame project() const;
    [[nodiscard]] Result<HakuiApplyReceipt> apply(const HakuiConsequenceBatch& batch);

private:
    VersionedWorld& world_;
};

} // namespace home
