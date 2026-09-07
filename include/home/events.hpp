#pragma once

#include "home/calendar.hpp"
#include "home/ids.hpp"
#include "home/result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

enum class EventKind : std::uint8_t { Holiday = 0, Festival, Observance, Story };

struct AnnualDateRule final {
    unsigned month{1};
    unsigned day{1};
    unsigned duration_days{1};
};

struct EventDefinition final {
    EventId id{};
    std::string key{};
    std::string display_name{};
    EventKind kind{EventKind::Observance};
    AnnualDateRule rule{};
    int priority{};
};

struct EventSelector final {
    bool include_calendar_events{true};
    std::optional<EventId> forced_event{};
};

Result<std::vector<EventDefinition>> resolve_events(
    const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar,
    EventSelector selector = {});

} // namespace home
