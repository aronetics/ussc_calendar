#include "GraphApiConnector.h"
#ifdef _WIN32
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#endif
#include <sstream>

// Graph API base URL
static const char* GRAPH_BASE = "https://graph.microsoft.com/v1.0";

// ── WinHTTP helpers (Windows) / stub (Linux) ─────────────────────────────────

static std::string HttpPost(const std::string& /*host*/,
                             const std::string& /*path*/,
                             const std::string& /*body*/,
                             std::string& errorMsg)
{
#ifdef _WIN32
    // Full WinHTTP implementation would go here.
    // Omitted in skeleton — see PushEvent / FetchEvents stubs below.
    errorMsg = "WinHTTP implementation pending.";
    return "";
#else
    errorMsg = "HTTP not available on this platform without libcurl.";
    return "";
#endif
}

// ── Connector ─────────────────────────────────────────────────────────────────

bool GraphApiConnector::StartDeviceCodeFlow(std::string& userCode,
                                             std::string& verificationUrl,
                                             std::string& errorMsg)
{
    if (settings_.graphTenantId.empty() || settings_.graphClientId.empty()) {
        errorMsg = "Tenant ID and Client ID are required.";
        return false;
    }

    // POST https://login.microsoftonline.com/{tenant}/oauth2/v2.0/devicecode
    // body: client_id=...&scope=Calendars.ReadWrite offline_access
    std::string host = "login.microsoftonline.com";
    std::string path = "/" + settings_.graphTenantId + "/oauth2/v2.0/devicecode";
    std::string body = "client_id=" + settings_.graphClientId +
                       "&scope=Calendars.ReadWrite%20offline_access";

    std::string resp = HttpPost(host, path, body, errorMsg);
    if (!errorMsg.empty() || resp.empty()) return false;

    // Parse JSON response for user_code and verification_uri
    auto extract = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = resp.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        std::string val;
        while (pos < resp.size() && resp[pos] != '"') val += resp[pos++];
        return val;
    };

    userCode        = extract("user_code");
    verificationUrl = extract("verification_uri");
    deviceCode_     = extract("device_code");

    if (userCode.empty()) {
        errorMsg = "Unexpected response from Microsoft authentication service.";
        return false;
    }
    return true;
}

bool GraphApiConnector::PollForToken(ConnectorSettings& outSettings,
                                      std::string& errorMsg)
{
    if (deviceCode_.empty()) {
        errorMsg = "No active device code flow. Call StartDeviceCodeFlow first.";
        return false;
    }
    // POST to token endpoint — pending WinHTTP implementation
    errorMsg = "Token polling: WinHTTP implementation pending.";
    (void)outSettings;
    return false;
}

bool GraphApiConnector::Connect(std::string& errorMsg)
{
    if (settings_.graphAccessToken.empty()) {
        errorMsg = "Not authenticated. Use 'Sign in with Microsoft' first.";
        return false;
    }
    // A real implementation would verify the token is still valid here.
    return true;
}

std::vector<CalendarEvent> GraphApiConnector::FetchEvents(
    const wxDateTime& /*from*/, const wxDateTime& /*to*/, std::string& errorMsg)
{
    errorMsg = "Microsoft Graph event fetch: WinHTTP implementation pending.";
    return {};
}

std::string GraphApiConnector::PushEvent(const CalendarEvent& /*ev*/,
                                          std::string& errorMsg)
{
    errorMsg = "Microsoft Graph push: WinHTTP implementation pending.";
    return "";
}
