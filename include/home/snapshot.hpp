#pragma once

#include "home/entity.hpp"
#include "home/result.hpp"
#include "home/revision.hpp"
#include "home/topology.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace home {

struct SnapshotPlacement final {
    EntityId entity{};
    ZoneId zone{};
    auto operator<=>(const SnapshotPlacement&) const = default;
};

struct WorldSnapshot final {
    WorldId world{};
    WorldRevision revision{};
    std::vector<EntityRecord> entities{};
    std::vector<ZoneRecord> zones{};
    std::vector<ZoneConnection> connections{};
    std::vector<SnapshotPlacement> placements{};
};

inline constexpr std::uint32_t kSnapshotFormatVersion = 1;

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot);
Result<WorldSnapshot> decode_snapshot(std::string_view encoded);
Result<void> save_snapshot_file(const WorldSnapshot& snapshot, const std::string& path);
Result<WorldSnapshot> load_snapshot_file(const std::string& path);

} // namespace home
