#include "home/versioned_world.hpp"

namespace home {

VersionedWorld::VersionedWorld(WorldId world, WorldClockConfig clock_config) noexcept
    : VersionedWorld(world, clock_config, CalendarConfig{}) {}

VersionedWorld::VersionedWorld(WorldId world, WorldClockConfig clock_config, CalendarConfig calendar_config) noexcept
    : registry_(world),
      topology_(world),
      clock_(clock_config),
      calendar_config_(valid_calendar_date(calendar_config.epoch) ? calendar_config : CalendarConfig{}) {}

Result<WorldTime> VersionedWorld::advance_time(std::uint64_t real_milliseconds) {
    if (real_milliseconds == 0) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "time advancement must be non-zero");
    }

    WorldClock staged = clock_;
    const WorldTime before = staged.now();
    const std::uint64_t before_remainder = staged.remainder();
    const auto advanced = staged.advance_real_milliseconds(real_milliseconds);
    if (!advanced) {
        return Result<WorldTime>::failure(advanced.error().code, advanced.error().message);
    }

    const WorldTime after = advanced.value();
    const std::uint64_t after_remainder = staged.remainder();
    if (after == before && after_remainder == before_remainder) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "time advancement produced no canonical clock change");
    }

    const auto committed = commit({WorldChange{
        WorldChangeKind::WorldTimeAdvanced,
        WorldTimeAdvanced{before, after, real_milliseconds, before_remainder, after_remainder}
    }});
    if (!committed) {
        return Result<WorldTime>::failure(committed.error().code, committed.error().message);
    }

    clock_ = staged;
    return Result<WorldTime>::success(clock_.now());
}

} // namespace home
