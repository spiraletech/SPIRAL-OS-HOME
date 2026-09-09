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

bool has_zone(const WorldSnapshot& snapshot, ZoneId zone) {
    return std::any_of(snapshot.zones.begin(), snapshot.zones.end(), [&](const ZoneRecord& item) { return item.id == zone; });
}

const EntityRecord* find_entity(const WorldSnapshot& snapshot, EntityId entity) {
    const auto it = std::find_if(snapshot.entities.begin(), snapshot.entities.end(), [&](const EntityRecord& item) { return item.id == entity; });
    return it == snapshot.entities.end() ? nullptr : &*it;
}

const PlayerLifeState* find_life(const WorldSnapshot& snapshot, EntityId entity) {
    const auto it = std::find_if(snapshot.player_life.begin(), snapshot.player_life.end(), [&](const PlayerLifeState& item) { return item.entity == entity; });
    return it == snapshot.player_life.end() ? nullptr : &*it;
}

const PlayerDynamicsState* find_dynamics(const WorldSnapshot& snapshot, EntityId entity) {
    const auto it = std::find_if(snapshot.player_dynamics.begin(), snapshot.player_dynamics.end(), [&](const PlayerDynamicsState& item) { return item.entity == entity; });
    return it == snapshot.player_dynamics.end() ? nullptr : &*it;
}

bool valid_player(const WorldSnapshot& snapshot, EntityId entity) {
    const auto* record = find_entity(snapshot, entity);
    return record != nullptr && record->kind == EntityKind::Avatar && find_life(snapshot, entity) != nullptr;
}

bool valid_life_shape(const PlayerLifeState& state) {
    return state.entity.valid()
        && static_cast<unsigned>(state.stage) <= static_cast<unsigned>(LifeStage::Elder)
        && static_cast<unsigned>(state.presence) <= static_cast<unsigned>(LifePresence::Deceased)
        && state.updated_world_minute >= state.born_world_minute
        && state.sequence != 0;
}

Result<void> validate_social(const WorldSnapshot& snapshot, std::uint64_t current_world_minute) {
    std::vector<std::pair<EntityId, EntityId>> relationship_keys;
    for (const auto& relationship : snapshot.relationships) {
        if (!relationship.from.valid() || !relationship.to.valid() || relationship.from == relationship.to
            || static_cast<unsigned>(relationship.kind) > static_cast<unsigned>(RelationshipKind::Dependent)
            || relationship.affinity < kRelationshipAffinityMinimum || relationship.affinity > kRelationshipAffinityMaximum
            || relationship.trust < kRelationshipTrustMinimum || relationship.trust > kRelationshipTrustMaximum
            || relationship.sequence == 0 || relationship.updated_world_minute > current_world_minute
            || !valid_player(snapshot, relationship.from) || !valid_player(snapshot, relationship.to)) {
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
            || (household.home_zone.has_value() && !has_zone(snapshot, *household.home_zone))) {
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
            if (!valid_player(snapshot, member)) {
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

Result<void> validate_items(const WorldSnapshot& snapshot, std::uint64_t current_world_minute) {
    std::vector<ItemId> ids;
    for (const auto& item : snapshot.items) {
        if (!item.id.valid() || item.archetype_key.empty() || item.display_name.empty()
            || static_cast<unsigned>(item.kind) > static_cast<unsigned>(ItemKind::Material)
            || item.quantity == 0 || item.max_stack == 0 || item.quantity > item.max_stack
            || item.durability > kItemDurabilityMaximum || item.sequence == 0
            || item.updated_world_minute > current_world_minute || (item.owner && item.zone)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot item state is invalid");
        }
        if (item.owner && !valid_player(snapshot, *item.owner)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot item owner lacks canonical avatar life state");
        }
        if (item.zone && !has_zone(snapshot, *item.zone)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot item references a missing zone");
        }
        if (std::find(ids.begin(), ids.end(), item.id) != ids.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot item id");
        }
        ids.push_back(item.id);
    }
    return Result<void>::success();
}

const TaskState* find_task(const WorldSnapshot& snapshot, TaskId id) {
    const auto it = std::find_if(snapshot.tasks.begin(), snapshot.tasks.end(), [&](const TaskState& task) { return task.id == id; });
    return it == snapshot.tasks.end() ? nullptr : &*it;
}

const QuestState* find_quest(const WorldSnapshot& snapshot, QuestId id) {
    const auto it = std::find_if(snapshot.quests.begin(), snapshot.quests.end(), [&](const QuestState& quest) { return quest.id == id; });
    return it == snapshot.quests.end() ? nullptr : &*it;
}

Result<void> validate_progression(const WorldSnapshot& snapshot, std::uint64_t current_world_minute) {
    std::vector<std::pair<EntityId, std::string>> skill_keys;
    for (const auto& skill : snapshot.skills) {
        const auto valid = validate_skill_shape(skill);
        if (!valid || skill.updated_world_minute > current_world_minute || !valid_player(snapshot, skill.player)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot skill state is invalid");
        }
        const auto key = std::pair{skill.player, skill.key};
        if (std::find(skill_keys.begin(), skill_keys.end(), key) != skill_keys.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot skill key");
        }
        skill_keys.push_back(key);
    }

    std::vector<TaskId> task_ids;
    std::vector<std::pair<EntityId, std::string>> task_keys;
    for (const auto& task : snapshot.tasks) {
        const auto valid = validate_task_shape(task);
        if (!valid || task.updated_world_minute > current_world_minute || !valid_player(snapshot, task.owner)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot task state is invalid");
        }
        if (std::find(task_ids.begin(), task_ids.end(), task.id) != task_ids.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot task id");
        }
        const auto key = std::pair{task.owner, task.key};
        if (std::find(task_keys.begin(), task_keys.end(), key) != task_keys.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot task key");
        }
        task_ids.push_back(task.id);
        task_keys.push_back(key);
    }

    std::vector<QuestId> quest_ids;
    std::vector<std::pair<EntityId, std::string>> quest_keys;
    for (const auto& quest : snapshot.quests) {
        const auto valid = validate_quest_shape(quest);
        if (!valid || quest.updated_world_minute > current_world_minute || !valid_player(snapshot, quest.owner)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot quest state is invalid");
        }
        if (std::find(quest_ids.begin(), quest_ids.end(), quest.id) != quest_ids.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot quest id");
        }
        const auto key = std::pair{quest.owner, quest.key};
        if (std::find(quest_keys.begin(), quest_keys.end(), key) != quest_keys.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot quest key");
        }
        for (const TaskId task_id : quest.tasks) {
            const auto* task = find_task(snapshot, task_id);
            if (!task || task->owner != quest.owner) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot quest references missing or foreign task");
            }
            if (quest.status == QuestStatus::Complete && task->status != TaskStatus::Complete) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "complete snapshot quest contains incomplete task");
            }
        }
        for (const QuestId prerequisite : quest.prerequisites) {
            const auto* prior = find_quest(snapshot, prerequisite);
            if (!prior || prior->owner != quest.owner || prerequisite.value() >= quest.id.value()) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot quest prerequisite graph is invalid");
            }
            if ((quest.status == QuestStatus::Active || quest.status == QuestStatus::Complete)
                && prior->status != QuestStatus::Complete) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "active snapshot quest has incomplete prerequisite");
            }
        }
        quest_ids.push_back(quest.id);
        quest_keys.push_back(key);
    }
    return Result<void>::success();
}

bool aura_semantics_equal(const AuraState& aura, const HalaluluResolution& resolution) noexcept {
    return aura.player == resolution.player
        && aura.dominant_affinity == resolution.dominant_affinity
        && aura.signature == resolution.signature
        && aura.intensity_permille == resolution.intensity_permille
        && aura.charge_milli == resolution.charge_milli
        && aura.coherence_permille == resolution.coherence_permille;
}

Result<void> validate_theorism(const WorldSnapshot& snapshot, std::uint64_t current_world_minute) {
    std::vector<std::pair<EntityId, std::string>> subclass_keys;
    std::vector<EntityId> players_with_subclasses;
    for (const auto& subclass : snapshot.subclasses) {
        const auto valid = validate_subclass_affinity_shape(subclass);
        const auto* life = find_life(snapshot, subclass.player);
        if (!valid || !valid_player(snapshot, subclass.player) || !life
            || subclass.discovered_world_minute < life->born_world_minute
            || subclass.discovered_world_minute > current_world_minute
            || subclass.updated_world_minute > current_world_minute) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot subclass affinity state is invalid");
        }
        const auto key = std::pair{subclass.player, subclass.key};
        if (std::find(subclass_keys.begin(), subclass_keys.end(), key) != subclass_keys.end()) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate snapshot subclass affinity key");
        }
        subclass_keys.push_back(key);
        if (std::find(players_with_subclasses.begin(), players_with_subclasses.end(), subclass.player) == players_with_subclasses.end()) {
            players_with_subclasses.push_back(subclass.player);
        }
    }

    std::vector<EntityId> aura_players;
    for (const auto& aura : snapshot.auras) {
        const auto valid = validate_aura_shape(aura);
        if (!valid || !valid_player(snapshot, aura.player) || aura.updated_world_minute > current_world_minute
            || std::find(aura_players.begin(), aura_players.end(), aura.player) != aura_players.end()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot aura state is invalid");
        }
        std::vector<SubclassAffinityState> subclasses;
        for (const auto& subclass : snapshot.subclasses) if (subclass.player == aura.player) subclasses.push_back(subclass);
        if (subclasses.empty()) return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot aura has no discovered subclass source");
        const auto resolution = resolve_halalulu(aura.player, subclasses, find_dynamics(snapshot, aura.player));
        if (!resolution || !aura_semantics_equal(aura, resolution.value())) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot aura does not match HALALULU resolution");
        }
        aura_players.push_back(aura.player);
    }

    for (const EntityId player : players_with_subclasses) {
        if (std::find(aura_players.begin(), aura_players.end(), player) == aura_players.end()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot player with subclasses is missing canonical aura");
        }
    }
    return Result<void>::success();
}

Result<void> validate_common_snapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.world.valid()) return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot requires a valid world id");
    if (snapshot.clock_config.real_milliseconds_per_home_minute == 0
        || snapshot.clock_remainder >= snapshot.clock_config.real_milliseconds_per_home_minute) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot clock state is invalid");
    }
    if (!valid_calendar_date(snapshot.calendar_config.epoch)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot calendar epoch is invalid");
    }

    EventCatalog event_validation;
    for (const auto& event : snapshot.events) {
        const auto added = event_validation.add(event);
        if (!added) return Result<void>::failure(added.error().code, added.error().message);
    }

    ClimateCatalog climate_validation;
    for (const auto& climate : snapshot.climates) {
        if (!has_zone(snapshot, climate.zone) || climate_validation.find(climate.zone)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot climate is invalid");
        }
        const auto added = climate_validation.set(climate);
        if (!added) return Result<void>::failure(added.error().code, added.error().message);
    }

    WeatherLedger weather_validation;
    for (const auto& state : snapshot.weather) {
        if (!has_zone(snapshot, state.zone) || !climate_validation.find(state.zone) || weather_validation.find(state.zone)) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot weather is invalid");
        }
        const auto added = weather_validation.set(state);
        if (!added) return Result<void>::failure(added.error().code, added.error().message);
    }

    const auto affect_valid = validate_world_affect_state(snapshot.affect);
    if (!affect_valid) return affect_valid;
    const auto anchor_valid = validate_world_anchor_state(snapshot.anchor);
    if (!anchor_valid) return anchor_valid;

    const std::uint64_t current_world_minute = snapshot.world_time.milliseconds / 60000ULL;
    std::vector<EntityId> life_entities;
    for (const auto& life : snapshot.player_life) {
        if (!valid_life_shape(life) || life.born_world_minute > current_world_minute || life.updated_world_minute > current_world_minute
            || !find_entity(snapshot, life.entity) || find_entity(snapshot, life.entity)->kind != EntityKind::Avatar
            || (life.home_zone && !has_zone(snapshot, *life.home_zone))
            || std::find(life_entities.begin(), life_entities.end(), life.entity) != life_entities.end()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot player life state is invalid");
        }
        life_entities.push_back(life.entity);
    }

    std::vector<EntityId> dynamics_entities;
    for (const auto& dynamics : snapshot.player_dynamics) {
        const auto valid = validate_player_dynamics_shape(dynamics);
        const auto* life = find_life(snapshot, dynamics.entity);
        if (!valid || dynamics.updated_world_minute > current_world_minute || !valid_player(snapshot, dynamics.entity) || !life
            || dynamics.updated_world_minute < life->born_world_minute
            || (life->presence == LifePresence::Deceased && dynamics.autonomy.mode != AutonomyMode::Disabled)
            || std::find(dynamics_entities.begin(), dynamics_entities.end(), dynamics.entity) != dynamics_entities.end()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "snapshot player dynamics state is invalid");
        }
        dynamics_entities.push_back(dynamics.entity);
    }

    const auto social_valid = validate_social(snapshot, current_world_minute);
    if (!social_valid) return social_valid;
    const auto item_valid = validate_items(snapshot, current_world_minute);
    if (!item_valid) return item_valid;
    const auto progression_valid = validate_progression(snapshot, current_world_minute);
    if (!progression_valid) return progression_valid;
    const auto theorism_valid = validate_theorism(snapshot, current_world_minute);
    if (!theorism_valid) return theorism_valid;
    return Result<void>::success();
}

} // namespace

Result<std::string> encode_snapshot(const WorldSnapshot& snapshot) {
    const auto valid = validate_common_snapshot(snapshot);
    if (!valid) return Result<std::string>::failure(valid.error().code, valid.error().message);

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

    out << "AFFECT " << static_cast<unsigned>(snapshot.affect.tone) << ' ' << snapshot.affect.valence_milli << ' '
        << snapshot.affect.intensity_permille << ' ' << snapshot.affect.stability_permille << '\n';
    out << "ANCHOR " << static_cast<unsigned>(snapshot.anchor.anchor) << ' ' << snapshot.anchor.strength_permille << ' '
        << static_cast<unsigned>(snapshot.anchor.source) << ' ' << std::quoted(snapshot.anchor.authority) << '\n';

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

    out << "ITEMS " << snapshot.items.size() << '\n';
    for (const auto& item : snapshot.items) {
        out << "I " << item.id.value() << ' ' << static_cast<unsigned>(item.kind) << ' ' << item.quantity << ' ' << item.max_stack << ' '
            << item.durability << ' ' << (item.owner ? item.owner->value() : 0) << ' ' << (item.zone ? item.zone->value() : 0) << ' '
            << item.updated_world_minute << ' ' << item.sequence << ' ' << std::quoted(item.archetype_key) << ' ' << std::quoted(item.display_name) << '\n';
    }

    out << "SKILLS " << snapshot.skills.size() << '\n';
    for (const auto& skill : snapshot.skills) {
        out << "S " << skill.player.value() << ' ' << skill.level << ' ' << skill.experience << ' '
            << skill.updated_world_minute << ' ' << skill.sequence << ' ' << std::quoted(skill.key) << '\n';
    }

    out << "TASKS " << snapshot.tasks.size() << '\n';
    for (const auto& task : snapshot.tasks) {
        out << "T " << task.id.value() << ' ' << task.owner.value() << ' ' << static_cast<unsigned>(task.status) << ' '
            << task.progress << ' ' << task.target << ' ' << task.updated_world_minute << ' ' << task.sequence << ' '
            << std::quoted(task.key) << ' ' << std::quoted(task.title) << '\n';
    }

    out << "QUESTS " << snapshot.quests.size() << '\n';
    for (const auto& quest : snapshot.quests) {
        out << "Q " << quest.id.value() << ' ' << quest.owner.value() << ' ' << static_cast<unsigned>(quest.status) << ' '
            << quest.updated_world_minute << ' ' << quest.sequence << ' ' << quest.tasks.size() << ' ' << quest.prerequisites.size() << ' '
            << std::quoted(quest.key) << ' ' << std::quoted(quest.title);
        for (const TaskId task : quest.tasks) out << ' ' << task.value();
        for (const QuestId prerequisite : quest.prerequisites) out << ' ' << prerequisite.value();
        out << '\n';
    }

    out << "SUBCLASSES " << snapshot.subclasses.size() << '\n';
    for (const auto& subclass : snapshot.subclasses) {
        out << "B " << subclass.player.value() << ' ' << subclass.evidence_points << ' ' << subclass.affinity_permille << ' '
            << subclass.discovered_world_minute << ' ' << subclass.updated_world_minute << ' ' << subclass.sequence << ' '
            << std::quoted(subclass.key) << '\n';
    }

    out << "AURAS " << snapshot.auras.size() << '\n';
    for (const auto& aura : snapshot.auras) {
        out << "U " << aura.player.value() << ' ' << aura.intensity_permille << ' ' << aura.charge_milli << ' '
            << aura.coherence_permille << ' ' << aura.updated_world_minute << ' ' << aura.sequence << ' '
            << std::quoted(aura.dominant_affinity) << ' ' << std::quoted(aura.signature) << '\n';
    }

    out << "ENTITIES " << snapshot.entities.size() << '\n';
    for (const auto& entity : snapshot.entities) {
        out << "E " << entity.id.value() << ' ' << static_cast<unsigned>(entity.kind) << ' ' << (entity.persistent ? 1 : 0) << ' '
            << std::quoted(entity.archetype) << ' ' << std::quoted(entity.display_name) << ' '
            << entity.transform.position.x << ' ' << entity.transform.position.y << ' ' << entity.transform.position.z << ' '
            << entity.transform.rotation.pitch << ' ' << entity.transform.rotation.yaw << ' ' << entity.transform.rotation.roll << '\n';
    }

    out << "ZONES " << snapshot.zones.size() << '\n';
    for (const auto& zone : snapshot.zones) {
        out << "Z " << zone.id.value() << ' ' << static_cast<unsigned>(zone.kind) << ' ' << (zone.persistent ? 1 : 0) << ' '
            << (zone.parent ? zone.parent->value() : 0) << ' ' << std::quoted(zone.key) << ' ' << std::quoted(zone.display_name) << '\n';
    }

    out << "CONNECTIONS " << snapshot.connections.size() << '\n';
    for (const auto& connection : snapshot.connections) {
        out << "C " << connection.from.value() << ' ' << connection.to.value() << ' ' << (connection.bidirectional ? 1 : 0) << ' '
            << (connection.traversable ? 1 : 0) << ' ' << std::quoted(connection.tag) << '\n';
    }

    out << "PLACEMENTS " << snapshot.placements.size() << '\n';
    for (const auto& placement : snapshot.placements) out << "P " << placement.entity.value() << ' ' << placement.zone.value() << '\n';
    out << "END\n";
    return Result<std::string>::success(out.str());
}

Result<WorldSnapshot> decode_snapshot(std::string_view encoded) {
    std::istringstream in{std::string(encoded)};
    std::string marker;
    unsigned format = 0;
    if (!(in >> marker >> format) || marker != "HOME_SNAPSHOT" || format < 1 || format > kSnapshotFormatVersion) {
        return parse_error("unsupported or malformed snapshot header");
    }

    WorldSnapshot snapshot{};
    std::uint64_t world_value = 0;
    std::uint64_t revision_value = 0;
    if (!(in >> marker >> world_value >> revision_value) || marker != "WORLD" || world_value == 0) return parse_error("malformed snapshot world header");
    snapshot.world = WorldId{world_value};
    snapshot.revision = WorldRevision{revision_value};

    if (format >= 2) {
        std::uint64_t ratio = 0, time = 0, remainder = 0;
        if (!(in >> marker >> ratio >> time >> remainder) || marker != "CLOCK" || ratio == 0 || remainder >= ratio) return parse_error("malformed snapshot clock state");
        snapshot.clock_config.real_milliseconds_per_home_minute = ratio;
        snapshot.world_time = WorldTime{time};
        snapshot.clock_remainder = remainder;
    }
    if (format >= 3) {
        int year = 0;
        unsigned month = 0, day = 0;
        if (!(in >> marker >> year >> month >> day) || marker != "CALENDAR") return parse_error("malformed snapshot calendar state");
        snapshot.calendar_config.epoch = CalendarDate{year, month, day};
        if (!valid_calendar_date(snapshot.calendar_config.epoch)) return parse_error("invalid snapshot calendar epoch");
    }

    std::size_t count = 0;
    if (format >= 4) {
        if (!(in >> marker >> count) || marker != "EVENTS") return parse_error("malformed event section");
        for (std::size_t i = 0; i < count; ++i) {
            EventDefinition event{};
            std::uint64_t id = 0;
            unsigned kind = 0, month = 0, day = 0, duration = 0;
            int priority = 0;
            std::size_t affinity_count = 0;
            if (!(in >> marker >> id >> kind >> priority >> month >> day >> duration >> affinity_count >> std::quoted(event.key) >> std::quoted(event.display_name))
                || marker != "V" || id == 0 || kind > static_cast<unsigned>(EventKind::Story)) return parse_error("malformed event record");
            event.id = EventId{id};
            event.kind = static_cast<EventKind>(kind);
            event.priority = priority;
            event.rule = AnnualDateRule{month, day, duration};
            for (std::size_t a = 0; a < affinity_count; ++a) {
                std::string affinity;
                if (!(in >> std::quoted(affinity))) return parse_error("malformed event affinity");
                event.affinities.push_back(std::move(affinity));
            }
            snapshot.events.push_back(std::move(event));
        }
    }

    if (format >= 5) {
        if (!(in >> marker >> count) || marker != "CLIMATES") return parse_error("malformed climate section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, seed = 0;
            int temperature = 0;
            unsigned wetness = 0, wind = 0;
            if (!(in >> marker >> zone >> temperature >> wetness >> wind >> seed) || marker != "K" || zone == 0) return parse_error("malformed climate record");
            snapshot.climates.push_back(ClimateProfile{ZoneId{zone}, temperature, wetness, wind, seed});
        }

        if (!(in >> marker >> count) || marker != "WEATHER") return parse_error("malformed weather section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t zone = 0, sequence = 0;
            unsigned intensity = 0, previous = 0, cloud = 0, precipitation = 0, wind = 0, age = 0;
            int temperature = 0;
            if (!(in >> marker >> zone >> intensity >> previous >> temperature >> cloud >> precipitation >> wind >> sequence >> age)
                || marker != "W" || zone == 0 || intensity > static_cast<unsigned>(RainIntensity::Deluge) || previous > static_cast<unsigned>(RainIntensity::Deluge)) return parse_error("malformed weather record");
            snapshot.weather.push_back(WeatherState{ZoneId{zone}, static_cast<RainIntensity>(intensity), static_cast<RainIntensity>(previous), temperature, cloud, precipitation, wind, sequence, age});
        }
    }

    if (format >= 6) {
        unsigned tone = 0, affect_intensity = 0, affect_stability = 0;
        int valence = 0;
        if (!(in >> marker >> tone >> valence >> affect_intensity >> affect_stability) || marker != "AFFECT" || tone > static_cast<unsigned>(WorldTone::Dreamlike)) return parse_error("malformed world affect state");
        snapshot.affect = WorldAffectState{static_cast<WorldTone>(tone), valence, affect_intensity, affect_stability};

        unsigned anchor = 0, strength = 0, source = 0;
        std::string authority;
        if (!(in >> marker >> anchor >> strength >> source >> std::quoted(authority)) || marker != "ANCHOR"
            || anchor > static_cast<unsigned>(ThemeAnchor::HauntedHalloweenRain) || source > static_cast<unsigned>(AnchorSource::AuthorizedOverride)) return parse_error("malformed world anchor state");
        snapshot.anchor = WorldAnchorState{static_cast<ThemeAnchor>(anchor), strength, static_cast<AnchorSource>(source), std::move(authority)};
    }

    if (format >= 7) {
        if (!(in >> marker >> count) || marker != "LIFE") return parse_error("malformed player life section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t entity = 0, home_zone = 0, born = 0, updated = 0, sequence = 0;
            unsigned stage = 0, presence = 0;
            if (!(in >> marker >> entity >> stage >> presence >> home_zone >> born >> updated >> sequence) || marker != "L" || entity == 0
                || stage > static_cast<unsigned>(LifeStage::Elder) || presence > static_cast<unsigned>(LifePresence::Deceased) || sequence == 0 || updated < born) return parse_error("malformed player life record");
            PlayerLifeState life{};
            life.entity = EntityId{entity};
            life.stage = static_cast<LifeStage>(stage);
            life.presence = static_cast<LifePresence>(presence);
            if (home_zone != 0) life.home_zone = ZoneId{home_zone};
            life.born_world_minute = born;
            life.updated_world_minute = updated;
            life.sequence = sequence;
            snapshot.player_life.push_back(life);
        }
    }

    if (format >= 8) {
        if (!(in >> marker >> count) || marker != "DYNAMICS") return parse_error("malformed player dynamics section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t entity = 0, updated = 0, sequence = 0;
            unsigned band = 0, mode = 0, initiative = 0;
            int may_change_zone = 0, may_interact = 0;
            PlayerDynamicsState dynamics{};
            if (!(in >> marker >> entity) || marker != "D" || entity == 0) return parse_error("malformed player dynamics record");
            for (auto& value : dynamics.needs.values) if (!(in >> value)) return parse_error("malformed player dynamics needs");
            if (!(in >> dynamics.mood.valence >> dynamics.mood.arousal >> band >> mode >> initiative >> may_change_zone >> may_interact >> updated >> sequence >> std::quoted(dynamics.active_drive))
                || band > static_cast<unsigned>(MoodBand::Elevated) || mode > static_cast<unsigned>(AutonomyMode::Bounded)
                || (may_change_zone != 0 && may_change_zone != 1) || (may_interact != 0 && may_interact != 1)) return parse_error("malformed player dynamics record");
            dynamics.entity = EntityId{entity};
            dynamics.mood.band = static_cast<MoodBand>(band);
            dynamics.autonomy.mode = static_cast<AutonomyMode>(mode);
            dynamics.autonomy.initiative_limit_per_hour = initiative;
            dynamics.autonomy.may_change_zone = may_change_zone != 0;
            dynamics.autonomy.may_interact_with_entities = may_interact != 0;
            dynamics.updated_world_minute = updated;
            dynamics.sequence = sequence;
            snapshot.player_dynamics.push_back(std::move(dynamics));
        }
    }

    if (format >= 9) {
        if (!(in >> marker >> count) || marker != "RELATIONSHIPS") return parse_error("malformed relationships section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t from = 0, to = 0, updated = 0, sequence = 0;
            unsigned kind = 0;
            std::int32_t affinity = 0, trust = 0;
            if (!(in >> marker >> from >> to >> kind >> affinity >> trust >> updated >> sequence) || marker != "R" || from == 0 || to == 0 || from == to
                || kind > static_cast<unsigned>(RelationshipKind::Dependent) || sequence == 0) return parse_error("malformed relationship record");
            snapshot.relationships.push_back(RelationshipState{EntityId{from}, EntityId{to}, static_cast<RelationshipKind>(kind), affinity, trust, updated, sequence});
        }

        if (!(in >> marker >> count) || marker != "HOUSEHOLDS") return parse_error("malformed households section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t id = 0, home_zone = 0, updated = 0, sequence = 0;
            std::size_t member_count = 0;
            HouseholdState household{};
            if (!(in >> marker >> id >> home_zone >> updated >> sequence >> member_count >> std::quoted(household.name)) || marker != "H" || id == 0 || sequence == 0 || member_count == 0) return parse_error("malformed household record");
            household.id = HouseholdId{id};
            if (home_zone != 0) household.home_zone = ZoneId{home_zone};
            household.updated_world_minute = updated;
            household.sequence = sequence;
            for (std::size_t m = 0; m < member_count; ++m) {
                std::uint64_t member = 0;
                if (!(in >> member) || member == 0) return parse_error("malformed household member");
                household.members.push_back(EntityId{member});
            }
            snapshot.households.push_back(std::move(household));
        }
    }

    if (format >= 10) {
        if (!(in >> marker >> count) || marker != "ITEMS") return parse_error("malformed items section");
        for (std::size_t i = 0; i < count; ++i) {
            std::uint64_t id = 0, owner = 0, zone = 0, updated = 0, sequence = 0;
            unsigned kind = 0, quantity = 0, max_stack = 0, durability = 0;
            ItemState item{};
            if (!(in >> marker >> id >> kind >> quantity >> max_stack >> durability >> owner >> zone >> updated >> sequence >> std::quoted(item.archetype_key) >> std::quoted(item.display_name))
                || marker != "I" || id == 0 || kind > static_cast<unsigned>(ItemKind::Material)) return parse_error("malformed item record");
            item.id = ItemId{id};
            item.kind = static_cast<ItemKind>(kind);
            item.quantity = quantity;
            item.max_stack = max_stack;
            item.durability = durability;
            if (owner != 0) item.owner = EntityId{owner};
            if (zone != 0) item.zone = ZoneId{zone};
            item.updated_world_minute = updated;
            item.sequence = sequence;
            snapshot.items.push_back(std::move(item));
        }
    }

    if (format >= 11) {
        if (!(in >> marker >> count) || marker != "SKILLS") return parse_error("malformed skills section");
        for (std::size_t i = 0; i < count; ++i) {
            SkillState skill{};
            std::uint64_t player = 0;
            if (!(in >> marker >> player >> skill.level >> skill.experience >> skill.updated_world_minute >> skill.sequence >> std::quoted(skill.key))
                || marker != "S" || player == 0) return parse_error("malformed skill record");
            skill.player = EntityId{player};
            snapshot.skills.push_back(std::move(skill));
        }

        if (!(in >> marker >> count) || marker != "TASKS") return parse_error("malformed tasks section");
        for (std::size_t i = 0; i < count; ++i) {
            TaskState task{};
            std::uint64_t id = 0, owner = 0;
            unsigned status = 0;
            if (!(in >> marker >> id >> owner >> status >> task.progress >> task.target >> task.updated_world_minute >> task.sequence >> std::quoted(task.key) >> std::quoted(task.title))
                || marker != "T" || id == 0 || owner == 0 || status > static_cast<unsigned>(TaskStatus::Cancelled)) return parse_error("malformed task record");
            task.id = TaskId{id};
            task.owner = EntityId{owner};
            task.status = static_cast<TaskStatus>(status);
            snapshot.tasks.push_back(std::move(task));
        }

        if (!(in >> marker >> count) || marker != "QUESTS") return parse_error("malformed quests section");
        for (std::size_t i = 0; i < count; ++i) {
            QuestState quest{};
            std::uint64_t id = 0, owner = 0;
            unsigned status = 0;
            std::size_t task_count = 0, prerequisite_count = 0;
            if (!(in >> marker >> id >> owner >> status >> quest.updated_world_minute >> quest.sequence >> task_count >> prerequisite_count >> std::quoted(quest.key) >> std::quoted(quest.title))
                || marker != "Q" || id == 0 || owner == 0 || status > static_cast<unsigned>(QuestStatus::Cancelled)) return parse_error("malformed quest record");
            quest.id = QuestId{id};
            quest.owner = EntityId{owner};
            quest.status = static_cast<QuestStatus>(status);
            for (std::size_t t = 0; t < task_count; ++t) {
                std::uint64_t task = 0;
                if (!(in >> task) || task == 0) return parse_error("malformed quest task reference");
                quest.tasks.push_back(TaskId{task});
            }
            for (std::size_t p = 0; p < prerequisite_count; ++p) {
                std::uint64_t prerequisite = 0;
                if (!(in >> prerequisite) || prerequisite == 0) return parse_error("malformed quest prerequisite reference");
                quest.prerequisites.push_back(QuestId{prerequisite});
            }
            snapshot.quests.push_back(std::move(quest));
        }
    }

    if (format >= 12) {
        if (!(in >> marker >> count) || marker != "SUBCLASSES") return parse_error("malformed subclasses section");
        for (std::size_t i = 0; i < count; ++i) {
            SubclassAffinityState subclass{};
            std::uint64_t player = 0;
            if (!(in >> marker >> player >> subclass.evidence_points >> subclass.affinity_permille
                  >> subclass.discovered_world_minute >> subclass.updated_world_minute >> subclass.sequence >> std::quoted(subclass.key))
                || marker != "B" || player == 0) return parse_error("malformed subclass affinity record");
            subclass.player = EntityId{player};
            snapshot.subclasses.push_back(std::move(subclass));
        }

        if (!(in >> marker >> count) || marker != "AURAS") return parse_error("malformed auras section");
        for (std::size_t i = 0; i < count; ++i) {
            AuraState aura{};
            std::uint64_t player = 0;
            if (!(in >> marker >> player >> aura.intensity_permille >> aura.charge_milli >> aura.coherence_permille
                  >> aura.updated_world_minute >> aura.sequence >> std::quoted(aura.dominant_affinity) >> std::quoted(aura.signature))
                || marker != "U" || player == 0) return parse_error("malformed aura record");
            aura.player = EntityId{player};
            snapshot.auras.push_back(std::move(aura));
        }
    }

    if (!(in >> marker >> count) || marker != "ENTITIES") return parse_error("malformed entity section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0;
        unsigned kind = 0;
        int persistent = 0;
        EntityRecord entity{};
        if (!(in >> marker >> id >> kind >> persistent >> std::quoted(entity.archetype) >> std::quoted(entity.display_name)
              >> entity.transform.position.x >> entity.transform.position.y >> entity.transform.position.z
              >> entity.transform.rotation.pitch >> entity.transform.rotation.yaw >> entity.transform.rotation.roll)
            || marker != "E" || id == 0 || kind > static_cast<unsigned>(EntityKind::Environment)) return parse_error("malformed entity record");
        entity.id = EntityId{id};
        entity.world = snapshot.world;
        entity.kind = static_cast<EntityKind>(kind);
        entity.persistent = persistent != 0;
        snapshot.entities.push_back(std::move(entity));
    }

    if (!(in >> marker >> count) || marker != "ZONES") return parse_error("malformed zone section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0, parent = 0;
        unsigned kind = 0;
        int persistent = 0;
        ZoneRecord zone{};
        if (!(in >> marker >> id >> kind >> persistent >> parent >> std::quoted(zone.key) >> std::quoted(zone.display_name))
            || marker != "Z" || id == 0 || kind > static_cast<unsigned>(ZoneKind::Restricted)) return parse_error("malformed zone record");
        zone.id = ZoneId{id};
        zone.world = snapshot.world;
        zone.kind = static_cast<ZoneKind>(kind);
        zone.persistent = persistent != 0;
        if (parent != 0) zone.parent = ZoneId{parent};
        snapshot.zones.push_back(std::move(zone));
    }

    if (!(in >> marker >> count) || marker != "CONNECTIONS") return parse_error("malformed connection section");
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t from = 0, to = 0;
        int bidirectional = 0, traversable = 0;
        std::string tag;
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

    const auto valid = validate_common_snapshot(snapshot);
    if (!valid) return Result<WorldSnapshot>::failure(valid.error().code, valid.error().message);
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
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) return Result<WorldSnapshot>::failure(ErrorCode::SerializationError, "snapshot read failed");
    return decode_snapshot(buffer.str());
}

} // namespace home
