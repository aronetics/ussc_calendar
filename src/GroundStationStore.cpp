#include "GroundStationStore.h"
#include "LanClient.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/dir.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

static std::string GsId()
{
    static int cnt = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "gs%llx%04x",
                  (unsigned long long)wxDateTime::Now().GetTicks(), cnt++);
    return buf;
}

static std::string GsEsc(const std::string& s)
{
    std::string o;
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else                o += c;
    }
    return o;
}

static std::string GsStr(const std::string& j, const std::string& key)
{
    std::string t = "\"" + key + "\":\"";
    size_t p = j.find(t);
    if (p == std::string::npos) return "";
    p += t.size();
    std::string v;
    for (; p < j.size() && j[p] != '"'; ++p) {
        if (j[p] == '\\' && p+1 < j.size()) v += j[++p];
        else v += j[p];
    }
    return v;
}

static double GsDbl(const std::string& j, const std::string& key)
{
    std::string v = GsStr(j, key);
    return v.empty() ? 0.0 : std::atof(v.c_str());
}

static bool GsBool(const std::string& j, const std::string& key)
{
    std::string t = "\"" + key + "\":";
    size_t p = j.find(t);
    if (p == std::string::npos) return false;
    p += t.size();
    return j.compare(p, 4, "true") == 0;
}

static std::vector<std::string> GsObjects(const std::string& j)
{
    std::vector<std::string> out;
    int depth = 0; size_t start = std::string::npos;
    for (size_t i = 0; i < j.size(); ++i) {
        if (j[i] == '{') { if (!depth) start = i; ++depth; }
        else if (j[i] == '}') { --depth; if (!depth && start != std::string::npos) { out.push_back(j.substr(start, i-start+1)); start = std::string::npos; } }
    }
    return out;
}

GroundStationStore::GroundStationStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir)) wxMkdir(dir);
    path_ = std::string((dir + "/groundstations.json").mb_str());
}

void GroundStationStore::Load()
{
    if (client_) {
        stations_.clear();
        std::string err;
        std::string json = client_->Get("/api/groundstations", err);
        if (json.empty()) return;
        size_t ap = json.find("\"groundstations\":");
        if (ap == std::string::npos) return;
        auto objs2 = GsObjects(json.substr(ap));
        for (size_t i = 1; i < objs2.size(); ++i) {
            const std::string& obj = objs2[i];
            GroundStation g;
            g.id   = GsStr(obj, "id");
            g.name = GsStr(obj, "name");
            g.latDeg   = GsDbl(obj, "lat");
            g.lonDeg   = GsDbl(obj, "lon");
            g.altKm    = GsDbl(obj, "alt");
            g.minElDeg = GsDbl(obj, "minEl");
            g.active = GsBool(obj, "active");
            if (!g.id.empty()) stations_.push_back(g);
        }
        return;
    }

    std::ifstream f(path_);
    if (!f.is_open()) return;
    std::ostringstream ss; ss << f.rdbuf();
    std::string json = ss.str();
    size_t ap = json.find("\"stations\":");
    if (ap == std::string::npos) return;
    auto objs = GsObjects(json.substr(ap));
    for (size_t i = 1; i < objs.size(); ++i) {
        const std::string& o = objs[i];
        GroundStation gs;
        gs.id       = GsStr(o, "id");
        gs.name     = GsStr(o, "name");
        gs.latDeg   = GsDbl(o, "lat");
        gs.lonDeg   = GsDbl(o, "lon");
        gs.altKm    = GsDbl(o, "alt");
        gs.minElDeg = GsDbl(o, "minel");
        gs.active   = GsBool(o, "active");
        if (!gs.id.empty()) stations_.push_back(gs);
    }
}

void GroundStationStore::Save() const
{
    std::ofstream f(path_);
    if (!f.is_open()) return;
    f << "{\"stations\":[\n";
    for (size_t i = 0; i < stations_.size(); ++i) {
        const auto& g = stations_[i];
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  {\"id\":\"%s\",\"name\":\"%s\","
            "\"lat\":\"%.6f\",\"lon\":\"%.6f\","
            "\"alt\":\"%.4f\",\"minel\":\"%.2f\","
            "\"active\":%s}",
            GsEsc(g.id).c_str(), GsEsc(g.name).c_str(),
            g.latDeg, g.lonDeg, g.altKm, g.minElDeg,
            g.active ? "true" : "false");
        f << buf;
        if (i + 1 < stations_.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

static std::string GsToJsonBody(const GroundStation& g)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"id\":\"%s\",\"name\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,"
        "\"alt\":%.3f,\"minEl\":%.2f,\"active\":%s}",
        GsEsc(g.id).c_str(), GsEsc(g.name).c_str(),
        g.latDeg, g.lonDeg, g.altKm, g.minElDeg,
        g.active ? "true" : "false");
    return buf;
}

void GroundStationStore::Add(const GroundStation& gs)
{
    GroundStation copy = gs;
    if (copy.id.empty()) copy.id = GsId();
    stations_.push_back(copy);
    if (client_) { std::string err; client_->Post("/api/groundstations", GsToJsonBody(copy), err); }
    else          Save();
}

void GroundStationStore::Update(const GroundStation& gs)
{
    for (auto& s : stations_) {
        if (s.id == gs.id) {
            s = gs;
            if (client_) { std::string err; client_->Put("/api/groundstations/" + gs.id, GsToJsonBody(gs), err); }
            else          Save();
            return;
        }
    }
}

void GroundStationStore::Remove(const std::string& id)
{
    stations_.erase(std::remove_if(stations_.begin(), stations_.end(),
        [&id](const GroundStation& g){ return g.id == id; }), stations_.end());
    if (client_) { std::string err; client_->Delete("/api/groundstations/" + id, err); }
    else          Save();
}
