#include "NLEventDialog.h"
#include <wx/sizer.h>
#include <algorithm>
#include <cctype>
#include <sstream>

enum {
    ID_NL_INPUT  = wxID_HIGHEST + 800,
    ID_NL_CREATE,
};

wxBEGIN_EVENT_TABLE(NLEventDialog, wxDialog)
    EVT_TEXT(ID_NL_INPUT,        NLEventDialog::OnTextChanged)
    EVT_BUTTON(ID_NL_CREATE,     NLEventDialog::OnCreate)
wxEND_EVENT_TABLE()

NLEventDialog::NLEventDialog(wxWindow* parent, UserStore* users)
    : wxDialog(parent, wxID_ANY, "Quick Add Events",
               wxDefaultPosition, wxSize(480, 280),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , users_(users)
{
    SetBackgroundColour(*wxWHITE);

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);

    wxStaticText* lbl = new wxStaticText(this, wxID_ANY,
        "Describe the event(s) in plain language:");
    lbl->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    vbox->Add(lbl, 0, wxLEFT | wxTOP | wxRIGHT, 16);

    inputCtrl_ = new wxTextCtrl(this, ID_NL_INPUT, wxEmptyString,
                                wxDefaultPosition, wxSize(-1, 60),
                                wxTE_MULTILINE | wxTE_PROCESS_ENTER);
    inputCtrl_->SetHint(
        "e.g. \"schedule five events every other fifth thursday with Bob, Alice and Jack\"");
    inputCtrl_->SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));
    vbox->Add(inputCtrl_, 0, wxEXPAND | wxALL, 16);

    previewLbl_ = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxST_NO_AUTORESIZE);
    previewLbl_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    previewLbl_->SetForegroundColour(wxColour(60, 64, 67));
    vbox->Add(previewLbl_, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    wxBoxSizer* btnRow = new wxBoxSizer(wxHORIZONTAL);
    btnRow->AddStretchSpacer(1);

    wxButton* cancel = new wxButton(this, wxID_CANCEL, "Cancel",
                                    wxDefaultPosition, wxSize(90, 34));
    cancel->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    btnRow->Add(cancel, 0, wxRIGHT, 8);

    createBtn_ = new wxButton(this, ID_NL_CREATE, "Create Events",
                              wxDefaultPosition, wxSize(120, 34));
    createBtn_->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    createBtn_->SetBackgroundColour(wxColour(26, 115, 232));
    createBtn_->SetForegroundColour(*wxWHITE);
    createBtn_->Enable(false);
    btnRow->Add(createBtn_, 0);

    vbox->Add(btnRow, 0, wxEXPAND | wxALL, 16);
}

// ── Event handlers ────────────────────────────────────────────────────────────

void NLEventDialog::OnTextChanged(wxCommandEvent&)
{
    Parse(inputCtrl_->GetValue());
    createBtn_->Enable(!events_.empty());
}

void NLEventDialog::OnCreate(wxCommandEvent&)
{
    Parse(inputCtrl_->GetValue());
    if (!events_.empty())
        EndModal(wxID_OK);
}

// ── Parser ────────────────────────────────────────────────────────────────────

static wxDateTime NthWeekdayOfMonth(int year, wxDateTime::Month month,
                                     wxDateTime::WeekDay wd, int n)
{
    wxDateTime d(1, month, year, 9, 0, 0);
    int found = 0;
    while (d.IsValid() && d.GetMonth() == month) {
        if (d.GetWeekDay() == wd && ++found == n)
            return d;
        d.Add(wxDateSpan::Day());
    }
    return wxDateTime(); // invalid — e.g. 5th Thursday doesn't exist this month
}

void NLEventDialog::Parse(const wxString& rawText)
{
    events_.clear();

    wxString text = rawText.Lower().Trim(true).Trim(false);
    if (text.IsEmpty()) { previewLbl_->SetLabel(wxEmptyString); return; }
    std::string s = text.ToStdString();

    // ── 1. Count ──────────────────────────────────────────────────────────────
    static const char* numWords[] = {
        "one","two","three","four","five",
        "six","seven","eight","nine","ten",
        "eleven","twelve"
    };
    int count = 1;
    for (int i = 0; i < 12; ++i) {
        if (s.find(numWords[i]) != std::string::npos) { count = i + 1; break; }
    }
    // Bare digit before "event"
    for (size_t i = 0; i < s.size(); ++i) {
        if (std::isdigit((unsigned char)s[i])) {
            int n = 0;
            size_t j = i;
            while (j < s.size() && std::isdigit((unsigned char)s[j]))
                n = n * 10 + (int)(s[j++] - '0');
            if (n > 0 && n <= 52) { count = n; break; }
        }
    }

    // ── 2. Recurrence interval ────────────────────────────────────────────────
    int interval = 1;
    bool monthly = false;
    if      (s.find("every other") != std::string::npos) interval = 2;
    else if (s.find("every third") != std::string::npos) interval = 3;
    else if (s.find("every fourth") != std::string::npos) interval = 4;

    // ── 3. Day of week ────────────────────────────────────────────────────────
    static const char* dayNames[] = {
        "sunday","monday","tuesday","wednesday","thursday","friday","saturday"
    };
    int targetDow = -1;
    for (int i = 0; i < 7; ++i) {
        if (s.find(dayNames[i]) != std::string::npos) { targetDow = i; break; }
    }

    // ── 4. Ordinal (Nth day of month) ─────────────────────────────────────────
    static const char* ordWords[] = {"first","second","third","fourth","fifth"};
    int ordinal = 0;
    for (int i = 0; i < 5; ++i) {
        if (s.find(ordWords[i]) != std::string::npos) {
            // "every third/fourth thursday" → interval, not ordinal
            bool usedAsInterval =
                (i == 2 && s.find("every third")  != std::string::npos) ||
                (i == 3 && s.find("every fourth") != std::string::npos);
            if (!usedAsInterval) { ordinal = i + 1; monthly = true; }
            break;
        }
    }

    // ── 5. Attendees — match first names against UserStore ────────────────────
    std::vector<std::string> participantIds;
    size_t withPos = s.find("with");
    if (withPos != std::string::npos) {
        std::string afterWith = s.substr(withPos + 4);
        for (const auto& u : users_->Users()) {
            wxString fullName = wxString::FromUTF8(u.name).Lower();
            int sp = fullName.Find(' ');
            wxString first = (sp != wxNOT_FOUND) ? fullName.Left(sp) : fullName;
            if (afterWith.find(first.ToStdString()) != std::string::npos)
                participantIds.push_back(u.id);
        }
    }

    // ── 6. Compute start date ─────────────────────────────────────────────────
    wxDateTime today = wxDateTime::Today();
    wxDateTime start = today;
    start.SetHour(9); start.SetMinute(0); start.SetSecond(0);

    if (targetDow >= 0) {
        auto wd = (wxDateTime::WeekDay)targetDow;
        if (monthly && ordinal > 0) {
            // Find Nth weekday of current month; if past or invalid, try next month
            wxDateTime cand = NthWeekdayOfMonth(today.GetYear(), today.GetMonth(), wd, ordinal);
            if (!cand.IsValid() || cand < today) {
                wxDateTime nm = today; nm.Add(wxDateSpan::Month());
                cand = NthWeekdayOfMonth(nm.GetYear(), nm.GetMonth(), wd, ordinal);
            }
            if (cand.IsValid()) start = cand;
        } else {
            while ((int)start.GetWeekDay() != targetDow)
                start.Add(wxDateSpan::Day());
        }
    }

    // ── 7. Generate events ────────────────────────────────────────────────────
    wxDateTime cur = start;
    for (int i = 0; i < count; ++i) {
        CalendarEvent ev;
        ev.title        = wxString(rawText).Trim(true).Trim(false).ToStdString();
        ev.start        = cur;
        ev.end          = cur;
        ev.end.Add(wxTimeSpan::Hour());
        ev.participants = participantIds;
        events_.push_back(ev);

        // Advance to next occurrence
        if (monthly && ordinal > 0 && targetDow >= 0) {
            auto wd = (wxDateTime::WeekDay)targetDow;
            wxDateTime nx = cur;
            for (int m = 0; m < interval; ++m) nx.Add(wxDateSpan::Month());
            wxDateTime cand = NthWeekdayOfMonth(nx.GetYear(), nx.GetMonth(), wd, ordinal);
            cur = cand.IsValid() ? cand : nx;
        } else if (targetDow >= 0) {
            cur.Add(wxDateSpan::Weeks(interval));
        } else {
            cur.Add(wxDateSpan::Day());
        }
    }

    // ── 8. Build preview string ───────────────────────────────────────────────
    wxString preview;
    preview << count << " event(s)";

    if (targetDow >= 0) {
        if (monthly && ordinal > 0)
            preview << wxString::Format(", ordinal %d %s", ordinal,
                                        wxDateTime::GetWeekDayName((wxDateTime::WeekDay)targetDow));
        else
            preview << " on " << wxDateTime::GetWeekDayName((wxDateTime::WeekDay)targetDow);
        if (interval > 1) preview << wxString::Format(" (every %d)", interval);
    }

    if (!participantIds.empty()) {
        preview << "\nAttendees: ";
        for (size_t i = 0; i < participantIds.size(); ++i) {
            if (i) preview << ", ";
            const UserEntry* u = users_->FindUser(participantIds[i]);
            if (u) preview << wxString::FromUTF8(u->name);
        }
    }

    if (!events_.empty()) {
        preview << "\nFirst: " << events_.front().start.Format("%a %b %e, %Y 9:00 AM");
        if (events_.size() > 1)
            preview << "  →  Last: " << events_.back().start.Format("%a %b %e, %Y");
    }

    previewLbl_->SetLabel(preview);
    previewLbl_->Wrap(440);
    Layout();
}
