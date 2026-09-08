#include "home/weather.hpp"
#include "home/versioned_world.hpp"

#include <algorithm>
#include <limits>
#include <optional>

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

bool valid_climate_profile(const ClimateProfile& climate) noexcept {
    return climate.zone.valid()
        && climate.wetness_permille <= 1000
        && climate.wind_permille <= 1000
        && climate.seed != 0
        && climate.mean_temperature_millicelsius >= -100000
        && climate.mean_temperature_millicelsius <= 100000;
}

bool valid_weather_state(const WeatherState& state) noexcept {
    return state.zone.valid()
        && static_cast<unsigned>(state.intensity) <= static_cast<unsigned>(RainIntensity::Deluge)
        && static_cast<unsigned>(state.previous_intensity) <= static_cast<unsigned>(RainIntensity::Deluge)
        && state.cloud_permille <= 1000
        && state.precipitation_permille <= 1000
        && state.wind_mm_per_second <= 100000
        && state.temperature_millicelsius >= -150000
        && state.temperature_millicelsius <= 150000;
}

WeatherState initial_weather_state(const ClimateProfile& climate) noexcept {
    WeatherState state{};
    state.zone = climate.zone;
    state.temperature_millicelsius = climate.mean_temperature_millicelsius;
    return state;
}

Result<WeatherState> evolve_weather(const WeatherState& previous, const ClimateProfile& climate, Season season, std::uint64_t entropy) {
    if (!valid_climate_profile(climate) || !valid_weather_state(previous) || previous.zone != climate.zone) {
        return Result<WeatherState>::failure(ErrorCode::ValidationFailed, "weather/climate zone state is invalid");
    }
    if (previous.sequence == std::numeric_limits<std::uint64_t>::max()) {
        return Result<WeatherState>::failure(ErrorCode::Overflow, "weather sequence exhausted");
    }

    const std::uint64_t mixed = entropy ^ climate.seed ^ (previous.sequence * 0x9E3779B97F4A7C15ULL);
    const unsigned roll = static_cast<unsigned>((mixed ^ (mixed >> 33) ^ (mixed << 11)) % 1001ULL);
    const unsigned persistence = intensity_memory(previous.intensity) + std::min(previous.age_minutes, 120u) * 2u;
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
    if (next.intensity == previous.intensity) {
        next.age_minutes = previous.age_minutes == std::numeric_limits<unsigned>::max()
            ? previous.age_minutes
            : previous.age_minutes + 1;
    } else {
        next.age_minutes = 0;
    }
    return Result<WeatherState>::success(next);
}

Result<void> ClimateCatalog::set(ClimateProfile profile) {
    if (!valid_climate_profile(profile)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "climate profile is invalid");
    }
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const ClimateProfile& item){ return item.zone == profile.zone; });
    if (it == profiles_.end()) profiles_.push_back(profile); else *it = profile;
    std::sort(profiles_.begin(), profiles_.end(), [](const ClimateProfile& a, const ClimateProfile& b){ return a.zone.value() < b.zone.value(); });
    return Result<void>::success();
}

Result<void> ClimateCatalog::remove(ZoneId zone) {
    const auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const ClimateProfile& item){ return item.zone == zone; });
    if (it == profiles_.end()) return Result<void>::failure(ErrorCode::NotFound, "climate profile not found");
    profiles_.erase(it);
    return Result<void>::success();
}

const ClimateProfile* ClimateCatalog::find(ZoneId zone) const noexcept {
    const auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const ClimateProfile& profile){ return profile.zone == zone; });
    return it == profiles_.end() ? nullptr : &*it;
}

std::vector<ClimateProfile> ClimateCatalog::snapshot() const { return profiles_; }

Result<void> WeatherLedger::set(WeatherState state) {
    if (!valid_weather_state(state)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "weather state is invalid");
    }
    auto it = std::find_if(states_.begin(), states_.end(), [&](const WeatherState& item){ return item.zone == state.zone; });
    if (it == states_.end()) states_.push_back(state); else *it = state;
    std::sort(states_.begin(), states_.end(), [](const WeatherState& a, const WeatherState& b){ return a.zone.value() < b.zone.value(); });
    return Result<void>::success();
}

Result<void> WeatherLedger::remove(ZoneId zone) {
    const auto it = std::find_if(states_.begin(), states_.end(), [&](const WeatherState& state){ return state.zone == zone; });
    if (it == states_.end()) return Result<void>::failure(ErrorCode::NotFound, "weather state not found");
    states_.erase(it);
    return Result<void>::success();
}

const WeatherState* WeatherLedger::find(ZoneId zone) const noexcept {
    const auto it = std::find_if(states_.begin(), states_.end(), [&](const WeatherState& state){ return state.zone == zone; });
    return it == states_.end() ? nullptr : &*it;
}

std::vector<WeatherState> WeatherLedger::snapshot() const { return states_; }

Result<void> VersionedWorld::set_climate_profile(ClimateProfile profile) {
    if (!topology_.contains(profile.zone)) {
        return Result<void>::failure(ErrorCode::NotFound, "climate profile zone does not exist");
    }
    if (!valid_climate_profile(profile)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "climate profile is invalid");
    }

    std::optional<ClimateProfile> before{};
    if (const auto* current = climates_.find(profile.zone)) {
        before = *current;
        if (*current == profile) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "climate profile update produced no state change");
        }
    }

    const auto staged = climates_.set(profile);
    if (!staged) return staged;
    const auto committed = commit({WorldChange{
        WorldChangeKind::ClimateProfileChanged,
        ClimateProfileChanged{before, profile}
    }});
    if (!committed) {
        if (before.has_value()) climates_.set(*before); else climates_.remove(profile.zone);
        return committed;
    }
    return Result<void>::success();
}

Result<WeatherState> VersionedWorld::evolve_zone_weather(ZoneId zone, std::uint64_t entropy) {
    if (!topology_.contains(zone)) {
        return Result<WeatherState>::failure(ErrorCode::NotFound, "weather zone does not exist");
    }
    const ClimateProfile* climate = climates_.find(zone);
    if (!climate) {
        return Result<WeatherState>::failure(ErrorCode::NotFound, "weather zone has no climate profile");
    }

    std::optional<WeatherState> before{};
    WeatherState previous = initial_weather_state(*climate);
    if (const auto* current = weather_.find(zone)) {
        before = *current;
        previous = *current;
    }

    const Season season = calendar().season;
    const auto evolved = home::evolve_weather(previous, *climate, season, entropy);
    if (!evolved) return evolved;

    const WeatherState next = evolved.value();
    const auto staged = weather_.set(next);
    if (!staged) return Result<WeatherState>::failure(staged.error().code, staged.error().message);
    const auto committed = commit({WorldChange{
        WorldChangeKind::WeatherStateChanged,
        WeatherStateChanged{before, next, season, entropy}
    }});
    if (!committed) {
        if (before.has_value()) weather_.set(*before); else weather_.remove(zone);
        return Result<WeatherState>::failure(committed.error().code, committed.error().message);
    }
    return Result<WeatherState>::success(next);
}

} // namespace home
