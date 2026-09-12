#include "home/hakui_adapter.hpp"

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <utility>

namespace home {
namespace {

bool hakui_authority(std::string_view authority) noexcept {
    return authority == "hakui" || authority.starts_with("hakui.");
}

bool contains_entity(const std::vector<EntityId>& ids, EntityId entity) {
    return std::find(ids.begin(), ids.end(), entity) != ids.end();
}

} // namespace

HakuiFrame HakuiAdapter::project() const {
    HakuiFrame frame{};
    frame.world = world_.world();
    frame.revision = world_.revision();
    frame.world_time = world_.clock().now();

    const auto entities = world_.entities().snapshot();
    frame.bodies.reserve(entities.size());
    for (const auto& entity : entities) {
        HakuiBodyState body{};
        body.entity = entity.id;
        body.kind = entity.kind;
        body.archetype = entity.archetype;
        body.transform = entity.transform;
        body.zone = world_.topology().zone_of(entity.id);
        body.persistent = entity.persistent;
        if (const auto* life = world_.player_life().find(entity.id)) {
            body.life_stage = life->stage;
            body.life_presence = life->presence;
        }
        frame.bodies.push_back(std::move(body));
    }

    std::sort(frame.bodies.begin(), frame.bodies.end(), [](const HakuiBodyState& a, const HakuiBodyState& b) {
        return a.entity.value() < b.entity.value();
    });
    return frame;
}

Result<HakuiApplyReceipt> HakuiAdapter::apply(const HakuiConsequenceBatch& batch) {
    if (!batch.id.valid()) {
        return Result<HakuiApplyReceipt>::failure(ErrorCode::InvalidArgument, "HAKUI batch id must be valid");
    }
    if (!hakui_authority(batch.authority)) {
        return Result<HakuiApplyReceipt>::failure(ErrorCode::ValidationFailed, "HAKUI batch authority must be hakui or hakui.*");
    }
    if (batch.expected_revision != world_.revision()) {
        return Result<HakuiApplyReceipt>::failure(ErrorCode::RevisionConflict, "HAKUI batch was produced from a stale HOME revision");
    }
    if (batch.consequences.empty()) {
        return Result<HakuiApplyReceipt>::failure(ErrorCode::ValidationFailed, "HAKUI batch must contain at least one consequence");
    }

    std::vector<EntityId> transform_entities;
    std::vector<EntityId> zone_entities;
    WorldTransaction tx{};
    tx.id = batch.id;
    tx.expected_revision = batch.expected_revision;
    tx.authority = batch.authority;
    tx.operations.reserve(batch.consequences.size());

    for (const auto& consequence : batch.consequences) {
        const auto validation = std::visit([&](const auto& item) -> Result<void> {
            using T = std::decay_t<decltype(item)>;
            if (!item.entity.valid()) {
                return Result<void>::failure(ErrorCode::InvalidArgument, "HAKUI consequence entity id must be valid");
            }
            const EntityRecord* entity = world_.entities().find(item.entity);
            if (entity == nullptr) {
                return Result<void>::failure(ErrorCode::NotFound, "HAKUI consequence references an unknown HOME entity");
            }

            if constexpr (std::is_same_v<T, HakuiTransformConsequence>) {
                if (contains_entity(transform_entities, item.entity)) {
                    return Result<void>::failure(ErrorCode::ValidationFailed, "HAKUI batch contains duplicate transform consequences for one entity");
                }
                if (entity->transform == item.transform) {
                    return Result<void>::failure(ErrorCode::ValidationFailed, "HAKUI transform consequence produced no HOME state change");
                }
                transform_entities.push_back(item.entity);
                tx.operations.push_back(TxUpdateTransform{item.entity, item.transform});
                return Result<void>::success();
            } else {
                if (contains_entity(zone_entities, item.entity)) {
                    return Result<void>::failure(ErrorCode::ValidationFailed, "HAKUI batch contains duplicate zone consequences for one entity");
                }
                if (item.zone.has_value() && !world_.topology().contains(*item.zone)) {
                    return Result<void>::failure(ErrorCode::NotFound, "HAKUI zone consequence references an unknown HOME zone");
                }
                const auto current_zone = world_.topology().zone_of(item.entity);
                if (current_zone == item.zone) {
                    return Result<void>::failure(ErrorCode::ValidationFailed, "HAKUI zone consequence produced no HOME state change");
                }
                zone_entities.push_back(item.entity);
                tx.operations.push_back(TxPlaceEntity{item.entity, item.zone});
                return Result<void>::success();
            }
        }, consequence);
        if (!validation) {
            return Result<HakuiApplyReceipt>::failure(validation.error().code, validation.error().message);
        }
    }

    const auto applied = world_.execute(tx);
    if (!applied) {
        return Result<HakuiApplyReceipt>::failure(applied.error().code, applied.error().message);
    }

    return Result<HakuiApplyReceipt>::success(HakuiApplyReceipt{
        batch.id,
        applied.value().from_revision,
        applied.value().to_revision,
        batch.consequences.size()
    });
}

} // namespace home
