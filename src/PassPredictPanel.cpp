#include "PassPredictPanel.h"
#include "PassPredictor.h"
#include <wx/valnum.h>
#include <wx/statline.h>
#include <algorithm>
#include <cstdio>

// ── Ground station edit dialog ────────────────────────────────────────────────

class GsEditDlg : public wxDialog {
public:
    GsEditDlg(wxWindow* parent, const GroundStation& gs, bool isNew)
        : wxDialog(parent, wxID_ANY,
                   isNew ? "Add Ground Station" : "Edit Ground Station",
                   wxDefaultPosition, wxSize(400, 320),
                   wxDEFAULT_DIALOG_STYLE)
    {
        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
        wxPanel* form = new wxPanel(this);
        wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
        form->SetSizer(vbox);

        auto addRow = [&](const wxString& lbl, wxWindow* ctrl) {
            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* lt = new wxStaticText(form, wxID_ANY, lbl,
                wxDefaultPosition, wxSize(120,-1), wxALIGN_RIGHT);
            lt->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
            row->Add(lt, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
            row->Add(ctrl, 1, wxEXPAND);
            vbox->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        };

        nameCtrl_  = new wxTextCtrl(form, wxID_ANY, wxString::FromUTF8(gs.name));
        latCtrl_   = new wxTextCtrl(form, wxID_ANY, wxString::Format("%.6f", gs.latDeg));
        lonCtrl_   = new wxTextCtrl(form, wxID_ANY, wxString::Format("%.6f", gs.lonDeg));
        altCtrl_   = new wxTextCtrl(form, wxID_ANY, wxString::Format("%.4f", gs.altKm));
        minelCtrl_ = new wxTextCtrl(form, wxID_ANY, wxString::Format("%.1f", gs.minElDeg));
        activeCtrl_= new wxCheckBox(form, wxID_ANY, "Active");
        activeCtrl_->SetValue(gs.active);

        addRow("Name:",                nameCtrl_);
        addRow("Latitude (°N):",       latCtrl_);
        addRow("Longitude (°E):",      lonCtrl_);
        addRow("Altitude (km):",       altCtrl_);
        addRow("Min Elevation (°):",   minelCtrl_);
        vbox->Add(activeCtrl_, 0, wxLEFT | wxBOTTOM, 8);

        outer->Add(form, 1, wxEXPAND | wxALL, 8);
        outer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0,
                   wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        SetSizer(outer);
        Centre();
    }

    GroundStation GetStation(const std::string& existingId) const
    {
        GroundStation gs;
        gs.id = existingId;
        gs.name = std::string(nameCtrl_->GetValue().mb_str());
        latCtrl_->GetValue().ToDouble(&gs.latDeg);
        lonCtrl_->GetValue().ToDouble(&gs.lonDeg);
        altCtrl_->GetValue().ToDouble(&gs.altKm);
        minelCtrl_->GetValue().ToDouble(&gs.minElDeg);
        gs.active = activeCtrl_->IsChecked();
        return gs;
    }

private:
    wxTextCtrl* nameCtrl_;
    wxTextCtrl* latCtrl_;
    wxTextCtrl* lonCtrl_;
    wxTextCtrl* altCtrl_;
    wxTextCtrl* minelCtrl_;
    wxCheckBox* activeCtrl_;
};

// ── PassPredictPanel ──────────────────────────────────────────────────────────

PassPredictPanel::PassPredictPanel(wxWindow* parent,
                                   GroundStationStore* gsStore,
                                   SatelliteStore*     satStore,
                                   TleStore*           tleStore,
                                   EventStore*         eventStore)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(280, -1))
    , gsStore_(gsStore), satStore_(satStore)
    , tleStore_(tleStore), eventStore_(eventStore)
{
    SetBackgroundColour(wxColour(248, 249, 250));
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);

    // Header
    wxPanel* hdr = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 36));
    hdr->SetBackgroundColour(wxColour(11, 61, 145));
    wxBoxSizer* hdrBox = new wxBoxSizer(wxHORIZONTAL);
    hdr->SetSizer(hdrBox);

    wxStaticText* title = new wxStaticText(hdr, wxID_ANY, "Ground Stations & Passes");
    title->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
    title->SetForegroundColour(*wxWHITE);
    hdrBox->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

    wxButton* addBtn = new wxButton(hdr, wxID_ANY, "+",
                                    wxDefaultPosition, wxSize(28, 28));
    addBtn->SetFont(wxFont(wxFontInfo(14).Family(wxFONTFAMILY_SWISS)));
    addBtn->SetForegroundColour(*wxWHITE);
    addBtn->SetBackgroundColour(wxColour(26, 115, 232));
    addBtn->Bind(wxEVT_BUTTON, &PassPredictPanel::OnAddStation, this);
    hdrBox->Add(addBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    vbox->Add(hdr, 0, wxEXPAND);

    // Scrollable list of ground stations
    scroll_ = new wxScrolledWindow(this, wxID_ANY);
    scroll_->SetBackgroundColour(wxColour(248, 249, 250));
    scroll_->SetScrollRate(0, 10);
    rowSizer_ = new wxBoxSizer(wxVERTICAL);
    scroll_->SetSizer(rowSizer_);
    vbox->Add(scroll_, 1, wxEXPAND);

    // Action buttons
    wxPanel* btns = new wxPanel(this);
    btns->SetBackgroundColour(wxColour(232, 240, 254));
    wxBoxSizer* bsz = new wxBoxSizer(wxVERTICAL);
    btns->SetSizer(bsz);

    wxButton* fetchBtn   = new wxButton(btns, wxID_ANY, "Fetch TLEs from Celestrak");
    wxButton* predictBtn = new wxButton(btns, wxID_ANY, "Generate Pass Events");
    wxButton* clearBtn   = new wxButton(btns, wxID_ANY, "Clear Pass Events");

    for (wxButton* b : {fetchBtn, predictBtn, clearBtn})
        b->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));

    fetchBtn->SetBackgroundColour(wxColour(26, 115, 232));
    fetchBtn->SetForegroundColour(*wxWHITE);
    predictBtn->SetBackgroundColour(wxColour(11, 128, 67));
    predictBtn->SetForegroundColour(*wxWHITE);

    fetchBtn->Bind(wxEVT_BUTTON,   &PassPredictPanel::OnFetchTles,    this);
    predictBtn->Bind(wxEVT_BUTTON, &PassPredictPanel::OnPredictPasses, this);
    clearBtn->Bind(wxEVT_BUTTON,   &PassPredictPanel::OnClearPasses,   this);

    bsz->Add(fetchBtn,   0, wxEXPAND | wxALL, 4);
    bsz->Add(predictBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
    bsz->Add(clearBtn,   0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    statusLabel_ = new wxStaticText(btns, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxST_ELLIPSIZE_END);
    statusLabel_->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
    statusLabel_->SetForegroundColour(wxColour(60, 64, 67));
    bsz->Add(statusLabel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    vbox->Add(btns, 0, wxEXPAND);

    BuildRows();
}

void PassPredictPanel::Reload()
{
    ClearRows();
    BuildRows();
}

void PassPredictPanel::ClearRows()
{
    rowSizer_->Clear(true);
    scroll_->FitInside();
}

void PassPredictPanel::BuildRows()
{
    const auto& stations = gsStore_->Stations();
    if (stations.empty()) {
        wxStaticText* empty = new wxStaticText(scroll_, wxID_ANY,
            "No ground stations.\nClick + to add.",
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        empty->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        empty->SetForegroundColour(wxColour(128,128,128));
        rowSizer_->Add(empty, 0, wxALIGN_CENTER | wxTOP, 16);
        scroll_->FitInside();
        return;
    }

    for (const auto& gs : stations) {
        const std::string id = gs.id;

        wxPanel* row = new wxPanel(scroll_);
        row->SetBackgroundColour(*wxWHITE);
        wxBoxSizer* rv = new wxBoxSizer(wxVERTICAL);
        row->SetSizer(rv);

        // Name + active dot
        wxBoxSizer* line1 = new wxBoxSizer(wxHORIZONTAL);

        wxPanel* dot = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(10,10));
        dot->SetBackgroundColour(gs.active ? wxColour(52,168,83) : wxColour(158,158,158));
        dot->SetMinSize(wxSize(10,10));
        line1->Add(dot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        wxStaticText* nameLabel = new wxStaticText(row, wxID_ANY,
            wxString::FromUTF8(gs.name), wxDefaultPosition, wxDefaultSize,
            wxST_ELLIPSIZE_END);
        nameLabel->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
        line1->Add(nameLabel, 1, wxALIGN_CENTER_VERTICAL);

        wxButton* editBtn = new wxButton(row, wxID_ANY, wxString::FromUTF8("✎"),
                                         wxDefaultPosition, wxSize(22,22));
        wxButton* delBtn  = new wxButton(row, wxID_ANY, wxString::FromUTF8("×"),
                                         wxDefaultPosition, wxSize(22,22));
        editBtn->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
        delBtn->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        delBtn->SetForegroundColour(wxColour(176,0,0));

        editBtn->Bind(wxEVT_BUTTON, [this,id](wxCommandEvent&){ OnEditStation(id); });
        delBtn->Bind(wxEVT_BUTTON,  [this,id](wxCommandEvent&){ OnDeleteStation(id); });
        line1->Add(editBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        line1->Add(delBtn,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rv->Add(line1, 0, wxEXPAND | wxTOP, 4);

        // Coordinates
        wxString coord = wxString::Format("%.4f°  %.4f°  MinEl: %.0f°",
            gs.latDeg, gs.lonDeg, gs.minElDeg);
        wxStaticText* coordLabel = new wxStaticText(row, wxID_ANY, coord);
        coordLabel->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
        coordLabel->SetForegroundColour(wxColour(95,99,104));
        rv->Add(coordLabel, 0, wxLEFT | wxBOTTOM, 22);

        // Separator
        wxPanel* sep = new wxPanel(scroll_, wxID_ANY, wxDefaultPosition, wxSize(-1,1));
        sep->SetBackgroundColour(wxColour(0xDA,0xDC,0xE0));

        rowSizer_->Add(row, 0, wxEXPAND);
        rowSizer_->Add(sep, 0, wxEXPAND);
    }

    scroll_->FitInside();
    scroll_->Layout();
}

void PassPredictPanel::OnAddStation(wxCommandEvent&)
{
    GroundStation blank;
    blank.active   = true;
    blank.minElDeg = 5.0;
    GsEditDlg dlg(this, blank, true);
    if (dlg.ShowModal() == wxID_OK) {
        gsStore_->Add(dlg.GetStation(""));
        Reload();
    }
}

void PassPredictPanel::OnEditStation(const std::string& id)
{
    for (const auto& gs : gsStore_->Stations()) {
        if (gs.id == id) {
            GsEditDlg dlg(this, gs, false);
            if (dlg.ShowModal() == wxID_OK) {
                gsStore_->Update(dlg.GetStation(id));
                Reload();
            }
            return;
        }
    }
}

void PassPredictPanel::OnDeleteStation(const std::string& id)
{
    for (const auto& gs : gsStore_->Stations()) {
        if (gs.id == id) {
            wxString msg = wxString::Format("Delete ground station \"%s\"?",
                                            wxString::FromUTF8(gs.name));
            if (wxMessageBox(msg, "Confirm Delete",
                             wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) == wxYES) {
                gsStore_->Remove(id);
                Reload();
            }
            return;
        }
    }
}

void PassPredictPanel::OnFetchTles(wxCommandEvent&)
{
    const auto& sats = satStore_->Satellites();
    std::vector<int> ids;
    for (const auto& s : sats) {
        if (!s.noradId.empty()) {
            int id = std::atoi(s.noradId.c_str());
            if (id > 0) ids.push_back(id);
        }
    }

    if (ids.empty()) {
        wxMessageBox("No satellites with NORAD IDs found.\n"
                     "Add satellites with NORAD IDs in the Satellites panel first.",
                     "No NORAD IDs", wxOK | wxICON_INFORMATION, this);
        return;
    }

    statusLabel_->SetLabel(wxString::Format("Fetching %d TLEs...", (int)ids.size()));
    GetSizer()->Layout();

    std::string err;
    int fetched = tleStore_->FetchFromCelestrak(ids, err);

    wxString status;
    if (fetched == (int)ids.size()) {
        status = wxString::Format("Fetched %d/%d TLEs successfully.", fetched, (int)ids.size());
    } else {
        status = wxString::Format("Fetched %d/%d TLEs. %s",
            fetched, (int)ids.size(),
            err.empty() ? "" : wxString::FromUTF8(err));
    }
    statusLabel_->SetLabel(status);
}

void PassPredictPanel::OnPredictPasses(wxCommandEvent&)
{
    if (gsStore_->Stations().empty()) {
        wxMessageBox("Add at least one ground station first.",
                     "No Ground Stations", wxOK | wxICON_INFORMATION, this);
        return;
    }

    // Check we have TLEs
    bool anyTle = false;
    for (const auto& s : satStore_->Satellites()) {
        if (!s.noradId.empty()) {
            int id = std::atoi(s.noradId.c_str());
            if (id > 0 && tleStore_->Get(id)) { anyTle = true; break; }
        }
    }
    if (!anyTle) {
        wxMessageBox("No TLEs cached. Click \"Fetch TLEs from Celestrak\" first.",
                     "No TLE Data", wxOK | wxICON_INFORMATION, this);
        return;
    }

    statusLabel_->SetLabel("Computing passes...");
    GetSizer()->Layout();

    double nowJd = 2440587.5 + wxDateTime::Now().GetTicks() / 86400.0;

    auto passes = PredictPasses(
        satStore_->Satellites(),
        gsStore_->Stations(),
        *tleStore_,
        nowJd,
        3.0,   // 3-day lookahead
        30.0,  // 30-second coarse step
        2.0);  // 2-second refinement

    if (passes.empty()) {
        statusLabel_->SetLabel("No passes found in 3-day window.");
        return;
    }

    // Add pass events, coloured teal to distinguish from user events
    wxColour passColor(0, 172, 193);
    for (const auto& p : passes)
        eventStore_->Add(PassToEvent(p, passColor));

    statusLabel_->SetLabel(wxString::Format(
        "Generated %d pass events (3-day window).", (int)passes.size()));

    if (OnPassesGenerated) OnPassesGenerated();
}

void PassPredictPanel::OnClearPasses(wxCommandEvent&)
{
    int removed = 0;
    auto& evs = eventStore_->Events();
    std::vector<std::string> toRemove;
    for (const auto& e : evs) {
        if (e.location.find("sat_pass:") == 0)
            toRemove.push_back(e.id);
    }
    for (const auto& id : toRemove) {
        eventStore_->Remove(id);
        ++removed;
    }
    statusLabel_->SetLabel(wxString::Format("Removed %d pass events.", removed));
    if (OnPassesGenerated) OnPassesGenerated();
}
