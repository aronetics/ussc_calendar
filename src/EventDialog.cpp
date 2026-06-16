#include "EventDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

enum {
    ID_ED_ALLDAY = wxID_HIGHEST + 300,
    ID_COLOR_BASE        // IDs ID_COLOR_BASE+0..ID_COLOR_BASE+11 for color chips
};

wxBEGIN_EVENT_TABLE(EventDialog, wxDialog)
    EVT_CHECKBOX(ID_ED_ALLDAY, EventDialog::OnAllDayToggle)
    EVT_BUTTON(wxID_OK, EventDialog::OnOK)
wxEND_EVENT_TABLE()

EventDialog::EventDialog(wxWindow* parent, const CalendarEvent& tmpl,
                         UserStore* users, CalendarSourceStore* sources)
    : wxDialog(parent, wxID_ANY, tmpl.id.empty() ? "New Event" : "Edit Event",
               wxDefaultPosition, wxSize(480, (users || sources) ? 720 : 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      event_(tmpl),
      users_(users),
      sources_(sources)
{
    selectedColor_ = tmpl.color;

    palette_ = {
        GCTomato(), GCFlamingo(), GCTangerine(), GCBanana(),
        GCSage(),   GCBasil(),   GCPeacock(),   GCBlueberry(),
        GCLavender(), GCGrape(), GCGraphite(),  GCDefault()
    };

    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    SetSizer(root);

    // ── Title ─────────────────────────────────────────────────────────────
    titleCtrl_ = new wxTextCtrl(this, wxID_ANY, tmpl.title,
                                wxDefaultPosition, wxDefaultSize,
                                wxTE_PROCESS_ENTER);
    titleCtrl_->SetFont(wxFont(wxFontInfo(14).Family(wxFONTFAMILY_SWISS)));
    titleCtrl_->SetHint("Add title");
    root->Add(titleCtrl_, 0, wxEXPAND | wxALL, 12);

    // ── All-day ───────────────────────────────────────────────────────────
    allDayChk_ = new wxCheckBox(this, ID_ED_ALLDAY, "All day");
    allDayChk_->SetValue(tmpl.allDay);
    root->Add(allDayChk_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // ── Start row ─────────────────────────────────────────────────────────
    wxBoxSizer* startRow = new wxBoxSizer(wxHORIZONTAL);
    root->Add(startRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    startDatePicker_ = new wxDatePickerCtrl(this, wxID_ANY,
        tmpl.start.IsValid() ? tmpl.start : wxDateTime::Today());
    startRow->Add(new wxStaticText(this, wxID_ANY, "Start:"),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    startRow->Add(startDatePicker_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    wxDateTime st = tmpl.start.IsValid() ? tmpl.start : wxDateTime::Now();
    startTimePicker_ = new wxTimePickerCtrl(this, wxID_ANY, st);
    startRow->Add(startTimePicker_, 0, wxALIGN_CENTER_VERTICAL);

    // ── End row ───────────────────────────────────────────────────────────
    wxBoxSizer* endRow = new wxBoxSizer(wxHORIZONTAL);
    root->Add(endRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    endDatePicker_ = new wxDatePickerCtrl(this, wxID_ANY,
        tmpl.end.IsValid() ? tmpl.end : wxDateTime::Today());
    endRow->Add(new wxStaticText(this, wxID_ANY, "End:  "),
                0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    endRow->Add(endDatePicker_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    wxDateTime et = tmpl.end.IsValid() ? tmpl.end : wxDateTime::Now();
    endTimePicker_ = new wxTimePickerCtrl(this, wxID_ANY, et);
    endRow->Add(endTimePicker_, 0, wxALIGN_CENTER_VERTICAL);

    // Store sizer refs so we can show/hide
    timeSizer_ = startRow;   // used just as marker; we hide the widgets directly

    // ── Description ───────────────────────────────────────────────────────
    root->Add(new wxStaticText(this, wxID_ANY, "Description"),
              0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    descCtrl_ = new wxTextCtrl(this, wxID_ANY, tmpl.description,
                                wxDefaultPosition, wxSize(-1, 80),
                                wxTE_MULTILINE);
    root->Add(descCtrl_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // ── Location ──────────────────────────────────────────────────────────
    root->Add(new wxStaticText(this, wxID_ANY, "Location"),
              0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    locCtrl_ = new wxTextCtrl(this, wxID_ANY, tmpl.location);
    locCtrl_->SetHint("Add location");
    root->Add(locCtrl_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // ── Color picker ──────────────────────────────────────────────────────
    root->Add(new wxStaticText(this, wxID_ANY, "Color"),
              0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    wxBoxSizer* colorRow = new wxBoxSizer(wxHORIZONTAL);
    BuildColorPicker(this, colorRow);
    root->Add(colorRow, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // ── Calendar (country/org source) ────────────────────────────────────
    if (sources_) {
        root->Add(new wxStaticText(this, wxID_ANY, "Calendar"),
                  0, wxLEFT | wxRIGHT | wxBOTTOM, 4);

        wxArrayString choices;
        choices.Add("(None)");
        srcIds_.push_back("");

        static const Alliance alliances[] = {
            Alliance::CSpO, Alliance::NATO, Alliance::INDOPACOM
        };
        for (Alliance a : alliances) {
            choices.Add(wxString("── ") + CalendarSourceStore::AllianceName(a) + " ──");
            srcIds_.push_back("__hdr__");  // non-selectable separator
            for (const auto& src : sources_->Sources()) {
                if (src.alliance != a) continue;
                choices.Add(wxString("  ") + wxString::FromUTF8(src.name));
                srcIds_.push_back(src.id);
            }
        }

        srcChoice_ = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, choices);
        // Select current value
        int sel = 0;
        for (size_t i = 0; i < srcIds_.size(); ++i) {
            if (srcIds_[i] == tmpl.calendarSource) { sel = (int)i; break; }
        }
        srcChoice_->SetSelection(sel);
        root->Add(srcChoice_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    // ── Participants ──────────────────────────────────────────────────────
    if (users_) {
        root->Add(new wxStaticText(this, wxID_ANY, "Participants (select 3\xe2\x80\x934):"),
                  0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
        wxScrolledWindow* pscroll = new wxScrolledWindow(this, wxID_ANY,
            wxDefaultPosition, wxSize(-1, 130),
            wxVSCROLL | wxBORDER_SIMPLE);
        pscroll->SetScrollRate(0, 5);
        wxBoxSizer* psizer = new wxBoxSizer(wxVERTICAL);
        pscroll->SetSizer(psizer);
        BuildParticipantPicker(pscroll, psizer);
        root->Add(pscroll, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    // ── Buttons ───────────────────────────────────────────────────────────
    wxStdDialogButtonSizer* btns = new wxStdDialogButtonSizer();
    btns->AddButton(new wxButton(this, wxID_OK,     "Save"));
    btns->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    btns->Realize();
    root->Add(btns, 0, wxEXPAND | wxALL, 12);

    UpdateTimeVisibility();
    Layout();
    Centre();
}

void EventDialog::BuildColorPicker(wxWindow* parent, wxSizer* sizer)
{
    static const char* names[] = {
        "Tomato","Flamingo","Tangerine","Banana",
        "Sage","Basil","Peacock","Blueberry",
        "Lavender","Grape","Graphite","Default"
    };

    colorBtns_.clear();
    for (int i = 0; i < (int)palette_.size(); ++i) {
        wxButton* btn = new wxButton(parent, ID_COLOR_BASE + i,
                                     wxEmptyString,
                                     wxDefaultPosition, wxSize(24, 24),
                                     wxBORDER_SIMPLE);
        btn->SetBackgroundColour(palette_[i]);
        btn->SetToolTip(names[i]);
        btn->Bind(wxEVT_BUTTON, &EventDialog::OnColorChip, this);
        sizer->Add(btn, 0, wxRIGHT, 4);
        colorBtns_.push_back(btn);
    }
}

void EventDialog::BuildParticipantPicker(wxWindow* parent, wxSizer* sizer)
{
    if (!users_) return;

    participantChks_.clear();
    participantIds_.clear();

    wxFont font(wxFontInfo(9).Family(wxFONTFAMILY_SWISS));

    for (const auto& grp : users_->Groups()) {
        wxStaticText* grpLbl = new wxStaticText(parent, wxID_ANY,
                                                 wxString::FromUTF8(grp));
        grpLbl->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
        grpLbl->SetForegroundColour(wxColour(95, 99, 104));
        sizer->Add(grpLbl, 0, wxLEFT | wxTOP, 6);

        for (const auto* u : users_->UsersInGroup(grp)) {
            bool preChecked = false;
            for (const auto& pid : event_.participants)
                if (pid == u->id) { preChecked = true; break; }

            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxPanel* dot = new wxPanel(parent, wxID_ANY,
                                        wxDefaultPosition, wxSize(10, 10));
            dot->SetBackgroundColour(u->color);
            wxCheckBox* chk = new wxCheckBox(parent, wxID_ANY,
                                              wxString::FromUTF8(u->name));
            chk->SetFont(font);
            chk->SetValue(preChecked);
            row->Add(dot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
            row->Add(chk, 0, wxALIGN_CENTER_VERTICAL);
            sizer->Add(row, 0, wxLEFT | wxBOTTOM, 4);

            participantChks_.push_back(chk);
            participantIds_.push_back(u->id);
        }
    }

    parent->FitInside();
}

void EventDialog::UpdateTimeVisibility()
{
    bool allDay = allDayChk_->GetValue();
    startTimePicker_->Show(!allDay);
    endTimePicker_->Show(!allDay);
    Layout();
}

void EventDialog::OnAllDayToggle(wxCommandEvent&)
{
    UpdateTimeVisibility();
}

void EventDialog::OnColorChip(wxCommandEvent& e)
{
    int idx = e.GetId() - ID_COLOR_BASE;
    if (idx >= 0 && idx < (int)palette_.size()) {
        selectedColor_ = palette_[idx];
        // Highlight selected chip with a border
        for (int i = 0; i < (int)colorBtns_.size(); ++i) {
            colorBtns_[i]->SetLabel(i == idx ? "✓" : "");
        }
    }
}

void EventDialog::OnOK(wxCommandEvent&)
{
    if (titleCtrl_->GetValue().IsEmpty()) {
        wxMessageBox("Please enter a title.", "Validation", wxOK | wxICON_WARNING, this);
        return;
    }

    // Calendar source
    if (srcChoice_) {
        int sel = srcChoice_->GetSelection();
        if (sel >= 0 && sel < (int)srcIds_.size() && srcIds_[sel] != "__hdr__")
            event_.calendarSource = srcIds_[sel];
    }

    // Collect participants
    event_.participants.clear();
    for (size_t i = 0; i < participantChks_.size(); ++i)
        if (participantChks_[i]->GetValue())
            event_.participants.push_back(participantIds_[i]);

    event_.title       = std::string(titleCtrl_->GetValue().mb_str());
    event_.description = std::string(descCtrl_->GetValue().mb_str());
    event_.location    = std::string(locCtrl_->GetValue().mb_str());
    event_.allDay      = allDayChk_->GetValue();
    event_.color       = selectedColor_;

    wxDateTime startDate = startDatePicker_->GetValue();
    wxDateTime endDate   = endDatePicker_->GetValue();

    if (!event_.allDay) {
        wxDateTime st = startTimePicker_->GetValue();
        wxDateTime et = endTimePicker_->GetValue();
        event_.start = wxDateTime(startDate.GetDay(), startDate.GetMonth(), startDate.GetYear(),
                                  st.GetHour(), st.GetMinute(), 0);
        event_.end   = wxDateTime(endDate.GetDay(), endDate.GetMonth(), endDate.GetYear(),
                                  et.GetHour(), et.GetMinute(), 0);
    } else {
        event_.start = wxDateTime(startDate.GetDay(), startDate.GetMonth(),
                                  startDate.GetYear(), 0, 0, 0);
        event_.end   = wxDateTime(endDate.GetDay(), endDate.GetMonth(),
                                  endDate.GetYear(), 0, 0, 0);
    }

    if (event_.end < event_.start) event_.end = event_.start;

    EndModal(wxID_OK);
}

CalendarEvent EventDialog::GetEvent() const
{
    return event_;
}
