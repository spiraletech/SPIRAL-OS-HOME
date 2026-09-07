#include "home/weather.hpp"

#include <algorithm>

namespace home {
namespace {
unsigned clamp_permille(unsigned value) { return std::min(value, 1000u); }
unsigned season_wet_bias(Season season) {
    switch (season) {
        case Season::Winter: return 180;
        case Season::Spring: return 80;
        case Season::Summer: return 0;
        case Season::Autumn: return 100;
    }
    return 0;
}
unsigned intensity_memory(RainIntensity intensity) {
    return static_cast<unsigned>(intensity) * 85u;
}
RainIntensity intensity_from_score(unsigned score) {
    if (score < 260) return RainIntensity::Dry;
    if (score < 390) return RainIntensity::Drizzle;
    if (score < 540) return RainIntensity::Rain;
    if (score < 690) return RainIntensity::Downpour;
    if (score < 850) return RainIntensity::Storm;
    return RainIntensity::Deluge;
}
} // namespace

Result<WeatherState> evolve_weather(const WeatherState& previous, const ClimateProfile& climate, Season season, std::uint64_t entropy) {
    if (!climate.zone.valid() || previous.zone != climate.zone || climate.wetness_permille > 1000 || climate.wind_permille > 1000) {
        return Result<WeatherState>::failure(ErrorCode::ValidationFailed, "weather/climate zone state is invalid");
    }
    const std::uint64_t mixed = entropy ^ climate.seed ^ (previous.sequence * 0x9E3779B97F4A7C15ULL);
    const unsigned roll = static_cast<unsigned>((mixed ^ (mixed >> 33) ^ (mixed << 11)) % 1001ULL);
    const unsigned persistence = intensity_memory(previous.intensity);
    const unsigned score = clamp_permille((climate.wetness_permille * 5u + roll * 2u + season_wet_bias(season) * 2u + persistence) / 8u);

    WeatherState next = previous;
    next.previous_intensity = previous.intensity;
    next.intensity = intensity_from_score(score);
    next.precipitation_permille = next.intensity == RainIntensity::Dry ? 0u : clamp_permille(score);
    next.cloud_permille = clamp_permille((score + climate.wetness_permille) / 2u + (next.intensity == RainIntensity::Dry ? 80u : 220u));
    next.wind_mm_per_second = (climate.wind_permille * 12000u + roll * 5000u) / 1000u;
    const int seasonal_delta = season == Season::Summer ? 6000 : season == Season::Winter ? -5000 : season == Season::Spring ? 1000 : -1000;
    next.temperature_millicelsius = climate.mean_temperature_millicelsius + seasonal_delta - static_cast<int>(next.precipitation_permille * 3u);
    next.sequence = previous.sequence + 1;
    next.age_minutes = next.intensity == previous.intensity ? previous.age_minutes + 1 : 0;
    return Result<WeatherState>::success(next);
}

Result<void> WeatherLedger::set(WeatherState state) {
    if (!state.zone.valid() || state.cloud_permille > 1000 || state.precipitation_permille > 1000) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "weather state is invalid");
    }
    auto it = std::find_if(states_.begin(), states_.end(), [&](const WeatherState& item){ return item.zone == state.zone; });
    if (it == states_.end()) states_.push_back(state); else *it = state;
    std::sort(states_.begin(), states_.end(), [](const WeatherState& a, const WeatherState& b){ return a.zone.value() < b.zone.value(); });
    return Result<void>::success();
}

const WeatherState* WeatherLedger::find(ZoneId zone) const noexcept {
    const auto it = std::find_if(states_.begin(), states_.end(), [&](const WeatherState& state){ return state.zone == zone; });
    return it == states_.end() ? nullptr : &*it;
}

std::vector<WeatherState> WeatherLedger::snapshot() const { return states_; }

} // namespace home
