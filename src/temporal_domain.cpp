#include "home/temporal_domain.hpp"

#include <limits>
#include <utility>

namespace home {
namespace {
Result<std::uint64_t> scale_delta(std::uint64_t delta, std::uint64_t num, std::uint64_t den) {
    if (num == 0 || den == 0) return Result<std::uint64_t>::failure(ErrorCode::ValidationFailed, "temporal rate must be non-zero");
    const std::uint64_t whole = delta / den;
    const std::uint64_t rem = delta % den;
    if (whole > std::numeric_limits<std::uint64_t>::max() / num) return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection overflow");
    const std::uint64_t whole_scaled = whole * num;
    if (rem > std::numeric_limits<std::uint64_t>::max() / num) return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection remainder overflow");
    const std::uint64_t rem_scaled = (rem * num) / den;
    if (whole_scaled > std::numeric_limits<std::uint64_t>::max() - rem_scaled) return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection overflow");
    return Result<std::uint64_t>::success(whole_scaled + rem_scaled);
}
} // namespace

Result<TemporalDomain> make_temporal_domain(TimeDomainId id, std::string key, WorldTime base_world_time,
    WorldTime base_domain_time, std::uint64_t rate_numerator, std::uint64_t rate_denominator, bool paused) {
    if (!id.valid() || key.empty()) return Result<TemporalDomain>::failure(ErrorCode::InvalidArgument, "temporal domain requires valid id and key");
    if (rate_numerator == 0 || rate_denominator == 0) return Result<TemporalDomain>::failure(ErrorCode::ValidationFailed, "temporal domain rate must be non-zero");
    return Result<TemporalDomain>::success(TemporalDomain{id, std::move(key), base_world_time, base_domain_time, rate_numerator, rate_denominator, paused});
}

Result<WorldTime> project_domain_time(const TemporalDomain& domain, WorldTime world_time) {
    if (!domain.id.valid() || domain.key.empty() || domain.rate_numerator == 0 || domain.rate_denominator == 0) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "temporal domain is invalid");
    }
    if (domain.paused) return Result<WorldTime>::success(domain.base_domain_time);
    if (world_time < domain.base_world_time) return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "world time predates temporal domain base");
    const auto scaled = scale_delta(world_time.milliseconds - domain.base_world_time.milliseconds, domain.rate_numerator, domain.rate_denominator);
    if (!scaled) return Result<WorldTime>::failure(scaled.error().code, scaled.error().message);
    if (scaled.value() > std::numeric_limits<std::uint64_t>::max() - domain.base_domain_time.milliseconds) return Result<WorldTime>::failure(ErrorCode::Overflow, "temporal domain time overflow");
    return Result<WorldTime>::success(WorldTime{domain.base_domain_time.milliseconds + scaled.value()});
}

Result<TemporalDomain> rebase_temporal_domain(const TemporalDomain& domain, WorldTime world_time, bool paused) {
    const auto projected = project_domain_time(domain, world_time);
    if (!projected) return Result<TemporalDomain>::failure(projected.error().code, projected.error().message);
    TemporalDomain copy = domain;
    copy.base_world_time = world_time;
    copy.base_domain_time = projected.value();
    copy.paused = paused;
    return Result<TemporalDomain>::success(std::move(copy));
}

} // namespace home
