#pragma once
#include "CalendarEvent.h"
#include <vector>
#include <string>

class LanClient;

class EventStore {
public:
    EventStore();
    void Load();
    void Save() const;

    // Enable remote mode: all CRUD is proxied through the LAN client.
    void SetLanClient(LanClient* c) { client_ = c; }

    std::vector<CalendarEvent>&       Events()       { return events_; }
    const std::vector<CalendarEvent>& Events() const { return events_; }

    void Add(const CalendarEvent& e);
    void Update(const CalendarEvent& e);
    void Remove(const std::string& id);
    void AddException(const std::string& id, const wxDateTime& date); // exclude one occurrence

    std::vector<const CalendarEvent*> EventsOnDate(const wxDateTime& date) const;

private:
    std::vector<CalendarEvent> events_;
    std::string                path_;
    LanClient*                 client_ = nullptr;
};
