#pragma once
#include <wx/wx.h>
#include "CalendarEvent.h"
#include "SatelliteStore.h"
#include "CalendarSourceStore.h"
#include "ConflictDetector.h"
#include <vector>

class AllianceReportDialog : public wxDialog {
public:
    AllianceReportDialog(wxWindow* parent,
                         const std::vector<CalendarEvent>& events,
                         const SatelliteStore& satStore,
                         const CalendarSourceStore& srcStore);
private:
    void BuildReport(const std::vector<CalendarEvent>& events,
                     const SatelliteStore& satStore,
                     const CalendarSourceStore& srcStore);
    wxTextCtrl* text_;
};
