#include "EventStore.h"
#include <wx/wx.h>
#include <wx/dir.h>
#include <wx/filename.h>
#ifdef _WIN32
#  include <wx/stdpaths.h>
#endif
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string GenerateId()
{
    static int counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%llx%04x",
                  (unsigned long long)wxDateTime::Now().GetTicks(), counter++);
    return std::string(buf);
}

// Escape a plain string for JSON (only handles the characters we store)
static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Unescape a JSON string value
static std::string JsonUnescape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Extract value for "key":"value" or "key":value from a JSON object string
static std::string ExtractJsonString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string val;
    bool escape = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escape) {
            val += c;
            escape = false;
        } else if (c == '\\') {
            val += c;
            escape = true;
        } else if (c == '"') {
            break;
        } else {
            val += c;
        }
    }
    return JsonUnescape(val);
}

static bool ExtractJsonBool(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    return json.substr(pos, 4) == "true";
}

static wxColour HexToColour(const std::string& hex)
{
    if (hex.size() < 7) return GCDefault();
    unsigned r = 0, g = 0, b = 0;
    std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    return wxColour(r, g, b);
}

static std::string ColourToHex(const wxColour& c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
    return std::string(buf);
}

static wxDateTime ParseDateTime(const std::string& s)
{
    wxDateTime dt;
    dt.ParseFormat(s, "%Y-%m-%dT%H:%M:%S");
    return dt;
}

static std::string FormatDateTime(const wxDateTime& dt)
{
    if (!dt.IsValid()) return "1970-01-01T00:00:00";
    return std::string(dt.Format("%Y-%m-%dT%H:%M:%S").mb_str());
}

// Split the JSON array of event objects.  Each object is delimited by { }.
static std::vector<std::string> SplitJsonObjects(const std::string& json)
{
    std::vector<std::string> objs;
    int depth = 0;
    size_t start = std::string::npos;
    for (size_t i = 0; i < json.size(); ++i) {
        if (json[i] == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                objs.push_back(json.substr(start, i - start + 1));
                start = std::string::npos;
            }
        }
    }
    return objs;
}

// ── CalendarEvent helpers ─────────────────────────────────────────────────────

bool CalendarEvent::OccursOnDate(const wxDateTime& d) const
{
    if (!start.IsValid()) return false;
    wxDateTime dayStart = wxDateTime(d.GetDay(), d.GetMonth(), d.GetYear(), 0, 0, 0);
    wxDateTime dayEnd   = dayStart;
    dayEnd.Add(wxTimeSpan::Day());

    if (allDay) {
        wxDateTime s = wxDateTime(start.GetDay(), start.GetMonth(), start.GetYear(), 0, 0, 0);
        wxDateTime e = end.IsValid()
                     ? wxDateTime(end.GetDay(), end.GetMonth(), end.GetYear(), 0, 0, 0)
                     : s;
        e.Add(wxTimeSpan::Day());  // inclusive end day
        return s < dayEnd && e > dayStart;
    }
    wxDateTime e = end.IsValid() ? end : start;
    return start < dayEnd && e > dayStart;
}

bool CalendarEvent::StartsOnDate(const wxDateTime& d) const
{
    if (!start.IsValid()) return false;
    return start.GetDay()   == d.GetDay()   &&
           start.GetMonth() == d.GetMonth() &&
           start.GetYear()  == d.GetYear();
}

// ── EventStore ────────────────────────────────────────────────────────────────

// ── Participant array helpers ────────────────────────────────────────────────

static std::vector<std::string> ExtractJsonArray(const std::string& json,
                                                  const std::string& key)
{
    std::string search = "\"" + key + "\":[";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();

    std::vector<std::string> result;
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '"') break;
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) val += json[++pos];
            else val += json[pos];
            ++pos;
        }
        ++pos;  // skip closing "
        if (!val.empty()) result.push_back(val);
    }
    return result;
}

EventStore::EventStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir))
        wxMkdir(dir);
    path_ = std::string((dir + "/events.json").mb_str());
}

void EventStore::Load()
{
    std::ifstream f(path_);
    if (!f.is_open()) return;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    // Find the array inside "events":[...]
    size_t arrPos = json.find("\"events\":");
    if (arrPos == std::string::npos) return;

    std::vector<std::string> objs = SplitJsonObjects(json.substr(arrPos));
    // First object is the outer wrapper; rest are the events
    for (size_t i = 1; i < objs.size(); ++i) {
        const std::string& obj = objs[i];
        CalendarEvent e;
        e.id          = ExtractJsonString(obj, "id");
        e.title       = ExtractJsonString(obj, "title");
        e.description = ExtractJsonString(obj, "desc");
        e.location    = ExtractJsonString(obj, "loc");
        e.allDay       = ExtractJsonBool(obj, "allDay");
        e.start        = ParseDateTime(ExtractJsonString(obj, "start"));
        e.end          = ParseDateTime(ExtractJsonString(obj, "end"));
        e.color        = HexToColour(ExtractJsonString(obj, "color"));
        e.participants    = ExtractJsonArray(obj, "participants");
        e.calendarSource  = ExtractJsonString(obj, "src");
        if (!e.id.empty()) events_.push_back(e);
    }
}

void EventStore::Save() const
{
    std::ofstream f(path_);
    if (!f.is_open()) return;

    f << "{\"events\":[\n";
    for (size_t i = 0; i < events_.size(); ++i) {
        const CalendarEvent& e = events_[i];
        f << "  {"
          << "\"id\":\""    << JsonEscape(e.id)          << "\","
          << "\"title\":\""  << JsonEscape(e.title)       << "\","
          << "\"desc\":\""   << JsonEscape(e.description) << "\","
          << "\"loc\":\""    << JsonEscape(e.location)    << "\","
          << "\"start\":\""  << FormatDateTime(e.start)   << "\","
          << "\"end\":\""    << FormatDateTime(e.end)     << "\","
          << "\"allDay\":"   << (e.allDay ? "true" : "false") << ","
          << "\"color\":\""  << ColourToHex(e.color)      << "\","
          << "\"participants\":[";
        for (size_t j = 0; j < e.participants.size(); ++j) {
            f << "\"" << JsonEscape(e.participants[j]) << "\"";
            if (j + 1 < e.participants.size()) f << ",";
        }
        f << "],\"src\":\"" << JsonEscape(e.calendarSource) << "\"}";
        if (i + 1 < events_.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

void EventStore::Add(const CalendarEvent& e)
{
    CalendarEvent copy = e;
    if (copy.id.empty()) copy.id = GenerateId();
    events_.push_back(copy);
    Save();
}

void EventStore::Update(const CalendarEvent& e)
{
    for (auto& ev : events_) {
        if (ev.id == e.id) {
            ev = e;
            Save();
            return;
        }
    }
}

void EventStore::Remove(const std::string& id)
{
    events_.erase(std::remove_if(events_.begin(), events_.end(),
        [&id](const CalendarEvent& e){ return e.id == id; }),
        events_.end());
    Save();
}

std::vector<const CalendarEvent*> EventStore::EventsOnDate(const wxDateTime& date) const
{
    std::vector<const CalendarEvent*> result;
    for (const auto& e : events_) {
        if (e.OccursOnDate(date)) result.push_back(&e);
    }
    return result;
}
