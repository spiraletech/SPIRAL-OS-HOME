#include "home/events.hpp"

#include <cassert>

int main() {
    using namespace home;
    const EventDefinition halloween{EventId{1}, "halloween", "Halloween", EventKind::Holiday, AnnualDateRule{10,31,1}, 10};
    const EventDefinition year_turn{EventId{2}, "year_turn", "Year Turn", EventKind::Festival, AnnualDateRule{12,31,2}, 5};
    const std::vector<EventDefinition> defs{halloween, year_turn};

    CalendarState oct{}; oct.date = CalendarDate{2026,10,31};
    const auto active = resolve_events(defs, oct);
    assert(active.ok()); assert(active.value().size()==1); assert(active.value()[0].id==EventId{1});

    CalendarState jan{}; jan.date = CalendarDate{2027,1,1};
    const auto crossover = resolve_events(defs, jan);
    assert(crossover.ok()); assert(crossover.value().size()==1); assert(crossover.value()[0].id==EventId{2});

    CalendarState july{}; july.date = CalendarDate{2026,7,4};
    EventSelector forced{}; forced.include_calendar_events=false; forced.forced_event=EventId{1};
    const auto override_result = resolve_events(defs, july, forced);
    assert(override_result.ok()); assert(override_result.value().size()==1); assert(override_result.value()[0].id==EventId{1});
    assert(july.date == CalendarDate{2026,7,4});

    EventSelector missing{}; missing.forced_event=EventId{99};
    const auto missing_result=resolve_events(defs,july,missing); assert(!missing_result.ok()); assert(missing_result.error().code==ErrorCode::NotFound);
    return 0;
}
