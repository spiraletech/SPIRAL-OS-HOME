#include "home/versioned_world.hpp"

namespace home {

Result<void> VersionedWorld::set_skill_state(SkillState state) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > now) return Result<void>::failure(ErrorCode::ValidationFailed, "skill references a future HOME minute");
    std::optional<SkillState> before{};
    if (const auto* existing = progression_.find_skill(state.player, state.key)) before = *existing;
    ProgressionLedger staged = progression_;
    const auto set = staged.set_skill(registry_, player_life_, state);
    if (!set) return set;
    const SkillState* after = staged.find_skill(state.player, state.key);
    if (!after) return Result<void>::failure(ErrorCode::InternalError, "skill missing after update");
    const auto committed = commit({WorldChange{WorldChangeKind::SkillStateChanged, SkillStateChanged{before, *after}}});
    if (!committed) return committed;
    progression_ = std::move(staged);
    return Result<void>::success();
}

Result<TaskId> VersionedWorld::create_task(TaskCreateInfo info) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (info.updated_world_minute > now) return Result<TaskId>::failure(ErrorCode::ValidationFailed, "task references a future HOME minute");
    ProgressionLedger staged = progression_;
    const auto created = staged.create_task(registry_, player_life_, std::move(info));
    if (!created) return created;
    const TaskState* after = staged.find_task(created.value());
    if (!after) return Result<TaskId>::failure(ErrorCode::InternalError, "task missing after creation");
    const auto committed = commit({WorldChange{WorldChangeKind::TaskStateChanged, TaskStateChanged{std::nullopt, *after}}});
    if (!committed) return Result<TaskId>::failure(committed.error().code, committed.error().message);
    progression_ = std::move(staged);
    return created;
}

Result<void> VersionedWorld::set_task_state(TaskState state) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > now) return Result<void>::failure(ErrorCode::ValidationFailed, "task references a future HOME minute");
    const TaskState* current = progression_.find_task(state.id);
    if (!current) return Result<void>::failure(ErrorCode::NotFound, "task not found");
    const TaskState before = *current;
    ProgressionLedger staged = progression_;
    const auto set = staged.set_task(registry_, player_life_, state);
    if (!set) return set;
    const TaskState* after = staged.find_task(state.id);
    if (!after) return Result<void>::failure(ErrorCode::InternalError, "task missing after update");
    const auto committed = commit({WorldChange{WorldChangeKind::TaskStateChanged, TaskStateChanged{before, *after}}});
    if (!committed) return committed;
    progression_ = std::move(staged);
    return Result<void>::success();
}

Result<QuestId> VersionedWorld::create_quest(QuestCreateInfo info) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (info.updated_world_minute > now) return Result<QuestId>::failure(ErrorCode::ValidationFailed, "quest references a future HOME minute");
    for (const QuestId prerequisite : info.prerequisites) {
        const QuestState* prior = progression_.find_quest(prerequisite);
        if (!prior || prior->owner != info.owner) {
            return Result<QuestId>::failure(ErrorCode::ValidationFailed, "quest prerequisite must exist and belong to the same player");
        }
    }
    ProgressionLedger staged = progression_;
    const auto created = staged.create_quest(registry_, player_life_, std::move(info));
    if (!created) return created;
    const QuestState* after = staged.find_quest(created.value());
    if (!after) return Result<QuestId>::failure(ErrorCode::InternalError, "quest missing after creation");
    const auto committed = commit({WorldChange{WorldChangeKind::QuestStateChanged, QuestStateChanged{std::nullopt, *after}}});
    if (!committed) return Result<QuestId>::failure(committed.error().code, committed.error().message);
    progression_ = std::move(staged);
    return created;
}

Result<void> VersionedWorld::set_quest_state(QuestState state) {
    const std::uint64_t now = clock_.now().milliseconds / 60000ULL;
    if (state.updated_world_minute > now) return Result<void>::failure(ErrorCode::ValidationFailed, "quest references a future HOME minute");
    for (const QuestId prerequisite : state.prerequisites) {
        const QuestState* prior = progression_.find_quest(prerequisite);
        if (!prior || prior->owner != state.owner) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "quest prerequisite must exist and belong to the same player");
        }
        if (prerequisite.value() >= state.id.value()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "quest prerequisite must reference an earlier stable quest id");
        }
    }
    const QuestState* current = progression_.find_quest(state.id);
    if (!current) return Result<void>::failure(ErrorCode::NotFound, "quest not found");
    const QuestState before = *current;
    ProgressionLedger staged = progression_;
    const auto set = staged.set_quest(registry_, player_life_, state);
    if (!set) return set;
    const QuestState* after = staged.find_quest(state.id);
    if (!after) return Result<void>::failure(ErrorCode::InternalError, "quest missing after update");
    const auto committed = commit({WorldChange{WorldChangeKind::QuestStateChanged, QuestStateChanged{before, *after}}});
    if (!committed) return committed;
    progression_ = std::move(staged);
    return Result<void>::success();
}

} // namespace home
