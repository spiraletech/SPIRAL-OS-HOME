#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace home {

template <typename Tag>
class StrongId final {
public:
    using value_type = std::uint64_t;

    constexpr StrongId() noexcept = default;
    explicit constexpr StrongId(value_type value) noexcept : value_(value) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != 0; }
    explicit constexpr operator bool() const noexcept { return valid(); }

    auto operator<=>(const StrongId&) const = default;

private:
    value_type value_{0};
};

struct EntityIdTag;
struct ZoneIdTag;
struct WorldIdTag;
struct WorldTransactionIdTag;
struct TimeDomainIdTag;
struct PlayerIdTag;
struct HouseholdIdTag;
struct ItemIdTag;
struct TaskIdTag;
struct QuestIdTag;
struct EventIdTag;

using EntityId = StrongId<EntityIdTag>;
using ZoneId = StrongId<ZoneIdTag>;
using WorldId = StrongId<WorldIdTag>;
using WorldTransactionId = StrongId<WorldTransactionIdTag>;
using TimeDomainId = StrongId<TimeDomainIdTag>;
using PlayerId = StrongId<PlayerIdTag>;
using HouseholdId = StrongId<HouseholdIdTag>;
using ItemId = StrongId<ItemIdTag>;
using TaskId = StrongId<TaskIdTag>;
using QuestId = StrongId<QuestIdTag>;
using EventId = StrongId<EventIdTag>;

template <typename Id>
struct StrongIdHash final {
    [[nodiscard]] std::size_t operator()(Id id) const noexcept {
        static_assert(std::is_same_v<typename Id::value_type, std::uint64_t>);
        return std::hash<std::uint64_t>{}(id.value());
    }
};

} // namespace home
