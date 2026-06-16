#pragma once
#include "CalendarConnector.h"
#include <string>

// Imports/exports iCalendar (.ics) files per RFC 5545.
// Fully implemented — no network or external dependencies required.
class IcsConnector : public CalendarConnector {
public:
    bool Connect(std::string& errorMsg) override;

    // Parse events from an ICS file path
    std::vector<CalendarEvent> FetchEvents(const wxDateTime& from,
                                            const wxDateTime& to,
                                            std::string& errorMsg) override;

    // Import from a specific file (ignores date range — imports all)
    std::vector<CalendarEvent> ImportFromFile(const std::string& path,
                                               std::string& errorMsg);

    // Export events to an ICS file
    bool ExportToFile(const std::vector<CalendarEvent>& events,
                      const std::string& path,
                      std::string& errorMsg);

    std::string PushEvent(const CalendarEvent& ev,
                           std::string& errorMsg) override;
    const char* DisplayName() const override { return "ICS File"; }

private:
    static wxDateTime ParseIcsDateTime(const std::string& val);
    static std::string FormatIcsDateTime(const wxDateTime& dt, bool allDay);
    static std::string UnfoldLines(const std::string& raw);
    static wxColour CategoryToColor(const std::string& categories);
};
