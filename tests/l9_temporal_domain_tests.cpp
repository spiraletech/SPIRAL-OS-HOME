#include "home/versioned_world.hpp"

#include <cassert>
#include <limits>

int main() {
    using namespace home;

    const auto slow = make_temporal_domain(
        TimeDomainId{1}, "dream", WorldTime{1000}, WorldTime{5000}, 1, 2, false,
        TemporalLayer::Event, 0);
    assert(slow.ok());
    const auto t = project_domain_time(slow.value(), WorldTime{5000});
    assert(t.ok());
    assert(t.value().milliseconds == 7000);

    const auto fast = make_temporal_domain(
        TimeDomainId{2}, "scene", WorldTime{0}, WorldTime{0}, 3, 2, false,
        TemporalLayer::Scene, 0);
    assert(fast.ok());
    const auto f = project_domain_time(fast.value(), WorldTime{4000});
    assert(f.ok());
    assert(f.value().milliseconds == 6000);

    const auto paused = rebase_temporal_domain(fast.value(), WorldTime{4000}, true);
    assert(paused.ok());
    const auto frozen = project_domain_time(paused.value(), WorldTime{999999});
    assert(frozen.ok());
    assert(frozen.value().milliseconds == 6000);

    const auto resumed = rebase_temporal_domain(paused.value(), WorldTime{10000}, false);
    assert(resumed.ok());
    const auto resumed_time = project_domain_time(resumed.value(), WorldTime{12000});
    assert(resumed_time.ok());
    assert(resumed_time.value().milliseconds == 9000);

    TemporalDomainRegistry registry{};
    const auto event = make_temporal_domain(
        TimeDomainId{10}, "festival", WorldTime{0}, WorldTime{100000}, 1, 1, false,
        TemporalLayer::Event, 100);
    const auto scene = make_temporal_domain(
        TimeDomainId{11}, "cutscene", WorldTime{0}, WorldTime{200000}, 1, 1, false,
        TemporalLayer::Scene, -100);
    assert(event.ok() && scene.ok());
    assert(registry.add(event.value()).ok());
    assert(registry.add(scene.value()).ok());

    // Scene overlays always outrank Event overlays, regardless of event priority.
    const auto resolved_scene = registry.resolve(WorldTime{5000});
    assert(resolved_scene.ok());
    assert(resolved_scene.value().canonical_time.milliseconds == 5000);
    assert(resolved_scene.value().effective_time.milliseconds == 205000);
    assert(resolved_scene.value().overlay == TimeDomainId{11});

    // Same-layer precedence is deterministic: priority first, then lower id.
    TemporalDomainRegistry same_layer{};
    const auto low_priority = make_temporal_domain(
        TimeDomainId{20}, "event_low", WorldTime{0}, WorldTime{1000}, 1, 1, false,
        TemporalLayer::Event, 1);
    const auto high_priority = make_temporal_domain(
        TimeDomainId{21}, "event_high", WorldTime{0}, WorldTime{2000}, 1, 1, false,
        TemporalLayer::Event, 5);
    const auto high_priority_lower_id = make_temporal_domain(
        TimeDomainId{19}, "event_high_lower_id", WorldTime{0}, WorldTime{3000}, 1, 1, false,
        TemporalLayer::Event, 5);
    assert(low_priority.ok() && high_priority.ok() && high_priority_lower_id.ok());
    assert(same_layer.add(low_priority.value()).ok());
    assert(same_layer.add(high_priority.value()).ok());
    assert(same_layer.add(high_priority_lower_id.value()).ok());
    const auto same_resolved = same_layer.resolve(WorldTime{100});
    assert(same_resolved.ok());
    assert(same_resolved.value().overlay == TimeDomainId{19});
    assert(same_resolved.value().effective_time.milliseconds == 3100);

    // Domains do not exist before their canonical start point.
    TemporalDomainRegistry future_registry{};
    const auto future = make_temporal_domain(
        TimeDomainId{30}, "future_scene", WorldTime{10000}, WorldTime{900000}, 1, 1, false,
        TemporalLayer::Scene, 0);
    assert(future.ok());
    assert(future_registry.add(future.value()).ok());
    const auto before_start = future_registry.resolve(WorldTime{9999});
    assert(before_start.ok());
    assert(!before_start.value().overlay.has_value());
    assert(before_start.value().effective_time.milliseconds == 9999);
    const auto at_start = future_registry.resolve(WorldTime{10000});
    assert(at_start.ok());
    assert(at_start.value().overlay == TimeDomainId{30});
    assert(at_start.value().effective_time.milliseconds == 900000);

    // Duplicate ids and duplicate keys are rejected.
    assert(!registry.add(event.value()).ok());
    const auto duplicate_key = make_temporal_domain(
        TimeDomainId{12}, "festival", WorldTime{0}, WorldTime{0}, 1, 1, false,
        TemporalLayer::Event, 0);
    assert(duplicate_key.ok());
    assert(!registry.add(duplicate_key.value()).ok());

    // HOME canonical chronology is independent from contextual overlays.
    VersionedWorld world{WorldId{90}};
    const auto advance = world.advance_time(30000);
    assert(advance.ok());
    assert(world.clock().now().milliseconds == 60000);
    const WorldRevision canonical_revision = world.revision();
    const WorldTime canonical_before_overlay = world.clock().now();

    const auto world_scene = make_temporal_domain(
        TimeDomainId{40}, "dialogue_scene", canonical_before_overlay, WorldTime{43200000}, 1, 1, false,
        TemporalLayer::Scene, 0);
    assert(world_scene.ok());
    assert(world.add_temporal_domain(world_scene.value()).ok());
    assert(world.revision() == canonical_revision);
    assert(world.clock().now() == canonical_before_overlay);

    const auto world_resolved = world.resolve_temporal_time();
    assert(world_resolved.ok());
    assert(world_resolved.value().canonical_time == canonical_before_overlay);
    assert(world_resolved.value().effective_time.milliseconds == 43200000);
    assert(world.clock().now() == canonical_before_overlay);
    assert(world.revision() == canonical_revision);

    const auto world_advance = world.advance_time(15000);
    assert(world_advance.ok());
    assert(world.clock().now().milliseconds == 90000);
    const auto world_resolved_later = world.resolve_temporal_time();
    assert(world_resolved_later.ok());
    assert(world_resolved_later.value().canonical_time.milliseconds == 90000);
    assert(world_resolved_later.value().effective_time.milliseconds == 43230000);

    const WorldRevision before_remove = world.revision();
    const WorldTime before_remove_time = world.clock().now();
    assert(world.remove_temporal_domain(TimeDomainId{40}).ok());
    assert(world.revision() == before_remove);
    assert(world.clock().now() == before_remove_time);
    const auto canonical_only = world.resolve_temporal_time();
    assert(canonical_only.ok());
    assert(!canonical_only.value().overlay.has_value());
    assert(canonical_only.value().effective_time == world.clock().now());

    // Projection overflow fails without mutating the domain.
    const auto near_limit = make_temporal_domain(
        TimeDomainId{50}, "overflow", WorldTime{0},
        WorldTime{std::numeric_limits<std::uint64_t>::max() - 1}, 2, 1, false,
        TemporalLayer::Event, 0);
    assert(near_limit.ok());
    const auto overflow = project_domain_time(near_limit.value(), WorldTime{1});
    assert(!overflow.ok());
    assert(overflow.error().code == ErrorCode::Overflow);

    const auto invalid = make_temporal_domain(
        TimeDomainId{}, "", WorldTime{}, WorldTime{}, 1, 1, false,
        TemporalLayer::Event, 0);
    assert(!invalid.ok());

    return 0;
}
