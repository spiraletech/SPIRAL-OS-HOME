#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace home {

struct SubclassState final {
    PlayerId player{};
    std::string key{};
    std::uint32_t rank{1};
    std::uint64_t subclass_revision{};
    auto operator<=>(const SubclassState&) const = default;
};

struct TheorismState final {
    PlayerId player{};
    std::string key{};
    std::uint32_t conviction{};
    std::uint64_t theorism_revision{};
    auto operator<=>(const TheorismState&) const = default;
};

struct AuraState final {
    PlayerId player{};
    std::string signature{};
    std::uint32_t intensity{};
    std::int32_t charge{};
    std::uint64_t aura_revision{};
    auto operator<=>(const AuraState&) const = default;
};

class TheorismLedger final {
public:
    static constexpr std::uint32_t max_subclass_rank = 100;
    static constexpr std::uint32_t max_conviction = 1000;
    static constexpr std::uint32_t max_aura_intensity = 1000;
    static constexpr std::int32_t min_aura_charge = -1000;
    static constexpr std::int32_t max_aura_charge = 1000;

    Result<void> set_subclass(PlayerId player, std::string key, std::uint32_t rank = 1);
    Result<void> set_subclass_rank(PlayerId player, std::uint32_t rank);
    Result<void> clear_subclass(PlayerId player);

    Result<void> set_theorism(PlayerId player, std::string key, std::uint32_t conviction = 0);
    Result<void> set_theorism_conviction(PlayerId player, std::uint32_t conviction);
    Result<void> clear_theorism(PlayerId player);

    Result<void> set_aura(PlayerId player,
                          std::string signature,
                          std::uint32_t intensity = 0,
                          std::int32_t charge = 0);
    Result<void> set_aura_intensity(PlayerId player, std::uint32_t intensity);
    Result<void> set_aura_charge(PlayerId player, std::int32_t charge);
    Result<void> clear_aura(PlayerId player);

    [[nodiscard]] const SubclassState* find_subclass(PlayerId player) const noexcept;
    [[nodiscard]] const TheorismState* find_theorism(PlayerId player) const noexcept;
    [[nodiscard]] const AuraState* find_aura(PlayerId player) const noexcept;

    [[nodiscard]] std::vector<SubclassState> subclass_snapshot() const;
    [[nodiscard]] std::vector<TheorismState> theorism_snapshot() const;
    [[nodiscard]] std::vector<AuraState> aura_snapshot() const;

private:
    std::vector<SubclassState> subclasses_{};
    std::vector<TheorismState> theorisms_{};
    std::vector<AuraState> auras_{};
};

} // namespace home
