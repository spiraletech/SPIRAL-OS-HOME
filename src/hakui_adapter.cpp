#include "home/hakui_adapter.hpp"

#include <utility>

namespace home {

HakuiWorldView HakuiAdapter::snapshot() const {
    HakuiWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();

    const auto records = world_.entities().snapshot();
    view.entities.reserve(records.size());
    for (const auto& record : records) {
        view.entities.push_back(HakuiEntityState{
            record.id,
            record.kind,
            record.archetype,
            record.transform,
            world_.topology().zone_of(record.id)
        });
    }

    return view;
}

Result<WorldRevision> HakuiAdapter::apply(const HakuiConsequence& consequence) {
    if (consequence.expected_revision != world_.revision()) {
        return Result<WorldRevision>::failure(
            ErrorCode::RevisionConflict,
            "HAKUI consequence was produced from a stale HOME revision");
    }

    if (!world_.entities().contains(consequence.entity)) {
        return Result<WorldRevision>::failure(
            ErrorCode::NotFound,
            "HAKUI consequence references an unknown HOME entity");
    }

    Result<void> result = Result<void>::failure(ErrorCode::InvalidArgument, "unknown HAKUI consequence kind");
    switch (consequence.kind) {
    case HakuiConsequenceKind::TransformChanged:
        result = world_.update_transform(consequence.entity, consequence.transform);
        break;
    case HakuiConsequenceKind::EnteredZone:
        result = world_.place_entity(consequence.entity, consequence.zone);
        break;
    case HakuiConsequenceKind::LeftZone:
        result = world_.clear_entity_zone(consequence.entity);
        break;
    }

    if (!result) {
        return Result<WorldRevision>::failure(result.error().code, result.error().message);
    }

    return Result<WorldRevision>::success(world_.revision());
}

} // namespace home
