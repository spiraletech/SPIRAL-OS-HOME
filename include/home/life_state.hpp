#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace home {

struct PlayerLifeState final {
    PlayerId player{};
    EntityId avatar{};
    std::string display_name{};
    std::optional<ZoneId> home_zone{};
    std::optional<ZoneId> current_zone{};
    std::uint64_t life_revision{};
    bool active{true};
    auto operator<=>(const PlayerLifeState&) const = default;
};

class LifeLedger final {
public:
    Result<void> create(PlayerLifeState state);
    Result<void> set_home(PlayerId player, std::optional<ZoneId> zone);
    Result<void> set_current_zone(PlayerId player, std::optional<ZoneId> zone);
    Result<void> set_active(PlayerId player, bool active);
    [[nodiscard]] const PlayerLifeState* find(PlayerId player) const noexcept;
    [[nodiscard]] std::vector<PlayerLifeState> snapshot() const;
private:
    std::vector<PlayerLifeState> states_{};
};

} // namespace home
