#pragma once

#include "home/ids.hpp"
#include "home/revision.hpp"

#include <compare>
#include <cstdint>

namespace home {

// HOME stores canonical spatial quantities as integer millimetres.
// Render/physics adapters may project these into their own floating-point spaces.
struct Vec3Mm final {
    std::int64_t x{0};
    std::int64_t y{0};
    std::int64_t z{0};
    auto operator<=>(const Vec3Mm&) const = default;
};

// Orientation is stored in integer millidegrees to keep snapshots deterministic.
struct EulerMilliDegrees final {
    std::int32_t pitch{0};
    std::int32_t yaw{0};
    std::int32_t roll{0};
    auto operator<=>(const EulerMilliDegrees&) const = default;
};

struct Transform final {
    Vec3Mm position{};
    EulerMilliDegrees rotation{};
    auto operator<=>(const Transform&) const = default;
};

struct WorldStamp final {
    WorldId world{};
    WorldRevision revision{};
    auto operator<=>(const WorldStamp&) const = default;
};

inline constexpr std::uint32_t kHomeProtocolMajor = 1;
inline constexpr std::uint32_t kHomeProtocolMinor = 0;

struct ProtocolVersion final {
    std::uint32_t major{kHomeProtocolMajor};
    std::uint32_t minor{kHomeProtocolMinor};
    auto operator<=>(const ProtocolVersion&) const = default;
};

[[nodiscard]] const char* protocol_name() noexcept;

} // namespace home
