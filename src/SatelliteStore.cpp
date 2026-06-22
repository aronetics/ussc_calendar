#include "SatelliteStore.h"
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
#include <cstdlib>

// ── JSON helpers (same conventions as EventStore) ────────────────────────────

static std::string SsGenerateId()
{
    static int counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "sat%llx%04x",
                  (unsigned long long)wxDateTime::Now().GetTicks(), counter++);
    return std::string(buf);
}

static std::string SsJsonEscape(const std::string& s)
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

static std::string SsJsonUnescape(const std::string& s)
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

static std::string SsExtractString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string val;
    bool escape = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escape)          { val += c; escape = false; }
        else if (c == '\\')  { val += c; escape = true;  }
        else if (c == '"')   { break; }
        else                 { val += c; }
    }
    return SsJsonUnescape(val);
}

static int SsExtractInt(const std::string& json, const std::string& key, int def = 0)
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

static bool SsExtractBool(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    return json.substr(pos, 4) == "true";
}

static double SsExtractDouble(const std::string& json, const std::string& key)
{
    std::string s = SsExtractString(json, key);
    if (s.empty()) return 0.0;
    return std::atof(s.c_str());
}

static std::vector<std::string> SsSplitObjects(const std::string& json)
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

static wxDateTime SsParseDate(const std::string& s)
{
    wxDateTime dt;
    dt.ParseFormat(s, "%Y-%m-%dT%H:%M:%S");
    return dt;
}

static std::string SsFormatDate(const wxDateTime& dt)
{
    if (!dt.IsValid()) return "1970-01-01T00:00:00";
    return std::string(dt.Format("%Y-%m-%dT%H:%M:%S").mb_str());
}

// ── SatelliteStore ────────────────────────────────────────────────────────────

SatelliteStore::SatelliteStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir))
        wxMkdir(dir);
    path_ = std::string((dir + "/satellites.json").mb_str());
}

void SatelliteStore::Load()
{
    std::ifstream f(path_);
    if (!f.is_open()) return;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    size_t arrPos = json.find("\"satellites\":");
    if (arrPos == std::string::npos) return;

    std::vector<std::string> objs = SsSplitObjects(json.substr(arrPos));
    // First object is the outer wrapper; rest are satellites
    for (size_t i = 1; i < objs.size(); ++i) {
        const std::string& obj = objs[i];
        Satellite s;
        s.id          = SsExtractString(obj, "id");
        s.name        = SsExtractString(obj, "name");
        s.noradId     = SsExtractString(obj, "norad");
        s.orbitType   = SsExtractString(obj, "orbit");
        s.status      = static_cast<SatStatus>(SsExtractInt(obj, "status", 0));
        s.launchKnown = SsExtractBool(obj, "lk");
        s.launchDate  = SsParseDate(SsExtractString(obj, "launch"));

        int bc = SsExtractInt(obj, "bc", 0);
        for (int b = 0; b < bc; ++b) {
            CommBand band;
            char key[16];
            std::snprintf(key, sizeof(key), "b%dn", b);
            band.name = SsExtractString(obj, key);
            std::snprintf(key, sizeof(key), "b%dt", b);
            band.bandType = SsExtractString(obj, key);
            std::snprintf(key, sizeof(key), "b%df", b);
            band.freqMHz = SsExtractDouble(obj, key);
            std::snprintf(key, sizeof(key), "b%du", b);
            band.uplink = SsExtractBool(obj, key);
            s.bands.push_back(band);
        }

        if (!s.id.empty()) sats_.push_back(s);
    }
}

void SatelliteStore::Save() const
{
    std::ofstream f(path_);
    if (!f.is_open()) return;

    f << "{\"satellites\":[\n";
    for (size_t i = 0; i < sats_.size(); ++i) {
        const Satellite& s = sats_[i];
        f << "  {"
          << "\"id\":\""     << SsJsonEscape(s.id)        << "\","
          << "\"name\":\""   << SsJsonEscape(s.name)      << "\","
          << "\"norad\":\""  << SsJsonEscape(s.noradId)   << "\","
          << "\"orbit\":\""  << SsJsonEscape(s.orbitType) << "\","
          << "\"status\":"   << (int)s.status              << ","
          << "\"lk\":"       << (s.launchKnown ? "true" : "false") << ","
          << "\"launch\":\"" << SsFormatDate(s.launchDate) << "\","
          << "\"bc\":"       << s.bands.size();
        for (size_t b = 0; b < s.bands.size(); ++b) {
            char freqBuf[32];
            std::snprintf(freqBuf, sizeof(freqBuf), "%.3f", s.bands[b].freqMHz);
            f << ",\"b" << b << "n\":\"" << SsJsonEscape(s.bands[b].name)     << "\""
              << ",\"b" << b << "t\":\"" << SsJsonEscape(s.bands[b].bandType) << "\""
              << ",\"b" << b << "f\":\"" << freqBuf                            << "\""
              << ",\"b" << b << "u\":"   << (s.bands[b].uplink ? "true" : "false");
        }
        f << "}";
        if (i + 1 < sats_.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

void SatelliteStore::Add(const Satellite& s)
{
    Satellite copy = s;
    if (copy.id.empty()) copy.id = SsGenerateId();
    sats_.push_back(copy);
    Save();
}

void SatelliteStore::Update(const Satellite& s)
{
    for (auto& sat : sats_) {
        if (sat.id == s.id) { sat = s; Save(); return; }
    }
}

void SatelliteStore::Remove(const std::string& id)
{
    sats_.erase(std::remove_if(sats_.begin(), sats_.end(),
        [&id](const Satellite& s){ return s.id == id; }), sats_.end());
    Save();
}
