#include "home/versioned_world.hpp"

#include <cassert>
#include <variant>

int main() {
    using namespace home;

    WorldClock clock{};
    const auto minute = clock.advance_real_milliseconds(30000);
    assert(minute.ok());
    assert(minute.value().milliseconds == 60000);

    WorldClock odd{WorldClockConfig{7000}};
    for (int i = 0; i < 7; ++i) {
        const auto step = odd.advance_real_milliseconds(1000);
        assert(step.ok());
    }
    assert(odd.now().milliseconds == 60000);
    assert(odd.remainder() == 0);

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

    const auto legacy = decode_snapshot("HOME_SNAPSHOT 1\nWORLD 2 0\nENTITIES 0\nZONES 0\nCONNECTIONS 0\nPLACEMENTS 0\nEND\n");
    assert(legacy.ok());
    assert(legacy.value().clock_config.real_milliseconds_per_home_minute == 30000);
    assert(legacy.value().world_time.milliseconds == 0);

    const auto zero = world.advance_time(0);
    assert(!zero.ok());
    assert(world.revision() == WorldRevision{1});

    return 0;
}
