#include "home/events.hpp"

#include <algorithm>
#include <chrono>
#include <set>

namespace home {
namespace {
using namespace std::chrono;

bool valid_date(int year_value, unsigned month_value, unsigned day_value) {
    const year_month_day ymd{year{year_value}, month{month_value}, day{day_value}};
    return ymd.ok();
}

bool active_on(const AnnualDateRule& rule, const CalendarDate& date) {
    if (rule.duration_days == 0 || !valid_date(date.year, rule.month, rule.day)) return false;
    const sys_days current{year{date.year}/month{date.month}/day{date.day}};
    const sys_days start_this{year{date.year}/month{rule.month}/day{rule.day}};
    const auto duration = days{static_cast<days::rep>(rule.duration_days)};
    if (current >= start_this && current < start_this + duration) return true;
    if (!valid_date(date.year - 1, rule.month, rule.day)) return false;
    const sys_days start_previous{year{date.year - 1}/month{rule.month}/day{rule.day}};
    return current >= start_previous && current < start_previous + duration;
}
} // namespace

Result<std::vector<EventDefinition>> resolve_events(const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar, EventSelector selector) {
    std::set<std::uint64_t> seen;
    for (const auto& event : definitions) {
        if (!event.id.valid() || event.key.empty() || event.rule.duration_days == 0 || event.rule.month < 1 || event.rule.month > 12) {
            return Result<std::vector<EventDefinition>>::failure(ErrorCode::ValidationFailed, "event definition is invalid");
        }
        if (!seen.insert(event.id.value()).second) {
            return Result<std::vector<EventDefinition>>::failure(ErrorCode::AlreadyExists, "duplicate event id");
        }
    }

    std::vector<EventDefinition> active;
    if (selector.include_calendar_events) {
        for (const auto& event : definitions) if (active_on(event.rule, calendar.date)) active.push_back(event);
    }

    if (selector.forced_event.has_value()) {
        const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const EventDefinition& event) { return event.id == *selector.forced_event; });
        if (it == definitions.end()) return Result<std::vector<EventDefinition>>::failure(ErrorCode::NotFound, "forced event not found");
        if (std::none_of(active.begin(), active.end(), [&](const EventDefinition& event) { return event.id == it->id; })) active.push_back(*it);
    }

    std::sort(active.begin(), active.end(), [](const EventDefinition& a, const EventDefinition& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.id.value() < b.id.value();
    });
    return Result<std::vector<EventDefinition>>::success(std::move(active));
}

} // namespace home
