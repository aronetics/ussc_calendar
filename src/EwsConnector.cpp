#include "EwsConnector.h"

bool EwsConnector::Connect(std::string& errorMsg)
{
    if (settings_.ewsUrl.empty()) {
        errorMsg = "Exchange server URL is required.";
        return false;
    }
    // A real implementation would send a GetFolder SOAP request with
    // NTLM/Negotiate auth via WinHTTP and check the HTTP 200 response.
    // WinHTTP NTLM auth is built-in and passes Windows credentials automatically
    // when WINHTTP_AUTOLOGON_SECURITY_LEVEL_LOW is set.
    connected_ = true;
    return true;
}

std::string EwsConnector::BuildFindItemSoap(const wxDateTime& from,
                                              const wxDateTime& to) const
{
    std::string startStr = std::string(from.Format("%Y-%m-%dT00:00:00").mb_str());
    std::string endStr   = std::string(to.Format("%Y-%m-%dT23:59:59").mb_str());

    return
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
        "  xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\""
        "  xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\""
        "  xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<soap:Body>"
        "<m:FindItem Traversal=\"Shallow\">"
        "<m:ItemShape><t:BaseShape>AllProperties</t:BaseShape></m:ItemShape>"
        "<m:CalendarView MaxEntriesReturned=\"100\""
        "  StartDate=\"" + startStr + "\" EndDate=\"" + endStr + "\"/>"
        "<m:ParentFolderIds>"
        "<t:DistinguishedFolderId Id=\"calendar\"/>"
        "</m:ParentFolderIds>"
        "</m:FindItem>"
        "</soap:Body>"
        "</soap:Envelope>";
}

std::vector<CalendarEvent> EwsConnector::FetchEvents(
    const wxDateTime& from, const wxDateTime& to, std::string& errorMsg)
{
    if (!connected_) { errorMsg = "Not connected."; return {}; }

    // A real implementation would:
    // 1. Build the SOAP envelope via BuildFindItemSoap()
    // 2. POST it to settings_.ewsUrl via WinHTTP with NTLM auth
    // 3. Parse the XML response and extract CalendarItem elements
    // 4. Map each item to a CalendarEvent
    (void)from; (void)to;
    errorMsg = "EWS event fetch: WinHTTP + XML implementation pending.";
    return {};
}

std::string EwsConnector::PushEvent(const CalendarEvent& /*ev*/,
                                     std::string& errorMsg)
{
    errorMsg = "EWS push: implementation pending.";
    return "";
}
