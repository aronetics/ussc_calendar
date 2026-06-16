#pragma once
#include "CalendarConnector.h"

// Connects to Microsoft 365 / Exchange Online via the Microsoft Graph REST API.
// Uses OAuth 2.0 device-code flow (suitable for government / air-gapped networks).
// Requires: Azure AD tenant ID + client/app ID (registered by IT admin).
class GraphApiConnector : public CalendarConnector {
public:
    explicit GraphApiConnector(const ConnectorSettings& s) : settings_(s) {}

    bool Connect(std::string& errorMsg) override;
    std::vector<CalendarEvent> FetchEvents(const wxDateTime& from,
                                            const wxDateTime& to,
                                            std::string& errorMsg) override;
    std::string PushEvent(const CalendarEvent& ev,
                           std::string& errorMsg) override;
    const char* DisplayName() const override { return "Microsoft 365 / Graph API"; }

    // Returns the device-code URL + user code for the sign-in prompt
    bool StartDeviceCodeFlow(std::string& userCode,
                              std::string& verificationUrl,
                              std::string& errorMsg);

    // Poll for token after user completes sign-in
    bool PollForToken(ConnectorSettings& outSettings, std::string& errorMsg);

private:
    ConnectorSettings settings_;
    std::string       deviceCode_;
};
