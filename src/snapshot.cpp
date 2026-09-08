#include "home/snapshot.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace home {
namespace {
Result<WorldSnapshot> parse_error(const char* message) {
    return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, message);
}

bool snapshot_has_zone(const WorldSnapshot& snapshot, ZoneId zone) {
    return std::any_of(snapshot.zones.begin(), snapshot.zones.end(), [&](const ZoneRecord& item) {
        return item.id == zone;
    });
}
} // namespace

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.world.valid()) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot requires a valid world id");
    if (snapshot.clock_config.real_milliseconds_per_home_minute == 0 || snapshot.clock_remainder >= snapshot.clock_config.real_milliseconds_per_home_minute) {
        return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot clock state is invalid");
    }
    if (!valid_calendar_date(snapshot.calendar_config.epoch)) {
        return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot calendar epoch is invalid");
    }

    EventCatalog event_validation;
    for (const auto& event : snapshot.events) {
        const auto added = event_validation.add(event);
        if (!added) return Result<std::string>::failure(added.error().code, added.error().message);
    }

    ClimateCatalog climate_validation;
    for (const auto& climate : snapshot.climates) {
        if (!snapshot_has_zone(snapshot, climate.zone)) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot climate references a missing zone");
        }
        if (climate_validation.find(climate.zone)) {
            return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot climate zone");
        }
        const auto added = climate_validation.set(climate);
        if (!added) return Result<std::string>::failure(added.error().code, added.error().message);
    }

    WeatherLedger weather_validation;
    for (const auto& state : snapshot.weather) {
        if (!snapshot_has_zone(snapshot, state.zone) || !climate_validation.find(state.zone)) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot weather references a missing zone or climate");
        }
        if (weather_validation.find(state.zone)) {
            return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot weather zone");
        }
        const auto added = weather_validation.set(state);
        if (!added) return Result<std::string>::failure(added.error().code, added.error().message);
    }

    std::ostringstream out;
    out << "HOME_SNAPSHOT " << kSnapshotFormatVersion << '\n';
    out << "WORLD " << snapshot.world.value() << ' ' << snapshot.revision.value() << '\n';
    out << "CLOCK " << snapshot.clock_config.real_milliseconds_per_home_minute << ' '
        << snapshot.world_time.milliseconds << ' ' << snapshot.clock_remainder << '\n';
    out << "CALENDAR " << snapshot.calendar_config.epoch.year << ' '
        << snapshot.calendar_config.epoch.month << ' ' << snapshot.calendar_config.epoch.day << '\n';

    out << "EVENTS " << snapshot.events.size() << '\n';
    for (const auto& event : snapshot.events) {
        out << "V " << event.id.value() << ' ' << static_cast<unsigned>(event.kind) << ' '
            << event.priority << ' ' << event.rule.month << ' ' << event.rule.day << ' '
            << event.rule.duration_days << ' ' << event.affinities.size() << ' '
            << std::quoted(event.key) << ' ' << std::quoted(event.display_name);
        for (const auto& affinity : event.affinities) out << ' ' << std::quoted(affinity);
        out << '\n';
    }

    out << "CLIMATES " << snapshot.climates.size() << '\n';
    for (const auto& climate : snapshot.climates) {
        out << "K " << climate.zone.value() << ' ' << climate.mean_temperature_millicelsius << ' '
            << climate.wetness_permille << ' ' << climate.wind_permille << ' ' << climate.seed << '\n';
    }

    out << "WEATHER " << snapshot.weather.size() << '\n';
    for (const auto& state : snapshot.weather) {
        out << "W " << state.zone.value() << ' ' << static_cast<unsigned>(state.intensity) << ' '
            << static_cast<unsigned>(state.previous_intensity) << ' ' << state.temperature_millicelsius << ' '
            << state.cloud_permille << ' ' << state.precipitation_permille << ' ' << state.wind_mm_per_second << ' '
            << state.sequence << ' ' << state.age_minutes << '\n';
    }

    out << "ENTITIES " << snapshot.entities.size() << '\n';
    for (const auto& e : snapshot.entities) {
        out << "E " << e.id.value() << ' ' << static_cast<unsigned>(e.kind) << ' ' << (e.persistent ? 1 : 0) << ' '
            << std::quoted(e.archetype) << ' ' << std::quoted(e.display_name) << ' '
            << e.transform.position.x << ' ' << e.transform.position.y << ' ' << e.transform.position.z << ' '
            << e.transform.rotation.pitch << ' ' << e.transform.rotation.yaw << ' ' << e.transform.rotation.roll << '\n';
    }
    out << "ZONES " << snapshot.zones.size() << '\n';
    for (const auto& z : snapshot.zones) {
        out << "Z " << z.id.value() << ' ' << static_cast<unsigned>(z.kind) << ' ' << (z.persistent ? 1 : 0) << ' '
            << (z.parent ? z.parent->value() : 0) << ' ' << std::quoted(z.key) << ' ' << std::quoted(z.display_name) << '\n';
    }
    out << "CONNECTIONS " << snapshot.connections.size() << '\n';
    for (const auto& c : snapshot.connections) {
        out << "C " << c.from.value() << ' ' << c.to.value() << ' ' << (c.bidirectional ? 1 : 0) << ' '
            << (c.traversable ? 1 : 0) << ' ' << std::quoted(c.tag) << '\n';
    }
    out << "PLACEMENTS " << snapshot.placements.size() << '\n';
    for (const auto& p : snapshot.placements) out << "P " << p.entity.value() << ' ' << p.zone.value() << '\n';
    out << "END\n";
    return Result<std::string>::success(out.str());
}

Result<WorldSnapshot> decode_snapshot(std::string_view encoded) {
    std::istringstream in{std::string(encoded)};
    std::string marker; unsigned format = 0;
    if (!(in >> marker >> format) || marker != "HOME_SNAPSHOT" || format < 1 || format > kSnapshotFormatVersion) {
        return parse_error("unsupported or malformed snapshot header");
    }

    WorldSnapshot snapshot{};
    std::uint64_t world_value = 0, revision_value = 0;
    if (!(in >> marker >> world_value >> revision_value) || marker != "WORLD" || world_value == 0) return parse_error("malformed snapshot world header");
    snapshot.world = WorldId{world_value}; snapshot.revision = WorldRevision{revision_value};

    if (format >= 2) {
        std::uint64_t ratio = 0, time = 0, remainder = 0;
        if (!(in >> marker >> ratio >> time >> remainder) || marker != "CLOCK" || ratio == 0 || remainder >= ratio) return parse_error("malformed snapshot clock state");
        snapshot.clock_config.real_milliseconds_per_home_minute = ratio;
        snapshot.world_time = WorldTime{time}; snapshot.clock_remainder = remainder;
    }

    if (format >= 3) {
        int year = 0; unsigned month = 0, day = 0;
        if (!(in >> marker >> year >> month >> day) || marker != "CALENDAR") return parse_error("malformed snapshot calendar state");
        snapshot.calendar_config.epoch = CalendarDate{year, month, day};
        if (!valid_calendar_date(snapshot.calendar_config.epoch)) return parse_error("invalid snapshot calendar epoch");
    }

    std::size_t count = 0;
    if (format >= 4) {
        if (!(in >> marker >> count) || marker != "EVENTS") return parse_error("malformed event section");
        EventCatalog event_validation;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t id = 0;
            unsigned kind = 0, month = 0, day = 0, duration = 0;
            int priority = 0;
            std::size_t affinity_count = 0;
            EventDefinition event{};
            if (!(in >> marker >> id >> kind >> priority >> month >> day >> duration >> affinity_count
                  >> std::quoted(event.key) >> std::quoted(event.display_name)) || marker != "V" || id == 0) {
                return parse_error("malformed event record");
            }
            if (kind > static_cast<unsigned>(EventKind::Story)) return parse_error("unknown event kind");
            event.id = EventId{id};
            event.kind = static_cast<EventKind>(kind);
            event.priority = priority;
            event.rule = AnnualDateRule{month, day, duration};
            for (std::size_t a = 0; a < affinity_count; ++a) {
                std::string affinity;
                if (!(in >> std::quoted(affinity))) return parse_error("malformed event affinity");
                event.affinities.push_back(std::move(affinity));
            }
            const auto added = event_validation.add(event);
            if (!added) return parse_error("invalid or duplicate event record");
            snapshot.events.push_back(std::move(event));
        }
    }

    if (format >= 5) {
        if (!(in >> marker >> count) || marker != "CLIMATES") return parse_error("malformed climate section");
        ClimateCatalog climate_validation;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, seed = 0;
            int temperature = 0;
            unsigned wetness = 0, wind = 0;
            if (!(in >> marker >> zone >> temperature >> wetness >> wind >> seed) || marker != "K" || zone == 0) {
                return parse_error("malformed climate record");
            }
            ClimateProfile climate{ZoneId{zone}, temperature, wetness, wind, seed};
            if (climate_validation.find(climate.zone) || !climate_validation.set(climate)) {
                return parse_error("invalid or duplicate climate record");
            }
            snapshot.climates.push_back(climate);
        }

        if (!(in >> marker >> count) || marker != "WEATHER") return parse_error("malformed weather section");
        WeatherLedger weather_validation;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, sequence = 0;
            unsigned intensity = 0, previous = 0, cloud = 0, precipitation = 0, wind = 0, age = 0;
            int temperature = 0;
            if (!(in >> marker >> zone >> intensity >> previous >> temperature >> cloud >> precipitation >> wind >> sequence >> age)
                || marker != "W" || zone == 0
                || intensity > static_cast<unsigned>(RainIntensity::Deluge)
                || previous > static_cast<unsigned>(RainIntensity::Deluge)) {
                return parse_error("malformed weather record");
            }
            WeatherState state{ZoneId{zone}, static_cast<RainIntensity>(intensity), static_cast<RainIntensity>(previous),
                temperature, cloud, precipitation, wind, sequence, age};
            if (weather_validation.find(state.zone) || !weather_validation.set(state)) {
                return parse_error("invalid or duplicate weather record");
            }
            snapshot.weather.push_back(state);
        }
    }

    if (!(in >> marker >> count) || marker != "ENTITIES") return parse_error("malformed entity section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0; unsigned kind = 0; int persistent = 0; EntityRecord e{};
        if (!(in >> marker >> id >> kind >> persistent >> std::quoted(e.archetype) >> std::quoted(e.display_name)
              >> e.transform.position.x >> e.transform.position.y >> e.transform.position.z
              >> e.transform.rotation.pitch >> e.transform.rotation.yaw >> e.transform.rotation.roll) || marker != "E" || id == 0) return parse_error("malformed entity record");
        if (kind > static_cast<unsigned>(EntityKind::Environment)) return parse_error("unknown entity kind");
        e.id=EntityId{id}; e.world=snapshot.world; e.kind=static_cast<EntityKind>(kind); e.persistent=persistent!=0; snapshot.entities.push_back(std::move(e));
    }

    if (!(in >> marker >> count) || marker != "ZONES") return parse_error("malformed zone section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id=0,parent=0; unsigned kind=0; int persistent=0; ZoneRecord z{};
        if (!(in >> marker >> id >> kind >> persistent >> parent >> std::quoted(z.key) >> std::quoted(z.display_name)) || marker!="Z" || id==0) return parse_error("malformed zone record");
        if (kind > static_cast<unsigned>(ZoneKind::Restricted)) return parse_error("unknown zone kind");
        z.id=ZoneId{id}; z.world=snapshot.world; z.kind=static_cast<ZoneKind>(kind); z.persistent=persistent!=0; if(parent) z.parent=ZoneId{parent}; snapshot.zones.push_back(std::move(z));
    }

    if (!(in >> marker >> count) || marker != "CONNECTIONS") return parse_error("malformed connection section");
    for (std::size_t i=0;i<count;++i) {
        std::uint64_t from=0,to=0; int bi=0,tr=0; std::string tag;
        if (!(in >> marker >> from >> to >> bi >> tr >> std::quoted(tag)) || marker!="C" || from==0 || to==0) return parse_error("malformed connection record");
        snapshot.connections.push_back(ZoneConnection{ZoneId{from},ZoneId{to},bi!=0,tr!=0,std::move(tag)});
    }

    if (!(in >> marker >> count) || marker != "PLACEMENTS") return parse_error("malformed placement section");
    for (std::size_t i=0;i<count;++i) {
        std::uint64_t entity=0,zone=0;
        if (!(in >> marker >> entity >> zone) || marker!="P" || entity==0 || zone==0) return parse_error("malformed placement record");
        snapshot.placements.push_back(SnapshotPlacement{EntityId{entity},ZoneId{zone}});
    }
    if (!(in >> marker) || marker!="END") return parse_error("snapshot missing END marker");

    if (format >= 5) {
        ClimateCatalog climate_validation;
        for (const auto& climate : snapshot.climates) {
            if (!snapshot_has_zone(snapshot, climate.zone) || climate_validation.find(climate.zone) || !climate_validation.set(climate)) {
                return parse_error("snapshot climate references missing zone or is invalid");
            }
        }
        for (const auto& state : snapshot.weather) {
            if (!snapshot_has_zone(snapshot, state.zone) || !climate_validation.find(state.zone)) {
                return parse_error("snapshot weather references missing zone or climate");
            }
        }
    }

    return Result<WorldSnapshot>::success(std::move(snapshot));
}

Result<void> save_snapshot_file(const WorldSnapshot& snapshot, const std::string& path) {
    if(path.empty()) return Result<void>::failure(ErrorCode::InvalidArgument,"snapshot path must not be empty");
    const auto encoded=encode_snapshot(snapshot); if(!encoded) return Result<void>::failure(encoded.error().code,encoded.error().message);
    std::ofstream out(path,std::ios::binary|std::ios::trunc); if(!out) return Result<void>::failure(ErrorCode::SerializationError,"could not open snapshot file for writing");
    out.write(encoded.value().data(),static_cast<std::streamsize>(encoded.value().size())); if(!out) return Result<void>::failure(ErrorCode::SerializationError,"snapshot write failed");
    return Result<void>::success();
}

Result<WorldSnapshot> load_snapshot_file(const std::string& path) {
    if(path.empty()) return Result<WorldSnapshot>::failure(ErrorCode::InvalidArgument,"snapshot path must not be empty");
    std::ifstream in(path,std::ios::binary); if(!in) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError,"could not open snapshot file for reading");
    std::ostringstream buffer; buffer<<in.rdbuf(); if(!in.good()&&!in.eof()) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError,"snapshot read failed");
    return decode_snapshot(buffer.str());
}

} // namespace home
