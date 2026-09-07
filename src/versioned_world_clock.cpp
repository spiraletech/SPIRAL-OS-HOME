#include "home/versioned_world.hpp"

namespace home {

VersionedWorld::VersionedWorld(WorldId world, WorldClockConfig clock_config) noexcept
    : registry_(world), topology_(world), clock_(clock_config) {}

Result<WorldTime> VersionedWorld::advance_time(std::uint64_t real_milliseconds) {
    if (real_milliseconds == 0) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "time advancement must be non-zero");
    }

    WorldClock staged = clock_;
    const WorldTime before = staged.now();
    const auto advanced = staged.advance_real_milliseconds(real_milliseconds);
    if (!advanced) {
        return Result<WorldTime>::failure(advanced.error().code, advanced.error().message);
    }
    if (advanced.value() == before) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "time advancement produced no canonical change");
    }

    const auto committed = commit({WorldChange{
        WorldChangeKind::WorldTimeAdvanced,
        WorldTimeAdvanced{before, advanced.value(), real_milliseconds}
    }});
    if (!committed) {
        return Result<WorldTime>::failure(committed.error().code, committed.error().message);
    }

    clock_ = staged;
    return Result<WorldTime>::success(clock_.now());
}

} // namespace home
