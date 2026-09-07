#include "home/progression.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace home {
namespace {

bool valid_task_status(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::Pending:
        case TaskStatus::Active:
        case TaskStatus::Complete:
        case TaskStatus::Failed:
        case TaskStatus::Cancelled:
            return true;
    }
    return false;
}

bool valid_quest_status(QuestStatus status) noexcept {
    switch (status) {
        case QuestStatus::Pending:
        case QuestStatus::Active:
        case QuestStatus::Complete:
        case QuestStatus::Failed:
        case QuestStatus::Cancelled:
            return true;
    }
    return false;
}

SkillState* find_skill_mutable(std::vector<SkillState>& skills, PlayerId player, const std::string& key) noexcept {
    auto it = std::find_if(skills.begin(), skills.end(), [&](const SkillState& skill) {
        return skill.player == player && skill.key == key;
    });
    return it == skills.end() ? nullptr : &*it;
}

TaskState* find_task_mutable(std::vector<TaskState>& tasks, TaskId id) noexcept {
    auto it = std::find_if(tasks.begin(), tasks.end(), [&](const TaskState& task) { return task.id == id; });
    return it == tasks.end() ? nullptr : &*it;
}

QuestState* find_quest_mutable(std::vector<QuestState>& quests, QuestId id) noexcept {
    auto it = std::find_if(quests.begin(), quests.end(), [&](const QuestState& quest) { return quest.id == id; });
    return it == quests.end() ? nullptr : &*it;
}

std::uint64_t experience_to_next_level(std::uint32_t level) noexcept {
    return static_cast<std::uint64_t>(level) * 100u;
}

void sort_skills(std::vector<SkillState>& skills) {
    std::sort(skills.begin(), skills.end(), [](const SkillState& a, const SkillState& b) {
        if (a.player.value() != b.player.value()) return a.player.value() < b.player.value();
        return a.key < b.key;
    });
}

void sort_tasks(std::vector<TaskState>& tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const TaskState& a, const TaskState& b) {
        return a.id.value() < b.id.value();
    });
}

void sort_quests(std::vector<QuestState>& quests) {
    std::sort(quests.begin(), quests.end(), [](const QuestState& a, const QuestState& b) {
        return a.id.value() < b.id.value();
    });
}

} // namespace

Result<void> ProgressionLedger::define_skill(PlayerId player,
                                             std::string key,
                                             std::uint32_t initial_level,
                                             std::uint64_t initial_experience) {
    if (!player.valid() || key.empty()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "skill requires a valid player and non-empty key");
    }
    if (initial_level == 0 || initial_level > max_skill_level) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "skill level is outside canonical bounds");
    }
    if (initial_level < max_skill_level && initial_experience >= experience_to_next_level(initial_level)) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "initial experience must be below the next-level threshold");
    }
    if (find_skill_mutable(skills_, player, key)) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "skill already exists for player");
    }
    skills_.push_back(SkillState{player, std::move(key), initial_level, initial_experience, 0});
    sort_skills(skills_);
    return Result<void>::success();
}

Result<void> ProgressionLedger::add_skill_experience(PlayerId player, const std::string& key, std::uint64_t amount) {
    if (!player.valid() || key.empty() || amount == 0) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "skill experience mutation requires player, key and positive amount");
    }
    auto* skill = find_skill_mutable(skills_, player, key);
    if (!skill) return Result<void>::failure(ErrorCode::NotFound, "skill not found");
    if (amount > std::numeric_limits<std::uint64_t>::max() - skill->experience) {
        return Result<void>::failure(ErrorCode::Overflow, "skill experience overflow");
    }

    skill->experience += amount;
    while (skill->level < max_skill_level) {
        const auto threshold = experience_to_next_level(skill->level);
        if (skill->experience < threshold) break;
        skill->experience -= threshold;
        ++skill->level;
    }
    ++skill->skill_revision;
    return Result<void>::success();
}

Result<TaskId> ProgressionLedger::create_task(PlayerId owner,
                                              std::string key,
                                              std::string title,
                                              std::uint32_t target) {
    if (!owner.valid() || key.empty() || title.empty() || target == 0) {
        return Result<TaskId>::failure(ErrorCode::InvalidArgument, "task requires owner, key, title and positive target");
    }
    const auto duplicate = std::find_if(tasks_.begin(), tasks_.end(), [&](const TaskState& task) {
        return task.owner == owner && task.key == key;
    });
    if (duplicate != tasks_.end()) {
        return Result<TaskId>::failure(ErrorCode::AlreadyExists, "task key already exists for player");
    }

    const TaskId id{next_task_id_++};
    tasks_.push_back(TaskState{id, owner, std::move(key), std::move(title), TaskStatus::Pending, 0, target, 0});
    sort_tasks(tasks_);
    return Result<TaskId>::success(id);
}

Result<void> ProgressionLedger::set_task_status(TaskId id, TaskStatus status) {
    if (!id.valid() || !valid_task_status(status)) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "invalid task id or status");
    }
    auto* task = find_task_mutable(tasks_, id);
    if (!task) return Result<void>::failure(ErrorCode::NotFound, "task not found");
    if (task->status == status) return Result<void>::failure(ErrorCode::ValidationFailed, "task status did not change");
    if (status == TaskStatus::Complete && task->progress != task->target) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "task cannot complete before reaching target progress");
    }
    if (task->status == TaskStatus::Complete || task->status == TaskStatus::Failed || task->status == TaskStatus::Cancelled) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "terminal task status cannot be changed");
    }
    task->status = status;
    ++task->task_revision;
    return Result<void>::success();
}

Result<void> ProgressionLedger::add_task_progress(TaskId id, std::uint32_t amount) {
    if (!id.valid() || amount == 0) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "task progress requires valid id and positive amount");
    }
    auto* task = find_task_mutable(tasks_, id);
    if (!task) return Result<void>::failure(ErrorCode::NotFound, "task not found");
    if (task->status == TaskStatus::Complete || task->status == TaskStatus::Failed || task->status == TaskStatus::Cancelled) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "terminal task cannot receive progress");
    }
    if (amount > task->target - task->progress) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "task progress would exceed target");
    }
    task->progress += amount;
    task->status = task->progress == task->target ? TaskStatus::Complete : TaskStatus::Active;
    ++task->task_revision;
    return Result<void>::success();
}

Result<QuestId> ProgressionLedger::create_quest(PlayerId owner,
                                                std::string key,
                                                std::string title,
                                                std::vector<TaskId> tasks) {
    if (!owner.valid() || key.empty() || title.empty()) {
        return Result<QuestId>::failure(ErrorCode::InvalidArgument, "quest requires owner, key and title");
    }
    const auto duplicate = std::find_if(quests_.begin(), quests_.end(), [&](const QuestState& quest) {
        return quest.owner == owner && quest.key == key;
    });
    if (duplicate != quests_.end()) {
        return Result<QuestId>::failure(ErrorCode::AlreadyExists, "quest key already exists for player");
    }

    std::sort(tasks.begin(), tasks.end(), [](TaskId a, TaskId b) { return a.value() < b.value(); });
    if (std::adjacent_find(tasks.begin(), tasks.end()) != tasks.end()) {
        return Result<QuestId>::failure(ErrorCode::InvalidArgument, "quest task list contains duplicates");
    }
    for (const auto task_id : tasks) {
        const auto* task = find_task(task_id);
        if (!task) return Result<QuestId>::failure(ErrorCode::NotFound, "quest task not found");
        if (task->owner != owner) {
            return Result<QuestId>::failure(ErrorCode::ValidationFailed, "quest cannot contain a task owned by another player");
        }
    }

    const QuestId id{next_quest_id_++};
    quests_.push_back(QuestState{id, owner, std::move(key), std::move(title), QuestStatus::Pending, std::move(tasks), 0});
    sort_quests(quests_);
    return Result<QuestId>::success(id);
}

Result<void> ProgressionLedger::attach_task(QuestId quest_id, TaskId task_id) {
    if (!quest_id.valid() || !task_id.valid()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "attach requires valid quest and task ids");
    }
    auto* quest = find_quest_mutable(quests_, quest_id);
    if (!quest) return Result<void>::failure(ErrorCode::NotFound, "quest not found");
    const auto* task = find_task(task_id);
    if (!task) return Result<void>::failure(ErrorCode::NotFound, "task not found");
    if (task->owner != quest->owner) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "quest and task owners must match");
    }
    if (quest->status == QuestStatus::Complete || quest->status == QuestStatus::Failed || quest->status == QuestStatus::Cancelled) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "terminal quest cannot accept tasks");
    }
    if (std::find(quest->tasks.begin(), quest->tasks.end(), task_id) != quest->tasks.end()) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "task is already attached to quest");
    }
    quest->tasks.push_back(task_id);
    std::sort(quest->tasks.begin(), quest->tasks.end(), [](TaskId a, TaskId b) { return a.value() < b.value(); });
    ++quest->quest_revision;
    return Result<void>::success();
}

Result<void> ProgressionLedger::set_quest_status(QuestId id, QuestStatus status) {
    if (!id.valid() || !valid_quest_status(status)) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "invalid quest id or status");
    }
    auto* quest = find_quest_mutable(quests_, id);
    if (!quest) return Result<void>::failure(ErrorCode::NotFound, "quest not found");
    if (quest->status == status) return Result<void>::failure(ErrorCode::ValidationFailed, "quest status did not change");
    if (quest->status == QuestStatus::Complete || quest->status == QuestStatus::Failed || quest->status == QuestStatus::Cancelled) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "terminal quest status cannot be changed");
    }
    if (status == QuestStatus::Complete) {
        for (const auto task_id : quest->tasks) {
            const auto* task = find_task(task_id);
            if (!task || task->status != TaskStatus::Complete) {
                return Result<void>::failure(ErrorCode::ValidationFailed, "quest cannot complete while attached tasks are incomplete");
            }
        }
    }
    quest->status = status;
    ++quest->quest_revision;
    return Result<void>::success();
}

const SkillState* ProgressionLedger::find_skill(PlayerId player, const std::string& key) const noexcept {
    if (!player.valid() || key.empty()) return nullptr;
    auto it = std::find_if(skills_.begin(), skills_.end(), [&](const SkillState& skill) {
        return skill.player == player && skill.key == key;
    });
    return it == skills_.end() ? nullptr : &*it;
}

const TaskState* ProgressionLedger::find_task(TaskId id) const noexcept {
    if (!id.valid()) return nullptr;
    auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const TaskState& task) { return task.id == id; });
    return it == tasks_.end() ? nullptr : &*it;
}

const QuestState* ProgressionLedger::find_quest(QuestId id) const noexcept {
    if (!id.valid()) return nullptr;
    auto it = std::find_if(quests_.begin(), quests_.end(), [&](const QuestState& quest) { return quest.id == id; });
    return it == quests_.end() ? nullptr : &*it;
}

std::vector<SkillState> ProgressionLedger::skills_for(PlayerId player) const {
    std::vector<SkillState> result;
    if (!player.valid()) return result;
    for (const auto& skill : skills_) if (skill.player == player) result.push_back(skill);
    return result;
}

std::vector<TaskState> ProgressionLedger::tasks_for(PlayerId player) const {
    std::vector<TaskState> result;
    if (!player.valid()) return result;
    for (const auto& task : tasks_) if (task.owner == player) result.push_back(task);
    return result;
}

std::vector<QuestState> ProgressionLedger::quests_for(PlayerId player) const {
    std::vector<QuestState> result;
    if (!player.valid()) return result;
    for (const auto& quest : quests_) if (quest.owner == player) result.push_back(quest);
    return result;
}

std::vector<SkillState> ProgressionLedger::skill_snapshot() const { return skills_; }
std::vector<TaskState> ProgressionLedger::task_snapshot() const { return tasks_; }
std::vector<QuestState> ProgressionLedger::quest_snapshot() const { return quests_; }

} // namespace home
