#include "LanClient.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

LanClient::~LanClient() {}

// ── Core HTTP request via WinHTTP ────────────────────────────────────────────

std::string LanClient::Request(const std::string& method,
                                const std::string& path,
                                const std::string& body,
                                std::string& err)
{
    err.clear();

    HINTERNET hSession = WinHttpOpen(
        L"USSC-Calendar-Client/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { err = "WinHttpOpen failed"; return {}; }

    std::wstring wHost(host_.begin(), host_.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                         (INTERNET_PORT)port_, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); err = "WinHttpConnect failed"; return {}; }

    std::wstring wPath(path.begin(), path.end());
    std::wstring wMethod(method.begin(), method.end());
    DWORD tlsFlag = useTls_ ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wMethod.c_str(), wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, tlsFlag);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        err = "WinHttpOpenRequest failed";
        return {};
    }

    // When using TLS via IP address: verify cert chain (trust AD CS) but
    // ignore CN/SAN mismatch (server cert has FQDN, client connects by IP).
    if (useTls_) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &secFlags, sizeof(secFlags));
    }

    // Build headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!token_.empty()) {
        std::wstring tok(token_.begin(), token_.end());
        headers += L"Authorization: Bearer " + tok + L"\r\n";
    }

    LPVOID bodyPtr = body.empty() ? nullptr : (LPVOID)body.c_str();
    DWORD  bodyLen = body.empty() ? 0       : (DWORD)body.size();

    BOOL ok = WinHttpSendRequest(
        hRequest,
        headers.c_str(), (DWORD)headers.size(),
        bodyPtr, bodyLen, bodyLen, 0);

    if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        err = "HTTP send/receive failed";
        return {};
    }

    // Read response body
    std::string response;
    DWORD avail = 0, read = 0;
    char buf[4096];
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        DWORD toRead = avail < sizeof(buf) ? avail : sizeof(buf);
        if (!WinHttpReadData(hRequest, buf, toRead, &read)) break;
        response.append(buf, read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

// ── Public API ───────────────────────────────────────────────────────────────

bool LanClient::Probe(std::string& err)
{
    std::string resp = Request("GET", "/api/info", {}, err);
    if (!resp.empty() && resp.find("USSC") != std::string::npos) {
        connected_ = true;
        return true;
    }
    return false;
}

bool LanClient::Connect(const std::string& host, int port, std::string& err)
{
    host_ = host;
    port_ = port;

    // Try HTTPS first (AD CS machine cert present on domain-joined hosts).
    // Fall back to HTTP for non-domain or dev environments.
    useTls_ = true;
    err.clear();
    if (Probe(err)) return true;

    useTls_ = false;
    err.clear();
    if (Probe(err)) return true;

    if (err.empty()) err = "Server not responding at " + host + ":" + std::to_string(port);
    return false;
}

bool LanClient::Auth(const std::string& user, const std::string& pass, std::string& err)
{
    std::string body = "{\"user\":\"" + user + "\",\"pass\":\"" + pass + "\"}";
    std::string resp = Request("POST", "/api/auth", body, err);
    if (resp.empty()) return false;

    // Parse {"ok":true,"token":"..."}
    if (resp.find("\"ok\":true") == std::string::npos) {
        size_t mp = resp.find("\"msg\":\"");
        if (mp != std::string::npos) {
            mp += 7;
            err = resp.substr(mp, resp.find('"', mp) - mp);
        } else {
            err = "Authentication refused by server";
        }
        return false;
    }

    size_t tp = resp.find("\"token\":\"");
    if (tp == std::string::npos) { err = "No token in response"; return false; }
    tp += 9;
    token_ = resp.substr(tp, resp.find('"', tp) - tp);
    return true;
}

std::string LanClient::Get(const std::string& path, std::string& err)
{
    return Request("GET", path, {}, err);
}

std::string LanClient::Post(const std::string& path, const std::string& body, std::string& err)
{
    return Request("POST", path, body, err);
}

std::string LanClient::Put(const std::string& path, const std::string& body, std::string& err)
{
    return Request("PUT", path, body, err);
}

bool LanClient::Delete(const std::string& path, std::string& err)
{
    std::string resp = Request("DELETE", path, {}, err);
    return !resp.empty() && resp.find("\"ok\":true") != std::string::npos;
}
