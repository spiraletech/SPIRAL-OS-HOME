#include "home/topology.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {

TopologyRegistry::TopologyRegistry(WorldId world, std::uint64_t first_zone_value) noexcept
    : world_(world), next_zone_value_(first_zone_value == 0 ? 1 : first_zone_value) {}

bool TopologyRegistry::contains(ZoneId id) const noexcept {
    return find(id) != nullptr;
}

Result<ZoneId> TopologyRegistry::allocate_zone_id() {
    if (!world_.valid()) {
        return Result<ZoneId>::failure(ErrorCode::ValidationFailed, "topology requires a valid world id");
    }

    while (next_zone_value_ != 0) {
        const ZoneId candidate{next_zone_value_};
        if (next_zone_value_ == std::numeric_limits<std::uint64_t>::max()) {
            next_zone_value_ = 0;
        } else {
            ++next_zone_value_;
        }
        if (!contains(candidate)) {
            return Result<ZoneId>::success(candidate);
        }
    }

    return Result<ZoneId>::failure(ErrorCode::Overflow, "zone id space exhausted");
}

Result<ZoneId> TopologyRegistry::create_zone(ZoneCreateInfo info) {
    if (info.key.empty()) {
        return Result<ZoneId>::failure(ErrorCode::ValidationFailed, "zone key must not be empty");
    }
    if (info.parent.has_value() && !contains(*info.parent)) {
        return Result<ZoneId>::failure(ErrorCode::NotFound, "parent zone not found");
    }
    const auto duplicate_key = std::find_if(zones_.begin(), zones_.end(), [&info](const ZoneRecord& zone) {
        return zone.key == info.key;
    });
    if (duplicate_key != zones_.end()) {
        return Result<ZoneId>::failure(ErrorCode::AlreadyExists, "zone key already exists");
    }

    auto allocated = allocate_zone_id();
    if (!allocated) {
        return allocated;
    }

    const ZoneId id = allocated.value();
    zones_.push_back(ZoneRecord{
        id,
        world_,
        info.kind,
        std::move(info.key),
        std::move(info.display_name),
        info.parent,
        info.persistent
    });
    std::sort(zones_.begin(), zones_.end(), [](const ZoneRecord& a, const ZoneRecord& b) {
        return a.id.value() < b.id.value();
    });
    return Result<ZoneId>::success(id);
}

Result<void> TopologyRegistry::restore_zone(ZoneRecord zone) {
    if (!world_.valid()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "topology requires a valid world id");
    }
    if (!zone.id.valid() || zone.world != world_ || zone.key.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "restored zone is invalid");
    }
    if (contains(zone.id)) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "zone id already exists");
    }
    if (std::any_of(zones_.begin(), zones_.end(), [&zone](const ZoneRecord& existing) {
            return existing.key == zone.key;
        })) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "zone key already exists");
    }
    if (zone.parent.has_value() && !contains(*zone.parent)) {
        return Result<void>::failure(ErrorCode::NotFound, "parent zone not found");
    }

    const ZoneId id = zone.id;
    zones_.push_back(std::move(zone));
    std::sort(zones_.begin(), zones_.end(), [](const ZoneRecord& a, const ZoneRecord& b) {
        return a.id.value() < b.id.value();
    });
    advance_allocator_past(id);
    return Result<void>::success();
}

Result<void> TopologyRegistry::set_parent(ZoneId child, std::optional<ZoneId> parent) {
    ZoneRecord* child_record = nullptr;
    for (auto& zone : zones_) {
        if (zone.id == child) {
            child_record = &zone;
            break;
        }
    }
    if (child_record == nullptr) {
        return Result<void>::failure(ErrorCode::NotFound, "child zone not found");
    }
    if (parent.has_value()) {
        if (!contains(*parent)) {
            return Result<void>::failure(ErrorCode::NotFound, "parent zone not found");
        }
        if (*parent == child || would_create_cycle(child, *parent)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "zone parent would create a cycle");
        }
    }
    if (child_record->parent == parent) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "zone parent produced no state change");
    }
    child_record->parent = parent;
    return Result<void>::success();
}

Result<void> TopologyRegistry::connect(ZoneConnection connection) {
    if (!connection.from.valid() || !connection.to.valid() || connection.from == connection.to) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "zone connection endpoints are invalid");
    }
    if (!contains(connection.from) || !contains(connection.to)) {
        return Result<void>::failure(ErrorCode::NotFound, "zone connection endpoint not found");
    }

    const auto duplicate = std::find_if(connections_.begin(), connections_.end(), [&connection](const ZoneConnection& existing) {
        if (existing == connection) {
            return true;
        }
        return connection.bidirectional && existing.bidirectional &&
               existing.from == connection.to && existing.to == connection.from &&
               existing.traversable == connection.traversable && existing.tag == connection.tag;
    });
    if (duplicate != connections_.end()) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "zone connection already exists");
    }

    connections_.push_back(std::move(connection));
    return Result<void>::success();
}

Result<void> TopologyRegistry::disconnect(ZoneId from, ZoneId to) {
    const auto it = std::find_if(connections_.begin(), connections_.end(), [from, to](const ZoneConnection& connection) {
        return (connection.from == from && connection.to == to) ||
               (connection.bidirectional && connection.from == to && connection.to == from);
    });
    if (it == connections_.end()) {
        return Result<void>::failure(ErrorCode::NotFound, "zone connection not found");
    }
    connections_.erase(it);
    return Result<void>::success();
}

Result<void> TopologyRegistry::place_entity(EntityId entity, ZoneId zone) {
    if (!entity.valid() || !zone.valid()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "entity and zone ids must be valid");
    }
    if (!contains(zone)) {
        return Result<void>::failure(ErrorCode::NotFound, "zone not found");
    }

    for (auto& placement : placements_) {
        if (placement.entity == entity) {
            if (placement.zone == zone) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "entity is already in zone");
            }
            placement.zone = zone;
            return Result<void>::success();
        }
    }

    placements_.push_back(Placement{entity, zone});
    std::sort(placements_.begin(), placements_.end(), [](const Placement& a, const Placement& b) {
        return a.entity.value() < b.entity.value();
    });
    return Result<void>::success();
}

Result<void> TopologyRegistry::clear_entity(EntityId entity) {
    const auto it = std::find_if(placements_.begin(), placements_.end(), [entity](const Placement& placement) {
        return placement.entity == entity;
    });
    if (it == placements_.end()) {
        return Result<void>::failure(ErrorCode::NotFound, "entity has no zone placement");
    }
    placements_.erase(it);
    return Result<void>::success();
}

const ZoneRecord* TopologyRegistry::find(ZoneId id) const noexcept {
    const auto it = std::find_if(zones_.begin(), zones_.end(), [id](const ZoneRecord& zone) {
        return zone.id == id;
    });
    return it == zones_.end() ? nullptr : &*it;
}

std::optional<ZoneId> TopologyRegistry::zone_of(EntityId entity) const noexcept {
    const auto it = std::find_if(placements_.begin(), placements_.end(), [entity](const Placement& placement) {
        return placement.entity == entity;
    });
    return it == placements_.end() ? std::nullopt : std::optional<ZoneId>{it->zone};
}

std::vector<ZoneId> TopologyRegistry::children_of(ZoneId parent) const {
    std::vector<ZoneId> output;
    for (const auto& zone : zones_) {
        if (zone.parent == parent) {
            output.push_back(zone.id);
        }
    }
    return output;
}

std::vector<ZoneId> TopologyRegistry::neighbors_of(ZoneId zone, bool traversable_only) const {
    std::vector<ZoneId> output;
    for (const auto& connection : connections_) {
        if (traversable_only && !connection.traversable) {
            continue;
        }
        if (connection.from == zone) {
            output.push_back(connection.to);
        } else if (connection.bidirectional && connection.to == zone) {
            output.push_back(connection.from);
        }
    }
    std::sort(output.begin(), output.end(), [](ZoneId a, ZoneId b) { return a.value() < b.value(); });
    output.erase(std::unique(output.begin(), output.end()), output.end());
    return output;
}

bool TopologyRegistry::directly_traversable(ZoneId from, ZoneId to) const noexcept {
    if (from == to && from.valid() && contains(from)) {
        return true;
    }
    return std::any_of(connections_.begin(), connections_.end(), [from, to](const ZoneConnection& connection) {
        if (!connection.traversable) {
            return false;
        }
        if (connection.from == from && connection.to == to) {
            return true;
        }
        return connection.bidirectional && connection.from == to && connection.to == from;
    });
}

std::vector<ZoneRecord> TopologyRegistry::snapshot_zones() const {
    return zones_;
}

std::vector<ZoneConnection> TopologyRegistry::snapshot_connections() const {
    auto output = connections_;
    std::sort(output.begin(), output.end(), [](const ZoneConnection& a, const ZoneConnection& b) {
        if (a.from != b.from) return a.from.value() < b.from.value();
        if (a.to != b.to) return a.to.value() < b.to.value();
        return a.tag < b.tag;
    });
    return output;
}

bool TopologyRegistry::would_create_cycle(ZoneId child, ZoneId parent) const noexcept {
    std::optional<ZoneId> cursor = parent;
    while (cursor.has_value()) {
        if (*cursor == child) {
            return true;
        }
        const ZoneRecord* zone = find(*cursor);
        if (zone == nullptr) {
            return false;
        }
        cursor = zone->parent;
    }
    return false;
}

void TopologyRegistry::advance_allocator_past(ZoneId id) noexcept {
    if (next_zone_value_ == 0 || id.value() < next_zone_value_) {
        return;
    }
    if (id.value() == std::numeric_limits<std::uint64_t>::max()) {
        next_zone_value_ = 0;
    } else {
        next_zone_value_ = id.value() + 1;
    }
}

} // namespace home
