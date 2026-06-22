#include "SatellitePanel.h"
#include <wx/statline.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/valnum.h>
#include <cstdio>

// ── Band colours ─────────────────────────────────────────────────────────────

static wxColour BandColour(const std::string& bt)
{
    if (bt == "UHF") return wxColour(123, 31, 162);
    if (bt == "VHF") return wxColour(  2,119,189);
    if (bt == "L"  ) return wxColour(  2,136,209);
    if (bt == "S"  ) return wxColour(  0,131,143);
    if (bt == "C"  ) return wxColour( 46,125, 50);
    if (bt == "X"  ) return wxColour(245,127, 23);
    if (bt == "Ku" ) return wxColour(230, 81,  0);
    if (bt == "K"  ) return wxColour(191, 54, 12);
    if (bt == "Ka" ) return wxColour(183, 28, 28);
    if (bt == "EHF") return wxColour(136, 14, 79);
    if (bt == "SHF") return wxColour( 69, 39,160);
    return wxColour(97, 97, 97);
}

static wxColour StatusColour(SatStatus s)
{
    switch (s) {
        case SatStatus::Operational:    return wxColour( 52,168, 83);
        case SatStatus::Degraded:       return wxColour(251,188,  4);
        case SatStatus::Decommissioned: return wxColour(158,158,158);
    }
    return wxColour(158,158,158);
}

// ── Edit / Add dialog ─────────────────────────────────────────────────────────

class SatelliteEditDlg : public wxDialog {
public:
    SatelliteEditDlg(wxWindow* parent, const Satellite& sat, bool isNew)
        : wxDialog(parent, wxID_ANY,
                   isNew ? "Add Satellite" : "Edit Satellite",
                   wxDefaultPosition, wxSize(480, 580),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
        wxPanel* form = new wxPanel(this);
        wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
        form->SetSizer(vbox);

        auto addRow = [&](const wxString& label, wxWindow* ctrl) {
            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* lbl = new wxStaticText(form, wxID_ANY, label,
                wxDefaultPosition, wxSize(110, -1), wxALIGN_RIGHT);
            lbl->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
            row->Add(lbl,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
            row->Add(ctrl, 1, wxEXPAND);
            vbox->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        };

        // Name
        nameCtrl_ = new wxTextCtrl(form, wxID_ANY, wxString::FromUTF8(sat.name));
        addRow("Name:", nameCtrl_);

        // NORAD ID
        noradCtrl_ = new wxTextCtrl(form, wxID_ANY, wxString::FromUTF8(sat.noradId));
        addRow("NORAD ID:", noradCtrl_);

        // Orbit type
        wxArrayString orbits;
        orbits.Add("LEO"); orbits.Add("MEO"); orbits.Add("GEO");
        orbits.Add("HEO"); orbits.Add("SSO");
        orbitCtrl_ = new wxChoice(form, wxID_ANY, wxDefaultPosition, wxDefaultSize, orbits);
        int oi = orbitCtrl_->FindString(wxString::FromUTF8(sat.orbitType));
        orbitCtrl_->SetSelection(oi >= 0 ? oi : 0);
        addRow("Orbit Type:", orbitCtrl_);

        // Status
        wxArrayString statuses;
        statuses.Add("Operational"); statuses.Add("Degraded"); statuses.Add("Decommissioned");
        statusCtrl_ = new wxChoice(form, wxID_ANY, wxDefaultPosition, wxDefaultSize, statuses);
        statusCtrl_->SetSelection((int)sat.status);
        addRow("Status:", statusCtrl_);

        // Launch date
        launchKnown_ = new wxCheckBox(form, wxID_ANY, "Known");
        launchKnown_->SetValue(sat.launchKnown);
        wxDateTime ld = sat.launchDate.IsValid() ? sat.launchDate : wxDateTime::Today();
        launchPicker_ = new wxDatePickerCtrl(form, wxID_ANY, ld);
        launchPicker_->Enable(sat.launchKnown);
        launchKnown_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            launchPicker_->Enable(launchKnown_->IsChecked());
        });
        wxBoxSizer* launchRow = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* launchLbl = new wxStaticText(form, wxID_ANY, "Launch Date:",
            wxDefaultPosition, wxSize(110, -1), wxALIGN_RIGHT);
        launchLbl->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        launchRow->Add(launchLbl,    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        launchRow->Add(launchKnown_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        launchRow->Add(launchPicker_,1, wxEXPAND);
        vbox->Add(launchRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

        // Bands section
        wxStaticText* bandsLbl = new wxStaticText(form, wxID_ANY, "Comm Bands:");
        bandsLbl->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
        vbox->Add(bandsLbl, 0, wxLEFT | wxBOTTOM, 8);

        bandPanel_ = new wxScrolledWindow(form, wxID_ANY,
            wxDefaultPosition, wxSize(-1, 160));
        bandPanel_->SetScrollRate(0, 10);
        bandSizer_ = new wxBoxSizer(wxVERTICAL);
        bandPanel_->SetSizer(bandSizer_);
        vbox->Add(bandPanel_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

        for (const auto& b : sat.bands)
            AddBandRow(b);

        wxButton* addBandBtn = new wxButton(form, wxID_ANY, "+ Add Band",
            wxDefaultPosition, wxSize(100, 28));
        addBandBtn->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        addBandBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            CommBand empty; empty.bandType = "S"; empty.uplink = true;
            AddBandRow(empty);
            bandPanel_->FitInside();
            bandPanel_->Layout();
        });
        vbox->Add(addBandBtn, 0, wxLEFT | wxBOTTOM, 8);

        outer->Add(form, 1, wxEXPAND | wxALL, 8);
        outer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0,
                   wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        SetSizer(outer);
        Centre();
    }

    Satellite GetSatellite(const std::string& existingId) const
    {
        Satellite s;
        s.id         = existingId;
        s.name       = std::string(nameCtrl_->GetValue().mb_str());
        s.noradId    = std::string(noradCtrl_->GetValue().mb_str());
        s.orbitType  = std::string(orbitCtrl_->GetStringSelection().mb_str());
        s.status     = static_cast<SatStatus>(statusCtrl_->GetSelection());
        s.launchKnown = launchKnown_->IsChecked();
        if (s.launchKnown)
            s.launchDate = launchPicker_->GetValue();

        for (const auto& row : bandRows_) {
            CommBand b;
            b.name     = std::string(row.nameCtrl->GetValue().mb_str());
            b.bandType = std::string(row.typeChoice->GetStringSelection().mb_str());
            b.uplink   = (row.dirChoice->GetSelection() == 0);
            wxString freqStr = row.freqCtrl->GetValue();
            double freq = 0.0;
            freqStr.ToDouble(&freq);
            b.freqMHz  = freq;
            s.bands.push_back(b);
        }
        return s;
    }

private:
    struct BandRow {
        wxPanel*    panel;
        wxChoice*   typeChoice;
        wxTextCtrl* freqCtrl;
        wxChoice*   dirChoice;
        wxTextCtrl* nameCtrl;
    };

    void AddBandRow(const CommBand& b)
    {
        wxArrayString bandTypes;
        for (const char* t : {"UHF","VHF","L","S","C","X","Ku","K","Ka","EHF","SHF"})
            bandTypes.Add(t);
        wxArrayString dirs;
        dirs.Add("Uplink"); dirs.Add("Downlink");

        wxPanel* row = new wxPanel(bandPanel_);
        wxBoxSizer* hs = new wxBoxSizer(wxHORIZONTAL);

        BandRow br;
        br.panel      = row;
        br.typeChoice = new wxChoice(row, wxID_ANY, wxDefaultPosition, wxSize(60,-1),
                                     bandTypes);
        int ti = br.typeChoice->FindString(wxString::FromUTF8(b.bandType));
        br.typeChoice->SetSelection(ti >= 0 ? ti : 3); // default S

        br.freqCtrl = new wxTextCtrl(row, wxID_ANY,
            b.freqMHz > 0 ? wxString::Format("%.3f", b.freqMHz) : wxString(""),
            wxDefaultPosition, wxSize(80,-1));

        br.dirChoice = new wxChoice(row, wxID_ANY, wxDefaultPosition, wxSize(90,-1), dirs);
        br.dirChoice->SetSelection(b.uplink ? 0 : 1);

        br.nameCtrl = new wxTextCtrl(row, wxID_ANY, wxString::FromUTF8(b.name),
                                     wxDefaultPosition, wxSize(-1,-1));

        wxButton* remBtn = new wxButton(row, wxID_ANY, "x",
                                        wxDefaultPosition, wxSize(24,24));
        remBtn->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));

        hs->Add(br.typeChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        hs->Add(br.freqCtrl,   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        wxStaticText* mhz = new wxStaticText(row, wxID_ANY, "MHz");
        mhz->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
        hs->Add(mhz,           0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        hs->Add(br.dirChoice,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        hs->Add(br.nameCtrl,   1, wxEXPAND | wxRIGHT, 4);
        hs->Add(remBtn,        0, wxALIGN_CENTER_VERTICAL);
        row->SetSizer(hs);

        wxPanel* rowPtr = row;
        bandRows_.push_back(br);

        remBtn->Bind(wxEVT_BUTTON, [this, rowPtr](wxCommandEvent&) {
            RemoveBandRow(rowPtr);
        });

        bandSizer_->Add(row, 0, wxEXPAND | wxBOTTOM, 4);
    }

    void RemoveBandRow(wxPanel* rowPtr)
    {
        for (auto it = bandRows_.begin(); it != bandRows_.end(); ++it) {
            if (it->panel == rowPtr) {
                it->panel->Destroy();
                bandRows_.erase(it);
                bandPanel_->FitInside();
                bandPanel_->Layout();
                return;
            }
        }
    }

    wxTextCtrl*       nameCtrl_;
    wxTextCtrl*       noradCtrl_;
    wxChoice*         orbitCtrl_;
    wxChoice*         statusCtrl_;
    wxCheckBox*       launchKnown_;
    wxDatePickerCtrl* launchPicker_;
    wxScrolledWindow* bandPanel_;
    wxBoxSizer*       bandSizer_;
    std::vector<BandRow> bandRows_;
};

// ── SatellitePanel ────────────────────────────────────────────────────────────

SatellitePanel::SatellitePanel(wxWindow* parent, SatelliteStore* store)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(260, -1))
    , store_(store)
{
    SetBackgroundColour(wxColour(248, 249, 250));

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);

    // Header bar
    wxPanel* hdr = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 36));
    hdr->SetBackgroundColour(wxColour(32, 33, 36));
    wxBoxSizer* hdrBox = new wxBoxSizer(wxHORIZONTAL);
    hdr->SetSizer(hdrBox);

    wxStaticText* title = new wxStaticText(hdr, wxID_ANY, "Satellites");
    title->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    title->SetForegroundColour(*wxWHITE);
    hdrBox->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

    wxButton* addBtn = new wxButton(hdr, wxID_ANY, "+",
                                    wxDefaultPosition, wxSize(28, 28));
    addBtn->SetFont(wxFont(wxFontInfo(14).Family(wxFONTFAMILY_SWISS)));
    addBtn->SetForegroundColour(*wxWHITE);
    addBtn->SetBackgroundColour(wxColour(26, 115, 232));
    addBtn->Bind(wxEVT_BUTTON, &SatellitePanel::OnAdd, this);
    hdrBox->Add(addBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

    vbox->Add(hdr, 0, wxEXPAND);

    // Scrollable satellite rows
    scroll_ = new wxScrolledWindow(this, wxID_ANY);
    scroll_->SetBackgroundColour(wxColour(248, 249, 250));
    scroll_->SetScrollRate(0, 10);
    rowSizer_ = new wxBoxSizer(wxVERTICAL);
    scroll_->SetSizer(rowSizer_);

    vbox->Add(scroll_, 1, wxEXPAND);

    store_->Load();
    BuildRows();
}

void SatellitePanel::Reload()
{
    ClearRows();
    BuildRows();
}

void SatellitePanel::ClearRows()
{
    rowSizer_->Clear(true);
    scroll_->FitInside();
    scroll_->Layout();
}

void SatellitePanel::BuildRows()
{
    const auto& sats = store_->Satellites();
    if (sats.empty()) {
        wxStaticText* empty = new wxStaticText(scroll_, wxID_ANY,
            "No satellites.\nClick + to add one.",
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        empty->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        empty->SetForegroundColour(wxColour(128,128,128));
        rowSizer_->Add(empty, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 20);
        scroll_->FitInside();
        return;
    }

    for (const auto& sat : sats) {
        const std::string id = sat.id;

        // Row container
        wxPanel* row = new wxPanel(scroll_, wxID_ANY);
        row->SetBackgroundColour(*wxWHITE);
        wxBoxSizer* rv = new wxBoxSizer(wxVERTICAL);
        row->SetSizer(rv);

        // Line 1: status dot + name + edit/delete buttons
        wxBoxSizer* line1 = new wxBoxSizer(wxHORIZONTAL);

        wxPanel* dot = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(10, 10));
        dot->SetBackgroundColour(StatusColour(sat.status));
        dot->SetMinSize(wxSize(10, 10));
        line1->Add(dot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        wxStaticText* nameLabel = new wxStaticText(row, wxID_ANY,
            wxString::FromUTF8(sat.name), wxDefaultPosition, wxDefaultSize,
            wxST_ELLIPSIZE_END);
        nameLabel->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
        nameLabel->SetForegroundColour(wxColour(32,33,36));
        line1->Add(nameLabel, 1, wxALIGN_CENTER_VERTICAL);

        wxButton* editBtn = new wxButton(row, wxID_ANY, wxString::FromUTF8("✎"),
                                         wxDefaultPosition, wxSize(22, 22));
        editBtn->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
        editBtn->SetToolTip("Edit");
        editBtn->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent&) { OnEdit(id); });

        wxButton* delBtn = new wxButton(row, wxID_ANY, wxString::FromUTF8("×"),
                                        wxDefaultPosition, wxSize(22, 22));
        delBtn->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        delBtn->SetForegroundColour(wxColour(176,0,0));
        delBtn->SetToolTip("Delete");
        delBtn->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent&) { OnDelete(id); });

        line1->Add(editBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        line1->Add(delBtn,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rv->Add(line1, 0, wxEXPAND | wxTOP, 6);

        // Line 2: NORAD ID + orbit type
        wxString infoStr = wxString::Format("NORAD: %s  %s",
            wxString::FromUTF8(sat.noradId.empty() ? "—" : sat.noradId),
            wxString::FromUTF8(sat.orbitType.empty() ? "—" : sat.orbitType));
        wxStaticText* infoLabel = new wxStaticText(row, wxID_ANY, infoStr);
        infoLabel->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
        infoLabel->SetForegroundColour(wxColour(95,99,104));
        rv->Add(infoLabel, 0, wxLEFT, 22);

        // Line 3: launch date
        if (sat.launchKnown && sat.launchDate.IsValid()) {
            wxStaticText* ldLabel = new wxStaticText(row, wxID_ANY,
                wxString::Format("Launch: %s",
                    sat.launchDate.Format("%Y-%m-%d")));
            ldLabel->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
            ldLabel->SetForegroundColour(wxColour(95,99,104));
            rv->Add(ldLabel, 0, wxLEFT, 22);
        }

        // Line 4: band chips
        if (!sat.bands.empty()) {
            wxBoxSizer* bands = new wxBoxSizer(wxHORIZONTAL);
            for (const auto& b : sat.bands) {
                wxString chip = wxString::Format("%s%s",
                    wxString::FromUTF8(b.bandType),
                    b.uplink ? wxString::FromUTF8("↑") : wxString::FromUTF8("↓"));
                wxStaticText* ct = new wxStaticText(row, wxID_ANY, chip);
                ct->SetFont(wxFont(wxFontInfo(8).Bold().Family(wxFONTFAMILY_SWISS)));
                ct->SetForegroundColour(BandColour(b.bandType));
                if (!b.name.empty())
                    ct->SetToolTip(wxString::Format("%s  %.1f MHz",
                        wxString::FromUTF8(b.name), b.freqMHz));
                bands->Add(ct, 0, wxRIGHT, 6);
            }
            rv->Add(bands, 0, wxLEFT | wxBOTTOM, 22);
        } else {
            rv->AddSpacer(4);
        }

        // Bottom separator
        wxPanel* sep = new wxPanel(scroll_, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
        sep->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));

        rowSizer_->Add(row, 0, wxEXPAND);
        rowSizer_->Add(sep, 0, wxEXPAND);
    }

    scroll_->FitInside();
    scroll_->Layout();
}

void SatellitePanel::OnAdd(wxCommandEvent&)
{
    Satellite blank;
    blank.orbitType = "LEO";
    blank.status    = SatStatus::Operational;
    SatelliteEditDlg dlg(this, blank, true);
    if (dlg.ShowModal() == wxID_OK) {
        store_->Add(dlg.GetSatellite(""));
        Reload();
    }
}

void SatellitePanel::OnEdit(const std::string& id)
{
    for (const auto& s : store_->Satellites()) {
        if (s.id == id) {
            SatelliteEditDlg dlg(this, s, false);
            if (dlg.ShowModal() == wxID_OK) {
                Satellite updated = dlg.GetSatellite(id);
                store_->Update(updated);
                Reload();
            }
            return;
        }
    }
}

void SatellitePanel::OnDelete(const std::string& id)
{
    for (const auto& s : store_->Satellites()) {
        if (s.id == id) {
            wxString msg = wxString::Format("Delete satellite \"%s\"?",
                                            wxString::FromUTF8(s.name));
            if (wxMessageBox(msg, "Confirm Delete",
                             wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) == wxYES) {
                store_->Remove(id);
                Reload();
            }
            return;
        }
    }
}
