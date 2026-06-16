#pragma once
#ifdef _WIN32
#include "CalendarConnector.h"

// Connects to a locally-installed Microsoft Outlook via COM automation.
// Requires Outlook 2016+ to be installed; no network or OAuth needed.
class OutlookComConnector : public CalendarConnector {
public:
    bool Connect(std::string& errorMsg) override;
    std::vector<CalendarEvent> FetchEvents(const wxDateTime& from,
                                            const wxDateTime& to,
                                            std::string& errorMsg) override;
    std::string PushEvent(const CalendarEvent& ev,
                           std::string& errorMsg) override;
    const char* DisplayName() const override { return "Local Outlook (COM)"; }

private:
    bool        connected_ = false;
};
#endif // _WIN32
