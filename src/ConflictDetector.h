#pragma once
#include "CalendarEvent.h"
#include <vector>
#include <string>

struct ConflictPair {
    std::string idA;
    std::string idB;
    std::string reason;   // e.g. "Same satellite double-booked"
};

// Detect scheduling conflicts among events.
// Currently detects: satellite double-booking (two events with same sat_pass satId
// whose times overlap) and raw time overlaps on satellite-tagged events.
std::vector<ConflictPair> DetectConflicts(
    const std::vector<CalendarEvent>& events);

// Returns true if event 'id' is part of any conflict
bool EventHasConflict(const std::string& id,
                      const std::vector<ConflictPair>& conflicts);
