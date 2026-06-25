#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include "SatelliteStore.h"

class SatellitePanel : public wxPanel {
public:
    SatellitePanel(wxWindow* parent, SatelliteStore* store);
    void Reload();

private:
    void BuildRows();
    void ClearRows();
    void OnAdd(wxCommandEvent&);
    void OnEdit(const std::string& id);
    void OnDelete(const std::string& id);

    SatelliteStore*   store_;
    wxScrolledWindow* scroll_;
    wxBoxSizer*       rowSizer_;
};
