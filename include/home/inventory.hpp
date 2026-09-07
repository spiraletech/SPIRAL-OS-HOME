#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace home {

enum class ItemKind : std::uint8_t {
    Generic = 0,
    Consumable,
    Equipment,
    Key,
    Quest
};

struct ItemState final {
    ItemId id{};
    std::string archetype_key{};
    std::string display_name{};
    ItemKind kind{ItemKind::Generic};
    std::uint32_t quantity{1};
    std::uint32_t max_stack{1};
    PlayerId owner{};
    std::uint64_t item_revision{};
    auto operator<=>(const ItemState&) const = default;
};

class InventoryLedger final {
public:
    Result<ItemId> create_item(std::string archetype_key,
                               std::string display_name,
                               ItemKind kind,
                               std::uint32_t quantity = 1,
                               std::uint32_t max_stack = 1,
                               PlayerId owner = {});

    Result<void> set_owner(ItemId id, PlayerId owner);
    Result<void> set_quantity(ItemId id, std::uint32_t quantity);
    Result<void> add_quantity(ItemId id, std::uint32_t amount);
    Result<void> consume(ItemId id, std::uint32_t amount);
    Result<void> remove_item(ItemId id);

    [[nodiscard]] const ItemState* find(ItemId id) const noexcept;
    [[nodiscard]] std::vector<ItemState> inventory_for(PlayerId owner) const;
    [[nodiscard]] std::vector<ItemState> unowned_items() const;
    [[nodiscard]] std::vector<ItemState> snapshot() const;

private:
    std::vector<ItemState> items_{};
    std::uint64_t next_item_id_{1};
};

} // namespace home
