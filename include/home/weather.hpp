#pragma once

#include "home/calendar.hpp"
#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <vector>

namespace home {

enum class RainIntensity : std::uint8_t { Dry = 0, Drizzle, Rain, Downpour, Storm, Deluge };

struct ClimateProfile final {
    ZoneId zone{};
    int mean_temperature_millicelsius{18000};
    unsigned wetness_permille{300};
    unsigned wind_permille{250};
    std::uint64_t seed{1};
    auto operator<=>(const ClimateProfile&) const = default;
};

struct WeatherState final {
    ZoneId zone{};
    RainIntensity intensity{RainIntensity::Dry};
    RainIntensity previous_intensity{RainIntensity::Dry};
    int temperature_millicelsius{18000};
    unsigned cloud_permille{};
    unsigned precipitation_permille{};
    unsigned wind_mm_per_second{};
    std::uint64_t sequence{};
    unsigned age_minutes{};
    auto operator<=>(const WeatherState&) const = default;
};

[[nodiscard]] bool valid_climate_profile(const ClimateProfile& climate) noexcept;
[[nodiscard]] bool valid_weather_state(const WeatherState& state) noexcept;
[[nodiscard]] WeatherState initial_weather_state(const ClimateProfile& climate) noexcept;

Result<WeatherState> evolve_weather(
    const WeatherState& previous,
    const ClimateProfile& climate,
    Season season,
    std::uint64_t entropy);

class ClimateCatalog final {
public:
    Result<void> set(ClimateProfile profile);
    Result<void> remove(ZoneId zone);
    [[nodiscard]] const ClimateProfile* find(ZoneId zone) const noexcept;
    [[nodiscard]] std::vector<ClimateProfile> snapshot() const;
private:
    std::vector<ClimateProfile> profiles_{};
};

class WeatherLedger final {
public:
    Result<void> set(WeatherState state);
    Result<void> remove(ZoneId zone);
    [[nodiscard]] const WeatherState* find(ZoneId zone) const noexcept;
    [[nodiscard]] std::vector<WeatherState> snapshot() const;
private:
    std::vector<WeatherState> states_{};
};

} // namespace home
