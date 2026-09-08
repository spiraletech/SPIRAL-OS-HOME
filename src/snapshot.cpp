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
    return std::any_of(snapshot.zones.begin(), snapshot.zones.end(), [&](const ZoneRecord& item) { return item.id == zone; });
}

const EntityRecord* snapshot_find_entity(const WorldSnapshot& snapshot, EntityId entity) {
    const auto it = std::find_if(snapshot.entities.begin(), snapshot.entities.end(), [&](const EntityRecord& item) { return item.id == entity; });
    return it == snapshot.entities.end() ? nullptr : &*it;
}

const PlayerLifeState* snapshot_find_life(const WorldSnapshot& snapshot, EntityId entity) {
    const auto it = std::find_if(snapshot.player_life.begin(), snapshot.player_life.end(), [&](const PlayerLifeState& item) { return item.entity == entity; });
    return it == snapshot.player_life.end() ? nullptr : &*it;
}

bool valid_player_life_shape(const PlayerLifeState& state) {
    return state.entity.valid()
        && static_cast<unsigned>(state.stage) <= static_cast<unsigned>(LifeStage::Elder)
        && static_cast<unsigned>(state.presence) <= static_cast<unsigned>(LifePresence::Deceased)
        && state.updated_world_minute >= state.born_world_minute
        && state.sequence != 0;
}

bool snapshot_valid_player(const WorldSnapshot& snapshot, EntityId entity) {
    const EntityRecord* record = snapshot_find_entity(snapshot, entity);
    return record != nullptr && record->kind == EntityKind::Avatar && snapshot_find_life(snapshot, entity) != nullptr;
}

Result<void> validate_snapshot_social(const WorldSnapshot& snapshot, std::uint64_t current_world_minute) {
    std::vector<std::pair<EntityId, EntityId>> relationship_keys;
    for (const auto& relationship : snapshot.relationships) {
        if (!relationship.from.valid() || !relationship.to.valid() || relationship.from == relationship.to
            || static_cast<unsigned>(relationship.kind) > static_cast<unsigned>(RelationshipKind::Dependent)
            || relationship.affinity < kRelationshipAffinityMinimum || relationship.affinity > kRelationshipAffinityMaximum
            || relationship.trust < kRelationshipTrustMinimum || relationship.trust > kRelationshipTrustMaximum
            || relationship.sequence == 0 || relationship.updated_world_minute > current_world_minute
            || !snapshot_valid_player(snapshot, relationship.from) || !snapshot_valid_player(snapshot, relationship.to)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot relationship state is invalid");
        }
        const auto key = std::pair{relationship.from, relationship.to};
        if (std::find(relationship_keys.begin(), relationship_keys.end(), key) != relationship_keys.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot relationship");
        }
        relationship_keys.push_back(key);
    }

    std::vector<HouseholdId> household_ids;
    std::vector<EntityId> household_members;
    for (const auto& household : snapshot.households) {
        if (!household.id.valid() || household.name.empty() || household.members.empty() || household.sequence == 0
            || household.updated_world_minute > current_world_minute
            || (household.home_zone.has_value() && !snapshot_has_zone(snapshot, *household.home_zone))) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot household state is invalid");
        }
        if (std::find(household_ids.begin(), household_ids.end(), household.id) != household_ids.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot household id");
        }
        household_ids.push_back(household.id);
        std::vector<EntityId> local_members = household.members;
        std::sort(local_members.begin(), local_members.end(), [](EntityId a, EntityId b) { return a.value() < b.value(); });
        if (std::adjacent_find(local_members.begin(), local_members.end()) != local_members.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot household member");
        }
        for (const EntityId member : local_members) {
            if (!snapshot_valid_player(snapshot, member)) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot household member lacks canonical avatar life state");
            }
            if (std::find(household_members.begin(), household_members.end(), member) != household_members.end()) {
                return Result<void>::failure(ErrorCode::AlreadyExists, "snapshot player belongs to multiple households");
            }
            household_members.push_back(member);
        }
    }
    return Result<void>::success();
}

} // namespace

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.world.valid()) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot requires a valid world id");
    if (snapshot.clock_config.real_milliseconds_per_home_minute == 0
        || snapshot.clock_remainder >= snapshot.clock_config.real_milliseconds_per_home_minute) {
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
        if (!snapshot_has_zone(snapshot, climate.zone)) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot climate references a missing zone");
        if (climate_validation.find(climate.zone)) return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot climate zone");
        const auto added = climate_validation.set(climate);
        if (!added) return Result<std::string>::failure(added.error().code, added.error().message);
    }

    WeatherLedger weather_validation;
    for (const auto& state : snapshot.weather) {
        if (!snapshot_has_zone(snapshot, state.zone) || !climate_validation.find(state.zone)) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot weather references a missing zone or climate");
        }
        if (weather_validation.find(state.zone)) return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot weather zone");
        const auto added = weather_validation.set(state);
        if (!added) return Result<std::string>::failure(added.error().code, added.error().message);
    }

    const auto affect_valid = validate_world_affect_state(snapshot.affect);
    if (!affect_valid) return Result<std::string>::failure(affect_valid.error().code, affect_valid.error().message);
    const auto anchor_valid = validate_world_anchor_state(snapshot.anchor);
    if (!anchor_valid) return Result<std::string>::failure(anchor_valid.error().code, anchor_valid.error().message);

    const std::uint64_t current_world_minute = snapshot.world_time.milliseconds / 60000ULL;
    std::vector<EntityId> life_entities;
    for (const auto& life : snapshot.player_life) {
        if (!valid_player_life_shape(life)) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player life state is invalid");
        if (life.born_world_minute > current_world_minute || life.updated_world_minute > current_world_minute) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player life references a future HOME minute");
        }
        if (std::find(life_entities.begin(), life_entities.end(), life.entity) != life_entities.end()) {
            return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot player life entity");
        }
        life_entities.push_back(life.entity);
        const EntityRecord* entity = snapshot_find_entity(snapshot, life.entity);
        if (entity == nullptr || entity->kind != EntityKind::Avatar) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player life references a missing or non-avatar entity");
        }
        if (life.home_zone.has_value() && !snapshot_has_zone(snapshot, *life.home_zone)) {
            return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player life references a missing home zone");
        }
    }

    std::vector<EntityId> dynamics_entities;
    for (const auto& dynamics : snapshot.player_dynamics) {
        const auto valid = validate_player_dynamics_shape(dynamics);
        if (!valid) return Result<std::string>::failure(valid.error().code, valid.error().message);
        if (dynamics.updated_world_minute > current_world_minute) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player dynamics references a future HOME minute");
        if (std::find(dynamics_entities.begin(), dynamics_entities.end(), dynamics.entity) != dynamics_entities.end()) return Result<std::string>::failure(ErrorCode::AlreadyExists, "duplicate snapshot player dynamics entity");
        dynamics_entities.push_back(dynamics.entity);
        const EntityRecord* entity = snapshot_find_entity(snapshot, dynamics.entity);
        const PlayerLifeState* life = snapshot_find_life(snapshot, dynamics.entity);
        if (entity == nullptr || entity->kind != EntityKind::Avatar || life == nullptr) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player dynamics requires matching avatar life state");
        if (dynamics.updated_world_minute < life->born_world_minute) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot player dynamics predates player birth minute");
        if (life->presence == LifePresence::Deceased && dynamics.autonomy.mode != AutonomyMode::Disabled) return Result<std::string>::failure(ErrorCode::ValidationFailed, "snapshot deceased player retains active autonomy");
    }

    const auto social_valid = validate_snapshot_social(snapshot, current_world_minute);
    if (!social_valid) return Result<std::string>::failure(social_valid.error().code, social_valid.error().message);

    std::ostringstream out;
    out << "HOME_SNAPSHOT " << kSnapshotFormatVersion << '\n';
    out << "WORLD " << snapshot.world.value() << ' ' << snapshot.revision.value() << '\n';
    out << "CLOCK " << snapshot.clock_config.real_milliseconds_per_home_minute << ' ' << snapshot.world_time.milliseconds << ' ' << snapshot.clock_remainder << '\n';
    out << "CALENDAR " << snapshot.calendar_config.epoch.year << ' ' << snapshot.calendar_config.epoch.month << ' ' << snapshot.calendar_config.epoch.day << '\n';

    out << "EVENTS " << snapshot.events.size() << '\n';
    for (const auto& event : snapshot.events) {
        out << "V " << event.id.value() << ' ' << static_cast<unsigned>(event.kind) << ' ' << event.priority << ' '
            << event.rule.month << ' ' << event.rule.day << ' ' << event.rule.duration_days << ' ' << event.affinities.size() << ' '
            << std::quoted(event.key) << ' ' << std::quoted(event.display_name);
        for (const auto& affinity : event.affinities) out << ' ' << std::quoted(affinity);
        out << '\n';
    }

    out << "CLIMATES " << snapshot.climates.size() << '\n';
    for (const auto& climate : snapshot.climates) out << "K " << climate.zone.value() << ' ' << climate.mean_temperature_millicelsius << ' ' << climate.wetness_permille << ' ' << climate.wind_permille << ' ' << climate.seed << '\n';

    out << "WEATHER " << snapshot.weather.size() << '\n';
    for (const auto& state : snapshot.weather) {
        out << "W " << state.zone.value() << ' ' << static_cast<unsigned>(state.intensity) << ' ' << static_cast<unsigned>(state.previous_intensity) << ' '
            << state.temperature_millicelsius << ' ' << state.cloud_permille << ' ' << state.precipitation_permille << ' ' << state.wind_mm_per_second << ' '
            << state.sequence << ' ' << state.age_minutes << '\n';
    }

    out << "AFFECT " << static_cast<unsigned>(snapshot.affect.tone) << ' ' << snapshot.affect.valence_milli << ' ' << snapshot.affect.intensity_permille << ' ' << snapshot.affect.stability_permille << '\n';
    out << "ANCHOR " << static_cast<unsigned>(snapshot.anchor.anchor) << ' ' << snapshot.anchor.strength_permille << ' ' << static_cast<unsigned>(snapshot.anchor.source) << ' ' << std::quoted(snapshot.anchor.authority) << '\n';

    out << "LIFE " << snapshot.player_life.size() << '\n';
    for (const auto& life : snapshot.player_life) {
        out << "L " << life.entity.value() << ' ' << static_cast<unsigned>(life.stage) << ' ' << static_cast<unsigned>(life.presence) << ' '
            << (life.home_zone ? life.home_zone->value() : 0) << ' ' << life.born_world_minute << ' ' << life.updated_world_minute << ' ' << life.sequence << '\n';
    }

    out << "DYNAMICS " << snapshot.player_dynamics.size() << '\n';
    for (const auto& dynamics : snapshot.player_dynamics) {
        out << "D " << dynamics.entity.value();
        for (const auto value : dynamics.needs.values) out << ' ' << value;
        out << ' ' << dynamics.mood.valence << ' ' << dynamics.mood.arousal << ' ' << static_cast<unsigned>(dynamics.mood.band) << ' '
            << static_cast<unsigned>(dynamics.autonomy.mode) << ' ' << dynamics.autonomy.initiative_limit_per_hour << ' '
            << (dynamics.autonomy.may_change_zone ? 1 : 0) << ' ' << (dynamics.autonomy.may_interact_with_entities ? 1 : 0) << ' '
            << dynamics.updated_world_minute << ' ' << dynamics.sequence << ' ' << std::quoted(dynamics.active_drive) << '\n';
    }

    out << "RELATIONSHIPS " << snapshot.relationships.size() << '\n';
    for (const auto& relationship : snapshot.relationships) {
        out << "R " << relationship.from.value() << ' ' << relationship.to.value() << ' ' << static_cast<unsigned>(relationship.kind) << ' '
            << relationship.affinity << ' ' << relationship.trust << ' ' << relationship.updated_world_minute << ' ' << relationship.sequence << '\n';
    }

    out << "HOUSEHOLDS " << snapshot.households.size() << '\n';
    for (const auto& household : snapshot.households) {
        out << "H " << household.id.value() << ' ' << (household.home_zone ? household.home_zone->value() : 0) << ' '
            << household.updated_world_minute << ' ' << household.sequence << ' ' << household.members.size() << ' ' << std::quoted(household.name);
        for (const EntityId member : household.members) out << ' ' << member.value();
        out << '\n';
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
    for (const auto& c : snapshot.connections) out << "C " << c.from.value() << ' ' << c.to.value() << ' ' << (c.bidirectional ? 1 : 0) << ' ' << (c.traversable ? 1 : 0) << ' ' << std::quoted(c.tag) << '\n';
    out << "PLACEMENTS " << snapshot.placements.size() << '\n';
    for (const auto& p : snapshot.placements) out << "P " << p.entity.value() << ' ' << p.zone.value() << '\n';
    out << "END\n";
    return Result<std::string>::success(out.str());
}

Result<WorldSnapshot> decode_snapshot(std::string_view encoded) {
    std::istringstream in{std::string(encoded)};
    std::string marker;
    unsigned format = 0;
    if (!(in >> marker >> format) || marker != "HOME_SNAPSHOT" || format < 1 || format > kSnapshotFormatVersion) return parse_error("unsupported or malformed snapshot header");

    WorldSnapshot snapshot{};
    std::uint64_t world_value = 0, revision_value = 0;
    if (!(in >> marker >> world_value >> revision_value) || marker != "WORLD" || world_value == 0) return parse_error("malformed snapshot world header");
    snapshot.world = WorldId{world_value}; snapshot.revision = WorldRevision{revision_value};

    if (format >= 2) {
        std::uint64_t ratio = 0, time = 0, remainder = 0;
        if (!(in >> marker >> ratio >> time >> remainder) || marker != "CLOCK" || ratio == 0 || remainder >= ratio) return parse_error("malformed snapshot clock state");
        snapshot.clock_config.real_milliseconds_per_home_minute = ratio; snapshot.world_time = WorldTime{time}; snapshot.clock_remainder = remainder;
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
            std::uint64_t id = 0; unsigned kind = 0, month = 0, day = 0, duration = 0; int priority = 0; std::size_t affinity_count = 0; EventDefinition event{};
            if (!(in >> marker >> id >> kind >> priority >> month >> day >> duration >> affinity_count >> std::quoted(event.key) >> std::quoted(event.display_name))
                || marker != "V" || id == 0 || kind > static_cast<unsigned>(EventKind::Story)) return parse_error("malformed event record");
            event.id = EventId{id}; event.kind = static_cast<EventKind>(kind); event.priority = priority; event.rule = AnnualDateRule{month, day, duration};
            for (std::size_t a = 0; a < affinity_count; ++a) { std::string affinity; if (!(in >> std::quoted(affinity))) return parse_error("malformed event affinity"); event.affinities.push_back(std::move(affinity)); }
            const auto added = event_validation.add(event); if (!added) return parse_error("invalid or duplicate event record"); snapshot.events.push_back(std::move(event));
        }
    }

    if (format >= 5) {
        if (!(in >> marker >> count) || marker != "CLIMATES") return parse_error("malformed climate section");
        ClimateCatalog climate_validation;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, seed = 0; int temperature = 0; unsigned wetness = 0, wind = 0;
            if (!(in >> marker >> zone >> temperature >> wetness >> wind >> seed) || marker != "K" || zone == 0) return parse_error("malformed climate record");
            ClimateProfile climate{ZoneId{zone}, temperature, wetness, wind, seed};
            if (climate_validation.find(climate.zone) || !climate_validation.set(climate)) return parse_error("invalid or duplicate climate record");
            snapshot.climates.push_back(climate);
        }
        if (!(in >> marker >> count) || marker != "WEATHER") return parse_error("malformed weather section");
        WeatherLedger weather_validation;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, sequence = 0; unsigned intensity = 0, previous = 0, cloud = 0, precipitation = 0, wind = 0, age = 0; int temperature = 0;
            if (!(in >> marker >> zone >> intensity >> previous >> temperature >> cloud >> precipitation >> wind >> sequence >> age)
                || marker != "W" || zone == 0 || intensity > static_cast<unsigned>(RainIntensity::Deluge) || previous > static_cast<unsigned>(RainIntensity::Deluge)) return parse_error("malformed weather record");
            WeatherState state{ZoneId{zone}, static_cast<RainIntensity>(intensity), static_cast<RainIntensity>(previous), temperature, cloud, precipitation, wind, sequence, age};
            if (weather_validation.find(state.zone) || !weather_validation.set(state)) return parse_error("invalid or duplicate weather record");
            snapshot.weather.push_back(state);
        }
    }

    if (format >= 6) {
        unsigned tone = 0, affect_intensity = 0, affect_stability = 0; int valence = 0;
        if (!(in >> marker >> tone >> valence >> affect_intensity >> affect_stability) || marker != "AFFECT" || tone > static_cast<unsigned>(WorldTone::Dreamlike)) return parse_error("malformed world affect state");
        snapshot.affect = WorldAffectState{static_cast<WorldTone>(tone), valence, affect_intensity, affect_stability};
        if (!validate_world_affect_state(snapshot.affect)) return parse_error("invalid world affect state");
        unsigned anchor = 0, strength = 0, source = 0; std::string authority;
        if (!(in >> marker >> anchor >> strength >> source >> std::quoted(authority)) || marker != "ANCHOR"
            || anchor > static_cast<unsigned>(ThemeAnchor::HauntedHalloweenRain) || source > static_cast<unsigned>(AnchorSource::AuthorizedOverride)) return parse_error("malformed world anchor state");
        snapshot.anchor = WorldAnchorState{static_cast<ThemeAnchor>(anchor), strength, static_cast<AnchorSource>(source), std::move(authority)};
        if (!validate_world_anchor_state(snapshot.anchor)) return parse_error("invalid world anchor state");
    }

    if (format >= 7) {
        if (!(in >> marker >> count) || marker != "LIFE") return parse_error("malformed player life section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t entity = 0, home_zone = 0, born = 0, updated = 0, sequence = 0; unsigned stage = 0, presence = 0;
            if (!(in >> marker >> entity >> stage >> presence >> home_zone >> born >> updated >> sequence) || marker != "L" || entity == 0
                || stage > static_cast<unsigned>(LifeStage::Elder) || presence > static_cast<unsigned>(LifePresence::Deceased) || sequence == 0 || updated < born) return parse_error("malformed player life record");
            PlayerLifeState life{}; life.entity = EntityId{entity}; life.stage = static_cast<LifeStage>(stage); life.presence = static_cast<LifePresence>(presence);
            if (home_zone != 0) life.home_zone = ZoneId{home_zone}; life.born_world_minute = born; life.updated_world_minute = updated; life.sequence = sequence;
            if (snapshot_find_life(snapshot, life.entity) != nullptr) return parse_error("duplicate player life entity");
            snapshot.player_life.push_back(life);
        }
    }

    if (format >= 8) {
        if (!(in >> marker >> count) || marker != "DYNAMICS") return parse_error("malformed player dynamics section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t entity = 0, updated = 0, sequence = 0; unsigned band = 0, mode = 0, initiative = 0; int may_change_zone = 0, may_interact = 0; PlayerDynamicsState dynamics{};
            if (!(in >> marker >> entity) || marker != "D" || entity == 0) return parse_error("malformed player dynamics record");
            for (auto& value : dynamics.needs.values) if (!(in >> value)) return parse_error("malformed player dynamics needs");
            if (!(in >> dynamics.mood.valence >> dynamics.mood.arousal >> band >> mode >> initiative >> may_change_zone >> may_interact >> updated >> sequence >> std::quoted(dynamics.active_drive))
                || band > static_cast<unsigned>(MoodBand::Elevated) || mode > static_cast<unsigned>(AutonomyMode::Bounded)
                || (may_change_zone != 0 && may_change_zone != 1) || (may_interact != 0 && may_interact != 1)) return parse_error("malformed player dynamics record");
            dynamics.entity = EntityId{entity}; dynamics.mood.band = static_cast<MoodBand>(band); dynamics.autonomy.mode = static_cast<AutonomyMode>(mode);
            dynamics.autonomy.initiative_limit_per_hour = initiative; dynamics.autonomy.may_change_zone = may_change_zone != 0; dynamics.autonomy.may_interact_with_entities = may_interact != 0;
            dynamics.updated_world_minute = updated; dynamics.sequence = sequence;
            const auto valid = validate_player_dynamics_shape(dynamics); if (!valid) return parse_error("invalid player dynamics record");
            if (std::any_of(snapshot.player_dynamics.begin(), snapshot.player_dynamics.end(), [&](const PlayerDynamicsState& existing) { return existing.entity == dynamics.entity; })) return parse_error("duplicate player dynamics entity");
            snapshot.player_dynamics.push_back(std::move(dynamics));
        }
    }

    if (format >= 9) {
        if (!(in >> marker >> count) || marker != "RELATIONSHIPS") return parse_error("malformed relationships section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t from = 0, to = 0, updated = 0, sequence = 0; unsigned kind = 0; std::int32_t affinity = 0, trust = 0;
            if (!(in >> marker >> from >> to >> kind >> affinity >> trust >> updated >> sequence) || marker != "R" || from == 0 || to == 0 || from == to
                || kind > static_cast<unsigned>(RelationshipKind::Dependent) || sequence == 0) return parse_error("malformed relationship record");
            RelationshipState state{EntityId{from}, EntityId{to}, static_cast<RelationshipKind>(kind), affinity, trust, updated, sequence};
            if (std::any_of(snapshot.relationships.begin(), snapshot.relationships.end(), [&](const RelationshipState& existing) { return existing.from == state.from && existing.to == state.to; })) return parse_error("duplicate relationship record");
            snapshot.relationships.push_back(state);
        }

        if (!(in >> marker >> count) || marker != "HOUSEHOLDS") return parse_error("malformed households section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t id = 0, home_zone = 0, updated = 0, sequence = 0; std::size_t member_count = 0; HouseholdState household{};
            if (!(in >> marker >> id >> home_zone >> updated >> sequence >> member_count >> std::quoted(household.name)) || marker != "H" || id == 0 || sequence == 0 || member_count == 0) return parse_error("malformed household record");
            household.id = HouseholdId{id}; if (home_zone != 0) household.home_zone = ZoneId{home_zone}; household.updated_world_minute = updated; household.sequence = sequence;
            for (std::size_t m = 0; m < member_count; ++m) { std::uint64_t member = 0; if (!(in >> member) || member == 0) return parse_error("malformed household member"); household.members.push_back(EntityId{member}); }
            snapshot.households.push_back(std::move(household));
        }
    }

    if (!(in >> marker >> count) || marker != "ENTITIES") return parse_error("malformed entity section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0; unsigned kind = 0; int persistent = 0; EntityRecord e{};
        if (!(in >> marker >> id >> kind >> persistent >> std::quoted(e.archetype) >> std::quoted(e.display_name)
              >> e.transform.position.x >> e.transform.position.y >> e.transform.position.z
              >> e.transform.rotation.pitch >> e.transform.rotation.yaw >> e.transform.rotation.roll)
            || marker != "E" || id == 0 || kind > static_cast<unsigned>(EntityKind::Environment)) return parse_error("malformed entity record");
        e.id = EntityId{id}; e.world = snapshot.world; e.kind = static_cast<EntityKind>(kind); e.persistent = persistent != 0; snapshot.entities.push_back(std::move(e));
    }

    if (!(in >> marker >> count) || marker != "ZONES") return parse_error("malformed zone section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0, parent = 0; unsigned kind = 0; int persistent = 0; ZoneRecord z{};
        if (!(in >> marker >> id >> kind >> persistent >> parent >> std::quoted(z.key) >> std::quoted(z.display_name))
            || marker != "Z" || id == 0 || kind > static_cast<unsigned>(ZoneKind::Restricted)) return parse_error("malformed zone record");
        z.id = ZoneId{id}; z.world = snapshot.world; z.kind = static_cast<ZoneKind>(kind); z.persistent = persistent != 0; if (parent != 0) z.parent = ZoneId{parent}; snapshot.zones.push_back(std::move(z));
    }

    if (!(in >> marker >> count) || marker != "CONNECTIONS") return parse_error("malformed connection section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t from = 0, to = 0; int bidirectional = 0, traversable = 0; std::string tag;
        if (!(in >> marker >> from >> to >> bidirectional >> traversable >> std::quoted(tag)) || marker != "C" || from == 0 || to == 0) return parse_error("malformed connection record");
        snapshot.connections.push_back(ZoneConnection{ZoneId{from}, ZoneId{to}, bidirectional != 0, traversable != 0, std::move(tag)});
    }

    if (!(in >> marker >> count) || marker != "PLACEMENTS") return parse_error("malformed placement section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t entity = 0, zone = 0;
        if (!(in >> marker >> entity >> zone) || marker != "P" || entity == 0 || zone == 0) return parse_error("malformed placement record");
        snapshot.placements.push_back(SnapshotPlacement{EntityId{entity}, ZoneId{zone}});
    }
    if (!(in >> marker) || marker != "END") return parse_error("snapshot missing END marker");

    if (format >= 5) {
        ClimateCatalog climate_validation;
        for (const auto& climate : snapshot.climates) {
            if (!snapshot_has_zone(snapshot, climate.zone) || climate_validation.find(climate.zone) || !climate_validation.set(climate)) return parse_error("snapshot climate references missing zone or is invalid");
        }
        for (const auto& state : snapshot.weather) if (!snapshot_has_zone(snapshot, state.zone) || !climate_validation.find(state.zone)) return parse_error("snapshot weather references missing zone or climate");
    }
    if (format >= 7) {
        const std::uint64_t current_world_minute = snapshot.world_time.milliseconds / 60000ULL;
        for (const auto& life : snapshot.player_life) {
            const EntityRecord* entity = snapshot_find_entity(snapshot, life.entity);
            if (entity == nullptr || entity->kind != EntityKind::Avatar) return parse_error("snapshot player life references a missing or non-avatar entity");
            if (life.home_zone.has_value() && !snapshot_has_zone(snapshot, *life.home_zone)) return parse_error("snapshot player life references a missing home zone");
            if (life.born_world_minute > current_world_minute || life.updated_world_minute > current_world_minute) return parse_error("snapshot player life references a future HOME minute");
        }
    }
    if (format >= 8) {
        const std::uint64_t current_world_minute = snapshot.world_time.milliseconds / 60000ULL;
        for (const auto& dynamics : snapshot.player_dynamics) {
            const EntityRecord* entity = snapshot_find_entity(snapshot, dynamics.entity); const PlayerLifeState* life = snapshot_find_life(snapshot, dynamics.entity);
            if (entity == nullptr || entity->kind != EntityKind::Avatar || life == nullptr) return parse_error("snapshot player dynamics requires matching avatar life state");
            if (dynamics.updated_world_minute > current_world_minute || dynamics.updated_world_minute < life->born_world_minute) return parse_error("snapshot player dynamics chronology is invalid");
            if (life->presence == LifePresence::Deceased && dynamics.autonomy.mode != AutonomyMode::Disabled) return parse_error("snapshot deceased player retains active autonomy");
        }
    }
    if (format >= 9) {
        const std::uint64_t current_world_minute = snapshot.world_time.milliseconds / 60000ULL;
        const auto social_valid = validate_snapshot_social(snapshot, current_world_minute);
        if (!social_valid) return parse_error("snapshot social state is invalid");
    }

    return Result<WorldSnapshot>::success(std::move(snapshot));
}

Result<void> save_snapshot_file(const WorldSnapshot& snapshot, const std::string& path) {
    if (path.empty()) return Result<void>::failure(ErrorCode::InvalidArgument, "snapshot path must not be empty");
    const auto encoded = encode_snapshot(snapshot); if (!encoded) return Result<void>::failure(encoded.error().code, encoded.error().message);
    std::ofstream out(path, std::ios::binary | std::ios::trunc); if (!out) return Result<void>::failure(ErrorCode::SerializationError, "could not open snapshot file for writing");
    out.write(encoded.value().data(), static_cast<std::streamsize>(encoded.value().size())); if (!out) return Result<void>::failure(ErrorCode::SerializationError, "snapshot write failed");
    return Result<void>::success();
}

Result<WorldSnapshot> load_snapshot_file(const std::string& path) {
    if (path.empty()) return Result<WorldSnapshot>::failure(ErrorCode::InvalidArgument, "snapshot path must not be empty");
    std::ifstream in(path, std::ios::binary); if (!in) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, "could not open snapshot file for reading");
    std::ostringstream buffer; buffer << in.rdbuf(); if (!in.good() && !in.eof()) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, "snapshot read failed");
    return decode_snapshot(buffer.str());
}

} // namespace home
