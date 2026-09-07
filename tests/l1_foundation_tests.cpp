#include "home/ids.hpp"
#include "home/result.hpp"
#include "home/revision.hpp"
#include "home/world_types.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

int main() {
    using namespace home;

    static_assert(!std::is_convertible_v<std::uint64_t, EntityId>);
    static_assert(!std::is_same_v<EntityId, ZoneId>);

    const EntityId invalid{};
    const EntityId entity{42};
    assert(!invalid.valid());
    assert(entity.valid());
    assert(entity.value() == 42);

    const WorldRevision initial{};
    assert(initial.is_initial());
    const auto next = initial.next();
    assert(next.has_value());
    assert(next->value() == 1);

    const WorldRevision maximum{std::numeric_limits<std::uint64_t>::max()};
    assert(!maximum.next().has_value());

    const Transform transform{
        Vec3Mm{1250, -400, 3000},
        EulerMilliDegrees{0, 90000, 0}
    };
    assert(transform.position.x == 1250);
    assert(transform.rotation.yaw == 90000);

    const auto success = Result<int>::success(7);
    assert(success.ok());
    assert(success.value() == 7);

    const auto failure = Result<int>::failure(ErrorCode::ValidationFailed, "bad state");
    assert(!failure.ok());
    assert(failure.error().code == ErrorCode::ValidationFailed);

    const auto void_success = Result<void>::success();
    assert(void_success.ok());

    assert(std::string_view{protocol_name()} == "SPIRAL-OS-HOME");
    assert(kHomeProtocolMajor == 1);
    assert(kHomeProtocolMinor == 0);

    return 0;
}
