#include "EventDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

enum {
    ID_ED_ALLDAY     = wxID_HIGHEST + 300,
    ID_COLOR_BASE,        // IDs ID_COLOR_BASE+0..ID_COLOR_BASE+11 for color chips
    ID_ED_RECUR_FREQ = wxID_HIGHEST + 320,
    ID_ED_RECUR_ENDS
};

wxBEGIN_EVENT_TABLE(EventDialog, wxDialog)
    EVT_CHECKBOX(ID_ED_ALLDAY,     EventDialog::OnAllDayToggle)
    EVT_CHOICE(ID_ED_RECUR_FREQ,   EventDialog::OnRecurFreqChange)
    EVT_CHOICE(ID_ED_RECUR_ENDS,   EventDialog::OnRecurEndsChange)
    EVT_BUTTON(wxID_OK,            EventDialog::OnOK)
wxEND_EVENT_TABLE()

EventDialog::EventDialog(wxWindow* parent, const CalendarEvent& tmpl,
                         UserStore* users, CalendarSourceStore* sources)
    : wxDialog(parent, wxID_ANY, tmpl.id.empty() ? "New Event" : "Edit Event",
               wxDefaultPosition, wxSize(480, (users || sources) ? 820 : 680),
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

    // ── Recurrence ────────────────────────────────────────────────────────
    root->Add(new wxStaticText(this, wxID_ANY, "Recurrence"),
              0, wxLEFT | wxRIGHT | wxBOTTOM, 4);

    {
        auto ordinal = [](int n) -> wxString {
            if (n >= 11 && n <= 13) return wxString::Format("%dth", n);
            switch (n % 10) {
                case 1: return wxString::Format("%dst", n);
                case 2: return wxString::Format("%dnd", n);
                case 3: return wxString::Format("%drd", n);
                default: return wxString::Format("%dth", n);
            }
        };

        wxString wdayName = tmpl.start.IsValid() ? tmpl.start.Format("%A") : "day";
        wxString monthDay = tmpl.start.IsValid() ? ordinal(tmpl.start.GetDay()) : "1st";
        wxString yearDate;
        if (tmpl.start.IsValid())
            yearDate = wxString::Format("%s %d",
                wxDateTime::GetMonthName(tmpl.start.GetMonth()),
                (int)tmpl.start.GetDay());
        else
            yearDate = "this date";

        wxArrayString freqChoices;
        freqChoices.Add("Does not repeat");
        freqChoices.Add("Daily");
        freqChoices.Add(wxString::Format("Weekly on %s", wdayName));
        freqChoices.Add(wxString::Format("Monthly on the %s", monthDay));
        freqChoices.Add(wxString::Format("Annually on %s", yearDate));

        recurFreq_ = new wxChoice(this, ID_ED_RECUR_FREQ,
                                   wxDefaultPosition, wxDefaultSize, freqChoices);
        int freqSel = 0;
        switch (tmpl.recurrence.freq) {
            case RecurFreq::Daily:   freqSel = 1; break;
            case RecurFreq::Weekly:  freqSel = 2; break;
            case RecurFreq::Monthly: freqSel = 3; break;
            case RecurFreq::Yearly:  freqSel = 4; break;
            default:                 freqSel = 0; break;
        }
        recurFreq_->SetSelection(freqSel);
        root->Add(recurFreq_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

        // Details panel — shown only when a frequency is selected
        recurPanel_ = new wxPanel(this, wxID_ANY);
        wxBoxSizer* rpSizer = new wxBoxSizer(wxVERTICAL);
        recurPanel_->SetSizer(rpSizer);

        // "Every N unit(s)" row
        wxBoxSizer* everyRow = new wxBoxSizer(wxHORIZONTAL);
        everyRow->Add(new wxStaticText(recurPanel_, wxID_ANY, "Every"),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        recurInterval_ = new wxSpinCtrl(recurPanel_, wxID_ANY,
            wxString::Format("%d", std::max(1, tmpl.recurrence.interval)),
            wxDefaultPosition, wxSize(60, -1));
        recurInterval_->SetRange(1, 99);
        everyRow->Add(recurInterval_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        recurUnitLbl_ = new wxStaticText(recurPanel_, wxID_ANY, "day(s)");
        everyRow->Add(recurUnitLbl_, 0, wxALIGN_CENTER_VERTICAL);
        rpSizer->Add(everyRow, 0, wxBOTTOM, 8);

        // "Ends" row
        wxBoxSizer* endsRow = new wxBoxSizer(wxHORIZONTAL);
        endsRow->Add(new wxStaticText(recurPanel_, wxID_ANY, "Ends"),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        wxArrayString endsChoices;
        endsChoices.Add("Never");
        endsChoices.Add("On date");
        endsChoices.Add("After");
        recurEnds_ = new wxChoice(recurPanel_, ID_ED_RECUR_ENDS,
                                   wxDefaultPosition, wxDefaultSize, endsChoices);
        int endsSel = 0;
        if (tmpl.recurrence.count > 0)             endsSel = 2;
        else if (tmpl.recurrence.until.IsValid())  endsSel = 1;
        recurEnds_->SetSelection(endsSel);
        endsRow->Add(recurEnds_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        wxDateTime defaultUntil = wxDateTime::Today();
        defaultUntil.Add(wxDateSpan::Year());
        recurUntilPicker_ = new wxDatePickerCtrl(recurPanel_, wxID_ANY,
            tmpl.recurrence.until.IsValid() ? tmpl.recurrence.until : defaultUntil);
        endsRow->Add(recurUntilPicker_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        recurCountCtrl_ = new wxSpinCtrl(recurPanel_, wxID_ANY,
            wxString::Format("%d", tmpl.recurrence.count > 0 ? tmpl.recurrence.count : 10),
            wxDefaultPosition, wxSize(60, -1));
        recurCountCtrl_->SetRange(1, 999);
        endsRow->Add(recurCountCtrl_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        recurCountLbl_ = new wxStaticText(recurPanel_, wxID_ANY, "occurrences");
        endsRow->Add(recurCountLbl_, 0, wxALIGN_CENTER_VERTICAL);

        rpSizer->Add(endsRow, 0, wxEXPAND);
        root->Add(recurPanel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

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
        root->Add(new wxStaticText(this, wxID_ANY, "Participants (select 3\xe2\x80\x93" "4):"),
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
    wxBoxSizer* btnRow = new wxBoxSizer(wxHORIZONTAL);

    if (!tmpl.id.empty()) {
        auto* del = new wxButton(this, wxID_DELETE, "Delete");
        del->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_DELETE); });
        btnRow->Add(del, 0, wxALIGN_CENTER_VERTICAL);
    }

    btnRow->AddStretchSpacer(1);
    btnRow->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    btnRow->Add(new wxButton(this, wxID_OK, "Save"), 0, wxALIGN_CENTER_VERTICAL);
    root->Add(btnRow, 0, wxEXPAND | wxALL, 12);

    UpdateTimeVisibility();
    UpdateRecurVisibility();
    Fit();
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

void EventDialog::UpdateRecurVisibility()
{
    if (!recurFreq_ || !recurPanel_) return;

    int freq = recurFreq_->GetSelection();
    recurPanel_->Show(freq > 0);

    static const wxString units[] = { "", "day(s)", "week(s)", "month(s)", "year(s)" };
    if (freq > 0 && freq <= 4)
        recurUnitLbl_->SetLabel(units[freq]);

    if (freq > 0) {
        int ends = recurEnds_->GetSelection();
        recurUntilPicker_->Show(ends == 1);
        recurCountCtrl_->Show(ends == 2);
        recurCountLbl_->Show(ends == 2);
    }

    // Grow the window if the sizer now needs more space; never shrink automatically.
    Layout();
    wxSize needed = GetSizer()->ComputeFittingWindowSize(this);
    wxSize cur    = GetSize();
    if (needed.GetHeight() > cur.GetHeight() || needed.GetWidth() > cur.GetWidth())
        SetSize(wxSize(std::max(needed.GetWidth(),  cur.GetWidth()),
                       std::max(needed.GetHeight(), cur.GetHeight())));
}

void EventDialog::OnRecurFreqChange(wxCommandEvent&) { UpdateRecurVisibility(); }
void EventDialog::OnRecurEndsChange(wxCommandEvent&) { UpdateRecurVisibility(); }

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

    // Recurrence rule
    RecurrenceRule& rr = event_.recurrence;
    int freqSel = recurFreq_->GetSelection();
    switch (freqSel) {
        case 1: rr.freq = RecurFreq::Daily;   break;
        case 2: rr.freq = RecurFreq::Weekly;  break;
        case 3: rr.freq = RecurFreq::Monthly; break;
        case 4: rr.freq = RecurFreq::Yearly;  break;
        default: rr = RecurrenceRule{}; break;
    }
    if (rr.freq != RecurFreq::None) {
        rr.interval = recurInterval_->GetValue();
        rr.until    = wxDateTime();
        rr.count    = 0;
        int endsSel = recurEnds_->GetSelection();
        if (endsSel == 1) rr.until = recurUntilPicker_->GetValue();
        if (endsSel == 2) rr.count = recurCountCtrl_->GetValue();
    }

    EndModal(wxID_OK);
}

CalendarEvent EventDialog::GetEvent() const
{
    return event_;
}
