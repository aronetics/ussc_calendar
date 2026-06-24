#include "EventStore.h"
#include "LanClient.h"
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

static int ExtractJsonInt(const std::string& json, const std::string& key, int def = 0)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (pos >= json.size() || !std::isdigit((unsigned char)json[pos])) return def;
    int val = 0;
    while (pos < json.size() && std::isdigit((unsigned char)json[pos]))
        val = val * 10 + (json[pos++] - '0');
    return val;
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
    sscanf_s(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
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

// Returns how many calendar days separate two midnight-normalised dates.
// Uses seconds to avoid DST-induced off-by-one errors.
static long DaysBetween(const wxDateTime& earlier, const wxDateTime& later)
{
    return (long)((later.GetTicks() - earlier.GetTicks() + 43200) / 86400);
}

// Checks whether `day` (midnight-normalised) is a valid occurrence start for ev.
static bool IsOccurrenceDay(const CalendarEvent& ev, const wxDateTime& day)
{
    wxDateTime startDay(ev.start.GetDay(), ev.start.GetMonth(), ev.start.GetYear(), 0, 0, 0);
    if (day < startDay) return false;

    if (ev.recurrence.until.IsValid()) {
        wxDateTime untilDay(ev.recurrence.until.GetDay(),
                            ev.recurrence.until.GetMonth(),
                            ev.recurrence.until.GetYear(), 0, 0, 0);
        if (day > untilDay) return false;
    }

    int iv = ev.recurrence.interval > 0 ? ev.recurrence.interval : 1;
    long diff = DaysBetween(startDay, day);
    int occNum = 0;
    bool hit   = false;

    switch (ev.recurrence.freq) {
        case RecurFreq::Daily:
            hit    = (diff % iv == 0);
            occNum = (int)(diff / iv) + 1;
            break;
        case RecurFreq::Weekly:
            hit    = (diff % (iv * 7) == 0);
            occNum = (int)(diff / (iv * 7)) + 1;
            break;
        case RecurFreq::Monthly: {
            if (day.GetDay() != startDay.GetDay()) return false;
            int months = (day.GetYear() - startDay.GetYear()) * 12
                       + ((int)day.GetMonth() - (int)startDay.GetMonth());
            hit    = (months >= 0 && months % iv == 0);
            occNum = months / iv + 1;
            break;
        }
        case RecurFreq::Yearly: {
            if (day.GetDay()   != startDay.GetDay() ||
                day.GetMonth() != startDay.GetMonth()) return false;
            int years = day.GetYear() - startDay.GetYear();
            hit    = (years >= 0 && years % iv == 0);
            occNum = years / iv + 1;
            break;
        }
        default:
            return false;
    }

    if (!hit) return false;
    if (ev.recurrence.count > 0 && occNum > ev.recurrence.count) return false;
    return true;
}

bool CalendarEvent::OccursOnDate(const wxDateTime& d) const
{
    if (!start.IsValid()) return false;

    wxDateTime day(d.GetDay(), d.GetMonth(), d.GetYear(), 0, 0, 0);

    if (recurrence.freq == RecurFreq::None) {
        // Original single-occurrence logic
        wxDateTime dayEnd = day;
        dayEnd.Add(wxTimeSpan::Day());
        if (allDay) {
            wxDateTime s(start.GetDay(), start.GetMonth(), start.GetYear(), 0, 0, 0);
            wxDateTime e = end.IsValid()
                         ? wxDateTime(end.GetDay(), end.GetMonth(), end.GetYear(), 0, 0, 0)
                         : s;
            e.Add(wxTimeSpan::Day());
            return s < dayEnd && e > day;
        }
        wxDateTime e = end.IsValid() ? end : start;
        return start < dayEnd && e > day;
    }

    // Recurring: how many extra days does one occurrence span?
    wxDateTime startDay(start.GetDay(), start.GetMonth(), start.GetYear(), 0, 0, 0);
    int spanDays = 0;
    if (end.IsValid() && end > start) {
        wxDateTime endDay(end.GetDay(), end.GetMonth(), end.GetYear(), 0, 0, 0);
        long s = DaysBetween(startDay, endDay);
        if (s > 0) spanDays = (int)s;
    }

    // Check if any occurrence starting in [day-spanDays, day] covers `day`
    for (int i = 0; i <= spanDays; ++i) {
        wxDateTime candidate = day;
        candidate.Subtract(wxDateSpan::Days(i));
        if (IsOccurrenceDay(*this, candidate))
            return true;
    }
    return false;
}

bool CalendarEvent::StartsOnDate(const wxDateTime& d) const
{
    if (!start.IsValid()) return false;
    if (recurrence.freq == RecurFreq::None) {
        return start.GetDay()   == d.GetDay()   &&
               start.GetMonth() == d.GetMonth() &&
               start.GetYear()  == d.GetYear();
    }
    wxDateTime day(d.GetDay(), d.GetMonth(), d.GetYear(), 0, 0, 0);
    return IsOccurrenceDay(*this, day);
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

// Serialize one event to a JSON object string (matches Save() field names)
static std::string SerializeOneEvent(const CalendarEvent& e)
{
    std::ostringstream o;
    o << "{\"id\":\""   << JsonEscape(e.id)          << "\","
      << "\"title\":\"" << JsonEscape(e.title)        << "\","
      << "\"desc\":\""  << JsonEscape(e.description)  << "\","
      << "\"loc\":\""   << JsonEscape(e.location)     << "\","
      << "\"start\":\"" << FormatDateTime(e.start)    << "\","
      << "\"end\":\""   << FormatDateTime(e.end)      << "\","
      << "\"allDay\":"  << (e.allDay ? "true" : "false") << ","
      << "\"color\":\"" << ColourToHex(e.color)       << "\","
      << "\"participants\":[";
    for (size_t i = 0; i < e.participants.size(); ++i) {
        o << "\"" << JsonEscape(e.participants[i]) << "\"";
        if (i + 1 < e.participants.size()) o << ",";
    }
    o << "],\"src\":\"" << JsonEscape(e.calendarSource) << "\"";
    if (e.recurrence.freq != RecurFreq::None) {
        o << ",\"rf\":" << (int)e.recurrence.freq
          << ",\"ri\":" << e.recurrence.interval;
        if (e.recurrence.until.IsValid())
            o << ",\"ru\":\"" << FormatDateTime(e.recurrence.until) << "\"";
        if (e.recurrence.count > 0)
            o << ",\"rc\":" << e.recurrence.count;
    }
    o << "}";
    return o.str();
}

void EventStore::Load()
{
    if (client_) {
        events_.clear();
        std::string err;
        std::string json = client_->Get("/api/events", err);
        if (json.empty()) return;
        // Fall through to parse the JSON string using same logic as file-based load
        std::string srcJson = json;
        size_t arrPos = srcJson.find("\"events\":");
        if (arrPos == std::string::npos) return;
        auto objs2 = SplitJsonObjects(srcJson.substr(arrPos));
        for (size_t i = 1; i < objs2.size(); ++i) {
            const std::string& obj = objs2[i];
            CalendarEvent e;
            e.id          = ExtractJsonString(obj, "id");
            e.title       = ExtractJsonString(obj, "title");
            e.description = ExtractJsonString(obj, "desc");
            e.location    = ExtractJsonString(obj, "loc");
            e.allDay      = ExtractJsonBool  (obj, "allDay");
            e.start       = ParseDateTime(ExtractJsonString(obj, "start"));
            e.end         = ParseDateTime(ExtractJsonString(obj, "end"));
            e.color       = HexToColour(ExtractJsonString(obj, "color"));
            e.participants   = ExtractJsonArray(obj, "participants");
            e.calendarSource = ExtractJsonString(obj, "src");
            e.recurrence.freq     = static_cast<RecurFreq>(ExtractJsonInt(obj, "rf", 0));
            e.recurrence.interval = ExtractJsonInt(obj, "ri", 1);
            e.recurrence.count    = ExtractJsonInt(obj, "rc", 0);
            std::string ru = ExtractJsonString(obj, "ru");
            if (!ru.empty()) e.recurrence.until = ParseDateTime(ru);
            if (!e.id.empty()) events_.push_back(e);
        }
        return;
    }

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
        e.recurrence.freq     = static_cast<RecurFreq>(ExtractJsonInt(obj, "rf", 0));
        e.recurrence.interval = ExtractJsonInt(obj, "ri", 1);
        e.recurrence.count    = ExtractJsonInt(obj, "rc", 0);
        std::string ru = ExtractJsonString(obj, "ru");
        if (!ru.empty()) e.recurrence.until = ParseDateTime(ru);
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
        f << "],\"src\":\"" << JsonEscape(e.calendarSource) << "\"";
        if (e.recurrence.freq != RecurFreq::None) {
            f << ",\"rf\":" << (int)e.recurrence.freq
              << ",\"ri\":" << e.recurrence.interval;
            if (e.recurrence.until.IsValid())
                f << ",\"ru\":\"" << FormatDateTime(e.recurrence.until) << "\"";
            if (e.recurrence.count > 0)
                f << ",\"rc\":" << e.recurrence.count;
        }
        f << "}";
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
    if (client_) {
        std::string err;
        client_->Post("/api/events", SerializeOneEvent(copy), err);
    } else {
        Save();
    }
}

void EventStore::Update(const CalendarEvent& e)
{
    for (auto& ev : events_) {
        if (ev.id == e.id) {
            ev = e;
            if (client_) {
                std::string err;
                client_->Put("/api/events/" + e.id, SerializeOneEvent(e), err);
            } else {
                Save();
            }
            return;
        }
    }
}

void EventStore::Remove(const std::string& id)
{
    events_.erase(std::remove_if(events_.begin(), events_.end(),
        [&id](const CalendarEvent& e){ return e.id == id; }),
        events_.end());
    if (client_) {
        std::string err;
        client_->Delete("/api/events/" + id, err);
    } else {
        Save();
    }
}

std::vector<const CalendarEvent*> EventStore::EventsOnDate(const wxDateTime& date) const
{
    std::vector<const CalendarEvent*> result;
    for (const auto& e : events_) {
        if (e.OccursOnDate(date)) result.push_back(&e);
    }
    return result;
}
