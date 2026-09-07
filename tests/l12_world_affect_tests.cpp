#include "home/world_affect.hpp"

#include <cassert>

int main() {
    using namespace home;
    CalendarState halloween_date{}; halloween_date.date = CalendarDate{2026,10,31};
    const EventDefinition halloween{EventId{1}, "halloween", "Halloween", EventKind::Holiday, AnnualDateRule{10,31,1}, 10};
    WeatherState storm{}; storm.zone=ZoneId{1}; storm.intensity=RainIntensity::Storm;
    const auto haunted = resolve_world_context(halloween_date, {halloween}, &storm);
    assert(haunted.ok());
    assert(haunted.value().affect.tone == WorldTone::Haunted);
    assert(haunted.value().anchor.anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(!haunted.value().anchor.authorized_override);
    assert(haunted.value().calendar.date == halloween_date.date);
    assert(storm.intensity == RainIntensity::Storm);

    CalendarState ordinary{}; ordinary.date = CalendarDate{2026,11,8};
    const auto weather_only = resolve_world_context(ordinary, {}, &storm);
    assert(weather_only.ok());
    assert(weather_only.value().affect.tone == WorldTone::Tense);
    assert(weather_only.value().anchor.anchor == ThemeAnchor::None);

    WorldAnchorState override_state{ThemeAnchor::HauntedHalloweenRain, 500, false};
    const auto forced = resolve_world_context(ordinary, {}, nullptr, override_state);
    assert(forced.ok());
    assert(forced.value().anchor.anchor == ThemeAnchor::HauntedHalloweenRain);
    assert(forced.value().anchor.authorized_override);
    assert(forced.value().calendar.date == ordinary.date);
    return 0;
}
