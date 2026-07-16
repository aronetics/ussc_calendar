#include "UserStore.h"
#include <wx/wx.h>
#include <wx/dir.h>
#include <wx/filename.h>
#ifdef _WIN32
#  include <wx/stdpaths.h>
#endif
#include <fstream>
#include <sstream>
#include <algorithm>

// ── JSON helpers (mirrors EventStore pattern) ─────────────────────────────────

static std::string JsonEsc(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string ExtractStr(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string val;
    for (size_t i = pos; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) { val += json[++i]; }
        else if (json[i] == '"') break;
        else val += json[i];
    }
    return val;
}

static bool ExtractBool(const std::string& json, const std::string& key,
                         bool dflt = true)
{
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return dflt;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    return json.substr(pos, 4) == "true";
}

static wxColour HexColor(const std::string& hex)
{
    if (hex.size() < 7) return wxColour(26, 115, 232);
    unsigned r = 0, g = 0, b = 0;
    if (std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3)
        return wxColour(26, 115, 232);
    return wxColour(r, g, b);
}

static std::string ColorHex(const wxColour& c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
    return buf;
}

static std::vector<std::string> SplitObjects(const std::string& json)
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

// ── UserStore ────────────────────────────────────────────────────────────────

UserStore::UserStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir))
        wxMkdir(dir);
    path_ = std::string((dir + "/users.json").mb_str());
}

void UserStore::Load()
{
    std::ifstream f(path_);
    if (!f.is_open()) { CreateDefaultUsers(); return; }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    size_t arrPos = json.find("\"users\":");
    if (arrPos == std::string::npos) { CreateDefaultUsers(); return; }

    auto objs = SplitObjects(json.substr(arrPos));
    for (size_t i = 1; i < objs.size(); ++i) {
        const auto& obj = objs[i];
        UserEntry u;
        u.id      = ExtractStr(obj, "id");
        u.name    = ExtractStr(obj, "name");
        u.group   = ExtractStr(obj, "group");
        u.color   = HexColor(ExtractStr(obj, "color"));
        u.visible = ExtractBool(obj, "visible", true);
        if (!u.id.empty()) users_.push_back(u);
    }
    if (users_.empty()) CreateDefaultUsers();
}

void UserStore::Save() const
{
    std::ofstream f(path_);
    if (!f.is_open()) return;
    f << "{\"users\":[\n";
    for (size_t i = 0; i < users_.size(); ++i) {
        const auto& u = users_[i];
        f << "  {\"id\":\""      << JsonEsc(u.id)
          << "\",\"name\":\""    << JsonEsc(u.name)
          << "\",\"group\":\""   << JsonEsc(u.group)
          << "\",\"color\":\""   << ColorHex(u.color)
          << "\",\"visible\":"   << (u.visible ? "true" : "false")
          << "}";
        if (i + 1 < users_.size()) f << ",";
        f << "\n";
    }
    f << "]}\n";
}

std::vector<std::string> UserStore::Groups() const
{
    std::vector<std::string> groups;
    for (const auto& u : users_) {
        if (std::find(groups.begin(), groups.end(), u.group) == groups.end())
            groups.push_back(u.group);
    }
    return groups;
}

std::vector<const UserEntry*> UserStore::UsersInGroup(const std::string& grp) const
{
    std::vector<const UserEntry*> result;
    for (const auto& u : users_)
        if (u.group == grp) result.push_back(&u);
    return result;
}

const UserEntry* UserStore::FindUser(const std::string& id) const
{
    for (const auto& u : users_)
        if (u.id == id) return &u;
    return nullptr;
}

void UserStore::SetVisible(const std::string& id, bool vis)
{
    for (auto& u : users_) {
        if (u.id == id) {
            u.visible = vis;
            Save();
            if (OnVisibilityChanged) OnVisibilityChanged();
            return;
        }
    }
}

void UserStore::CreateDefaultUsers()
{
    users_ = {
        // Alpha Team
        {"alice",  "Alice Chen",    "Alpha Team",   wxColour(213,   0,   0), true},
        {"bob",    "Bob Smith",     "Alpha Team",   wxColour(244,  81,  30), true},
        {"carol",  "Carol Davis",   "Alpha Team",   wxColour( 51, 182, 121), true},
        {"dave",   "Dave Wilson",   "Alpha Team",   wxColour( 63,  81, 181), true},
        // Bravo Team
        {"eve",    "Eve Martinez",  "Bravo Team",   wxColour(246, 191,  38), true},
        {"frank",  "Frank Lee",     "Bravo Team",   wxColour( 11, 128,  67), true},
        {"grace",  "Grace Kim",     "Bravo Team",   wxColour(  3, 155, 229), true},
        {"henry",  "Henry Brown",   "Bravo Team",   wxColour(142,  36, 170), true},
        // Charlie Team
        {"iris",   "Iris Taylor",   "Charlie Team", wxColour(121, 134, 203), true},
        {"jack",   "Jack Johnson",  "Charlie Team", wxColour(230, 124, 115), true},
        {"kate",   "Kate Anderson", "Charlie Team", wxColour( 97,  97,  97), true},
        // Delta Team
        {"liam",   "Liam Moore",    "Delta Team",   wxColour( 26, 115, 232), true},
        {"mia",    "Mia Jackson",   "Delta Team",   wxColour(180,   0,  60), true},
        {"noah",   "Noah White",    "Delta Team",   wxColour( 30, 160, 100), true},
        {"olivia", "Olivia Harris", "Delta Team",   wxColour(  0, 120, 200), true},
    };
    Save();
}
