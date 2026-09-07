#include "home/world_clock.hpp"

#include <limits>

namespace home {

Result<WorldTime> WorldClock::advance_real_milliseconds(std::uint64_t real_milliseconds) {
    if (config_.real_milliseconds_per_home_minute == 0) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "clock ratio denominator must be non-zero");
    }

    constexpr std::uint64_t kHomeMillisecondsPerMinute = 60000;
    const auto denom = config_.real_milliseconds_per_home_minute;

    if (real_milliseconds > (std::numeric_limits<std::uint64_t>::max() - remainder_) / kHomeMillisecondsPerMinute) {
        return Result<WorldTime>::failure(ErrorCode::Overflow, "world clock accumulator overflow");
    }

    const std::uint64_t numerator = real_milliseconds * kHomeMillisecondsPerMinute + remainder_;
    const std::uint64_t delta = numerator / denom;
    const std::uint64_t next_remainder = numerator % denom;

    if (delta > std::numeric_limits<std::uint64_t>::max() - home_milliseconds_) {
        return Result<WorldTime>::failure(ErrorCode::Overflow, "world clock time overflow");
    }

    home_milliseconds_ += delta;
    remainder_ = next_remainder;
    return Result<WorldTime>::success(now());
}

Result<void> WorldClock::restore(WorldTime time, std::uint64_t remainder) {
    if (config_.real_milliseconds_per_home_minute == 0 || remainder >= config_.real_milliseconds_per_home_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "invalid restored clock state");
    }
    home_milliseconds_ = time.milliseconds;
    remainder_ = remainder;
    return Result<void>::success();
}

} // namespace home
