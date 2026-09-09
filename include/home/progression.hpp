#pragma once

#include "home/entity_registry.hpp"
#include "home/ids.hpp"
#include "home/player_life.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace home {

inline constexpr std::uint32_t kMaxSkillLevel = 100;
inline constexpr std::uint64_t kExperiencePerSkillLevel = 1000;

enum class TaskStatus : std::uint8_t {
    Pending = 0,
    Active,
    Complete,
    Failed,
    Cancelled
};

enum class QuestStatus : std::uint8_t {
    Pending = 0,
    Active,
    Complete,
    Failed,
    Cancelled
};

struct SkillState final {
    EntityId player{};
    std::string key{};
    std::uint32_t level{1};
    std::uint64_t experience{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};
    auto operator<=>(const SkillState&) const = default;
};

struct TaskCreateInfo final {
    EntityId owner{};
    std::string key{};
    std::string title{};
    std::uint32_t target{1};
    std::uint64_t updated_world_minute{};
};

struct TaskState final {
    TaskId id{};
    EntityId owner{};
    std::string key{};
    std::string title{};
    TaskStatus status{TaskStatus::Pending};
    std::uint32_t progress{};
    std::uint32_t target{1};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};
    auto operator<=>(const TaskState&) const = default;
};

struct QuestCreateInfo final {
    EntityId owner{};
    std::string key{};
    std::string title{};
    std::vector<TaskId> tasks{};
    std::vector<QuestId> prerequisites{};
    std::uint64_t updated_world_minute{};
};

struct QuestState final {
    QuestId id{};
    EntityId owner{};
    std::string key{};
    std::string title{};
    QuestStatus status{QuestStatus::Pending};
    std::vector<TaskId> tasks{};
    std::vector<QuestId> prerequisites{};
    std::uint64_t updated_world_minute{};
    std::uint64_t sequence{1};
    auto operator<=>(const QuestState&) const = default;
};

struct ProgressionPurgeResult final {
    std::vector<SkillState> skills{};
    std::vector<TaskState> tasks{};
    std::vector<QuestState> quests{};
};

[[nodiscard]] std::uint32_t skill_level_for_experience(std::uint64_t experience) noexcept;
[[nodiscard]] Result<void> validate_skill_shape(const SkillState& state);
[[nodiscard]] Result<void> validate_task_shape(const TaskState& state);
[[nodiscard]] Result<void> validate_quest_shape(const QuestState& state);

class ProgressionLedger final {
public:
    Result<void> set_skill(const EntityRegistry& entities, const PlayerLifeLedger& life, SkillState state);
    Result<void> restore_skill(const EntityRegistry& entities, const PlayerLifeLedger& life, SkillState state);

    Result<TaskId> create_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskCreateInfo info);
    Result<void> set_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskState state);
    Result<void> restore_task(const EntityRegistry& entities, const PlayerLifeLedger& life, TaskState state);

    Result<QuestId> create_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestCreateInfo info);
    Result<void> set_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestState state);
    Result<void> restore_quest(const EntityRegistry& entities, const PlayerLifeLedger& life, QuestState state);

    [[nodiscard]] const SkillState* find_skill(EntityId player, std::string_view key) const noexcept;
    [[nodiscard]] const TaskState* find_task(TaskId id) const noexcept;
    [[nodiscard]] const QuestState* find_quest(QuestId id) const noexcept;
    [[nodiscard]] std::vector<SkillState> skills_for(EntityId player) const;
    [[nodiscard]] std::vector<TaskState> tasks_for(EntityId player) const;
    [[nodiscard]] std::vector<QuestState> quests_for(EntityId player) const;
    [[nodiscard]] std::vector<SkillState> skill_snapshot() const { return skills_; }
    [[nodiscard]] std::vector<TaskState> task_snapshot() const { return tasks_; }
    [[nodiscard]] std::vector<QuestState> quest_snapshot() const { return quests_; }

    Result<ProgressionPurgeResult> purge_player(EntityId player);

private:
    [[nodiscard]] Result<void> validate_task_references(const TaskState& state) const;
    [[nodiscard]] Result<void> validate_quest_references(const QuestState& state) const;
    Result<TaskId> allocate_task_id();
    Result<QuestId> allocate_quest_id();
    void advance_task_allocator_past(TaskId id) noexcept;
    void advance_quest_allocator_past(QuestId id) noexcept;

    std::vector<SkillState> skills_{};
    std::vector<TaskState> tasks_{};
    std::vector<QuestState> quests_{};
    std::uint64_t next_task_value_{1};
    std::uint64_t next_quest_value_{1};
};

} // namespace home
