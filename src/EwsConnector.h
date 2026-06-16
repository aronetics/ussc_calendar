#pragma once
#include "CalendarConnector.h"

// Connects to an on-premises Exchange server via Exchange Web Services (EWS).
// Uses NTLM/Kerberos authentication (Windows Integrated) or basic auth.
// Typical DoD Exchange URL: https://mail.example.mil/EWS/Exchange.asmx
class EwsConnector : public CalendarConnector {
public:
    explicit EwsConnector(const ConnectorSettings& s) : settings_(s) {}

    bool Connect(std::string& errorMsg) override;
    std::vector<CalendarEvent> FetchEvents(const wxDateTime& from,
                                            const wxDateTime& to,
                                            std::string& errorMsg) override;
    std::string PushEvent(const CalendarEvent& ev,
                           std::string& errorMsg) override;
    const char* DisplayName() const override { return "Exchange On-Premises (EWS)"; }

private:
    ConnectorSettings settings_;
    bool              connected_ = false;

    // Returns the SOAP envelope for a FindItem calendar request
    std::string BuildFindItemSoap(const wxDateTime& from,
                                   const wxDateTime& to) const;
};
