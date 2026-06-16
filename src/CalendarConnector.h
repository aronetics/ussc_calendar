#pragma once
#include "CalendarEvent.h"
#include <string>
#include <vector>
#include <functional>

enum class ConnectorType {
    None,
    OutlookCOM,   // locally installed Outlook via COM automation (Windows only)
    GraphAPI,     // Microsoft 365 / Exchange Online via Microsoft Graph REST API
    EWS,          // Exchange On-Premises via Exchange Web Services (SOAP)
    ICSFile       // iCalendar file import/export (RFC 5545)
};

// Settings persisted to cal_connector.json in the app data directory
struct ConnectorSettings {
    ConnectorType type = ConnectorType::None;

    // ── Microsoft Graph API (Option 2) ────────────────────────────────────────
    std::string graphTenantId;    // Azure AD tenant ID or "common"
    std::string graphClientId;    // Azure app client/application ID
    std::string graphAccessToken; // cached access token (short-lived)
    std::string graphRefreshToken;

    // ── Exchange Web Services (Option 3) ──────────────────────────────────────
    std::string ewsUrl;           // e.g. https://mail.example.mil/EWS/Exchange.asmx
    std::string ewsUsername;      // domain\username or UPN
    std::string ewsPassword;      // plaintext — use Windows Credential Manager in prod

    // ── ICS File (Option 4) ───────────────────────────────────────────────────
    std::string icsImportPath;    // last-used import path
    std::string icsExportPath;    // last-used export path

    // ── Shared ────────────────────────────────────────────────────────────────
    bool autoSync             = false;
    int  syncIntervalMinutes  = 30;
};

// Abstract connector interface — each backend implements this
class CalendarConnector {
public:
    virtual ~CalendarConnector() = default;

    // Attempt to connect/authenticate.  Returns true on success.
    // On failure, sets errorMsg.
    virtual bool Connect(std::string& errorMsg) = 0;

    // Fetch events in the given date range from the external calendar.
    // Returns newly imported events (caller adds them to EventStore).
    virtual std::vector<CalendarEvent> FetchEvents(
        const wxDateTime& from,
        const wxDateTime& to,
        std::string& errorMsg) = 0;

    // Push a local event to the external calendar.
    // Returns the external event ID on success, or "" on failure.
    virtual std::string PushEvent(const CalendarEvent& ev,
                                   std::string& errorMsg) = 0;

    // Human-readable name for UI labels
    virtual const char* DisplayName() const = 0;

    static const char* TypeName(ConnectorType t);
    static std::string TypeId(ConnectorType t);
};
