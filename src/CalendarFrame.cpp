#include "CalendarFrame.h"
#include "LoginPanel.h"
#include "MiniCalendar.h"
#include "MonthView.h"
#include "WeekView.h"
#include "DayView.h"
#include "EventDialog.h"
#include <wx/menu.h>
#include <wx/sizer.h>
#include <algorithm>

// ── ID enum ──────────────────────────────────────────────────────────────────
enum {
    ID_BTN_TODAY   = wxID_HIGHEST + 1,
    ID_BTN_PREV,
    ID_BTN_NEXT,
    ID_BTN_MONTH,
    ID_BTN_WEEK,
    ID_BTN_DAY,
    ID_BTN_NEW_FAB,
    ID_BTN_SIDEBAR_NEW,
    ID_MENU_NEW_EVENT,
    ID_MENU_QUIT,
    ID_MENU_ABOUT
};

wxBEGIN_EVENT_TABLE(CalendarFrame, wxFrame)
    EVT_BUTTON(ID_BTN_TODAY,       CalendarFrame::GoToday)
    EVT_BUTTON(ID_BTN_PREV,        CalendarFrame::NavigatePrev)
    EVT_BUTTON(ID_BTN_NEXT,        CalendarFrame::NavigateNext)
    EVT_BUTTON(ID_BTN_MONTH,       CalendarFrame::OnBtnMonth)
    EVT_BUTTON(ID_BTN_WEEK,        CalendarFrame::OnBtnWeek)
    EVT_BUTTON(ID_BTN_DAY,         CalendarFrame::OnBtnDay)
    EVT_BUTTON(ID_BTN_NEW_FAB,     CalendarFrame::OnNewEvent)
    EVT_BUTTON(ID_BTN_SIDEBAR_NEW, CalendarFrame::OnNewEvent)
    EVT_MENU(ID_MENU_NEW_EVENT,    CalendarFrame::OnMenuNewEvent)
    EVT_MENU(ID_MENU_QUIT,         CalendarFrame::OnMenuQuit)
    EVT_MENU(ID_MENU_ABOUT,        CalendarFrame::OnMenuAbout)
    EVT_KEY_DOWN(CalendarFrame::OnKeyDown)
wxEND_EVENT_TABLE()

// ── Construction ─────────────────────────────────────────────────────────────

CalendarFrame::CalendarFrame()
    : wxFrame(nullptr, wxID_ANY, "USSC Calendar",
              wxDefaultPosition, wxSize(1280, 800))
{
    SetMinSize(wxSize(800, 600));

    // ── Menu ─────────────────────────────────────────────────────────────
    wxMenuBar* mb = new wxMenuBar();

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_MENU_NEW_EVENT, "&New Event\tCtrl+N");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_QUIT, "&Quit\tCtrl+Q");
    mb->Append(fileMenu, "&File");

    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_ABOUT, "&About");
    mb->Append(helpMenu, "&Help");

    SetMenuBar(mb);

    // ── Root layout: header + body ────────────────────────────────────────
    wxBoxSizer* rootVBox = new wxBoxSizer(wxVERTICAL);
    SetSizer(rootVBox);

    headerPanel_ = BuildHeader();
    rootVBox->Add(headerPanel_, 0, wxEXPAND);

    // Body: sidebar + view area
    wxBoxSizer* bodyHBox = new wxBoxSizer(wxHORIZONTAL);
    rootVBox->Add(bodyHBox, 1, wxEXPAND);

    sidebarPanel_ = BuildSidebar();
    bodyHBox->Add(sidebarPanel_, 0, wxEXPAND);

    // Thin separator line
    wxPanel* sep = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(1, -1));
    sep->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    bodyHBox->Add(sep, 0, wxEXPAND);

    viewContainer_ = BuildViewArea();
    bodyHBox->Add(viewContainer_, 1, wxEXPAND);

    // ── Load events & set initial state ───────────────────────────────────
    store_.Load();
    displayDate_ = wxDateTime::Today();

    // Hide calendar area until the user authenticates
    viewContainer_->Show(false);

    SetViewMode(ViewMode::Month);
    SetDisplayDate(displayDate_);

    Centre();
}

CalendarFrame::~CalendarFrame() {}

// ── Header ───────────────────────────────────────────────────────────────────

wxPanel* CalendarFrame::BuildHeader()
{
    wxPanel* hdr = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 56));
    hdr->SetBackgroundColour(*wxWHITE);

    wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
    hdr->SetSizer(hbox);

    // ── Left side ─────────────────────────────────────────────────────────

    // "+ New event" FAB button (blue)
    wxButton* fab = new wxButton(hdr, ID_BTN_NEW_FAB, "+ New event",
                                 wxDefaultPosition, wxSize(130, 36));
    fab->SetBackgroundColour(wxColour(26, 115, 232));
    fab->SetForegroundColour(*wxWHITE);
    fab->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    hbox->Add(fab, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);

    // App title
    wxStaticText* title = new wxStaticText(hdr, wxID_ANY, "Calendar");
    title->SetFont(wxFont(wxFontInfo(18).Bold().Family(wxFONTFAMILY_SWISS)));
    title->SetForegroundColour(wxColour(32, 33, 36));
    hbox->Add(title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    // "Today" button
    wxButton* btnToday = new wxButton(hdr, ID_BTN_TODAY, "Today",
                                      wxDefaultPosition, wxSize(70, 32),
                                      wxBORDER_SIMPLE);
    btnToday->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    hbox->Add(btnToday, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    // < > navigation
    wxButton* btnPrev = new wxButton(hdr, ID_BTN_PREV, "<",
                                     wxDefaultPosition, wxSize(32, 32),
                                     wxBORDER_SIMPLE);
    wxButton* btnNext = new wxButton(hdr, ID_BTN_NEXT, ">",
                                     wxDefaultPosition, wxSize(32, 32),
                                     wxBORDER_SIMPLE);
    hbox->Add(btnPrev, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnNext, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    // Date label
    headerLabel_ = new wxStaticText(hdr, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(220, -1),
                                    wxST_NO_AUTORESIZE);
    headerLabel_->SetFont(wxFont(wxFontInfo(16).Family(wxFONTFAMILY_SWISS)));
    headerLabel_->SetForegroundColour(wxColour(32, 33, 36));
    hbox->Add(headerLabel_, 0, wxALIGN_CENTER_VERTICAL);

    hbox->AddStretchSpacer(1);

    // "Signed in as …" label — hidden until login
    userLabel_ = new wxStaticText(hdr, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxST_NO_AUTORESIZE);
    userLabel_->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    userLabel_->SetForegroundColour(wxColour(95, 99, 104));
    userLabel_->Show(false);
    hbox->Add(userLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    // ── Right side: view toggle buttons ───────────────────────────────────
    auto makeToggle = [&](int id, const wxString& label) -> wxButton* {
        wxButton* b = new wxButton(hdr, id, label,
                                   wxDefaultPosition, wxSize(60, 32),
                                   wxBORDER_SIMPLE);
        b->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
        return b;
    };

    btnDay_   = makeToggle(ID_BTN_DAY,   "Day");
    btnWeek_  = makeToggle(ID_BTN_WEEK,  "Week");
    btnMonth_ = makeToggle(ID_BTN_MONTH, "Month");

    hbox->Add(btnDay_,   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnWeek_,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnMonth_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    // Bottom separator
    wxPanel* line = new wxPanel(hdr, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    line->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));

    wxBoxSizer* vWrap = new wxBoxSizer(wxVERTICAL);
    vWrap->Add(hbox, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);
    vWrap->Add(line, 0, wxEXPAND);
    hdr->SetSizer(vWrap);

    return hdr;
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

wxPanel* CalendarFrame::BuildSidebar()
{
    wxPanel* sb = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(220, -1));
    sb->SetBackgroundColour(wxColour(248, 249, 250));
    sidebarPanel_ = sb;   // set early so BuildCalSidebar() can use it as parent

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    sb->SetSizer(vbox);

    loginPanel_ = new LoginPanel(sb);
    loginPanel_->OnLoginSuccess = [this](const std::string& user) {
        OnLoginSuccess(user);
    };
    vbox->Add(loginPanel_, 1, wxEXPAND);

    calSidebar_ = BuildCalSidebar();
    vbox->Add(calSidebar_, 1, wxEXPAND);
    calSidebar_->Show(false);

    return sb;
}

// ── Calendar sidebar (mini-cal + my-cals) — shown after auth ─────────────────

wxPanel* CalendarFrame::BuildCalSidebar()
{
    wxPanel* cs = new wxPanel(sidebarPanel_, wxID_ANY);
    cs->SetBackgroundColour(wxColour(248, 249, 250));

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    cs->SetSizer(vbox);

    // "+ New Event" button
    wxButton* newBtn = new wxButton(cs, ID_BTN_SIDEBAR_NEW, "+ New Event",
                                    wxDefaultPosition, wxSize(160, 40));
    newBtn->SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));
    newBtn->SetBackgroundColour(wxColour(26, 115, 232));
    newBtn->SetForegroundColour(*wxWHITE);
    vbox->Add(newBtn, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 12);

    // MiniCalendar
    miniCal_ = new MiniCalendar(cs, &store_);
    miniCal_->OnDateSelected = [this](const wxDateTime& d) {
        SetDisplayDate(d);
    };
    vbox->Add(miniCal_, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // Separator
    wxPanel* sep = new wxPanel(cs, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    sep->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    vbox->Add(sep, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT | wxRIGHT, 8);

    // "My calendars" section
    wxStaticText* myCalLabel = new wxStaticText(cs, wxID_ANY, "My calendars");
    myCalLabel->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    myCalLabel->SetForegroundColour(wxColour(60, 64, 67));
    vbox->Add(myCalLabel, 0, wxLEFT | wxBOTTOM, 12);

    struct CalEntry { const char* name; wxColour color; };
    static const CalEntry entries[] = {
        { "Personal",   wxColour( 26, 115, 232) },
        { "Work",       wxColour( 11, 128,  67) },
        { "Birthdays",  wxColour(142,  36, 170) },
        { "Reminders",  wxColour(  3, 155, 229) }
    };

    for (const auto& entry : entries) {
        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        wxPanel* dot = new wxPanel(cs, wxID_ANY, wxDefaultPosition, wxSize(12, 12));
        dot->SetBackgroundColour(entry.color);
        wxStaticText* lbl = new wxStaticText(cs, wxID_ANY, entry.name);
        lbl->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
        row->Add(dot, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
        vbox->Add(row, 0, wxLEFT | wxBOTTOM, 12);
    }

    return cs;
}

// ── Login callback ────────────────────────────────────────────────────────────

void CalendarFrame::OnLoginSuccess(const std::string& username)
{
    activeUser_ = username;

    loginPanel_->Show(false);
    calSidebar_->Show(true);
    sidebarPanel_->Layout();

    viewContainer_->Show(true);
    GetSizer()->Layout();

    if (userLabel_) {
        userLabel_->SetLabel(wxString::Format("Signed in as %s",
                                              wxString::FromUTF8(username)));
        userLabel_->Show(true);
        headerPanel_->GetSizer()->Layout();
    }

    SetDisplayDate(displayDate_);
}

// ── View area ─────────────────────────────────────────────────────────────────

wxPanel* CalendarFrame::BuildViewArea()
{
    wxPanel* area = new wxPanel(this, wxID_ANY);
    area->SetBackgroundColour(*wxWHITE);

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    area->SetSizer(vbox);

    monthView_ = new MonthView(area, &store_);
    weekView_  = new WeekView(area, &store_);
    dayView_   = new DayView(area, &store_);

    // Wire callbacks
    monthView_->OnDayActivated = [this](const wxDateTime& d) {
        SetDisplayDate(d);
        SetViewMode(ViewMode::Day);
    };
    monthView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };

    weekView_->OnSlotActivated = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };
    weekView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };

    dayView_->OnSlotActivated = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };
    dayView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };

    vbox->Add(monthView_, 1, wxEXPAND);
    vbox->Add(weekView_,  1, wxEXPAND);
    vbox->Add(dayView_,   1, wxEXPAND);

    monthView_->Show(true);
    weekView_->Show(false);
    dayView_->Show(false);

    return area;
}

// ── View toggle helpers ───────────────────────────────────────────────────────

static void StyleToggleBtn(wxButton* btn, bool active)
{
    if (active) {
        btn->SetBackgroundColour(wxColour(26, 115, 232));
        btn->SetForegroundColour(*wxWHITE);
    } else {
        btn->SetBackgroundColour(wxColour(240, 240, 240));
        btn->SetForegroundColour(wxColour(60, 64, 67));
    }
    btn->Refresh();
}

void CalendarFrame::SetViewMode(ViewMode m)
{
    viewMode_ = m;

    monthView_->Show(m == ViewMode::Month);
    weekView_->Show(m == ViewMode::Week);
    dayView_->Show(m == ViewMode::Day);

    StyleToggleBtn(btnMonth_, m == ViewMode::Month);
    StyleToggleBtn(btnWeek_,  m == ViewMode::Week);
    StyleToggleBtn(btnDay_,   m == ViewMode::Day);

    if (viewContainer_) viewContainer_->Layout();

    SetDisplayDate(displayDate_);
    UpdateHeaderLabel();
}

void CalendarFrame::OnBtnMonth(wxCommandEvent&) { SetViewMode(ViewMode::Month); }
void CalendarFrame::OnBtnWeek(wxCommandEvent&)  { SetViewMode(ViewMode::Week);  }
void CalendarFrame::OnBtnDay(wxCommandEvent&)   { SetViewMode(ViewMode::Day);   }

// ── Date navigation ───────────────────────────────────────────────────────────

void CalendarFrame::SetDisplayDate(const wxDateTime& d)
{
    displayDate_ = d;

    switch (viewMode_) {
        case ViewMode::Month:
            monthView_->SetDate(d);
            break;
        case ViewMode::Week:
            weekView_->SetDate(d);
            break;
        case ViewMode::Day:
            dayView_->SetDate(d);
            break;
    }

    if (miniCal_) miniCal_->SetDisplayDate(d);
    UpdateHeaderLabel();
}

void CalendarFrame::UpdateHeaderLabel()
{
    if (!headerLabel_) return;

    wxString label;
    switch (viewMode_) {
        case ViewMode::Month:
            label = wxString::Format("%s %d",
                wxDateTime::GetMonthName(displayDate_.GetMonth()),
                displayDate_.GetYear());
            break;

        case ViewMode::Week: {
            // Show Sunday–Saturday range
            wxDateTime ws = displayDate_;
            int dow = (int)ws.GetWeekDay();
            ws.Subtract(wxDateSpan::Days(dow));
            wxDateTime we = ws;
            we.Add(wxDateSpan::Days(6));
            if (ws.GetMonth() == we.GetMonth()) {
                label = wxString::Format("%s %d \xe2\x80\x93 %d, %d",
                    wxDateTime::GetMonthName(ws.GetMonth(), wxDateTime::Name_Abbr),
                    ws.GetDay(), we.GetDay(), ws.GetYear());
            } else {
                label = wxString::Format("%s %d \xe2\x80\x93 %s %d, %d",
                    wxDateTime::GetMonthName(ws.GetMonth(), wxDateTime::Name_Abbr),
                    ws.GetDay(),
                    wxDateTime::GetMonthName(we.GetMonth(), wxDateTime::Name_Abbr),
                    we.GetDay(), we.GetYear());
            }
            break;
        }

        case ViewMode::Day:
            label = displayDate_.Format("%A, %B %e, %Y");
            break;
    }

    headerLabel_->SetLabel(label);
    headerLabel_->GetParent()->Layout();
}

void CalendarFrame::NavigatePrev(wxCommandEvent&)
{
    switch (viewMode_) {
        case ViewMode::Month:
            displayDate_.Subtract(wxDateSpan::Month()); break;
        case ViewMode::Week:
            displayDate_.Subtract(wxDateSpan::Week()); break;
        case ViewMode::Day:
            displayDate_.Subtract(wxDateSpan::Day()); break;
    }
    SetDisplayDate(displayDate_);
}

void CalendarFrame::NavigateNext(wxCommandEvent&)
{
    switch (viewMode_) {
        case ViewMode::Month:
            displayDate_.Add(wxDateSpan::Month()); break;
        case ViewMode::Week:
            displayDate_.Add(wxDateSpan::Week()); break;
        case ViewMode::Day:
            displayDate_.Add(wxDateSpan::Day()); break;
    }
    SetDisplayDate(displayDate_);
}

void CalendarFrame::GoToday(wxCommandEvent&)
{
    displayDate_ = wxDateTime::Today();
    SetDisplayDate(displayDate_);
}

// ── Event CRUD ────────────────────────────────────────────────────────────────

void CalendarFrame::OnNewEvent(wxCommandEvent&)
{
    CreateEventOnDate(displayDate_);
}

void CalendarFrame::OnMenuNewEvent(wxCommandEvent& e)
{
    OnNewEvent(e);
}

void CalendarFrame::CreateEventOnDate(const wxDateTime& date)
{
    CalendarEvent tmpl;
    tmpl.start = wxDateTime(date.GetDay(), date.GetMonth(), date.GetYear(),
                            date.GetHour() > 0 ? date.GetHour() : 9, 0, 0);
    tmpl.end   = tmpl.start;
    tmpl.end.Add(wxTimeSpan::Hour());

    EventDialog dlg(this, tmpl);
    if (dlg.ShowModal() == wxID_OK) {
        CalendarEvent ev = dlg.GetEvent();
        store_.Add(ev);
        RefreshAllViews();
        if (miniCal_) miniCal_->Refresh();
    }
}

void CalendarFrame::EditEvent(const std::string& id)
{
    // Find event
    CalendarEvent* found = nullptr;
    for (auto& e : store_.Events()) {
        if (e.id == id) { found = &e; break; }
    }
    if (!found) return;

    EventDialog dlg(this, *found);
    if (dlg.ShowModal() == wxID_OK) {
        CalendarEvent updated = dlg.GetEvent();
        updated.id = id;
        store_.Update(updated);
        RefreshAllViews();
        if (miniCal_) miniCal_->Refresh();
    }
}

void CalendarFrame::RefreshAllViews()
{
    monthView_->Refresh();
    weekView_->Refresh();
    dayView_->Refresh();
}

// ── Menu handlers ─────────────────────────────────────────────────────────────

void CalendarFrame::OnMenuQuit(wxCommandEvent&)
{
    Close(true);
}

void CalendarFrame::OnMenuAbout(wxCommandEvent&)
{
    wxMessageBox("USSC Calendar\nBuilt with wxWidgets 3.2\n\nA Google Calendar-inspired desktop calendar.",
                 "About USSC Calendar", wxOK | wxICON_INFORMATION, this);
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void CalendarFrame::OnKeyDown(wxKeyEvent& e)
{
    int key = e.GetKeyCode();

    if (e.ControlDown() && key == 'N') {
        CreateEventOnDate(displayDate_);
        return;
    }

    switch (key) {
        case 'T': case 't': {
            wxCommandEvent dummy;
            GoToday(dummy);
            break;
        }
        case 'M': case 'm':
            SetViewMode(ViewMode::Month);
            break;
        case 'W': case 'w':
            SetViewMode(ViewMode::Week);
            break;
        case 'D': case 'd':
            SetViewMode(ViewMode::Day);
            break;
        case WXK_LEFT: {
            wxCommandEvent dummy;
            NavigatePrev(dummy);
            break;
        }
        case WXK_RIGHT: {
            wxCommandEvent dummy;
            NavigateNext(dummy);
            break;
        }
        default:
            e.Skip();
            break;
    }
}
