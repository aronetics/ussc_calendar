#include "AllianceReportDialog.h"
#include <map>
#include <set>
#include <cstdio>
#include <algorithm>

AllianceReportDialog::AllianceReportDialog(
    wxWindow* parent,
    const std::vector<CalendarEvent>& events,
    const SatelliteStore& satStore,
    const CalendarSourceStore& srcStore)
    : wxDialog(parent, wxID_ANY, "Alliance Deconfliction Report",
               wxDefaultPosition, wxSize(680, 540),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    wxStaticText* hdr = new wxStaticText(this, wxID_ANY,
        "Alliance Deconfliction Report — Satellite Asset Usage");
    hdr->SetFont(wxFont(wxFontInfo(11).Bold().Family(wxFONTFAMILY_SWISS)));
    vbox->Add(hdr, 0, wxALL, 12);

    text_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                           wxDefaultPosition, wxDefaultSize,
                           wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH |
                           wxHSCROLL | wxBORDER_SUNKEN);
    text_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE)));
    vbox->Add(text_, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

    vbox->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxALL, 8);
    SetSizer(vbox);
    Centre();

    BuildReport(events, satStore, srcStore);
}

void AllianceReportDialog::BuildReport(
    const std::vector<CalendarEvent>& events,
    const SatelliteStore& satStore,
    const CalendarSourceStore& srcStore)
{
    // Group pass events by satellite ID
    // sat_pass events have location "sat_pass:satId:gsId"
    struct PassInfo {
        std::string satName;
        std::string srcName;
        std::string alliance;
        wxDateTime  start, end;
        std::string gsName;
    };

    // Build satellite name lookup
    std::map<std::string, std::string> satNames;
    for (const auto& s : satStore.Satellites())
        satNames[s.id] = s.name;

    // Build source alliance lookup
    std::map<std::string, std::pair<std::string,std::string>> srcInfo; // id → (name,alliance)
    for (const auto& src : srcStore.Sources()) {
        std::string alliance;
        switch (src.alliance) {
            case Alliance::CSpO:     alliance = "CSpO";     break;
            case Alliance::NATO:     alliance = "NATO";     break;
            case Alliance::INDOPACOM:alliance = "INDOPACOM";break;
        }
        srcInfo[src.id] = {src.name, alliance};
    }

    // Collect pass events grouped by satellite
    std::map<std::string, std::vector<PassInfo>> bySat;
    for (const auto& ev : events) {
        if (ev.location.find("sat_pass:") != 0) continue;
        size_t p1 = ev.location.find(':', 9);
        std::string satId = (p1 != std::string::npos)
            ? ev.location.substr(9, p1 - 9)
            : ev.location.substr(9);
        size_t p2 = (p1 != std::string::npos) ? ev.location.find(':', p1+1) : std::string::npos;
        std::string gsId = (p1 != std::string::npos && p2 == std::string::npos)
            ? ev.location.substr(p1+1)
            : (p1 != std::string::npos ? ev.location.substr(p1+1, p2-p1-1) : "");

        PassInfo pi;
        pi.satName = satNames.count(satId) ? satNames[satId] : satId;
        pi.start   = ev.start;
        pi.end     = ev.end;
        if (srcInfo.count(ev.calendarSource)) {
            pi.srcName  = srcInfo[ev.calendarSource].first;
            pi.alliance = srcInfo[ev.calendarSource].second;
        }
        bySat[satId].push_back(pi);
    }

    // Detect conflicts
    auto conflicts = DetectConflicts(events);

    wxString report;
    report += wxString::Format("Generated: %s UTC\n",
        wxDateTime::Now().Format("%Y-%m-%d %H:%M:%S"));
    report += wxString(80, '=') + "\n\n";

    if (bySat.empty()) {
        report += "No satellite pass events found.\n"
                  "Generate passes using the Ground Stations & Passes panel.\n";
    } else {
        for (const auto& kv : bySat) {
            const auto& pis = kv.second;
            if (pis.empty()) continue;
            report += wxString::Format("SATELLITE: %s\n", wxString::FromUTF8(pis[0].satName));
            report += wxString(40, '-') + "\n";

            // Check for conflicts involving this satellite's events
            bool hasConflict = false;
            for (const auto& ev : events) {
                std::string loc = ev.location;
                if (loc.find("sat_pass:") != 0) continue;
                size_t p1 = loc.find(':', 9);
                std::string sid = (p1 != std::string::npos)
                    ? loc.substr(9, p1-9) : loc.substr(9);
                if (sid != kv.first) continue;
                if (EventHasConflict(ev.id, conflicts)) { hasConflict = true; break; }
            }

            if (hasConflict)
                report += "  *** CONFLICT DETECTED: Overlapping pass assignments ***\n";

            // Sort by start time
            auto sorted = pis;
            std::sort(sorted.begin(), sorted.end(),
                [](const PassInfo& a, const PassInfo& b){
                    return a.start.GetTicks() < b.start.GetTicks();
                });

            for (const auto& pi : sorted) {
                report += wxString::Format(
                    "  %s – %s  (%s / %s)\n",
                    pi.start.IsValid() ? pi.start.Format("%Y-%m-%d %H:%MZ") : wxString("?"),
                    pi.end.IsValid()   ? pi.end.Format("%H:%MZ")             : wxString("?"),
                    wxString::FromUTF8(pi.alliance.empty() ? "—" : pi.alliance),
                    wxString::FromUTF8(pi.srcName.empty()  ? "—" : pi.srcName));
            }
            report += "\n";
        }

        report += wxString::Format("Total conflicts detected: %d\n", (int)conflicts.size());
    }

    text_->SetValue(report);
}
