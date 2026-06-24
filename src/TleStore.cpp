#include "TleStore.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/dir.h>
#ifdef _WIN32
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

// ── WinHTTP GET helper ────────────────────────────────────────────────────────

static std::string HttpsGet(const std::wstring& host,
                             const std::wstring& path,
                             std::string& errorMsg)
{
#ifdef _WIN32
    HINTERNET hSess = WinHttpOpen(L"USSC-Calendar-TLE/1.0",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) { errorMsg = "WinHttpOpen failed"; return ""; }

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(),
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) {
        WinHttpCloseHandle(hSess);
        errorMsg = "WinHttpConnect failed";
        return "";
    }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path.c_str(),
                                         nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        errorMsg = "WinHttpOpenRequest failed";
        return "";
    }

    // Relax SSL checks for government networks that may use custom CAs
    DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));

    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        errorMsg = "HTTP request failed";
        return "";
    }

    std::string result;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(hReq, &dwSize)) break;
        if (dwSize == 0) break;
        std::vector<char> buf(dwSize + 1, 0);
        DWORD dwRead = 0;
        WinHttpReadData(hReq, buf.data(), dwSize, &dwRead);
        result.append(buf.data(), dwRead);
    } while (dwSize > 0);

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSess);
    return result;
#else
    (void)host; (void)path;
    errorMsg = "TLE fetch requires Windows build (WinHTTP)";
    return "";
#endif
}

// ── JSON helpers ─────────────────────────────────────────────────────────────

static std::string TleEsc(const std::string& s)
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

static std::string TleStr(const std::string& j, const std::string& key)
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

static double TleDbl(const std::string& j, const std::string& key)
{
    std::string v = TleStr(j, key);
    return v.empty() ? 0.0 : std::atof(v.c_str());
}

static int TleInt(const std::string& j, const std::string& key)
{
    std::string t = "\"" + key + "\":";
    size_t p = j.find(t);
    if (p == std::string::npos) return 0;
    p += t.size();
    while (p < j.size() && j[p] == ' ') ++p;
    return std::atoi(j.c_str() + p);
}

static std::vector<std::string> TleObjects(const std::string& j)
{
    std::vector<std::string> out;
    int depth = 0; size_t start = std::string::npos;
    for (size_t i = 0; i < j.size(); ++i) {
        if (j[i] == '{') { if (!depth) start = i; ++depth; }
        else if (j[i] == '}') {
            --depth;
            if (!depth && start != std::string::npos) {
                out.push_back(j.substr(start, i-start+1));
                start = std::string::npos;
            }
        }
    }
    return out;
}

// ── TleStore ─────────────────────────────────────────────────────────────────

TleStore::TleStore()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
#else
    wxString dir = wxGetHomeDir() + "/.ussc_calendar";
#endif
    if (!wxDirExists(dir)) wxMkdir(dir);
    path_ = std::string((dir + "/tle_cache.json").mb_str());
}

void TleStore::Load()
{
    std::ifstream f(path_);
    if (!f.is_open()) return;
    std::ostringstream ss; ss << f.rdbuf();
    std::string json = ss.str();
    size_t ap = json.find("\"tles\":");
    if (ap == std::string::npos) return;
    for (const auto& obj : TleObjects(json.substr(ap))) {
        int id = TleInt(obj, "norad");
        if (!id) continue;
        TleEntry e;
        e.noradId   = id;
        e.name      = TleStr(obj, "name");
        e.line1     = TleStr(obj, "l1");
        e.line2     = TleStr(obj, "l2");
        e.fetchedJd = TleDbl(obj, "jd");
        e.parsed    = ParseTle(e.name, e.line1, e.line2);
        cache_[id]  = e;
    }
}

void TleStore::Save() const
{
    std::ofstream f(path_);
    if (!f.is_open()) return;
    f << "{\"tles\":[\n";
    bool first = true;
    for (const auto& kv : cache_) {
        if (!first) f << ",\n"; first = false;
        const auto& e = kv.second;
        f << "  {\"norad\":" << e.noradId
          << ",\"name\":\""  << TleEsc(e.name)  << "\""
          << ",\"l1\":\""    << TleEsc(e.line1) << "\""
          << ",\"l2\":\""    << TleEsc(e.line2) << "\""
          << ",\"jd\":\""    << e.fetchedJd     << "\"}";
    }
    f << "\n]}\n";
}

int TleStore::FetchFromCelestrak(const std::vector<int>& noradIds,
                                  std::string& errorMsg)
{
    int fetched = 0;
    double nowJd = 2451545.0 + wxDateTime::Now().GetTicks() / 86400.0;

    for (int norad : noradIds) {
        if (norad <= 0) continue;

        char pathBuf[128];
        std::snprintf(pathBuf, sizeof(pathBuf),
                      "/satcat/tle.php?CATNR=%d", norad);
        std::wstring wpath(pathBuf, pathBuf + std::strlen(pathBuf));

        std::string err;
        std::string body = HttpsGet(L"celestrak.org", wpath, err);
        if (!err.empty() || body.empty()) {
            if (errorMsg.empty()) errorMsg = err;
            continue;
        }

        // Celestrak returns 3 lines: name, line1, line2
        std::istringstream ss(body);
        std::string nameLine, l1, l2;
        if (!std::getline(ss, nameLine) ||
            !std::getline(ss, l1)       ||
            !std::getline(ss, l2)) continue;

        // Trim trailing whitespace/CR
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                                  s.back() == ' '))
                s.pop_back();
        };
        trim(nameLine); trim(l1); trim(l2);

        if (l1.empty() || l1[0] != '1') continue;

        TleEntry e;
        e.noradId   = norad;
        e.name      = nameLine;
        e.line1     = l1;
        e.line2     = l2;
        e.fetchedJd = nowJd;
        e.parsed    = ParseTle(e.name, e.line1, e.line2);
        cache_[norad] = e;
        ++fetched;
    }

    if (fetched > 0) Save();
    return fetched;
}

const TleEntry* TleStore::Get(int noradId) const
{
    auto it = cache_.find(noradId);
    return (it != cache_.end()) ? &it->second : nullptr;
}

void TleStore::PurgeOlderThan(double maxAgeDays)
{
    double nowJd = 2451545.0 + wxDateTime::Now().GetTicks() / 86400.0;
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (nowJd - it->second.fetchedJd > maxAgeDays)
            it = cache_.erase(it);
        else
            ++it;
    }
    Save();
}
