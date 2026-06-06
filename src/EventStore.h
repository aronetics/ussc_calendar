#pragma once
#include "CalendarEvent.h"
#include <vector>
#include <string>

class EventStore {
public:
    EventStore();
    void Load();
    void Save() const;

    std::vector<CalendarEvent>&       Events()       { return events_; }
    const std::vector<CalendarEvent>& Events() const { return events_; }

    void Add(const CalendarEvent& e);
    void Update(const CalendarEvent& e);
    void Remove(const std::string& id);

    std::vector<const CalendarEvent*> EventsOnDate(const wxDateTime& date) const;

private:
    std::vector<CalendarEvent> events_;
    std::string                path_;
};
