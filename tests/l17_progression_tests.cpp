#include "home/progression.hpp"

#include <cassert>

int main() {
    using namespace home;

    ProgressionLedger ledger;
    const PlayerId player{1};
    const PlayerId other{2};

    assert(ledger.define_skill(player, "skating").ok());
    assert(!ledger.define_skill(player, "skating").ok());
    assert(ledger.add_skill_experience(player, "skating", 250).ok());
    const auto* skating = ledger.find_skill(player, "skating");
    assert(skating != nullptr);
    assert(skating->level == 2);
    assert(skating->experience == 150);
    assert(skating->skill_revision == 1);

    const auto task_a = ledger.create_task(player, "ollie-ten", "Land ten ollies", 10);
    const auto task_b = ledger.create_task(player, "manual-five", "Manual five ledges", 5);
    const auto foreign_task = ledger.create_task(other, "other-task", "Other task", 1);
    assert(task_a.ok() && task_b.ok() && foreign_task.ok());
    assert(!ledger.create_task(player, "ollie-ten", "Duplicate", 1).ok());

    assert(ledger.add_task_progress(task_a.value(), 4).ok());
    const auto* a = ledger.find_task(task_a.value());
    assert(a != nullptr);
    assert(a->status == TaskStatus::Active);
    assert(a->progress == 4);
    assert(!ledger.set_task_status(task_a.value(), TaskStatus::Complete).ok());
    assert(!ledger.add_task_progress(task_a.value(), 7).ok());
    assert(ledger.add_task_progress(task_a.value(), 6).ok());
    a = ledger.find_task(task_a.value());
    assert(a->status == TaskStatus::Complete);
    assert(a->progress == 10);
    assert(!ledger.add_task_progress(task_a.value(), 1).ok());

    const auto quest = ledger.create_quest(player,
                                           "mission-bay-lines",
                                           "Mission Bay Lines",
                                           {task_a.value(), task_b.value()});
    assert(quest.ok());
    assert(!ledger.create_quest(player, "mission-bay-lines", "Duplicate").ok());
    assert(!ledger.attach_task(quest.value(), foreign_task.value()).ok());
    assert(!ledger.set_quest_status(quest.value(), QuestStatus::Complete).ok());

    assert(ledger.add_task_progress(task_b.value(), 5).ok());
    assert(ledger.set_quest_status(quest.value(), QuestStatus::Active).ok());
    assert(ledger.set_quest_status(quest.value(), QuestStatus::Complete).ok());
    const auto* q = ledger.find_quest(quest.value());
    assert(q != nullptr);
    assert(q->status == QuestStatus::Complete);
    assert(q->tasks.size() == 2);
    assert(q->quest_revision == 2);
    assert(!ledger.attach_task(quest.value(), task_a.value()).ok());

    assert(ledger.skills_for(player).size() == 1);
    assert(ledger.tasks_for(player).size() == 2);
    assert(ledger.quests_for(player).size() == 1);
    assert(ledger.skill_snapshot().size() == 1);
    assert(ledger.task_snapshot().size() == 3);
    assert(ledger.quest_snapshot().size() == 1);

    return 0;
}
