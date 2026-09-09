#include "home/progression.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_player(const EntityRegistry& entities, const PlayerLifeLedger& life, EntityId player) {
    const auto* entity = entities.find(player);
    return entity != nullptr && entity->kind == EntityKind::Avatar && life.find(player) != nullptr;
}

template <typename T>
void sort_unique_ids(std::vector<T>& ids) {
    std::sort(ids.begin(), ids.end(), [](T a, T b) { return a.value() < b.value(); });
}

} // namespace

std::uint32_t skill_level_for_experience(std::uint64_t experience) noexcept {
    const std::uint64_t raw = 1ULL + (experience / kExperiencePerSkillLevel);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(raw, kMaxSkillLevel));
}

Result<void> validate_skill_shape(const SkillState& state) {
    if (!state.player.valid() || state.key.empty() || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "skill state is malformed");
    }
    if (state.level != skill_level_for_experience(state.experience)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "skill level does not match deterministic experience level");
    }
    return Result<void>::success();
}

Result<void> validate_task_shape(const TaskState& state) {
    if (!state.id.valid() || !state.owner.valid() || state.key.empty() || state.title.empty() || state.target == 0 || state.progress > state.target || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "task state is malformed");
    }
    if (static_cast<unsigned>(state.status) > static_cast<unsigned>(TaskStatus::Cancelled)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "task status is invalid");
    }
    if (state.status == TaskStatus::Complete && state.progress != state.target) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "complete task must reach target");
    }
    if ((state.status == TaskStatus::Pending || state.status == TaskStatus::Active) && state.progress == state.target) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "unfinished task cannot already satisfy target");
    }
    return Result<void>::success();
}

Result<void> validate_quest_shape(const QuestState& state) {
    if (!state.id.valid() || !state.owner.valid() || state.key.empty() || state.title.empty() || state.sequence == 0) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "quest state is malformed");
    }
    if (static_cast<unsigned>(state.status) > static_cast<unsigned>(QuestStatus::Cancelled)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "quest status is invalid");
    }
    auto tasks = state.tasks;
    auto prereqs = state.prerequisites;
    sort_unique_ids(tasks);
    sort_unique_ids(prereqs);
    if (std::adjacent_find(tasks.begin(), tasks.end()) != tasks.end()) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "quest contains duplicate task ids");
    }
    if (std::adjacent_find(prereqs.begin(), prereqs.end()) != prereqs.end()) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "quest contains duplicate prerequisite ids");
    }
    if (std::find(prereqs.begin(), prereqs.end(), state.id) != prereqs.end()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "quest cannot require itself");
    }
    return Result<void>::success();
}

const SkillState* ProgressionLedger::find_skill(EntityId player, std::string_view key) const noexcept {
    const auto it = std::find_if(skills_.begin(), skills_.end(), [&](const SkillState& s) { return s.player == player && s.key == key; });
    return it == skills_.end() ? nullptr : &*it;
}

const TaskState* ProgressionLedger::find_task(TaskId id) const noexcept {
    const auto it = std::lower_bound(tasks_.begin(), tasks_.end(), id, [](const TaskState& s, TaskId needle) { return s.id.value() < needle.value(); });
    return it == tasks_.end() || it->id != id ? nullptr : &*it;
}

const QuestState* ProgressionLedger::find_quest(QuestId id) const noexcept {
    const auto it = std::lower_bound(quests_.begin(), quests_.end(), id, [](const QuestState& s, QuestId needle) { return s.id.value() < needle.value(); });
    return it == quests_.end() || it->id != id ? nullptr : &*it;
}

std::vector<SkillState> ProgressionLedger::skills_for(EntityId player) const {
    std::vector<SkillState> out;
    for (const auto& s : skills_) if (s.player == player) out.push_back(s);
    return out;
}

std::vector<TaskState> ProgressionLedger::tasks_for(EntityId player) const {
    std::vector<TaskState> out;
    for (const auto& s : tasks_) if (s.owner == player) out.push_back(s);
    return out;
}

std::vector<QuestState> ProgressionLedger::quests_for(EntityId player) const {
    std::vector<QuestState> out;
    for (const auto& s : quests_) if (s.owner == player) out.push_back(s);
    return out;
}

Result<TaskId> ProgressionLedger::allocate_task_id() {
    if (next_task_value_ == 0) return Result<TaskId>::failure(ErrorCode::Overflow, "task id space exhausted");
    const TaskId id{next_task_value_};
    if (next_task_value_ == std::numeric_limits<std::uint64_t>::max()) next_task_value_ = 0;
    else ++next_task_value_;
    return Result<TaskId>::success(id);
}

Result<QuestId> ProgressionLedger::allocate_quest_id() {
    if (next_quest_value_ == 0) return Result<QuestId>::failure(ErrorCode::Overflow, "quest id space exhausted");
    const QuestId id{next_quest_value_};
    if (next_quest_value_ == std::numeric_limits<std::uint64_t>::max()) next_quest_value_ = 0;
    else ++next_quest_value_;
    return Result<QuestId>::success(id);
}

void ProgressionLedger::advance_task_allocator_past(TaskId id) noexcept {
    if (id.value() == std::numeric_limits<std::uint64_t>::max()) next_task_value_ = 0;
    else if (next_task_value_ != 0 && id.value() >= next_task_value_) next_task_value_ = id.value() + 1;
}

void ProgressionLedger::advance_quest_allocator_past(QuestId id) noexcept {
    if (id.value() == std::numeric_limits<std::uint64_t>::max()) next_quest_value_ = 0;
    else if (next_quest_value_ != 0 && id.value() >= next_quest_value_) next_quest_value_ = id.value() + 1;
}

Result<void> ProgressionLedger::set_skill(const EntityRegistry& entities, const PlayerLifeLedger& life, SkillState state) {
    const auto valid = validate_skill_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) return Result<void>::failure(ErrorCode::ValidationFailed, "skill owner requires canonical avatar life state");
    auto it = std::find_if(skills_.begin(), skills_.end(), [&](const SkillState& s) { return s.player == state.player && s.key == state.key; });
    if (it == skills_.end()) {
        if (state.sequence != 1) return Result<void>::failure(ErrorCode::ValidationFailed, "new skill must begin at sequence 1");
        skills_.push_back(std::move(state));
        std::sort(skills_.begin(), skills_.end(), [](const SkillState& a, const SkillState& b) { return a.player.value() != b.player.value() ? a.player.value() < b.player.value() : a.key < b.key; });
        return Result<void>::success();
    }
    if (state.sequence != it->sequence + 1 || state.updated_world_minute < it->updated_world_minute || state.experience < it->experience) {
        return Result<void>::failure(ErrorCode::RevisionConflict, "skill update is stale or regressive");
    }
    if (*it == state) return Result<void>::failure(ErrorCode::ValidationFailed, "skill update produced no state change");
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> ProgressionLedger::restore_skill(const EntityRegistry& entities, const PlayerLifeLedger& life, SkillState state) {
    const auto valid = validate_skill_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.player)) return Result<void>::failure(ErrorCode::ValidationFailed, "restored skill owner is invalid");
    if (find_skill(state.player, state.key)) return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate restored skill");
    skills_.push_back(std::move(state));
    std::sort(skills_.begin(), skills_.end(), [](const SkillState& a, const SkillState& b) { return a.player.value() != b.player.value() ? a.player.value() < b.player.value() : a.key < b.key; });
    return Result<void>::success();
}

Result<TaskId> ProgressionLedger::create_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskCreateInfo info) {
    if (!valid_player(entities, life, info.owner) || info.key.empty() || info.title.empty() || info.target == 0) return Result<TaskId>::failure(ErrorCode::ValidationFailed, "task create info is invalid");
    if (std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskState& t){ return t.owner == info.owner && t.key == info.key; })) return Result<TaskId>::failure(ErrorCode::AlreadyExists, "player already has task key");
    const auto id = allocate_task_id(); if (!id) return id;
    TaskState state{id.value(), info.owner, std::move(info.key), std::move(info.title), TaskStatus::Pending, 0, info.target, info.updated_world_minute, 1};
    tasks_.push_back(std::move(state));
    return id;
}

Result<void> ProgressionLedger::validate_task_references(const TaskState& state) const {
    for (const auto& q : quests_) {
        if (std::find(q.tasks.begin(), q.tasks.end(), state.id) != q.tasks.end() && q.owner != state.owner) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "task owner cannot diverge from referencing quest");
        }
    }
    return Result<void>::success();
}

Result<void> ProgressionLedger::set_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskState state) {
    const auto valid = validate_task_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.owner)) return Result<void>::failure(ErrorCode::ValidationFailed, "task owner requires canonical avatar life state");
    auto it = std::lower_bound(tasks_.begin(), tasks_.end(), state.id, [](const TaskState& s, TaskId id){ return s.id.value() < id.value(); });
    if (it == tasks_.end() || it->id != state.id) return Result<void>::failure(ErrorCode::NotFound, "task not found");
    if (state.owner != it->owner || state.key != it->key || state.target != it->target) return Result<void>::failure(ErrorCode::ValidationFailed, "task identity fields are immutable");
    if (state.sequence != it->sequence + 1 || state.updated_world_minute < it->updated_world_minute || state.progress < it->progress) return Result<void>::failure(ErrorCode::RevisionConflict, "task update is stale or regressive");
    if (it->status == TaskStatus::Complete && state.status != TaskStatus::Complete) return Result<void>::failure(ErrorCode::ValidationFailed, "complete task is terminal");
    const auto refs = validate_task_references(state); if (!refs) return refs;
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> ProgressionLedger::restore_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskState state) {
    const auto valid = validate_task_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.owner)) return Result<void>::failure(ErrorCode::ValidationFailed, "restored task owner is invalid");
    if (find_task(state.id) || std::any_of(tasks_.begin(), tasks_.end(), [&](const TaskState& t){ return t.owner == state.owner && t.key == state.key; })) return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate restored task");
    tasks_.push_back(std::move(state));
    std::sort(tasks_.begin(), tasks_.end(), [](const TaskState& a, const TaskState& b){ return a.id.value() < b.id.value(); });
    advance_task_allocator_past(tasks_.back().id);
    return Result<void>::success();
}

Result<void> ProgressionLedger::validate_quest_references(const QuestState& state) const {
    for (const TaskId task_id : state.tasks) {
        const auto* task = find_task(task_id);
        if (!task || task->owner != state.owner) return Result<void>::failure(ErrorCode::ValidationFailed, "quest task is missing or belongs to another player");
    }
    for (const QuestId prerequisite : state.prerequisites) {
        const auto* quest = find_quest(prerequisite);
        if (!quest || quest->owner != state.owner || quest->status != QuestStatus::Complete) {
            if (state.status == QuestStatus::Active || state.status == QuestStatus::Complete) return Result<void>::failure(ErrorCode::ValidationFailed, "active or complete quest requires completed prerequisites");
        }
    }
    if (state.status == QuestStatus::Complete) {
        for (const TaskId task_id : state.tasks) {
            const auto* task = find_task(task_id);
            if (!task || task->status != TaskStatus::Complete) return Result<void>::failure(ErrorCode::ValidationFailed, "complete quest requires all tasks complete");
        }
    }
    return Result<void>::success();
}

Result<QuestId> ProgressionLedger::create_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestCreateInfo info) {
    if (!valid_player(entities, life, info.owner) || info.key.empty() || info.title.empty()) return Result<QuestId>::failure(ErrorCode::ValidationFailed, "quest create info is invalid");
    if (std::any_of(quests_.begin(), quests_.end(), [&](const QuestState& q){ return q.owner == info.owner && q.key == info.key; })) return Result<QuestId>::failure(ErrorCode::AlreadyExists, "player already has quest key");
    sort_unique_ids(info.tasks); sort_unique_ids(info.prerequisites);
    if (std::adjacent_find(info.tasks.begin(), info.tasks.end()) != info.tasks.end() || std::adjacent_find(info.prerequisites.begin(), info.prerequisites.end()) != info.prerequisites.end()) return Result<QuestId>::failure(ErrorCode::AlreadyExists, "quest create info contains duplicate references");
    const auto id = allocate_quest_id(); if (!id) return id;
    QuestState state{id.value(), info.owner, std::move(info.key), std::move(info.title), QuestStatus::Pending, std::move(info.tasks), std::move(info.prerequisites), info.updated_world_minute, 1};
    const auto refs = validate_quest_references(state); if (!refs) return Result<QuestId>::failure(refs.error().code, refs.error().message);
    quests_.push_back(std::move(state));
    return id;
}

Result<void> ProgressionLedger::set_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestState state) {
    const auto valid = validate_quest_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.owner)) return Result<void>::failure(ErrorCode::ValidationFailed, "quest owner requires canonical avatar life state");
    sort_unique_ids(state.tasks); sort_unique_ids(state.prerequisites);
    auto it = std::lower_bound(quests_.begin(), quests_.end(), state.id, [](const QuestState& q, QuestId id){ return q.id.value() < id.value(); });
    if (it == quests_.end() || it->id != state.id) return Result<void>::failure(ErrorCode::NotFound, "quest not found");
    if (state.owner != it->owner || state.key != it->key) return Result<void>::failure(ErrorCode::ValidationFailed, "quest identity fields are immutable");
    if (state.sequence != it->sequence + 1 || state.updated_world_minute < it->updated_world_minute) return Result<void>::failure(ErrorCode::RevisionConflict, "quest update is stale or regressive");
    if (it->status == QuestStatus::Complete && state.status != QuestStatus::Complete) return Result<void>::failure(ErrorCode::ValidationFailed, "complete quest is terminal");
    const auto refs = validate_quest_references(state); if (!refs) return refs;
    *it = std::move(state);
    return Result<void>::success();
}

Result<void> ProgressionLedger::restore_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestState state) {
    const auto valid = validate_quest_shape(state); if (!valid) return valid;
    if (!valid_player(entities, life, state.owner)) return Result<void>::failure(ErrorCode::ValidationFailed, "restored quest owner is invalid");
    sort_unique_ids(state.tasks); sort_unique_ids(state.prerequisites);
    if (find_quest(state.id) || std::any_of(quests_.begin(), quests_.end(), [&](const QuestState& q){ return q.owner == state.owner && q.key == state.key; })) return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate restored quest");
    const auto refs = validate_quest_references(state); if (!refs && (state.status == QuestStatus::Active || state.status == QuestStatus::Complete)) return refs;
    quests_.push_back(std::move(state));
    std::sort(quests_.begin(), quests_.end(), [](const QuestState& a, const QuestState& b){ return a.id.value() < b.id.value(); });
    advance_quest_allocator_past(quests_.back().id);
    return Result<void>::success();
}

Result<ProgressionPurgeResult> ProgressionLedger::purge_player(EntityId player) {
    ProgressionPurgeResult out{};
    for (const auto& s : skills_) if (s.player == player) out.skills.push_back(s);
    for (const auto& t : tasks_) if (t.owner == player) out.tasks.push_back(t);
    for (const auto& q : quests_) if (q.owner == player) out.quests.push_back(q);
    skills_.erase(std::remove_if(skills_.begin(), skills_.end(), [&](const SkillState& s){ return s.player == player; }), skills_.end());
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(), [&](const TaskState& t){ return t.owner == player; }), tasks_.end());
    quests_.erase(std::remove_if(quests_.begin(), quests_.end(), [&](const QuestState& q){ return q.owner == player; }), quests_.end());
    return Result<ProgressionPurgeResult>::success(std::move(out));
}

} // namespace home
