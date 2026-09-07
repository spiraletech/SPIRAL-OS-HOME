#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <vector>

namespace home {

constexpr std::uint16_t kNeedMax = 1000;
constexpr std::int16_t kMoodMin = -1000;
constexpr std::int16_t kMoodMax = 1000;

enum class NeedKind : std::uint8_t {
    Hunger = 0,
    Energy,
    Hygiene,
    Social,
    Fun,
    Count
};

enum class AutonomyMode : std::uint8_t {
    Disabled = 0,
    Assisted,
    Autonomous
};

struct NeedValue final {
    NeedKind kind{NeedKind::Hunger};
    std::uint16_t value{kNeedMax};
    std::uint16_t decay_per_tick{0};
    auto operator<=>(const NeedValue&) const = default;
};

struct MoodState final {
    std::int16_t valence{0};
    std::uint16_t arousal{0};
    auto operator<=>(const MoodState&) const = default;
};

struct AutonomyState final {
    AutonomyMode mode{AutonomyMode::Assisted};
    std::uint16_t initiative_threshold{500};
    auto operator<=>(const AutonomyState&) const = default;
};

struct PlayerNeedsState final {
    PlayerId player{};
    std::array<NeedValue, static_cast<std::size_t>(NeedKind::Count)> needs{};
    MoodState mood{};
    AutonomyState autonomy{};
    std::uint64_t needs_revision{};
    auto operator<=>(const PlayerNeedsState&) const = default;
};

[[nodiscard]] PlayerNeedsState default_needs_state(PlayerId player) noexcept;

class NeedsLedger final {
public:
    Result<void> create(PlayerNeedsState state);
    Result<void> set_need(PlayerId player, NeedKind kind, std::uint16_t value);
    Result<void> set_decay(PlayerId player, NeedKind kind, std::uint16_t decay_per_tick);
    Result<void> apply_decay(PlayerId player, std::uint64_t ticks);
    Result<void> set_mood(PlayerId player, MoodState mood);
    Result<void> set_autonomy(PlayerId player, AutonomyState autonomy);
    [[nodiscard]] const PlayerNeedsState* find(PlayerId player) const noexcept;
    [[nodiscard]] std::vector<PlayerNeedsState> snapshot() const;

private:
    std::vector<PlayerNeedsState> states_{};
};

} // namespace home
