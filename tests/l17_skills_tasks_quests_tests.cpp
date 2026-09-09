#include "home/versioned_world.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>

namespace {

home::PlayerLifeState life_for(home::EntityId entity, std::uint64_t minute) {
    home::PlayerLifeState state{};
    state.entity = entity;
    state.stage = home::LifeStage::Adult;
    state.presence = home::LifePresence::Present;
    state.born_world_minute = 0;
    state.updated_world_minute = minute;
    state.sequence = 1;
    return state;
}

} // namespace

int main() {
    using namespace home;

    VersionedWorld world{WorldId{17}};
    assert(world.advance_time(30'000).ok()); // HOME minute 1
    const auto player = world.create_entity(EntityCreateInfo{EntityKind::Avatar, "runner", "Runner"});
    assert(player.ok());
    assert(world.set_player_life_state(life_for(player.value(), 1)).ok());

    // Skill level is derived from XP; contradictory levels never become canonical.
    SkillState skating{};
    skating.player = player.value();
    skating.key = "skating";
    skating.level = 1;
    skating.experience = 0;
    skating.updated_world_minute = 1;
    skating.sequence = 1;
    const WorldRevision before_skill = world.revision();
    assert(world.set_skill_state(skating).ok());
    assert(world.revision() == before_skill.next().value());
    assert(world.progression().find_skill(player.value(), "skating") != nullptr);

    SkillState contradictory = skating;
    contradictory.sequence = 2;
    contradictory.experience = 1'000;
    contradictory.level = 1;
    assert(!world.set_skill_state(contradictory).ok());

    SkillState progressed = skating;
    progressed.sequence = 2;
    progressed.experience = 2'500;
    progressed.level = 3;
    assert(world.set_skill_state(progressed).ok());
    assert(world.progression().find_skill(player.value(), "skating")->level == 3);

    SkillState regressive = progressed;
    regressive.sequence = 3;
    regressive.experience = 1'500;
    regressive.level = 2;
    assert(!world.set_skill_state(regressive).ok());

    SkillState future_skill = progressed;
    future_skill.sequence = 3;
    future_skill.experience = 3'000;
    future_skill.level = 4;
    future_skill.updated_world_minute = 2;
    assert(!world.set_skill_state(future_skill).ok());

    // Task completion requires target progress and progression never regresses.
    TaskCreateInfo task_info{};
    task_info.owner = player.value();
    task_info.key = "land_three_tricks";
    task_info.title = "Land Three Tricks";
    task_info.target = 3;
    task_info.updated_world_minute = 1;
    const auto task1 = world.create_task(task_info);
    assert(task1.ok());

    TaskState premature = *world.progression().find_task(task1.value());
    premature.status = TaskStatus::Complete;
    premature.progress = 2;
    premature.sequence = 2;
    assert(!world.set_task_state(premature).ok());

    TaskState active = *world.progression().find_task(task1.value());
    active.status = TaskStatus::Active;
    active.progress = 1;
    active.sequence = 2;
    assert(world.set_task_state(active).ok());

    TaskState complete1 = *world.progression().find_task(task1.value());
    complete1.status = TaskStatus::Complete;
    complete1.progress = 3;
    complete1.sequence = 3;
    assert(world.set_task_state(complete1).ok());

    // Quest 1 may complete only because all of its tasks are complete.
    QuestCreateInfo q1_info{};
    q1_info.owner = player.value();
    q1_info.key = "first_session";
    q1_info.title = "First Session";
    q1_info.tasks = {task1.value()};
    q1_info.updated_world_minute = 1;
    const auto quest1 = world.create_quest(q1_info);
    assert(quest1.ok());
    QuestState q1_complete = *world.progression().find_quest(quest1.value());
    q1_complete.status = QuestStatus::Complete;
    q1_complete.sequence = 2;
    assert(world.set_quest_state(q1_complete).ok());

    // Quest 2 depends on completed quest 1 and its own task.
    TaskCreateInfo task2_info{};
    task2_info.owner = player.value();
    task2_info.key = "visit_crown_point";
    task2_info.title = "Visit Crown Point";
    task2_info.target = 1;
    task2_info.updated_world_minute = 1;
    const auto task2 = world.create_task(task2_info);
    assert(task2.ok());

    QuestCreateInfo q2_info{};
    q2_info.owner = player.value();
    q2_info.key = "bay_route";
    q2_info.title = "Bay Route";
    q2_info.tasks = {task2.value()};
    q2_info.prerequisites = {quest1.value()};
    q2_info.updated_world_minute = 1;
    const auto quest2 = world.create_quest(q2_info);
    assert(quest2.ok());

    QuestState q2_active = *world.progression().find_quest(quest2.value());
    q2_active.status = QuestStatus::Active;
    q2_active.sequence = 2;
    assert(world.set_quest_state(q2_active).ok());

    QuestState q2_premature = *world.progression().find_quest(quest2.value());
    q2_premature.status = QuestStatus::Complete;
    q2_premature.sequence = 3;
    assert(!world.set_quest_state(q2_premature).ok());

    TaskState complete2 = *world.progression().find_task(task2.value());
    complete2.status = TaskStatus::Complete;
    complete2.progress = 1;
    complete2.sequence = 2;
    assert(world.set_task_state(complete2).ok());

    QuestState q2_complete = *world.progression().find_quest(quest2.value());
    q2_complete.status = QuestStatus::Complete;
    q2_complete.sequence = 3;
    assert(world.set_quest_state(q2_complete).ok());

    // Stable-ID ordering makes the prerequisite graph acyclic.
    QuestState cyclic = *world.progression().find_quest(quest1.value());
    cyclic.prerequisites = {quest2.value()};
    cyclic.sequence = 3;
    assert(!world.set_quest_state(cyclic).ok());

    QuestCreateInfo missing_prerequisite{};
    missing_prerequisite.owner = player.value();
    missing_prerequisite.key = "ghost_quest";
    missing_prerequisite.title = "Ghost Quest";
    missing_prerequisite.prerequisites = {QuestId{999999}};
    missing_prerequisite.updated_world_minute = 1;
    assert(!world.create_quest(missing_prerequisite).ok());

    // Progression changes are represented in world deltas.
    const auto& last_change = world.history().back().changes();
    assert(std::find_if(last_change.begin(), last_change.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::QuestStateChanged;
    }) != last_change.end());

    // Snapshot v11 round-trips exact progression and preserves canonical revision without synthetic history.
    const WorldSnapshot saved = world.snapshot();
    assert(saved.skills.size() == 1);
    assert(saved.tasks.size() == 2);
    assert(saved.quests.size() == 2);
    const auto encoded = encode_snapshot(saved);
    assert(encoded.ok());
    assert(encoded.value().rfind("HOME_SNAPSHOT ", 0) == 0);
    assert(encoded.value().find("SKILLS 1") != std::string::npos);
    assert(encoded.value().find("TASKS 2") != std::string::npos);
    assert(encoded.value().find("QUESTS 2") != std::string::npos);
    const auto decoded = decode_snapshot(encoded.value());
    assert(decoded.ok());
    assert(decoded.value().skills == saved.skills);
    assert(decoded.value().tasks == saved.tasks);
    assert(decoded.value().quests == saved.quests);
    const auto restored_result = VersionedWorld::from_snapshot(decoded.value());
    assert(restored_result.ok());
    VersionedWorld restored = std::move(restored_result.value());
    assert(restored.revision() == world.revision());
    assert(restored.history().empty());
    assert(restored.progression().find_skill(player.value(), "skating") != nullptr);
    assert(restored.progression().find_quest(quest2.value())->status == QuestStatus::Complete);

    // Restored allocators continue beyond persisted task and quest ids.
    TaskCreateInfo post_task{};
    post_task.owner = player.value();
    post_task.key = "post_restore_task";
    post_task.title = "Post Restore Task";
    post_task.target = 2;
    post_task.updated_world_minute = 1;
    const auto next_task = restored.create_task(post_task);
    assert(next_task.ok());
    assert(next_task.value().value() > task2.value().value());

    QuestCreateInfo post_quest{};
    post_quest.owner = player.value();
    post_quest.key = "post_restore_quest";
    post_quest.title = "Post Restore Quest";
    post_quest.updated_world_minute = 1;
    const auto next_quest = restored.create_quest(post_quest);
    assert(next_quest.ok());
    assert(next_quest.value().value() > quest2.value().value());

    // v10 remains readable and simply has no L17 progression sections.
    std::string legacy = encoded.value();
    const auto progression_start = legacy.find("SKILLS ");
    const auto entities_start = legacy.find("ENTITIES ", progression_start);
    assert(progression_start != std::string::npos && entities_start != std::string::npos);
    legacy.erase(progression_start, entities_start - progression_start);
    legacy.replace(0, std::string("HOME_SNAPSHOT 11").size(), "HOME_SNAPSHOT 10");
    const auto legacy_decoded = decode_snapshot(legacy);
    assert(legacy_decoded.ok());
    assert(legacy_decoded.value().skills.empty());
    assert(legacy_decoded.value().tasks.empty());
    assert(legacy_decoded.value().quests.empty());

    // Orphan progression cannot be serialized.
    WorldSnapshot orphan = world.snapshot();
    SkillState orphan_skill{};
    orphan_skill.player = EntityId{999999};
    orphan_skill.key = "orphan";
    orphan_skill.level = 1;
    orphan_skill.sequence = 1;
    orphan_skill.updated_world_minute = 1;
    orphan.skills.push_back(orphan_skill);
    assert(!encode_snapshot(orphan).ok());

    // Direct deletion purges all progression for the avatar atomically.
    const WorldRevision before_delete = world.revision();
    assert(world.remove_entity(player.value()).ok());
    assert(world.progression().skills_for(player.value()).empty());
    assert(world.progression().tasks_for(player.value()).empty());
    assert(world.progression().quests_for(player.value()).empty());
    assert(world.revision() == before_delete.next().value());
    const auto& delete_changes = world.history().back().changes();
    assert(std::find_if(delete_changes.begin(), delete_changes.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::SkillStateChanged;
    }) != delete_changes.end());
    assert(std::find_if(delete_changes.begin(), delete_changes.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::TaskStateChanged;
    }) != delete_changes.end());
    assert(std::find_if(delete_changes.begin(), delete_changes.end(), [](const WorldChange& change) {
        return change.kind == WorldChangeKind::QuestStateChanged;
    }) != delete_changes.end());

    // Transactional deletion obeys the same cleanup invariant.
    VersionedWorld tx_world{WorldId{1702}};
    assert(tx_world.advance_time(30'000).ok());
    const auto tx_player = tx_world.create_entity(EntityCreateInfo{EntityKind::Avatar, "tx", "Tx"});
    assert(tx_player.ok());
    assert(tx_world.set_player_life_state(life_for(tx_player.value(), 1)).ok());
    SkillState tx_skill{};
    tx_skill.player = tx_player.value();
    tx_skill.key = "walking";
    tx_skill.level = 1;
    tx_skill.updated_world_minute = 1;
    tx_skill.sequence = 1;
    assert(tx_world.set_skill_state(tx_skill).ok());
    TaskCreateInfo tx_task_info{};
    tx_task_info.owner = tx_player.value();
    tx_task_info.key = "walk_once";
    tx_task_info.title = "Walk Once";
    tx_task_info.updated_world_minute = 1;
    const auto tx_task = tx_world.create_task(tx_task_info);
    assert(tx_task.ok());
    QuestCreateInfo tx_quest_info{};
    tx_quest_info.owner = tx_player.value();
    tx_quest_info.key = "tx_quest";
    tx_quest_info.title = "Tx Quest";
    tx_quest_info.tasks = {tx_task.value()};
    tx_quest_info.updated_world_minute = 1;
    assert(tx_world.create_quest(tx_quest_info).ok());

    WorldTransaction transaction{};
    transaction.id = WorldTransactionId{1702};
    transaction.expected_revision = tx_world.revision();
    transaction.authority = "home.l17.test";
    transaction.operations.push_back(TxRemoveEntity{tx_player.value()});
    assert(tx_world.execute(transaction).ok());
    assert(tx_world.progression().skills_for(tx_player.value()).empty());
    assert(tx_world.progression().tasks_for(tx_player.value()).empty());
    assert(tx_world.progression().quests_for(tx_player.value()).empty());

    return 0;
}
