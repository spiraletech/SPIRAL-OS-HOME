#pragma once

#include "home/entity_registry.hpp"
#include "home/ids.hpp"
#include "home/player_life.hpp"
#include "home/result.hpp"
#include "home/topology.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

inline constexpr std::uint32_t kItemDurabilityMaximum = 10'000;

enum class ItemKind : std::uint8_t {
    Generic = 0,
    Consumable,
    Equipment,
    Key,
    Quest,
    Material
};

struct ItemState final {
    ItemId id{};
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    std::uint32_t durability{kItemDurabilityMaximum};
    std::optional<EntityId> owner{};
    std::optional<ZoneId> zone{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};
    auto operator<=>(const ItemState&) const = default;
};

struct ItemCreateInfo final {
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    std::uint32_t durability{kItemDurabilityMaximum};
    std::optional<EntityId> owner{};
    std::optional<ZoneId> zone{};
    std::uint64_t updated_world_minute{};
};

Result<void> validate_item_state(
    const EntityRegistry& entities,
    const TopologyRegistry& topology,
    const PlayerLifeLedger& life,
    const ItemState& state);

class InventoryLedger final {
public:
    Result<ItemId> create_item(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        const PlayerLifeLedger& life,
        ItemCreateInfo info);
    Result<void> set_item(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        const PlayerLifeLedger& life,
        ItemState state);
    Result<void> restore_item(
        const EntityRegistry& entities,
        const TopologyRegistry& topology,
        const PlayerLifeLedger& life,
        ItemState state);
    Result<ItemState> remove_item(ItemId id);
    Result<std::vector<ItemState>> purge_owner(EntityId owner, std::uint64_t world_minute);

    [[nodiscard]] const ItemState* find(ItemId id) const noexcept;
    [[nodiscard]] std::vector<ItemState> inventory_for(EntityId owner) const;
    [[nodiscard]] std::vector<ItemState> items_in_zone(ZoneId zone) const;
    [[nodiscard]] std::vector<ItemState> snapshot() const;

private:
    [[nodiscard]] Result<ItemId> allocate_item_id();
    void advance_allocator_past(ItemId id) noexcept;

    std::vector<ItemState> items_{};
    std::uint64_t next_item_value_{1};
};

} // namespace home
