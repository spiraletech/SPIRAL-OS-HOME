#pragma once

#include "home/entity_registry.hpp"
#include "home/result.hpp"
#include "home/topology.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

namespace home {

enum class LifeStage : std::uint8_t { Child = 0, Teen, Adult, Elder };
enum class LifePresence : std::uint8_t { Present = 0, Away, Incapacitated, Deceased };

struct PlayerLifeState final {
    EntityId entity{};
    LifeStage stage{LifeStage::Adult};
    LifePresence presence{LifePresence::Present};
    std::optional<ZoneId> home_zone{};
    std::uint64_t born_world_minute{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const PlayerLifeState&) const = default;
};

Result<void> validate_player_life_state(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeState& state);

class PlayerLifeLedger final {
public:
    Result<void> set(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        PlayerLifeState state);
    Result<PlayerLifeState> remove(EntityId entity);

    [[nodiscard]] const PlayerLifeState* find(EntityId entity) const noexcept;
    [[nodiscard]] std::vector<PlayerLifeState> snapshot() const;
    [[nodiscard]] std::size_t size() const noexcept { return states_.size(); }

private:
    std::vector<PlayerLifeState> states_{};
};

} // namespace home
