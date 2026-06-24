#include "ConflictDetector.h"
#include <algorithm>

// Extract satId from location field "sat_pass:satId:gsId"
static std::string SatIdFromLocation(const std::string& loc)
{
    if (loc.find("sat_pass:") != 0) return "";
    size_t p1 = loc.find(':', 9);
    if (p1 == std::string::npos) return loc.substr(9);
    return loc.substr(9, p1 - 9);
}

static bool Overlaps(const CalendarEvent& a, const CalendarEvent& b)
{
    if (!a.start.IsValid() || !a.end.IsValid() ||
        !b.start.IsValid() || !b.end.IsValid()) return false;
    return a.start < b.end && b.start < a.end;
}

std::vector<ConflictPair> DetectConflicts(
    const std::vector<CalendarEvent>& events)
{
    std::vector<ConflictPair> result;

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& A = events[i];
        std::string satA = SatIdFromLocation(A.location);

        for (size_t j = i + 1; j < events.size(); ++j) {
            const auto& B = events[j];
            std::string satB = SatIdFromLocation(B.location);

            if (satA.empty() || satB.empty()) continue;
            if (satA != satB) continue;
            if (!Overlaps(A, B)) continue;

            ConflictPair cp;
            cp.idA    = A.id;
            cp.idB    = B.id;
            cp.reason = "Satellite double-booked: overlapping pass events";
            result.push_back(cp);
        }
    }

    return result;
}

bool EventHasConflict(const std::string& id,
                      const std::vector<ConflictPair>& conflicts)
{
    for (const auto& cp : conflicts)
        if (cp.idA == id || cp.idB == id) return true;
    return false;
}
