#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace home {

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
    PlayerId player{};
    std::string key{};
    std::uint32_t level{1};
    std::uint64_t experience{};
    std::uint64_t skill_revision{};
    auto operator<=>(const SkillState&) const = default;
};

struct TaskState final {
    TaskId id{};
    PlayerId owner{};
    std::string key{};
    std::string title{};
    TaskStatus status{TaskStatus::Pending};
    std::uint32_t progress{};
    std::uint32_t target{1};
    std::uint64_t task_revision{};
    auto operator<=>(const TaskState&) const = default;
};

struct QuestState final {
    QuestId id{};
    PlayerId owner{};
    std::string key{};
    std::string title{};
    QuestStatus status{QuestStatus::Pending};
    std::vector<TaskId> tasks{};
    std::uint64_t quest_revision{};
    auto operator<=>(const QuestState&) const = default;
};

class ProgressionLedger final {
public:
    static constexpr std::uint32_t max_skill_level = 100;

    Result<void> define_skill(PlayerId player,
                              std::string key,
                              std::uint32_t initial_level = 1,
                              std::uint64_t initial_experience = 0);
    Result<void> add_skill_experience(PlayerId player, const std::string& key, std::uint64_t amount);

    Result<TaskId> create_task(PlayerId owner,
                               std::string key,
                               std::string title,
                               std::uint32_t target = 1);
    Result<void> set_task_status(TaskId id, TaskStatus status);
    Result<void> add_task_progress(TaskId id, std::uint32_t amount);

    Result<QuestId> create_quest(PlayerId owner,
                                 std::string key,
                                 std::string title,
                                 std::vector<TaskId> tasks = {});
    Result<void> attach_task(QuestId quest, TaskId task);
    Result<void> set_quest_status(QuestId id, QuestStatus status);

    [[nodiscard]] const SkillState* find_skill(PlayerId player, const std::string& key) const noexcept;
    [[nodiscard]] const TaskState* find_task(TaskId id) const noexcept;
    [[nodiscard]] const QuestState* find_quest(QuestId id) const noexcept;

    [[nodiscard]] std::vector<SkillState> skills_for(PlayerId player) const;
    [[nodiscard]] std::vector<TaskState> tasks_for(PlayerId player) const;
    [[nodiscard]] std::vector<QuestState> quests_for(PlayerId player) const;

    [[nodiscard]] std::vector<SkillState> skill_snapshot() const;
    [[nodiscard]] std::vector<TaskState> task_snapshot() const;
    [[nodiscard]] std::vector<QuestState> quest_snapshot() const;

private:
    std::vector<SkillState> skills_{};
    std::vector<TaskState> tasks_{};
    std::vector<QuestState> quests_{};
    std::uint64_t next_task_id_{1};
    std::uint64_t next_quest_id_{1};
};

} // namespace home
