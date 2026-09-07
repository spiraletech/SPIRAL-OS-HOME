#include "home/temporal_domain.hpp"

#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_layer(TemporalLayer layer) noexcept {
    return layer == TemporalLayer::Event || layer == TemporalLayer::Scene;
}

bool valid_domain(const TemporalDomain& domain) noexcept {
    return domain.id.valid() && !domain.key.empty() && domain.rate_numerator != 0 &&
           domain.rate_denominator != 0 && valid_layer(domain.layer);
}

Result<std::uint64_t> scale_delta(std::uint64_t delta, std::uint64_t num, std::uint64_t den) {
    if (num == 0 || den == 0) {
        return Result<std::uint64_t>::failure(ErrorCode::ValidationFailed, "temporal rate must be non-zero");
    }
    const std::uint64_t whole = delta / den;
    const std::uint64_t rem = delta % den;
    if (whole > std::numeric_limits<std::uint64_t>::max() / num) {
        return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection overflow");
    }
    const std::uint64_t whole_scaled = whole * num;
    if (rem > std::numeric_limits<std::uint64_t>::max() / num) {
        return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection remainder overflow");
    }
    const std::uint64_t rem_scaled = (rem * num) / den;
    if (whole_scaled > std::numeric_limits<std::uint64_t>::max() - rem_scaled) {
        return Result<std::uint64_t>::failure(ErrorCode::Overflow, "temporal projection overflow");
    }
    return Result<std::uint64_t>::success(whole_scaled + rem_scaled);
}

bool outranks(const TemporalDomain& candidate, const TemporalDomain& current) noexcept {
    const auto candidate_layer = static_cast<std::uint8_t>(candidate.layer);
    const auto current_layer = static_cast<std::uint8_t>(current.layer);
    if (candidate_layer != current_layer) return candidate_layer > current_layer;
    if (candidate.priority != current.priority) return candidate.priority > current.priority;
    return candidate.id.value() < current.id.value();
}

} // namespace

Result<TemporalDomain> make_temporal_domain(TimeDomainId id, std::string key, WorldTime base_world_time,
    WorldTime base_domain_time, std::uint64_t rate_numerator, std::uint64_t rate_denominator, bool paused,
    TemporalLayer layer, std::int32_t priority) {
    TemporalDomain domain{id, std::move(key), base_world_time, base_domain_time, rate_numerator,
                          rate_denominator, paused, layer, priority};
    if (!domain.id.valid() || domain.key.empty()) {
        return Result<TemporalDomain>::failure(ErrorCode::InvalidArgument, "temporal domain requires valid id and key");
    }
    if (domain.rate_numerator == 0 || domain.rate_denominator == 0 || !valid_layer(domain.layer)) {
        return Result<TemporalDomain>::failure(ErrorCode::ValidationFailed, "temporal domain configuration is invalid");
    }
    return Result<TemporalDomain>::success(std::move(domain));
}

Result<WorldTime> project_domain_time(const TemporalDomain& domain, WorldTime world_time) {
    if (!valid_domain(domain)) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "temporal domain is invalid");
    }
    if (world_time < domain.base_world_time) {
        return Result<WorldTime>::failure(ErrorCode::ValidationFailed, "world time predates temporal domain base");
    }
    if (domain.paused) return Result<WorldTime>::success(domain.base_domain_time);

    const auto scaled = scale_delta(
        world_time.milliseconds - domain.base_world_time.milliseconds,
        domain.rate_numerator,
        domain.rate_denominator);
    if (!scaled) return Result<WorldTime>::failure(scaled.error().code, scaled.error().message);
    if (scaled.value() > std::numeric_limits<std::uint64_t>::max() - domain.base_domain_time.milliseconds) {
        return Result<WorldTime>::failure(ErrorCode::Overflow, "temporal domain time overflow");
    }
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

Result<void> TemporalDomainRegistry::add(TemporalDomain domain) {
    if (!valid_domain(domain)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "cannot register invalid temporal domain");
    }
    if (find(domain.id) != nullptr || find_by_key(domain.key) != nullptr) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "temporal domain id or key already exists");
    }
    domains_.push_back(std::move(domain));
    return Result<void>::success();
}

Result<void> TemporalDomainRegistry::remove(TimeDomainId id) {
    if (!id.valid()) return Result<void>::failure(ErrorCode::InvalidArgument, "temporal domain id must be valid");
    for (auto it = domains_.begin(); it != domains_.end(); ++it) {
        if (it->id == id) {
            domains_.erase(it);
            return Result<void>::success();
        }
    }
    return Result<void>::failure(ErrorCode::NotFound, "temporal domain not found");
}

const TemporalDomain* TemporalDomainRegistry::find(TimeDomainId id) const noexcept {
    for (const auto& domain : domains_) if (domain.id == id) return &domain;
    return nullptr;
}

const TemporalDomain* TemporalDomainRegistry::find_by_key(const std::string& key) const noexcept {
    for (const auto& domain : domains_) if (domain.key == key) return &domain;
    return nullptr;
}

Result<WorldTime> TemporalDomainRegistry::project(TimeDomainId id, WorldTime canonical_time) const {
    const TemporalDomain* domain = find(id);
    if (!domain) return Result<WorldTime>::failure(ErrorCode::NotFound, "temporal domain not found");
    return project_domain_time(*domain, canonical_time);
}

Result<ResolvedTemporalTime> TemporalDomainRegistry::resolve(WorldTime canonical_time) const {
    const TemporalDomain* selected = nullptr;
    for (const auto& domain : domains_) {
        if (canonical_time < domain.base_world_time) continue;
        if (!selected || outranks(domain, *selected)) selected = &domain;
    }

    if (!selected) {
        return Result<ResolvedTemporalTime>::success(
            ResolvedTemporalTime{canonical_time, canonical_time, std::nullopt});
    }

    const auto projected = project_domain_time(*selected, canonical_time);
    if (!projected) {
        return Result<ResolvedTemporalTime>::failure(projected.error().code, projected.error().message);
    }
    return Result<ResolvedTemporalTime>::success(
        ResolvedTemporalTime{canonical_time, projected.value(), selected->id});
}

} // namespace home
