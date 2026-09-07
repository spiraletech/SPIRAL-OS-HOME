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
    std::vector<std::string> affinities{};
};

struct EventSelector final {
    bool include_calendar_events{true};
    std::optional<EventId> forced_event{};
    std::string authority{};
};

struct EventResolution final {
    std::vector<EventDefinition> events{};
    bool override_applied{false};
    std::optional<EventId> forced_event{};
    std::string authority{};
};

Result<void> validate_event_definition(const EventDefinition& definition);

Result<EventResolution> resolve_event_set(
    const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar,
    EventSelector selector = {});

Result<std::vector<EventDefinition>> resolve_events(
    const std::vector<EventDefinition>& definitions,
    const CalendarState& calendar,
    EventSelector selector = {});

class EventCatalog final {
public:
    Result<void> add(EventDefinition definition);
    Result<void> remove(EventId id);

    [[nodiscard]] const EventDefinition* find(EventId id) const noexcept;
    [[nodiscard]] const EventDefinition* find_by_key(const std::string& key) const noexcept;
    [[nodiscard]] std::vector<EventDefinition> snapshot() const;
    [[nodiscard]] Result<EventResolution> resolve(const CalendarState& calendar, EventSelector selector = {}) const;
    [[nodiscard]] std::size_t size() const noexcept { return definitions_.size(); }

private:
    std::vector<EventDefinition> definitions_{};
};

} // namespace home
