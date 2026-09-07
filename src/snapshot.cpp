#include "home/snapshot.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace home {
namespace {

template <typename T>
bool read_value(std::istream& in, T& value) {
    return static_cast<bool>(in >> value);
}

Result<WorldSnapshot> parse_error(const char* message) {
    return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, message);
}

} // namespace

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.world.valid()) {
        return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot requires a valid world id");
    }

    std::ostringstream out;
    out << "HOME_SNAPSHOT " << kSnapshotFormatVersion << '\n';
    out << "WORLD " << snapshot.world.value() << ' ' << snapshot.revision.value() << '\n';
    out << "ENTITIES " << snapshot.entities.size() << '\n';
    for (const auto& e : snapshot.entities) {
        out << "E " << e.id.value() << ' ' << static_cast<unsigned>(e.kind) << ' '
            << (e.persistent ? 1 : 0) << ' ' << std::quoted(e.archetype) << ' '
            << std::quoted(e.display_name) << ' '
            << e.transform.position.x << ' ' << e.transform.position.y << ' ' << e.transform.position.z << ' '
            << e.transform.rotation.pitch << ' ' << e.transform.rotation.yaw << ' ' << e.transform.rotation.roll << '\n';
    }

    out << "ZONES " << snapshot.zones.size() << '\n';
    for (const auto& z : snapshot.zones) {
        out << "Z " << z.id.value() << ' ' << static_cast<unsigned>(z.kind) << ' '
            << (z.persistent ? 1 : 0) << ' ' << (z.parent ? z.parent->value() : 0) << ' '
            << std::quoted(z.key) << ' ' << std::quoted(z.display_name) << '\n';
    }

    out << "CONNECTIONS " << snapshot.connections.size() << '\n';
    for (const auto& c : snapshot.connections) {
        out << "C " << c.from.value() << ' ' << c.to.value() << ' '
            << (c.bidirectional ? 1 : 0) << ' ' << (c.traversable ? 1 : 0) << ' '
            << std::quoted(c.tag) << '\n';
    }

    out << "PLACEMENTS " << snapshot.placements.size() << '\n';
    for (const auto& p : snapshot.placements) {
        out << "P " << p.entity.value() << ' ' << p.zone.value() << '\n';
    }
    out << "END\n";
    return Result<std::string>::success(out.str());
}

Result<WorldSnapshot> decode_snapshot(std::string_view encoded) {
    std::istringstream in{std::string(encoded)};
    std::string marker;
    unsigned format = 0;
    if (!(in >> marker >> format) || marker != "HOME_SNAPSHOT" || format != kSnapshotFormatVersion) {
        return parse_error("unsupported or malformed snapshot header");
    }

    WorldSnapshot snapshot{};
    std::uint64_t world_value = 0;
    std::uint64_t revision_value = 0;
    if (!(in >> marker >> world_value >> revision_value) || marker != "WORLD" || world_value == 0) {
        return parse_error("malformed snapshot world header");
    }
    snapshot.world = WorldId{world_value};
    snapshot.revision = WorldRevision{revision_value};

    std::size_t count = 0;
    if (!(in >> marker >> count) || marker != "ENTITIES") return parse_error("malformed entity section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0; unsigned kind = 0; int persistent = 0;
        EntityRecord e{};
        if (!(in >> marker >> id >> kind >> persistent >> std::quoted(e.archetype) >> std::quoted(e.display_name)
              >> e.transform.position.x >> e.transform.position.y >> e.transform.position.z
              >> e.transform.rotation.pitch >> e.transform.rotation.yaw >> e.transform.rotation.roll) || marker != "E" || id == 0) {
            return parse_error("malformed entity record");
        }
        if (kind > static_cast<unsigned>(EntityKind::Environment)) return parse_error("unknown entity kind");
        e.id = EntityId{id}; e.world = snapshot.world; e.kind = static_cast<EntityKind>(kind); e.persistent = persistent != 0;
        snapshot.entities.push_back(std::move(e));
    }

    if (!(in >> marker >> count) || marker != "ZONES") return parse_error("malformed zone section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0, parent = 0; unsigned kind = 0; int persistent = 0;
        ZoneRecord z{};
        if (!(in >> marker >> id >> kind >> persistent >> parent >> std::quoted(z.key) >> std::quoted(z.display_name)) || marker != "Z" || id == 0) {
            return parse_error("malformed zone record");
        }
        if (kind > static_cast<unsigned>(ZoneKind::Restricted)) return parse_error("unknown zone kind");
        z.id = ZoneId{id}; z.world = snapshot.world; z.kind = static_cast<ZoneKind>(kind); z.persistent = persistent != 0;
        if (parent != 0) z.parent = ZoneId{parent};
        snapshot.zones.push_back(std::move(z));
    }

    if (!(in >> marker >> count) || marker != "CONNECTIONS") return parse_error("malformed connection section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t from = 0, to = 0; int bidirectional = 0, traversable = 0; std::string tag;
        if (!(in >> marker >> from >> to >> bidirectional >> traversable >> std::quoted(tag)) || marker != "C" || from == 0 || to == 0) {
            return parse_error("malformed connection record");
        }
        snapshot.connections.push_back(ZoneConnection{ZoneId{from}, ZoneId{to}, bidirectional != 0, traversable != 0, std::move(tag)});
    }

    if (!(in >> marker >> count) || marker != "PLACEMENTS") return parse_error("malformed placement section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t entity = 0, zone = 0;
        if (!(in >> marker >> entity >> zone) || marker != "P" || entity == 0 || zone == 0) return parse_error("malformed placement record");
        snapshot.placements.push_back(SnapshotPlacement{EntityId{entity}, ZoneId{zone}});
    }

    if (!(in >> marker) || marker != "END") return parse_error("snapshot missing END marker");
    return Result<WorldSnapshot>::success(std::move(snapshot));
}

Result<void> save_snapshot_file(const WorldSnapshot& snapshot, const std::string& path) {
    if (path.empty()) return Result<void>::failure(ErrorCode::InvalidArgument, "snapshot path must not be empty");
    const auto encoded = encode_snapshot(snapshot);
    if (!encoded) return Result<void>::failure(encoded.error().code, encoded.error().message);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return Result<void>::failure(ErrorCode::SerializationError, "could not open snapshot file for writing");
    out.write(encoded.value().data(), static_cast<std::streamsize>(encoded.value().size()));
    if (!out) return Result<void>::failure(ErrorCode::SerializationError, "snapshot write failed");
    return Result<void>::success();
}

Result<WorldSnapshot> load_snapshot_file(const std::string& path) {
    if (path.empty()) return Result<WorldSnapshot>::failure(ErrorCode::InvalidArgument, "snapshot path must not be empty");
    std::ifstream in(path, std::ios::binary);
    if (!in) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, "could not open snapshot file for reading");
    std::ostringstream buffer; buffer << in.rdbuf();
    if (!in.good() && !in.eof()) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, "snapshot read failed");
    return decode_snapshot(buffer.str());
}

} // namespace home
