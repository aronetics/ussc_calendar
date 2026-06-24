#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <functional>
#include "GroundStationStore.h"
#include "SatelliteStore.h"
#include "TleStore.h"
#include "EventStore.h"

class PassPredictPanel : public wxPanel {
public:
    PassPredictPanel(wxWindow* parent,
                     GroundStationStore* gsStore,
                     SatelliteStore*     satStore,
                     TleStore*           tleStore,
                     EventStore*         eventStore);

    void Reload();

    // Called after passes are generated so the calendar redraws
    std::function<void()> OnPassesGenerated;

private:
    void BuildRows();
    void ClearRows();

    void OnAddStation(wxCommandEvent&);
    void OnEditStation(const std::string& id);
    void OnDeleteStation(const std::string& id);
    void OnFetchTles(wxCommandEvent&);
    void OnPredictPasses(wxCommandEvent&);
    void OnClearPasses(wxCommandEvent&);

    GroundStationStore* gsStore_;
    SatelliteStore*     satStore_;
    TleStore*           tleStore_;
    EventStore*         eventStore_;

    wxScrolledWindow* scroll_;
    wxBoxSizer*       rowSizer_;
    wxStaticText*     statusLabel_;
};
