#include "IcsConnector.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

// ── ICS line unfolding (RFC 5545 §3.1) ───────────────────────────────────────

std::string IcsConnector::UnfoldLines(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i+1] == '\n' &&
            i + 2 < raw.size() && (raw[i+2] == ' ' || raw[i+2] == '\t')) {
            i += 2;  // skip CRLF + whitespace continuation
        } else if (raw[i] != '\r') {
            out += raw[i];
        }
    }
    return out;
}

// ── DateTime parsing ──────────────────────────────────────────────────────────

wxDateTime IcsConnector::ParseIcsDateTime(const std::string& val)
{
    // Strip TZID=...: prefix if present
    std::string v = val;
    size_t colon = v.rfind(':');
    if (colon != std::string::npos) v = v.substr(colon + 1);

    wxDateTime dt;
    // YYYYMMDDTHHMMSSZ  or  YYYYMMDDTHHMMSS  or  YYYYMMDD (all-day)
    if (v.size() >= 15 && v[8] == 'T') {
        int Y = std::stoi(v.substr(0, 4));
        int M = std::stoi(v.substr(4, 2));
        int D = std::stoi(v.substr(6, 2));
        int h = std::stoi(v.substr(9, 2));
        int m = std::stoi(v.substr(11, 2));
        int s = std::stoi(v.substr(13, 2));
        dt.Set(D, static_cast<wxDateTime::Month>(M - 1), Y, h, m, s);
    } else if (v.size() >= 8) {
        int Y = std::stoi(v.substr(0, 4));
        int M = std::stoi(v.substr(4, 2));
        int D = std::stoi(v.substr(6, 2));
        dt.Set(D, static_cast<wxDateTime::Month>(M - 1), Y, 0, 0, 0);
    }
    return dt;
}

std::string IcsConnector::FormatIcsDateTime(const wxDateTime& dt, bool allDay)
{
    if (!dt.IsValid()) return "";
    char buf[32];
    if (allDay) {
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d",
                      dt.GetYear(), (int)dt.GetMonth() + 1, dt.GetDay());
    } else {
        std::snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02dZ",
                      dt.GetYear(), (int)dt.GetMonth() + 1, dt.GetDay(),
                      dt.GetHour(), dt.GetMinute(), dt.GetSecond());
    }
    return buf;
}

wxColour IcsConnector::CategoryToColor(const std::string& categories)
{
    // Map common Outlook calendar color categories to our palette
    std::string cat = categories;
    std::transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
    if (cat.find("red")    != std::string::npos) return wxColour(213,  0,   0);
    if (cat.find("orange") != std::string::npos) return wxColour(244, 81,  30);
    if (cat.find("yellow") != std::string::npos) return wxColour(246,191,  38);
    if (cat.find("green")  != std::string::npos) return wxColour( 11,128,  67);
    if (cat.find("blue")   != std::string::npos) return wxColour( 26,115, 232);
    if (cat.find("purple") != std::string::npos) return wxColour(142, 36, 170);
    return wxColour(26, 115, 232);
}

// ── Import ────────────────────────────────────────────────────────────────────

bool IcsConnector::Connect(std::string& /*errorMsg*/)
{
    return true;  // file-based — always available
}

std::vector<CalendarEvent> IcsConnector::ImportFromFile(const std::string& path,
                                                         std::string& errorMsg)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { errorMsg = "Cannot open file: " + path; return {}; }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string raw = ss.str();
    std::string ics = UnfoldLines(raw);

    std::vector<CalendarEvent> results;
    std::istringstream stream(ics);
    std::string line;

    CalendarEvent cur;
    bool inEvent = false;
    std::string categories;

    auto getValue = [](const std::string& l) -> std::string {
        size_t pos = l.find(':');
        if (pos == std::string::npos) return "";
        return l.substr(pos + 1);
    };
    auto getKey = [](const std::string& l) -> std::string {
        size_t pos = l.find(':');
        if (pos == std::string::npos) pos = l.find(';');
        if (pos == std::string::npos) return l;
        return l.substr(0, pos);
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "BEGIN:VEVENT") {
            cur = CalendarEvent();
            categories.clear();
            inEvent = true;
            continue;
        }
        if (line == "END:VEVENT") {
            if (inEvent) {
                cur.color = CategoryToColor(categories);
                cur.calendarSource = "ics";
                if (!cur.title.empty() && cur.start.IsValid())
                    results.push_back(cur);
            }
            inEvent = false;
            continue;
        }
        if (!inEvent) continue;

        std::string key = getKey(line);
        std::string val = getValue(line);

        if (key == "SUMMARY") {
            cur.title = val;
        } else if (key == "DESCRIPTION") {
            // Unescape \n and \,
            std::string desc;
            for (size_t i = 0; i < val.size(); ++i) {
                if (val[i] == '\\' && i + 1 < val.size()) {
                    if (val[i+1] == 'n') { desc += '\n'; ++i; }
                    else if (val[i+1] == ',') { desc += ','; ++i; }
                    else { desc += val[++i]; }
                } else {
                    desc += val[i];
                }
            }
            cur.description = desc;
        } else if (key == "LOCATION") {
            cur.location = val;
        } else if (key.substr(0, 7) == "DTSTART") {
            cur.start = ParseIcsDateTime(line.substr(line.find(':') != std::string::npos
                                                     ? line.find(':') : 0));
            cur.allDay = (key.find("VALUE=DATE") != std::string::npos ||
                          val.size() == 8);
        } else if (key.substr(0, 5) == "DTEND") {
            cur.end = ParseIcsDateTime(line.substr(line.find(':') != std::string::npos
                                                   ? line.find(':') : 0));
        } else if (key == "CATEGORIES") {
            categories = val;
        } else if (key == "ATTENDEE" || key.substr(0, 8) == "ATTENDEE") {
            // Extract CN= display name as participant hint
            size_t cn = line.find("CN=");
            if (cn != std::string::npos) {
                size_t end = line.find(';', cn + 3);
                if (end == std::string::npos) end = line.find(':', cn + 3);
                if (end != std::string::npos)
                    cur.participants.push_back(line.substr(cn + 3, end - cn - 3));
            }
        }
    }

    if (results.empty() && errorMsg.empty())
        errorMsg = "No valid VEVENT blocks found in file.";

    return results;
}

std::vector<CalendarEvent> IcsConnector::FetchEvents(const wxDateTime& /*from*/,
                                                      const wxDateTime& /*to*/,
                                                      std::string& errorMsg)
{
    errorMsg = "Use ImportFromFile() for ICS import.";
    return {};
}

// ── Export ────────────────────────────────────────────────────────────────────

bool IcsConnector::ExportToFile(const std::vector<CalendarEvent>& events,
                                 const std::string& path,
                                 std::string& errorMsg)
{
    std::ofstream f(path);
    if (!f.is_open()) { errorMsg = "Cannot write file: " + path; return false; }

    f << "BEGIN:VCALENDAR\r\n"
      << "VERSION:2.0\r\n"
      << "PRODID:-//USSC Calendar//EN\r\n"
      << "CALSCALE:GREGORIAN\r\n"
      << "METHOD:PUBLISH\r\n";

    static int uid_counter = 0;
    for (const auto& ev : events) {
        f << "BEGIN:VEVENT\r\n";

        // UID
        char uid[64];
        std::snprintf(uid, sizeof(uid), "ussc-%s-%d@ussc_calendar",
                      ev.id.empty() ? "event" : ev.id.c_str(), uid_counter++);
        f << "UID:" << uid << "\r\n";

        // Timestamps
        if (ev.allDay) {
            f << "DTSTART;VALUE=DATE:" << FormatIcsDateTime(ev.start, true) << "\r\n";
            f << "DTEND;VALUE=DATE:"   << FormatIcsDateTime(ev.end,   true) << "\r\n";
        } else {
            f << "DTSTART:" << FormatIcsDateTime(ev.start, false) << "\r\n";
            f << "DTEND:"   << FormatIcsDateTime(ev.end,   false) << "\r\n";
        }

        f << "SUMMARY:" << ev.title << "\r\n";
        if (!ev.description.empty())
            f << "DESCRIPTION:" << ev.description << "\r\n";
        if (!ev.location.empty())
            f << "LOCATION:" << ev.location << "\r\n";

        // Attendees
        for (const auto& pid : ev.participants)
            f << "ATTENDEE;CN=" << pid << ":mailto:\r\n";

        f << "END:VEVENT\r\n";
    }

    f << "END:VCALENDAR\r\n";
    return true;
}

std::string IcsConnector::PushEvent(const CalendarEvent& ev, std::string& errorMsg)
{
    // For ICS, "push" means append to a file — not implemented as single-event
    errorMsg = "Use ExportToFile() to export events as ICS.";
    (void)ev;
    return "";
}
