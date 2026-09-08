#pragma once

#include "home/entity_registry.hpp"
#include "home/player_life.hpp"
#include "home/result.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace home {

inline constexpr std::int32_t kNeedMinimum = 0;
inline constexpr std::int32_t kNeedMaximum = 10'000;
inline constexpr std::int32_t kMoodValenceMinimum = -10'000;
inline constexpr std::int32_t kMoodValenceMaximum = 10'000;
inline constexpr std::int32_t kMoodArousalMinimum = 0;
inline constexpr std::int32_t kMoodArousalMaximum = 10'000;
inline constexpr std::uint32_t kAutonomyInitiativeMaximumPerHour = 60;
inline constexpr std::int32_t kAutonomyNeedThreshold = 7'500;

enum class NeedKind : std::uint8_t { Energy = 0, Hunger, Hygiene, Social, Fun, Safety, Count };
enum class MoodBand : std::uint8_t { Distressed = 0, Low, Neutral, Positive, Elevated };
enum class AutonomyMode : std::uint8_t { Disabled = 0, Advisory, Bounded };
enum class AutonomyIntent : std::uint8_t { None = 0, Rest, Eat, Clean, Socialize, Play, SeekSafety };

struct NeedLevels final {
    std::array<std::int32_t, static_cast<std::size_t>(NeedKind::Count)> values{
        kNeedMaximum, kNeedMaximum, kNeedMaximum, kNeedMaximum, kNeedMaximum, kNeedMaximum};

    [[nodiscard]] std::int32_t get(NeedKind kind) const noexcept {
        return values[static_cast<std::size_t>(kind)];
    }

    void set(NeedKind kind, std::int32_t value) noexcept {
        values[static_cast<std::size_t>(kind)] = value;
    }

    auto operator<=>(const NeedLevels&) const = default;
};

struct MoodState final {
    std::int32_t valence{};
    std::int32_t arousal{};
    MoodBand band{MoodBand::Neutral};

    auto operator<=>(const MoodState&) const = default;
};

struct AutonomyPolicy final {
    AutonomyMode mode{AutonomyMode::Disabled};
    std::uint32_t initiative_limit_per_hour{};
    bool may_change_zone{false};
    bool may_interact_with_entities{false};

    auto operator<=>(const AutonomyPolicy&) const = default;
};

struct PlayerDynamicsState final {
    EntityId entity{};
    NeedLevels needs{};
    MoodState mood{};
    AutonomyPolicy autonomy{};
    std::string active_drive{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};

    auto operator<=>(const PlayerDynamicsState&) const = default;
};

struct AutonomyDecision final {
    EntityId entity{};
    AutonomyIntent intent{AutonomyIntent::None};
    NeedKind governing_need{NeedKind::Energy};
    std::int32_t urgency{};
    bool initiative_allowed{false};
    bool may_change_zone{false};
    bool may_interact_with_entities{false};
    std::string drive{};

    auto operator<=>(const AutonomyDecision&) const = default;
};

[[nodiscard]] MoodBand derive_mood_band(std::int32_t valence) noexcept;
[[nodiscard]] std::string_view drive_key(NeedKind kind) noexcept;
[[nodiscard]] Result<void> validate_player_dynamics_shape(const PlayerDynamicsState& state);
[[nodiscard]] Result<AutonomyDecision> resolve_autonomy_decision(const PlayerDynamicsState& state);

class PlayerDynamicsLedger final {
public:
    Result<void> set(
        const EntityRegistry& entities,
        const PlayerLifeLedger& life,
        PlayerDynamicsState state);
    Result<PlayerDynamicsState> remove(EntityId entity);

    [[nodiscard]] const PlayerDynamicsState* find(EntityId entity) const noexcept;
    [[nodiscard]] std::vector<PlayerDynamicsState> snapshot() const;
    [[nodiscard]] std::size_t size() const noexcept { return states_.size(); }

private:
    std::vector<PlayerDynamicsState> states_{};
};

} // namespace home
