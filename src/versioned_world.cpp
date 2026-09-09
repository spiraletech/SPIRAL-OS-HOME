#include "home/versioned_world.hpp"

#include <utility>

namespace home {

VersionedWorld::VersionedWorld(WorldId world) noexcept : registry_(world), topology_(world) {}

Result<void> VersionedWorld::commit(std::vector<WorldChange> changes) {
    if (changes.empty()) return Result<void>::failure(ErrorCode::ValidationFailed, "cannot commit an empty world delta");
    const auto next = revision_.next();
    if (!next.has_value()) return Result<void>::failure(ErrorCode::Overflow, "world revision space exhausted");
    const WorldRevision from = revision_;
    revision_ = *next;
    history_.emplace_back(from, revision_, std::move(changes));
    return Result<void>::success();
}

Result<EntityId> VersionedWorld::create_entity(EntityCreateInfo info) {
    const auto created = registry_.create(std::move(info)); if (!created) return created;
    const EntityId id = created.value(); const EntityRecord* record = registry_.find(id);
    if (!record) return Result<EntityId>::failure(ErrorCode::InternalError, "created entity missing from registry");
    const auto c = commit({WorldChange{WorldChangeKind::EntityCreated, EntityCreated{*record}}});
    if (!c) { registry_.remove(id); return Result<EntityId>::failure(c.error().code, c.error().message); }
    return Result<EntityId>::success(id);
}

Result<void> VersionedWorld::restore_entity(EntityRecord record) {
    const EntityRecord copy = record; const auto r = registry_.restore(std::move(record)); if (!r) return r;
    const auto c = commit({WorldChange{WorldChangeKind::EntityCreated, EntityCreated{copy}}});
    if (!c) { registry_.remove(copy.id); return c; } return Result<void>::success();
}

Result<EntityRecord> VersionedWorld::remove_entity(EntityId id) {
    const auto prior = topology_.zone_of(id);
    std::optional<PlayerLifeState> prior_life{};
    std::optional<PlayerDynamicsState> prior_dynamics{};
    if (const auto* life = player_life_.find(id)) prior_life = *life;
    if (const auto* dynamics = player_dynamics_.find(id)) prior_dynamics = *dynamics;

    RelationshipsLedger staged_relationships = relationships_;
    InventoryLedger staged_inventory = inventory_;
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;
    const auto social_purge = staged_relationships.purge_entity(id, current_world_minute);
    if (!social_purge) return Result<EntityRecord>::failure(social_purge.error().code, social_purge.error().message);
    const auto item_purge = staged_inventory.purge_owner(id, current_world_minute);
    if (!item_purge) return Result<EntityRecord>::failure(item_purge.error().code, item_purge.error().message);

    const auto removed = registry_.remove(id); if (!removed) return removed;
    EntityRecord record = removed.value(); std::vector<WorldChange> changes;
    if (prior) {
        const auto cleared = topology_.clear_entity(id);
        if (!cleared) { registry_.restore(record); return Result<EntityRecord>::failure(cleared.error().code, cleared.error().message); }
        changes.push_back({WorldChangeKind::EntityZoneChanged, EntityZoneChanged{id, prior, std::nullopt}});
    }
    for (const auto& relationship : social_purge.value().removed_relationships) {
        changes.push_back({WorldChangeKind::RelationshipStateChanged, RelationshipStateChanged{relationship, std::nullopt}});
    }
    if (social_purge.value().household_before.has_value()) {
        changes.push_back({WorldChangeKind::HouseholdStateChanged, HouseholdStateChanged{
            social_purge.value().household_before, social_purge.value().household_after}});
    }
    for (const auto& item_after : item_purge.value()) {
        const auto* prior_item = inventory_.find(item_after.id);
        if (prior_item) changes.push_back({WorldChangeKind::ItemStateChanged, ItemStateChanged{*prior_item, item_after}});
    }
    if (prior_dynamics.has_value()) {
        const auto dynamics_removed = player_dynamics_.remove(id);
        if (!dynamics_removed) {
            registry_.restore(record);
            if (prior) topology_.place_entity(id, *prior);
            return Result<EntityRecord>::failure(dynamics_removed.error().code, dynamics_removed.error().message);
        }
        changes.push_back({WorldChangeKind::PlayerDynamicsStateChanged, PlayerDynamicsStateChanged{prior_dynamics, std::nullopt}});
    }
    if (prior_life.has_value()) {
        const auto life_removed = player_life_.remove(id);
        if (!life_removed) {
            registry_.restore(record);
            if (prior) topology_.place_entity(id, *prior);
            if (prior_dynamics) player_dynamics_.set(registry_, player_life_, *prior_dynamics);
            return Result<EntityRecord>::failure(life_removed.error().code, life_removed.error().message);
        }
        changes.push_back({WorldChangeKind::PlayerLifeStateChanged, PlayerLifeStateChanged{prior_life, std::nullopt}});
    }
    changes.push_back({WorldChangeKind::EntityRemoved, EntityRemoved{record}});
    const auto c = commit(std::move(changes));
    if (!c) {
        registry_.restore(record);
        if (prior) topology_.place_entity(id, *prior);
        if (prior_life) player_life_.set(registry_, topology_, *prior_life);
        if (prior_dynamics) player_dynamics_.set(registry_, player_life_, *prior_dynamics);
        return Result<EntityRecord>::failure(c.error().code,c.error().message);
    }
    relationships_ = std::move(staged_relationships);
    inventory_ = std::move(staged_inventory);
    return Result<EntityRecord>::success(std::move(record));
}

Result<void> VersionedWorld::update_transform(EntityId id, Transform transform) {
    EntityRecord* r=registry_.find_mutable(id); if(!r) return Result<void>::failure(ErrorCode::NotFound,"entity not found");
    if(r->transform==transform) return Result<void>::failure(ErrorCode::ValidationFailed,"transform update produced no state change");
    const Transform before=r->transform; r->transform=transform; const auto c=commit({{WorldChangeKind::EntityTransformUpdated,EntityTransformUpdated{id,before,transform}}});
    if(!c){r->transform=before;return c;} return Result<void>::success();
}

Result<ZoneId> VersionedWorld::create_zone(EntityCreateInfo) = delete;

Result<ZoneId> VersionedWorld::create_zone(ZoneCreateInfo info) {
    const auto z=topology_.create_zone(std::move(info)); if(!z)return z; const auto* r=topology_.find(z.value());
    if(!r)return Result<ZoneId>::failure(ErrorCode::InternalError,"created zone missing from topology");
    const auto c=commit({{WorldChangeKind::ZoneCreated,ZoneCreated{*r}}}); if(!c)return Result<ZoneId>::failure(c.error().code,c.error().message); return z;
}
Result<void> VersionedWorld::restore_zone(ZoneRecord zone){const auto copy=zone;const auto r=topology_.restore_zone(std::move(zone));if(!r)return r;return commit({{WorldChangeKind::ZoneCreated,ZoneCreated{copy}}});}
Result<void> VersionedWorld::set_zone_parent(ZoneId child,std::optional<ZoneId> parent){const auto* z=topology_.find(child);if(!z)return Result<void>::failure(ErrorCode::NotFound,"child zone not found");const auto before=z->parent;const auto r=topology_.set_parent(child,parent);if(!r)return r;const auto c=commit({{WorldChangeKind::ZoneParentChanged,ZoneParentChanged{child,before,parent}}});if(!c){topology_.set_parent(child,before);return c;}return Result<void>::success();}
Result<void> VersionedWorld::connect_zones(ZoneConnection x){const auto copy=x;const auto r=topology_.connect(std::move(x));if(!r)return r;const auto c=commit({{WorldChangeKind::ZonesConnected,ZonesConnected{copy}}});if(!c){topology_.disconnect(copy.from,copy.to);return c;}return Result<void>::success();}
Result<void> VersionedWorld::place_entity(EntityId e,ZoneId z){if(!registry_.contains(e))return Result<void>::failure(ErrorCode::NotFound,"entity not found");const auto before=topology_.zone_of(e);const auto r=topology_.place_entity(e,z);if(!r)return r;const auto c=commit({{WorldChangeKind::EntityZoneChanged,EntityZoneChanged{e,before,z}}});if(!c){if(before)topology_.place_entity(e,*before);else topology_.clear_entity(e);return c;}return Result<void>::success();}
Result<void> VersionedWorld::clear_entity_zone(EntityId e){if(!registry_.contains(e))return Result<void>::failure(ErrorCode::NotFound,"entity not found");const auto before=topology_.zone_of(e);if(!before)return Result<void>::failure(ErrorCode::NotFound,"entity has no zone placement");const auto r=topology_.clear_entity(e);if(!r)return r;const auto c=commit({{WorldChangeKind::EntityZoneChanged,EntityZoneChanged{e,before,std::nullopt}}});if(!c){topology_.place_entity(e,*before);return c;}return Result<void>::success();}

Result<TransactionReceipt> VersionedWorld::execute(const WorldTransaction& tx) {
    if (!tx.id.valid()) return Result<TransactionReceipt>::failure(ErrorCode::InvalidArgument,"transaction id must be valid");
    if (tx.authority.empty()) return Result<TransactionReceipt>::failure(ErrorCode::ValidationFailed,"transaction authority must not be empty");
    if (tx.expected_revision != revision_) return Result<TransactionReceipt>::failure(ErrorCode::RevisionConflict,"transaction expected revision does not match canonical revision");
    if (tx.operations.empty()) return Result<TransactionReceipt>::failure(ErrorCode::ValidationFailed,"transaction must contain operations");

    EntityRegistry staged_registry = registry_;
    TopologyRegistry staged_topology = topology_;
    PlayerLifeLedger staged_life = player_life_;
    PlayerDynamicsLedger staged_dynamics = player_dynamics_;
    RelationshipsLedger staged_relationships = relationships_;
    InventoryLedger staged_inventory = inventory_;
    std::vector<WorldChange> changes; TransactionReceipt receipt{tx.id, revision_, revision_, {}, {}};
    const std::uint64_t current_world_minute = clock_.now().milliseconds / 60000ULL;

    for (const auto& op : tx.operations) {
        Result<void> status = Result<void>::success();
        std::visit([&](const auto& command) {
            using T = std::decay_t<decltype(command)>;
            if constexpr (std::is_same_v<T, TxCreateEntity>) {
                auto r=staged_registry.create(command.info); if(!r){status=Result<void>::failure(r.error().code,r.error().message);return;} const auto* rec=staged_registry.find(r.value()); receipt.created_entities.push_back(r.value()); changes.push_back({WorldChangeKind::EntityCreated,EntityCreated{*rec}});
            } else if constexpr (std::is_same_v<T, TxRemoveEntity>) {
                const auto prior=staged_topology.zone_of(command.id);
                std::optional<PlayerLifeState> prior_life{};
                std::optional<PlayerDynamicsState> prior_dynamics{};
                if (const auto* life = staged_life.find(command.id)) prior_life = *life;
                if (const auto* dynamics = staged_dynamics.find(command.id)) prior_dynamics = *dynamics;
                const auto social = staged_relationships.purge_entity(command.id, current_world_minute);
                if(!social){status=Result<void>::failure(social.error().code,social.error().message);return;}
                const auto items = staged_inventory.purge_owner(command.id, current_world_minute);
                if(!items){status=Result<void>::failure(items.error().code,items.error().message);return;}
                auto r=staged_registry.remove(command.id); if(!r){status=Result<void>::failure(r.error().code,r.error().message);return;}
                if(prior){staged_topology.clear_entity(command.id);changes.push_back({WorldChangeKind::EntityZoneChanged,EntityZoneChanged{command.id,prior,std::nullopt}});}
                for(const auto& relationship:social.value().removed_relationships)changes.push_back({WorldChangeKind::RelationshipStateChanged,RelationshipStateChanged{relationship,std::nullopt}});
                if(social.value().household_before)changes.push_back({WorldChangeKind::HouseholdStateChanged,HouseholdStateChanged{social.value().household_before,social.value().household_after}});
                for(const auto& item_after:items.value()){
                    const auto* item_before=inventory_.find(item_after.id);
                    if(item_before)changes.push_back({WorldChangeKind::ItemStateChanged,ItemStateChanged{*item_before,item_after}});
                }
                if(prior_dynamics){const auto removed_dynamics=staged_dynamics.remove(command.id);if(!removed_dynamics){status=Result<void>::failure(removed_dynamics.error().code,removed_dynamics.error().message);return;}changes.push_back({WorldChangeKind::PlayerDynamicsStateChanged,PlayerDynamicsStateChanged{prior_dynamics,std::nullopt}});}
                if(prior_life){const auto removed_life=staged_life.remove(command.id);if(!removed_life){status=Result<void>::failure(removed_life.error().code,removed_life.error().message);return;}changes.push_back({WorldChangeKind::PlayerLifeStateChanged,PlayerLifeStateChanged{prior_life,std::nullopt}});}
                changes.push_back({WorldChangeKind::EntityRemoved,EntityRemoved{r.value()}});
            } else if constexpr (std::is_same_v<T, TxUpdateTransform>) {
                auto* rec=staged_registry.find_mutable(command.id); if(!rec){status=Result<void>::failure(ErrorCode::NotFound,"entity not found");return;} if(rec->transform==command.transform){status=Result<void>::failure(ErrorCode::ValidationFailed,"transform update produced no state change");return;} const auto before=rec->transform;rec->transform=command.transform;changes.push_back({WorldChangeKind::EntityTransformUpdated,EntityTransformUpdated{command.id,before,command.transform}});
            } else if constexpr (std::is_same_v<T, TxCreateZone>) {
                auto r=staged_topology.create_zone(command.info);if(!r){status=Result<void>::failure(r.error().code,r.error().message);return;}const auto* rec=staged_topology.find(r.value());receipt.created_zones.push_back(r.value());changes.push_back({WorldChangeKind::ZoneCreated,ZoneCreated{*rec}});
            } else if constexpr (std::is_same_v<T, TxSetZoneParent>) {
                const auto* rec=staged_topology.find(command.child);if(!rec){status=Result<void>::failure(ErrorCode::NotFound,"child zone not found");return;}const auto before=rec->parent;auto r=staged_topology.set_parent(command.child,command.parent);if(!r){status=r;return;}changes.push_back({WorldChangeKind::ZoneParentChanged,ZoneParentChanged{command.child,before,command.parent}});
            } else if constexpr (std::is_same_v<T, TxConnectZones>) {
                const ZoneConnection x = command.connection; auto r=staged_topology.connect(x);if(!r){status=r;return;}changes.push_back({WorldChangeKind::ZonesConnected,ZonesConnected{x}});
            } else if constexpr (std::is_same_v<T, TxPlaceEntity>) {
                if(!staged_registry.contains(command.entity)){status=Result<void>::failure(ErrorCode::NotFound,"entity not found");return;}const auto before=staged_topology.zone_of(command.entity);if(command.zone){auto r=staged_topology.place_entity(command.entity,*command.zone);if(!r){status=r;return;}}else{if(!before){status=Result<void>::failure(ErrorCode::NotFound,"entity has no zone placement");return;}auto r=staged_topology.clear_entity(command.entity);if(!r){status=r;return;}}changes.push_back({WorldChangeKind::EntityZoneChanged,EntityZoneChanged{command.entity,before,command.zone}});
            }
        }, op);
        if (!status) return Result<TransactionReceipt>::failure(status.error().code,status.error().message);
    }

    const auto next=revision_.next(); if(!next) return Result<TransactionReceipt>::failure(ErrorCode::Overflow,"world revision space exhausted");
    registry_=std::move(staged_registry); topology_=std::move(staged_topology); player_life_=std::move(staged_life); player_dynamics_=std::move(staged_dynamics); relationships_=std::move(staged_relationships); inventory_=std::move(staged_inventory); const WorldRevision from=revision_; revision_=*next; history_.emplace_back(from,revision_,std::move(changes)); receipt.to_revision=revision_;
    return Result<TransactionReceipt>::success(std::move(receipt));
}

Result<std::vector<WorldDelta>> VersionedWorld::deltas_since(WorldRevision r) const {if(r>revision_)return Result<std::vector<WorldDelta>>::failure(ErrorCode::RevisionConflict,"requested revision is ahead of canonical world revision");std::vector<WorldDelta> out;for(const auto& d:history_)if(d.to_revision()>r)out.push_back(d);return Result<std::vector<WorldDelta>>::success(std::move(out));}

} // namespace home
