#include "CalendarSourcePanel.h"

CalendarSourcePanel::CalendarSourcePanel(wxWindow* parent, CalendarSourceStore* store)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxBORDER_NONE),
      store_(store)
{
    SetBackgroundColour(wxColour(248, 249, 250));
    SetScrollRate(0, 5);
    Rebuild();
}

void CalendarSourcePanel::Rebuild()
{
    DestroyChildren();

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);

    wxStaticText* hdr = new wxStaticText(this, wxID_ANY, "Calendars");
    hdr->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    hdr->SetForegroundColour(wxColour(60, 64, 67));
    vbox->Add(hdr, 0, wxLEFT | wxTOP | wxBOTTOM, 8);

    static const Alliance groups[] = {
        Alliance::CSpO, Alliance::NATO, Alliance::INDOPACOM
    };

    for (Alliance alliance : groups) {
        const char* label = CalendarSourceStore::AllianceName(alliance);

        wxStaticText* grpLbl = new wxStaticText(this, wxID_ANY, label);
        grpLbl->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
        grpLbl->SetForegroundColour(wxColour(95, 99, 104));
        vbox->Add(grpLbl, 0, wxLEFT | wxTOP, 6);

        for (const auto& src : store_->Sources()) {
            if (src.alliance != alliance) continue;

            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);

            wxPanel* dot = new wxPanel(this, wxID_ANY,
                                        wxDefaultPosition, wxSize(10, 10));
            dot->SetBackgroundColour(src.color);

            wxCheckBox* chk = new wxCheckBox(this, wxID_ANY,
                                              wxString::FromUTF8(src.name));
            chk->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
            chk->SetValue(src.visible);

            std::string sid = src.id;
            chk->Bind(wxEVT_CHECKBOX, [this, sid](wxCommandEvent& ev) {
                store_->SetVisible(sid, ev.IsChecked());
                if (OnVisibilityChanged) OnVisibilityChanged();
            });

            row->Add(dot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
            row->Add(chk, 0, wxALIGN_CENTER_VERTICAL);
            vbox->Add(row, 0, wxLEFT | wxBOTTOM, 6);
        }
    }

    FitInside();
    Layout();
}
