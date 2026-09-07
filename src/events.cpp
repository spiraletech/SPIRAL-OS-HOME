#include "home/events.hpp"

#include <algorithm>
#include <chrono>
#include <set>
#include <utility>

namespace home {
namespace {
using namespace std::chrono;

bool valid_date(int year_value, unsigned month_value, unsigned day_value) {
    const year_month_day ymd{year{year_value}, month{month_value}, day{day_value}};
    return ymd.ok();
}

bool valid_annual_rule_date(unsigned month_value, unsigned day_value) {
    // Year 2000 is leap-capable, so February 29 is a valid annual rule.
    return valid_date(2000, month_value, day_value);
}

bool active_on(const AnnualDateRule& rule, const CalendarDate& date) {
    const sys_days current{year{date.year}/month{date.month}/day{date.day}};
    const auto duration = days{static_cast<days::rep>(rule.duration_days)};

    if (valid_date(date.year, rule.month, rule.day)) {
        const sys_days start_this{year{date.year}/month{rule.month}/day{rule.day}};
        if (current >= start_this && current < start_this + duration) return true;
    }

    if (valid_date(date.year - 1, rule.month, rule.day)) {
        const sys_days start_previous{year{date.year - 1}/month{rule.month}/day{rule.day}};
        if (current >= start_previous && current < start_previous + duration) return true;
    }

    return false;
}

Result<void> validate_definition_set(const std::vector<EventDefinition>& definitions) {
    std::set<std::uint64_t> ids;
    std::set<std::string> keys;
    for (const auto& event : definitions) {
        const auto valid = validate_event_definition(event);
        if (!valid) return valid;
        if (!ids.insert(event.id.value()).second) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate event id");
        }
        if (!keys.insert(event.key).second) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate event key");
        }
    }
    return Result<void>::success();
}
} // namespace

Result<void> validate_event_definition(const EventDefinition& definition) {
    if (!definition.id.valid()) {
        return Result<void>::failure(ErrorCode::InvalidArgument, "event requires a valid id");
    }
    if (definition.key.empty() || definition.display_name.empty()) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "event requires key and display name");
    }
    if (!valid_annual_rule_date(definition.rule.month, definition.rule.day)) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "event annual date is invalid");
    }
    if (definition.rule.duration_days == 0 || definition.rule.duration_days > 366) {
        return Result<void>::failure(ErrorCode::ValidationFailed, "event duration must be between 1 and 366 days");
    }

    std::set<std::string> affinities;
    for (const auto& affinity : definition.affinities) {
        if (affinity.empty()) {
            return Result<void>::failure(ErrorCode::ValidationFailed, "event affinity must not be empty");
        }
        if (!affinities.insert(affinity).second) {
            return Result<void>::failure(ErrorCode::AlreadyExists, "duplicate event affinity");
        }
    }
    return Result<void>::success();
}

Result<EventResolution> resolve_event_set(const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar, EventSelector selector) {
    if (!valid_date(calendar.date.year, calendar.date.month, calendar.date.day)) {
        return Result<EventResolution>::failure(ErrorCode::ValidationFailed, "calendar date is invalid");
    }

    const auto valid_set = validate_definition_set(definitions);
    if (!valid_set) {
        return Result<EventResolution>::failure(valid_set.error().code, valid_set.error().message);
    }

    if (selector.forced_event.has_value() && selector.authority.empty()) {
        return Result<EventResolution>::failure(ErrorCode::ValidationFailed,
            "forced event override requires explicit authority");
    }

    EventResolution resolution{};
    if (selector.include_calendar_events) {
        for (const auto& event : definitions) {
            if (active_on(event.rule, calendar.date)) resolution.events.push_back(event);
        }
    }

    if (selector.forced_event.has_value()) {
        const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const EventDefinition& event) {
            return event.id == *selector.forced_event;
        });
        if (it == definitions.end()) {
            return Result<EventResolution>::failure(ErrorCode::NotFound, "forced event not found");
        }
        if (std::none_of(resolution.events.begin(), resolution.events.end(), [&](const EventDefinition& event) {
                return event.id == it->id;
            })) {
            resolution.events.push_back(*it);
        }
        resolution.override_applied = true;
        resolution.forced_event = it->id;
        resolution.authority = std::move(selector.authority);
    }

    std::sort(resolution.events.begin(), resolution.events.end(), [](const EventDefinition& a, const EventDefinition& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.id.value() < b.id.value();
    });

    return Result<EventResolution>::success(std::move(resolution));
}

Result<std::vector<EventDefinition>> resolve_events(const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar, EventSelector selector) {
    auto resolved = resolve_event_set(definitions, calendar, std::move(selector));
    if (!resolved) {
        return Result<std::vector<EventDefinition>>::failure(resolved.error().code, resolved.error().message);
    }
    return Result<std::vector<EventDefinition>>::success(std::move(resolved).value().events);
}

Result<void> EventCatalog::add(EventDefinition definition) {
    const auto valid = validate_event_definition(definition);
    if (!valid) return valid;
    if (find(definition.id) != nullptr) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "event id already exists");
    }
    if (find_by_key(definition.key) != nullptr) {
        return Result<void>::failure(ErrorCode::AlreadyExists, "event key already exists");
    }
    definitions_.push_back(std::move(definition));
    return Result<void>::success();
}

Result<void> EventCatalog::remove(EventId id) {
    const auto it = std::find_if(definitions_.begin(), definitions_.end(), [&](const EventDefinition& event) {
        return event.id == id;
    });
    if (it == definitions_.end()) return Result<void>::failure(ErrorCode::NotFound, "event not found");
    definitions_.erase(it);
    return Result<void>::success();
}

const EventDefinition* EventCatalog::find(EventId id) const noexcept {
    const auto it = std::find_if(definitions_.begin(), definitions_.end(), [&](const EventDefinition& event) {
        return event.id == id;
    });
    return it == definitions_.end() ? nullptr : &*it;
}

const EventDefinition* EventCatalog::find_by_key(const std::string& key) const noexcept {
    const auto it = std::find_if(definitions_.begin(), definitions_.end(), [&](const EventDefinition& event) {
        return event.key == key;
    });
    return it == definitions_.end() ? nullptr : &*it;
}

std::vector<EventDefinition> EventCatalog::snapshot() const {
    auto copy = definitions_;
    std::sort(copy.begin(), copy.end(), [](const EventDefinition& a, const EventDefinition& b) {
        return a.id.value() < b.id.value();
    });
    return copy;
}

Result<EventResolution> EventCatalog::resolve(const CalendarState& calendar, EventSelector selector) const {
    return resolve_event_set(definitions_, calendar, std::move(selector));
}

} // namespace home
