#include "home/versioned_world.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <variant>

int main() {
    using namespace home;

    // Canonical HOME law: 30 real seconds == 1 HOME minute.
    WorldClock clock{};
    const auto minute = clock.advance_real_milliseconds(30000);
    assert(minute.ok());
    assert(minute.value().milliseconds == 60000);
    assert(clock.remainder() == 0);

    // Integer accumulation must be chunk-invariant and drift-free.
    WorldClock odd{WorldClockConfig{7000}};
    for (int i = 0; i < 7; ++i) {
        const auto step = odd.advance_real_milliseconds(1000);
        assert(step.ok());
    }
    assert(odd.now().milliseconds == 60000);
    assert(odd.remainder() == 0);

    WorldClock chunked{WorldClockConfig{100000}};
    for (int i = 0; i < 5; ++i) {
        const auto step = chunked.advance_real_milliseconds(1);
        assert(step.ok());
    }
    WorldClock one_shot{WorldClockConfig{100000}};
    const auto five_ms = one_shot.advance_real_milliseconds(5);
    assert(five_ms.ok());
    assert(chunked.now() == one_shot.now());
    assert(chunked.remainder() == one_shot.remainder());
    assert(chunked.now().milliseconds == 3);
    assert(chunked.remainder() == 0);

    // Large safe advancements stay exact.
    WorldClock large{};
    const auto large_step = large.advance_real_milliseconds(1000000000000ULL);
    assert(large_step.ok());
    assert(large_step.value().milliseconds == 2000000000000ULL);
    assert(large.remainder() == 0);

    // Overflow is atomic: neither whole time nor fractional state may change.
    WorldClock near_limit{};
    const auto restored_near_limit = near_limit.restore(
        WorldTime{std::numeric_limits<std::uint64_t>::max() - 1}, 0);
    assert(restored_near_limit.ok());
    const auto overflow = near_limit.advance_real_milliseconds(1);
    assert(!overflow.ok());
    assert(overflow.error().code == ErrorCode::Overflow);
    assert(near_limit.now().milliseconds == std::numeric_limits<std::uint64_t>::max() - 1);
    assert(near_limit.remainder() == 0);

    VersionedWorld world{WorldId{8}};
    assert(world.clock().now().milliseconds == 0);
    const auto advanced = world.advance_time(30000);
    assert(advanced.ok());
    assert(advanced.value().milliseconds == 60000);
    assert(world.revision() == WorldRevision{1});
    assert(world.history().size() == 1);
    assert(world.history().back().changes().front().kind == WorldChangeKind::WorldTimeAdvanced);
    const auto& payload = std::get<WorldTimeAdvanced>(world.history().back().changes().front().payload);
    assert(payload.before.milliseconds == 0);
    assert(payload.after.milliseconds == 60000);
    assert(payload.real_milliseconds == 30000);
    assert(payload.before_remainder == 0);
    assert(payload.after_remainder == 0);

    const WorldSnapshot saved = world.snapshot();
    assert(saved.world_time.milliseconds == 60000);
    assert(saved.clock_config.real_milliseconds_per_home_minute == 30000);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    const auto restored = VersionedWorld::from_snapshot(decoded.value());
    assert(restored.ok());
    assert(restored.value().clock().now().milliseconds == 60000);
    assert(restored.value().revision() == WorldRevision{1});

    // A configured slow clock may advance only its fractional accumulator.
    // That fractional state is still canonical and must commit + persist.
    VersionedWorld slow_world{WorldId{9}, WorldClockConfig{100000}};
    const auto fractional = slow_world.advance_time(1);
    assert(fractional.ok());
    assert(fractional.value().milliseconds == 0);
    assert(slow_world.clock().remainder() == 60000);
    assert(slow_world.revision() == WorldRevision{1});
    assert(slow_world.history().size() == 1);
    const auto& fractional_payload =
        std::get<WorldTimeAdvanced>(slow_world.history().back().changes().front().payload);
    assert(fractional_payload.before.milliseconds == 0);
    assert(fractional_payload.after.milliseconds == 0);
    assert(fractional_payload.before_remainder == 0);
    assert(fractional_payload.after_remainder == 60000);

    const auto slow_encoded = encode_snapshot(slow_world.snapshot());
    assert(slow_encoded.ok());
    const auto slow_decoded = decode_snapshot(slow_encoded.value());
    assert(slow_decoded.ok());
    const auto slow_restored_result = VersionedWorld::from_snapshot(slow_decoded.value());
    assert(slow_restored_result.ok());
    VersionedWorld slow_restored = slow_restored_result.value();
    assert(slow_restored.clock().now().milliseconds == 0);
    assert(slow_restored.clock().remainder() == 60000);
    assert(slow_restored.revision() == WorldRevision{1});

    const auto fractional_second = slow_restored.advance_time(1);
    assert(fractional_second.ok());
    assert(fractional_second.value().milliseconds == 1);
    assert(slow_restored.clock().remainder() == 20000);
    assert(slow_restored.revision() == WorldRevision{2});

    const auto legacy = decode_snapshot("HOME_SNAPSHOT 1\nWORLD 2 0\nENTITIES 0\nZONES 0\nCONNECTIONS 0\nPLACEMENTS 0\nEND\n");
    assert(legacy.ok());
    assert(legacy.value().clock_config.real_milliseconds_per_home_minute == 30000);
    assert(legacy.value().world_time.milliseconds == 0);

    const WorldRevision before_zero_revision = world.revision();
    const WorldTime before_zero_time = world.clock().now();
    const std::uint64_t before_zero_remainder = world.clock().remainder();
    const auto zero = world.advance_time(0);
    assert(!zero.ok());
    assert(world.revision() == before_zero_revision);
    assert(world.clock().now() == before_zero_time);
    assert(world.clock().remainder() == before_zero_remainder);

    return 0;
}
