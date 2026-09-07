#include "home/weather.hpp"

#include <cassert>

int main() {
    using namespace home;
    const ClimateProfile coast{ZoneId{1}, 18000, 700, 300, 12345};
    WeatherState start{}; start.zone = ZoneId{1};
    const auto a = evolve_weather(start, coast, Season::Winter, 77);
    const auto b = evolve_weather(start, coast, Season::Winter, 77);
    assert(a.ok() && b.ok());
    assert(a.value() == b.value());
    assert(a.value().sequence == 1);

    WeatherState rainy = a.value(); rainy.intensity = RainIntensity::Storm; rainy.age_minutes = 12;
    const auto memory = evolve_weather(rainy, coast, Season::Winter, 77);
    assert(memory.ok());
    assert(memory.value().previous_intensity == RainIntensity::Storm);
    assert(memory.value().sequence == rainy.sequence + 1);

    WeatherLedger ledger;
    assert(ledger.set(a.value()).ok());
    assert(ledger.find(ZoneId{1}) != nullptr);
    assert(ledger.snapshot().size() == 1);

    ClimateProfile foreign = coast; foreign.zone = ZoneId{2};
    const auto mismatch = evolve_weather(start, foreign, Season::Summer, 1);
    assert(!mismatch.ok());
    return 0;
}
