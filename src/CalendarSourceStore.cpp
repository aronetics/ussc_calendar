#include "CalendarSourceStore.h"
#include <wx/wx.h>
#include <wx/dir.h>
#ifdef _WIN32
#  include <wx/stdpaths.h>
#endif
#include <fstream>
#include <sstream>
#include <algorithm>

const char* CalendarSourceStore::AllianceName(Alliance a)
{
    switch (a) {
        case Alliance::CSpO:      return "CSpO";
        case Alliance::NATO:      return "NATO";
        case Alliance::INDOPACOM: return "INDOPACOM";
    }
    return "";
}

CalendarSourceStore::CalendarSourceStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir)) wxMkdir(dir);
    visPath_ = std::string((dir + "/cal_visibility.json").mb_str());
    InitSources();
}

void CalendarSourceStore::InitSources()
{
    // CSpO = Combined Space Operations Center (Vandenberg SFB)
    // Members: US, UK, Australia, Canada, France, Germany, New Zealand, Japan, Norway
    sources_ = {
        // ── CSpO ────────────────────────────────────────────────────────────
        {"us",          "United States",   Alliance::CSpO,       wxColour( 26, 115, 232)},
        {"uk",          "United Kingdom",  Alliance::CSpO,       wxColour(213,   0,   0)},
        {"australia",   "Australia",       Alliance::CSpO,       wxColour( 11, 128,  67)},
        {"canada",      "Canada",          Alliance::CSpO,       wxColour(220,  20,  60)},
        {"france",      "France",          Alliance::CSpO,       wxColour(  0,  85, 164)},
        {"germany",     "Germany",         Alliance::CSpO,       wxColour( 63,  81, 181)},
        {"new_zealand", "New Zealand",     Alliance::CSpO,       wxColour(  3, 155, 229)},
        {"japan",       "Japan",           Alliance::CSpO,       wxColour(188,   0,  45)},
        {"norway",      "Norway",          Alliance::CSpO,       wxColour(239,  43,  45)},
        // ── NATO Allies (non-CSpO) ───────────────────────────────────────────
        {"italy",       "Italy",           Alliance::NATO,       wxColour(  0, 146,  70)},
        {"spain",       "Spain",           Alliance::NATO,       wxColour(170,  21,  27)},
        {"netherlands", "Netherlands",     Alliance::NATO,       wxColour(255, 102,   0)},
        {"denmark",     "Denmark",         Alliance::NATO,       wxColour(198,  12,  48)},
        {"belgium",     "Belgium",         Alliance::NATO,       wxColour(246, 191,  38)},
        {"poland",      "Poland",          Alliance::NATO,       wxColour(121, 134, 203)},
        {"czech_rep",   "Czech Republic",  Alliance::NATO,       wxColour( 17,  69, 126)},
        {"portugal",    "Portugal",        Alliance::NATO,       wxColour(  0, 102,   0)},
        {"turkey",      "Turkey",          Alliance::NATO,       wxColour(227,  10,  23)},
        {"romania",     "Romania",         Alliance::NATO,       wxColour(  0,  43, 127)},
        {"greece",      "Greece",          Alliance::NATO,       wxColour( 13,  94, 175)},
        {"luxembourg",  "Luxembourg",      Alliance::NATO,       wxColour(239,  65,  53)},
        // ── INDOPACOM Partners ───────────────────────────────────────────────
        {"south_korea", "South Korea",     Alliance::INDOPACOM,  wxColour(205,  46,  58)},
        {"singapore",   "Singapore",       Alliance::INDOPACOM,  wxColour(230, 124, 115)},
        {"india",       "India",           Alliance::INDOPACOM,  wxColour(255, 153,  51)},
        {"philippines", "Philippines",     Alliance::INDOPACOM,  wxColour(  0,  56, 168)},
        {"thailand",    "Thailand",        Alliance::INDOPACOM,  wxColour(  0,  53, 128)},
        {"malaysia",    "Malaysia",        Alliance::INDOPACOM,  wxColour(  0, 150,  71)},
    };
}

void CalendarSourceStore::LoadVisibility()
{
    std::ifstream f(visPath_);
    if (!f.is_open()) return;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    // Simple key:bool JSON — {"us":true,"uk":false,...}
    for (auto& src : sources_) {
        std::string key = "\"" + src.id + "\":";
        size_t pos = json.find(key);
        if (pos != std::string::npos) {
            pos += key.size();
            while (pos < json.size() && json[pos] == ' ') ++pos;
            src.visible = (json.substr(pos, 4) == "true");
        }
    }
}

void CalendarSourceStore::SaveVisibility() const
{
    std::ofstream f(visPath_);
    if (!f.is_open()) return;
    f << "{";
    for (size_t i = 0; i < sources_.size(); ++i) {
        f << "\"" << sources_[i].id << "\":"
          << (sources_[i].visible ? "true" : "false");
        if (i + 1 < sources_.size()) f << ",";
    }
    f << "}\n";
}

const CalendarSource* CalendarSourceStore::Find(const std::string& id) const
{
    for (const auto& s : sources_)
        if (s.id == id) return &s;
    return nullptr;
}

void CalendarSourceStore::SetVisible(const std::string& id, bool vis)
{
    for (auto& s : sources_) {
        if (s.id == id) {
            s.visible = vis;
            SaveVisibility();
            if (OnVisibilityChanged) OnVisibilityChanged();
            return;
        }
    }
}

bool CalendarSourceStore::IsVisible(const std::string& id) const
{
    const auto* s = Find(id);
    return s ? s->visible : true;
}
