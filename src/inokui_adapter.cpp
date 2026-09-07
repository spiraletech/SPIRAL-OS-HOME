#include "home/inokui_adapter.hpp"

namespace home {

InokuiWorldView InokuiAdapter::snapshot() const {
    InokuiWorldView view{};
    view.world = world_.world();
    view.revision = world_.revision();
    view.calendar = world_.calendar();

    const auto records = world_.entities().snapshot();
    view.entities.reserve(records.size());
    for (const auto& record : records) {
        view.entities.push_back(InokuiEntityState{
            record.id,
            record.kind,
            record.archetype,
            record.display_name,
            record.transform,
            world_.topology().zone_of(record.id)
        });
    }

    return view;
}

} // namespace home
