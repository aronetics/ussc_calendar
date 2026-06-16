#include "CalendarConnector.h"

const char* CalendarConnector::TypeName(ConnectorType t)
{
    switch (t) {
        case ConnectorType::OutlookCOM: return "Local Outlook (COM)";
        case ConnectorType::GraphAPI:   return "Microsoft 365 / Graph API";
        case ConnectorType::EWS:        return "Exchange On-Premises (EWS)";
        case ConnectorType::ICSFile:    return "ICS File (Import / Export)";
        default:                        return "None";
    }
}

std::string CalendarConnector::TypeId(ConnectorType t)
{
    switch (t) {
        case ConnectorType::OutlookCOM: return "outlook_com";
        case ConnectorType::GraphAPI:   return "graph_api";
        case ConnectorType::EWS:        return "ews";
        case ConnectorType::ICSFile:    return "ics_file";
        default:                        return "none";
    }
}
