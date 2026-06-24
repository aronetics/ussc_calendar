// winsock2.h must come before any windows.h / wx/wx.h include
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

#include "LanServer.h"
#include <wx/wx.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

// ── HTTP helpers ─────────────────────────────────────────────────────────────

static std::string MakeResponse(int code, const std::string& body)
{
    const char* phrase = "OK";
    if (code == 201) phrase = "Created";
    else if (code == 400) phrase = "Bad Request";
    else if (code == 401) phrase = "Unauthorized";
    else if (code == 404) phrase = "Not Found";
    else if (code == 500) phrase = "Internal Server Error";

    char hdr[256];
    std::snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, phrase, body.size());
    return std::string(hdr) + body;
}

static const int kMaxBodyBytes = 4 * 1024 * 1024; // 4 MB cap; prevents memory exhaustion

static std::string ReadFullRequest(SOCKET sock)
{
    // 5-second receive timeout — prevents slow-client thread exhaustion
    DWORD tvMs = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tvMs, sizeof(tvMs));

    std::string data;
    data.reserve(4096);
    char buf[4096];

    while (true) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        data.append(buf, n);

        size_t hdrEnd = data.find("\r\n\r\n");
        if (hdrEnd == std::string::npos) continue;

        // Find Content-Length
        std::string hdrBlock = data.substr(0, hdrEnd);
        std::string lower = hdrBlock;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        int clen = 0;
        size_t clPos = lower.find("content-length: ");
        if (clPos != std::string::npos) {
            clen = std::atoi(hdrBlock.c_str() + clPos + 16);
            if (clen < 0 || clen > kMaxBodyBytes) clen = 0; // clamp malicious values
        }

        int bodyRx = (int)data.size() - (int)(hdrEnd + 4);
        if (bodyRx >= clen) break;
    }
    return data;
}

// Parse "METHOD /path HTTP/1.1" into method and path
static bool ParseRequestLine(const std::string& raw,
                              std::string& method,
                              std::string& path,
                              std::string& body,
                              std::string& authToken)
{
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos) return false;
    std::string line = raw.substr(0, lineEnd);

    size_t s1 = line.find(' ');
    if (s1 == std::string::npos) return false;
    method = line.substr(0, s1);

    size_t s2 = line.find(' ', s1 + 1);
    path   = (s2 == std::string::npos) ? line.substr(s1 + 1) : line.substr(s1 + 1, s2 - s1 - 1);

    // Extract body
    size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd != std::string::npos) body = raw.substr(hdrEnd + 4);

    // Extract Authorization: Bearer TOKEN
    std::string lower = raw.substr(0, hdrEnd == std::string::npos ? raw.size() : hdrEnd);
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
    size_t aPos = lower.find("authorization: bearer ");
    if (aPos != std::string::npos) {
        aPos += 22;
        size_t aEnd = raw.find("\r\n", aPos);
        authToken = raw.substr(aPos, aEnd == std::string::npos ? std::string::npos : aEnd - aPos);
        while (!authToken.empty() && (authToken.back() == '\r' || authToken.back() == ' '))
            authToken.pop_back();
    }

    return true;
}

// ── JSON micro-helpers (mirrors EventStore) ──────────────────────────────────

static std::string JEsc(const std::string& s)
{
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
        }
    }
    return o;
}

static std::string ExtStr(const std::string& j, const std::string& k)
{
    std::string srch = "\"" + k + "\":\"";
    size_t p = j.find(srch);
    if (p == std::string::npos) return {};
    p += srch.size();
    std::string v;
    for (size_t i = p; i < j.size(); ++i) {
        if (j[i] == '\\' && i + 1 < j.size()) {
            char c = j[++i];
            if (c == 'n') v += '\n'; else if (c == 'r') v += '\r';
            else if (c == 't') v += '\t'; else v += c;
        } else if (j[i] == '"') break;
        else v += j[i];
    }
    return v;
}

static std::string ColourHex(const wxColour& c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
    return buf;
}

static std::string FmtDt(const wxDateTime& dt)
{
    if (!dt.IsValid()) return "1970-01-01T00:00:00";
    return std::string(dt.Format("%Y-%m-%dT%H:%M:%S").mb_str());
}

// ── Serialise one CalendarEvent to JSON object ───────────────────────────────

static std::string EventToJson(const CalendarEvent& e)
{
    std::ostringstream o;
    o << "{\"id\":\""     << JEsc(e.id)          << "\","
      << "\"title\":\""   << JEsc(e.title)        << "\","
      << "\"desc\":\""    << JEsc(e.description)  << "\","
      << "\"loc\":\""     << JEsc(e.location)     << "\","
      << "\"start\":\""   << FmtDt(e.start)       << "\","
      << "\"end\":\""     << FmtDt(e.end)         << "\","
      << "\"allDay\":"    << (e.allDay ? "true" : "false") << ","
      << "\"color\":\""   << ColourHex(e.color)   << "\","
      << "\"participants\":[";
    for (size_t i = 0; i < e.participants.size(); ++i) {
        o << "\"" << JEsc(e.participants[i]) << "\"";
        if (i + 1 < e.participants.size()) o << ",";
    }
    o << "],\"src\":\"" << JEsc(e.calendarSource) << "\"";
    if (e.recurrence.freq != RecurFreq::None) {
        o << ",\"rf\":" << (int)e.recurrence.freq
          << ",\"ri\":" << e.recurrence.interval;
        if (e.recurrence.until.IsValid())
            o << ",\"ru\":\"" << FmtDt(e.recurrence.until) << "\"";
        if (e.recurrence.count > 0)
            o << ",\"rc\":" << e.recurrence.count;
    }
    o << "}";
    return o.str();
}

// Deserialise one CalendarEvent JSON object
static CalendarEvent EventFromJson(const std::string& obj)
{
    CalendarEvent e;
    e.id          = ExtStr(obj, "id");
    e.title       = ExtStr(obj, "title");
    e.description = ExtStr(obj, "desc");
    e.location    = ExtStr(obj, "loc");
    e.calendarSource = ExtStr(obj, "src");

    std::string startStr = ExtStr(obj, "start");
    std::string endStr   = ExtStr(obj, "end");
    e.start.ParseFormat(startStr, "%Y-%m-%dT%H:%M:%S");
    e.end  .ParseFormat(endStr,   "%Y-%m-%dT%H:%M:%S");

    std::string colorHex = ExtStr(obj, "color");
    if (colorHex.size() >= 7) {
        unsigned r = 0, g = 0, b = 0;
        sscanf_s(colorHex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
        e.color = wxColour(r, g, b);
    }
    // allDay
    size_t adPos = obj.find("\"allDay\":");
    if (adPos != std::string::npos)
        e.allDay = obj.substr(adPos + 9, 4) == "true";
    return e;
}

// Serialise Satellite to JSON
static std::string SatToJson(const Satellite& s)
{
    std::ostringstream o;
    o << "{\"id\":\""     << JEsc(s.id)      << "\","
      << "\"name\":\""    << JEsc(s.name)    << "\","
      << "\"norad\":\""   << JEsc(s.noradId) << "\","
      << "\"orbit\":\""   << JEsc(s.orbitType) << "\","
      << "\"status\":"    << (int)s.status   << "}";
    return o.str();
}

static Satellite SatFromJson(const std::string& obj)
{
    Satellite s;
    s.id        = ExtStr(obj, "id");
    s.name      = ExtStr(obj, "name");
    s.noradId   = ExtStr(obj, "norad");
    s.orbitType = ExtStr(obj, "orbit");
    size_t sp = obj.find("\"status\":");
    if (sp != std::string::npos) s.status = (SatStatus)std::atoi(obj.c_str() + sp + 9);
    return s;
}

// Serialise GroundStation to JSON
static std::string GsToJson(const GroundStation& g)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"id\":\"%s\",\"name\":\"%s\","
        "\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.3f,"
        "\"minEl\":%.2f,\"active\":%s}",
        JEsc(g.id).c_str(), JEsc(g.name).c_str(),
        g.latDeg, g.lonDeg, g.altKm, g.minElDeg,
        g.active ? "true" : "false");
    return std::string(buf);
}

static GroundStation GsFromJson(const std::string& obj)
{
    GroundStation g;
    g.id   = ExtStr(obj, "id");
    g.name = ExtStr(obj, "name");

    auto ExtDbl = [&](const std::string& k, double def) -> double {
        std::string srch = "\"" + k + "\":";
        size_t p = obj.find(srch);
        if (p == std::string::npos) return def;
        return std::atof(obj.c_str() + p + srch.size());
    };
    g.latDeg  = ExtDbl("lat",   0.0);
    g.lonDeg  = ExtDbl("lon",   0.0);
    g.altKm   = ExtDbl("alt",   0.0);
    g.minElDeg= ExtDbl("minEl", 5.0);
    size_t ap = obj.find("\"active\":");
    if (ap != std::string::npos) g.active = obj.substr(ap + 9, 4) == "true";
    return g;
}

// ── Token management ─────────────────────────────────────────────────────────

std::string LanServer::IssueToken(const std::string& user)
{
    // 128 bits from the OS CSPRNG — not predictable from time or process state
    UCHAR bytes[16] = {};
    BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    char tok[33] = {};
    for (int i = 0; i < 16; ++i)
        std::snprintf(tok + i * 2, 3, "%02x", bytes[i]);
    std::string t(tok);
    std::lock_guard<std::mutex> lk(tokenMutex_);
    tokens_[t] = user;
    return t;
}

bool LanServer::ValidateToken(const std::string& token) const
{
    std::lock_guard<std::mutex> lk(tokenMutex_);
    return tokens_.find(token) != tokens_.end();
}

// ── Request handlers ─────────────────────────────────────────────────────────

std::string LanServer::HandleAuth(const std::string& body)
{
    std::string user = ExtStr(body, "user");
    std::string pass = ExtStr(body, "pass");

    if (user.empty() || pass.empty())
        return "{\"ok\":false,\"msg\":\"Missing user or pass\"}";

    if (!authFn_(user, pass))
        return "{\"ok\":false,\"msg\":\"Invalid credentials\"}";

    std::string tok = IssueToken(user);
    return "{\"ok\":true,\"token\":\"" + tok + "\",\"user\":\"" + JEsc(user) + "\"}";
}

std::string LanServer::HandleEvents(const std::string& method,
                                     const std::string& id,
                                     const std::string& body,
                                     int& st)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (method == "GET") {
        std::ostringstream o;
        o << "{\"events\":[\n";
        const auto& evs = ev_.Events();
        for (size_t i = 0; i < evs.size(); ++i) {
            o << EventToJson(evs[i]);
            if (i + 1 < evs.size()) o << ",";
            o << "\n";
        }
        o << "]}";
        st = 200;
        return o.str();
    }

    if (method == "POST") {
        CalendarEvent e = EventFromJson(body);
        if (e.title.empty()) { st = 400; return "{\"ok\":false,\"msg\":\"Missing title\"}"; }
        ev_.Add(e);
        // The Add() assigns an ID; find it
        const auto& evs = ev_.Events();
        std::string newId;
        for (auto it = evs.rbegin(); it != evs.rend(); ++it)
            if (it->title == e.title) { newId = it->id; break; }
        st = 201;
        return "{\"ok\":true,\"id\":\"" + newId + "\"}";
    }

    if (method == "PUT" && !id.empty()) {
        CalendarEvent e = EventFromJson(body);
        e.id = id;
        ev_.Update(e);
        st = 200;
        return "{\"ok\":true}";
    }

    if (method == "DELETE" && !id.empty()) {
        ev_.Remove(id);
        st = 200;
        return "{\"ok\":true}";
    }

    st = 400;
    return "{\"ok\":false,\"msg\":\"Bad request\"}";
}

std::string LanServer::HandleSats(const std::string& method,
                                   const std::string& id,
                                   const std::string& body,
                                   int& st)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (method == "GET") {
        std::ostringstream o;
        o << "{\"satellites\":[\n";
        const auto& sats = sat_.Satellites();
        for (size_t i = 0; i < sats.size(); ++i) {
            o << SatToJson(sats[i]);
            if (i + 1 < sats.size()) o << ",";
            o << "\n";
        }
        o << "]}";
        st = 200;
        return o.str();
    }

    if (method == "POST") {
        Satellite s = SatFromJson(body);
        if (s.name.empty()) { st = 400; return "{\"ok\":false,\"msg\":\"Missing name\"}"; }
        sat_.Add(s);
        st = 201;
        return "{\"ok\":true}";
    }

    if (method == "PUT" && !id.empty()) {
        Satellite s = SatFromJson(body);
        s.id = id;
        sat_.Update(s);
        st = 200;
        return "{\"ok\":true}";
    }

    if (method == "DELETE" && !id.empty()) {
        sat_.Remove(id);
        st = 200;
        return "{\"ok\":true}";
    }

    st = 400;
    return "{\"ok\":false}";
}

std::string LanServer::HandleGs(const std::string& method,
                                 const std::string& id,
                                 const std::string& body,
                                 int& st)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (method == "GET") {
        std::ostringstream o;
        o << "{\"groundstations\":[\n";
        const auto& gss = gs_.Stations();
        for (size_t i = 0; i < gss.size(); ++i) {
            o << GsToJson(gss[i]);
            if (i + 1 < gss.size()) o << ",";
            o << "\n";
        }
        o << "]}";
        st = 200;
        return o.str();
    }

    if (method == "POST") {
        GroundStation g = GsFromJson(body);
        if (g.name.empty()) { st = 400; return "{\"ok\":false,\"msg\":\"Missing name\"}"; }
        gs_.Add(g);
        st = 201;
        return "{\"ok\":true}";
    }

    if (method == "PUT" && !id.empty()) {
        GroundStation g = GsFromJson(body);
        g.id = id;
        gs_.Update(g);
        st = 200;
        return "{\"ok\":true}";
    }

    if (method == "DELETE" && !id.empty()) {
        gs_.Remove(id);
        st = 200;
        return "{\"ok\":true}";
    }

    st = 400;
    return "{\"ok\":false}";
}

std::string LanServer::HandleUsers(int& st)
{
    std::lock_guard<std::mutex> lk(mutex_);
    std::ostringstream o;
    o << "{\"users\":[\n";
    const auto& users = usr_.Users();
    for (size_t i = 0; i < users.size(); ++i) {
        const auto& u = users[i];
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "{\"id\":\"%s\",\"name\":\"%s\",\"group\":\"%s\"}",
            JEsc(u.id).c_str(), JEsc(u.name).c_str(), JEsc(u.group).c_str());
        o << buf;
        if (i + 1 < users.size()) o << ",";
        o << "\n";
    }
    o << "]}";
    st = 200;
    return o.str();
}

// ── Router ───────────────────────────────────────────────────────────────────

std::string LanServer::Dispatch(const std::string& method,
                                 const std::string& path,
                                 const std::string& body,
                                 const std::string& token,
                                 int& st)
{
    // Always allow info and auth
    if (path == "/api/info") {
        st = 200;
        return "{\"app\":\"USSC Calendar\",\"version\":\"1.0\",\"status\":\"ok\"}";
    }

    if (path == "/api/auth") {
        st = 200;
        return HandleAuth(body);
    }

    // Handle preflight
    if (method == "OPTIONS") { st = 200; return "{}"; }

    // All other routes require a valid token
    if (!ValidateToken(token)) {
        st = 401;
        return "{\"ok\":false,\"msg\":\"Unauthorized\"}";
    }

    // Route: /api/events[/id]
    if (path == "/api/events" || path.substr(0, 12) == "/api/events/") {
        std::string id = (path.size() > 12) ? path.substr(12) : "";
        return HandleEvents(method, id, body, st);
    }

    // Route: /api/satellites[/id]
    if (path == "/api/satellites" || path.substr(0, 16) == "/api/satellites/") {
        std::string id = (path.size() > 16) ? path.substr(16) : "";
        return HandleSats(method, id, body, st);
    }

    // Route: /api/groundstations[/id]
    if (path == "/api/groundstations" || path.substr(0, 20) == "/api/groundstations/") {
        std::string id = (path.size() > 20) ? path.substr(20) : "";
        return HandleGs(method, id, body, st);
    }

    if (path == "/api/users") return HandleUsers(st);

    st = 404;
    return "{\"ok\":false,\"msg\":\"Not found\"}";
}

// ── Connection handler (called from worker threads) ───────────────────────────

void LanServer::HandleConnection(uintptr_t rawSock)
{
    SOCKET sock = (SOCKET)rawSock;
    std::string raw = ReadFullRequest(sock);
    if (raw.empty()) { closesocket(sock); return; }

    std::string method, path, body, token;
    if (!ParseRequestLine(raw, method, path, body, token)) {
        closesocket(sock);
        return;
    }

    // Strip query string
    size_t qp = path.find('?');
    if (qp != std::string::npos) path = path.substr(0, qp);

    int st = 500;
    std::string respBody = Dispatch(method, path, body, token, st);
    std::string response = MakeResponse(st, respBody);

    send(sock, response.c_str(), (int)response.size(), 0);
    closesocket(sock);
}

// ── Thread pool ───────────────────────────────────────────────────────────────

void LanServer::EnqueueSocket(uintptr_t sock)
{
    {
        std::lock_guard<std::mutex> lk(workMutex_);
        workQueue_.push(sock);
    }
    workCv_.notify_one();
}

void LanServer::WorkerThread()
{
    while (true) {
        uintptr_t sock = (uintptr_t)INVALID_SOCKET;
        {
            std::unique_lock<std::mutex> lk(workMutex_);
            workCv_.wait(lk, [this]() {
                return !workQueue_.empty() || !running_.load();
            });
            if (!running_.load() && workQueue_.empty()) return;
            sock = workQueue_.front();
            workQueue_.pop();
        }
        if (sock != (uintptr_t)INVALID_SOCKET)
            HandleConnection(sock);
    }
}

// ── Accept thread (one thread, accepts only) ──────────────────────────────────

void LanServer::ListenThread()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { WSACleanup(); return; }

    int reuseAddr = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(ls, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(ls); WSACleanup(); return;
    }
    if (listen(ls, SOMAXCONN) != 0) {
        closesocket(ls); WSACleanup(); return;
    }

    listenSock_ = (uintptr_t)ls;
    running_.store(true);

    while (running_.load()) {
        SOCKET client = accept(ls, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        EnqueueSocket((uintptr_t)client);  // hand off to pool — no thread spawn here
    }

    closesocket(ls);
    WSACleanup();
    running_.store(false);
}

// ── Public interface ─────────────────────────────────────────────────────────

LanServer::LanServer(EventStore& ev, SatelliteStore& sat,
                     GroundStationStore& gs, UserStore& usr, TleStore& tle,
                     std::function<bool(const std::string&, const std::string&)> authFn)
    : ev_(ev), sat_(sat), gs_(gs), usr_(usr), tle_(tle), authFn_(std::move(authFn))
{}

LanServer::~LanServer() { Stop(); }

bool LanServer::Start(int port, std::string& err)
{
    if (running_.load()) return true;
    port_ = port;

    // Size pool to 2× hardware threads, floored at 4, capped at 32
    unsigned hw = std::thread::hardware_concurrency();
    threadCount_ = static_cast<int>(std::min(std::max(hw * 2u, 4u), 32u));
    workers_.reserve(threadCount_);
    for (int i = 0; i < threadCount_; ++i)
        workers_.emplace_back([this]() { WorkerThread(); });

    acceptThread_ = std::thread([this]() { ListenThread(); });

    // Wait up to 1 s for the listen socket to bind
    for (int i = 0; i < 20; ++i) {
        if (running_.load()) return true;
        ::Sleep(50);
    }

    if (!running_.load()) {
        err = "Server failed to bind on port " + std::to_string(port);
        Stop();
        return false;
    }
    return true;
}

void LanServer::Stop()
{
    if (!running_.load() && workers_.empty()) return;
    running_.store(false);

    // Unblock the accept() call by closing the listen socket
    SOCKET ls = (SOCKET)listenSock_;
    if (ls != INVALID_SOCKET) closesocket(ls);
    if (acceptThread_.joinable()) acceptThread_.join();

    // Wake all workers so they can exit their wait loop
    workCv_.notify_all();
    for (auto& w : workers_)
        if (w.joinable()) w.join();
    workers_.clear();
}

std::string LanServer::LocalIp() const
{
    // Connect a dummy UDP socket to 8.8.8.8 to discover the LAN interface IP
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) { WSACleanup(); return "127.0.0.1"; }

    sockaddr_in remote = {};
    remote.sin_family = AF_INET;
    remote.sin_port   = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    connect(s, (sockaddr*)&remote, sizeof(remote));

    sockaddr_in local = {};
    int len = sizeof(local);
    getsockname(s, (sockaddr*)&local, &len);
    closesocket(s);
    WSACleanup();

    char ip[INET_ADDRSTRLEN] = "127.0.0.1";
    inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
    return std::string(ip);
}
