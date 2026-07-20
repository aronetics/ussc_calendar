#include "CalendarFrame.h"
#include "LoginPanel.h"
#include "MiniCalendar.h"
#include "MonthView.h"
#include "WeekView.h"
#include "DayView.h"
#include "EventDialog.h"
#include "UserPanel.h"
#include "CalendarSourcePanel.h"
#include "ConnectorDialog.h"
#include "IcsConnector.h"
#include "NLEventDialog.h"
#include "SatellitePanel.h"
#include "PassPredictPanel.h"
#include "AllianceReportDialog.h"
#include "LanServer.h"
#include "LanClient.h"
#include "License.h"
#include "NetworkInfo.h"
#ifdef _WIN32
#  include <wx/stdpaths.h>
#endif
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <fstream>
#include <sstream>
#include <wx/sizer.h>
#include <algorithm>

// ── ID enum ──────────────────────────────────────────────────────────────────
enum {
    ID_MENU_ACCOUNTS  = wxID_HIGHEST + 50,
    ID_MENU_EXPORT_ICS,
    ID_MENU_ALLIANCE_REPORT,
    ID_MENU_NETWORK,
    ID_MENU_LICENSE,
    ID_TIMER_SYNC     = wxID_HIGHEST + 90,
    ID_TIMER_NETINFO  = wxID_HIGHEST + 91,

    ID_BTN_TODAY   = wxID_HIGHEST + 1,
    ID_BTN_PREV,
    ID_BTN_NEXT,
    ID_BTN_MONTH,
    ID_BTN_WEEK,
    ID_BTN_TWOWEEK,
    ID_BTN_DAY,
    ID_BTN_NEW_FAB,
    ID_BTN_SIDEBAR_NEW,
    ID_BTN_NL_ADD,
    ID_BTN_LOGOUT,
    ID_MENU_NEW_EVENT,
    ID_MENU_LOGOUT,
    ID_MENU_QUIT,
    ID_MENU_ABOUT
};

wxBEGIN_EVENT_TABLE(CalendarFrame, wxFrame)
    EVT_BUTTON(ID_BTN_TODAY,       CalendarFrame::GoToday)
    EVT_BUTTON(ID_BTN_PREV,        CalendarFrame::NavigatePrev)
    EVT_BUTTON(ID_BTN_NEXT,        CalendarFrame::NavigateNext)
    EVT_BUTTON(ID_BTN_MONTH,       CalendarFrame::OnBtnMonth)
    EVT_BUTTON(ID_BTN_WEEK,        CalendarFrame::OnBtnWeek)
    EVT_BUTTON(ID_BTN_TWOWEEK,    CalendarFrame::OnBtnTwoWeek)
    EVT_BUTTON(ID_BTN_DAY,         CalendarFrame::OnBtnDay)
    EVT_BUTTON(ID_BTN_NEW_FAB,     CalendarFrame::OnNewEvent)
    EVT_BUTTON(ID_BTN_SIDEBAR_NEW, CalendarFrame::OnNewEvent)
    EVT_MENU(ID_MENU_NEW_EVENT,    CalendarFrame::OnMenuNewEvent)
    EVT_MENU(ID_MENU_LOGOUT,       CalendarFrame::OnLogout)
    EVT_MENU(ID_MENU_ACCOUNTS,     CalendarFrame::OnMenuAccounts)
    EVT_MENU(ID_MENU_EXPORT_ICS,   CalendarFrame::OnMenuExportICS)
    EVT_MENU(ID_MENU_QUIT,         CalendarFrame::OnMenuQuit)
    EVT_MENU(ID_MENU_ABOUT,        CalendarFrame::OnMenuAbout)
    EVT_BUTTON(ID_BTN_NL_ADD,      CalendarFrame::OnNLAdd)
    EVT_BUTTON(ID_BTN_LOGOUT,           CalendarFrame::OnLogout)
    EVT_MENU(ID_MENU_ALLIANCE_REPORT,   CalendarFrame::OnMenuAllianceReport)
    EVT_MENU(ID_MENU_NETWORK,           CalendarFrame::OnMenuNetworkSettings)
    EVT_MENU(ID_MENU_LICENSE,           CalendarFrame::OnMenuLicense)
    EVT_TIMER(ID_TIMER_SYNC,            CalendarFrame::OnSyncTimer)
    EVT_TIMER(ID_TIMER_NETINFO,         CalendarFrame::OnNetInfoTimer)
    EVT_KEY_DOWN(CalendarFrame::OnKeyDown)
wxEND_EVENT_TABLE()

// ── Construction ─────────────────────────────────────────────────────────────

CalendarFrame::CalendarFrame()
    : wxFrame(nullptr, wxID_ANY, "USSC Calendar",
              wxDefaultPosition, wxSize(1280, 800))
    , syncTimer_   (this, ID_TIMER_SYNC)
    , netInfoTimer_(this, ID_TIMER_NETINFO)
{
    SetMinSize(wxSize(800, 600));
    SetIcon(wxIcon(wxT("#1"), wxBITMAP_TYPE_ICO_RESOURCE));

    // ── Menu ─────────────────────────────────────────────────────────────
    wxMenuBar* mb = new wxMenuBar();

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_MENU_NEW_EVENT, "&New Event\tCtrl+N");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_ACCOUNTS,   "&Accounts...\tCtrl+,");
    fileMenu->Append(ID_MENU_EXPORT_ICS, "Export &ICS...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_ALLIANCE_REPORT, "Alliance &Deconfliction Report...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_LOGOUT, "&Logout");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_QUIT, "&Quit\tCtrl+Q");
    mb->Append(fileMenu, "&File");

    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_NETWORK, "&Network Settings...");
    helpMenu->Append(ID_MENU_LICENSE, "&Enter License Key...");
    helpMenu->AppendSeparator();
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

    // Right separator + satellite panel
    wxPanel* sepR = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(1, -1));
    sepR->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    bodyHBox->Add(sepR, 0, wxEXPAND);

    satPanel_ = BuildSatellitePanel();
    bodyHBox->Add(satPanel_, 0, wxEXPAND);

    // Pass prediction panel (far right)
    wxPanel* sepPass = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(1, -1));
    sepPass->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    bodyHBox->Add(sepPass, 0, wxEXPAND);

    passPanel_ = new PassPredictPanel(this, &gsStore_, &satStore_,
                                      &tleStore_, &store_);
    passPanel_->OnPassesGenerated = [this]() { RefreshAllViews(); if (miniCal_) miniCal_->Refresh(); };
    bodyHBox->Add(passPanel_, 0, wxEXPAND);
    passPanel_->Show(false);

    displayDate_ = wxDateTime::Today();

    // Hide calendar area and satellite panel until the user authenticates
    viewContainer_->Show(false);
    satPanel_->Show(false);

    SetViewMode(ViewMode::Month);
    SetDisplayDate(displayDate_);

    Centre();

    // Defer file I/O until after the first frame is painted so the login
    // panel appears immediately. No widget creation here — only data loads.
    CallAfter([this]() {
        store_.Load();
        userStore_.Load();
        sourceStore_.LoadVisibility();
        LoadConnectorSettings();
        netCfg_ = LoadNetworkConfig();
        ApplyNetworkMode(netCfg_);
    });
}

CalendarFrame::~CalendarFrame()
{
    syncTimer_.Stop();
    netInfoTimer_.Stop();
    if (lanServer_) { lanServer_->Stop(); delete lanServer_; }
    delete lanClient_;
}

// ── Header ───────────────────────────────────────────────────────────────────

wxPanel* CalendarFrame::BuildHeader()
{
    wxPanel* hdr = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 56));
    hdr->SetBackgroundColour(*wxWHITE);

    wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);

    // ── Left side ─────────────────────────────────────────────────────────

    fabBtn_ = nullptr; // "+ New Event" lives above the mini calendar in the sidebar

    // App title — hidden until login
    calTitle_ = new wxStaticText(hdr, wxID_ANY, "Calendar");
    calTitle_->SetFont(wxFont(wxFontInfo(18).Bold().Family(wxFONTFAMILY_SWISS)));
    calTitle_->SetForegroundColour(wxColour(32, 33, 36));
    calTitle_->Show(false);
    hbox->Add(calTitle_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    // "Today" button — hidden until login
    btnToday_ = new wxButton(hdr, ID_BTN_TODAY, "Today",
                             wxDefaultPosition, wxSize(70, 32), wxBORDER_SIMPLE);
    btnToday_->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    btnToday_->Show(false);
    hbox->Add(btnToday_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    // < > navigation — hidden until login
    btnPrev_ = new wxButton(hdr, ID_BTN_PREV, "<",
                            wxDefaultPosition, wxSize(32, 32), wxBORDER_SIMPLE);
    btnNext_ = new wxButton(hdr, ID_BTN_NEXT, ">",
                            wxDefaultPosition, wxSize(32, 32), wxBORDER_SIMPLE);
    btnPrev_->Show(false);
    btnNext_->Show(false);
    hbox->Add(btnPrev_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnNext_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    // Date label — hidden until login
    headerLabel_ = new wxStaticText(hdr, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(220, -1),
                                    wxST_NO_AUTORESIZE);
    headerLabel_->SetFont(wxFont(wxFontInfo(16).Family(wxFONTFAMILY_SWISS)));
    headerLabel_->SetForegroundColour(wxColour(32, 33, 36));
    headerLabel_->Show(false);
    hbox->Add(headerLabel_, 0, wxALIGN_CENTER_VERTICAL);

    hbox->AddStretchSpacer(1);

    // "Signed in as …" label — hidden until login
    userLabel_ = new wxStaticText(hdr, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxST_NO_AUTORESIZE);
    userLabel_->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    userLabel_->SetForegroundColour(wxColour(95, 99, 104));
    userLabel_->Show(false);
    hbox->Add(userLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    logoutBtn_ = new wxButton(hdr, ID_BTN_LOGOUT, "Sign out",
                              wxDefaultPosition, wxSize(-1, 28), wxBORDER_SIMPLE);
    logoutBtn_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    logoutBtn_->Show(false);
    hbox->Add(logoutBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    // ── Right side: view toggle buttons ───────────────────────────────────
    auto makeToggle = [&](int id, const wxString& label) -> wxButton* {
        wxButton* b = new wxButton(hdr, id, label,
                                   wxDefaultPosition, wxSize(60, 32),
                                   wxBORDER_SIMPLE);
        b->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
        return b;
    };

    btnDay_     = makeToggle(ID_BTN_DAY,     "Day");
    btnWeek_    = makeToggle(ID_BTN_WEEK,    "Week");
    btnTwoWeek_ = makeToggle(ID_BTN_TWOWEEK, "2 Weeks");
    btnMonth_   = makeToggle(ID_BTN_MONTH,   "Month");

    btnTwoWeek_->SetMinSize(wxSize(72, 32));

    // Hidden until login
    btnDay_->Show(false);
    btnWeek_->Show(false);
    btnTwoWeek_->Show(false);
    btnMonth_->Show(false);

    hbox->Add(btnDay_,     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnWeek_,    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnTwoWeek_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    hbox->Add(btnMonth_,   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

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
    wxPanel* sb = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(310, -1));
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

    // "+ New Event" button above the mini calendar
    wxButton* newBtn = new wxButton(cs, ID_BTN_SIDEBAR_NEW, "+ New Event",
                                    wxDefaultPosition, wxSize(160, 40));
    newBtn->SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));
    newBtn->SetBackgroundColour(wxColour(26, 115, 232));
    newBtn->SetForegroundColour(*wxWHITE);
    vbox->Add(newBtn, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 12);

    // MiniCalendar + action button side by side
    miniCal_ = new MiniCalendar(cs, &store_);
    miniCal_->OnDateSelected = [this](const wxDateTime& d) {
        SetDisplayDate(d);
        // In month view, drill down to the clicked day for visible feedback
        if (viewMode_ == ViewMode::Month)
            SetViewMode(ViewMode::Day);
    };

    sidebarActionBtn_ = new wxButton(cs, ID_BTN_NL_ADD, "Quick Add",
                                     wxDefaultPosition, wxSize(80, 50));
    sidebarActionBtn_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    sidebarActionBtn_->SetToolTip("Describe events in plain language");

    wxBoxSizer* calRow = new wxBoxSizer(wxHORIZONTAL);
    calRow->Add(miniCal_, 1, wxEXPAND);
    calRow->Add(sidebarActionBtn_, 0, wxALIGN_TOP | wxLEFT, 4);

    vbox->Add(calRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // Separator
    wxPanel* sep = new wxPanel(cs, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    sep->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    vbox->Add(sep, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT | wxRIGHT, 8);

    // People panel — groups + users with visibility toggles
    userPanel_ = new UserPanel(cs, &userStore_);
    userPanel_->OnVisibilityChanged = [this]() { RefreshAllViews(); };
    vbox->Add(userPanel_, 1, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // Separator
    wxPanel* sep2 = new wxPanel(cs, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    sep2->SetBackgroundColour(wxColour(0xDA, 0xDC, 0xE0));
    vbox->Add(sep2, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT | wxRIGHT, 4);

    // Calendar sources — NATO / INDOPACOM country calendars
    srcPanel_ = new CalendarSourcePanel(cs, &sourceStore_);
    srcPanel_->OnVisibilityChanged = [this]() { RefreshAllViews(); };
    vbox->Add(srcPanel_, 1, wxEXPAND | wxLEFT | wxRIGHT, 4);

    return cs;
}

// ── Login callback ────────────────────────────────────────────────────────────

void CalendarFrame::OnLoginSuccess(const std::string& username)
{
    activeUser_ = username;

    // Load satellite data now (panel was built empty; hidden until this point).
    satStore_.Load();
    satPanel_->Reload();

    // Load ground stations and TLE cache
    gsStore_.Load();
    tleStore_.Load();
    passPanel_->Reload();

    // Rebuild sidebar panels with freshly loaded user/source data.
    userPanel_->Rebuild();
    srcPanel_->Rebuild();

    loginPanel_->Show(false);
    calSidebar_->Show(true);
    sidebarPanel_->Layout();

    viewContainer_->Show(true);
    satPanel_->Show(true);
    passPanel_->Show(true);
    GetSizer()->Layout();

    if (fabBtn_)    fabBtn_->Show(true);
    if (calTitle_)  calTitle_->Show(true);
    if (btnToday_)  btnToday_->Show(true);
    if (btnPrev_)   btnPrev_->Show(true);
    if (btnNext_)   btnNext_->Show(true);
    if (headerLabel_) headerLabel_->Show(true);
    if (btnDay_)    btnDay_->Show(true);
    if (btnWeek_)   btnWeek_->Show(true);
    if (btnTwoWeek_) btnTwoWeek_->Show(true);
    if (btnMonth_)  btnMonth_->Show(true);
    if (userLabel_) {
        userLabel_->SetLabel(wxString::Format("Signed in as %s",
                                              wxString::FromUTF8(username)));
        userLabel_->Show(true);
        logoutBtn_->Show(true);
        headerPanel_->GetSizer()->Layout();
    }

    SetDisplayDate(displayDate_);
}

void CalendarFrame::OnLogout(wxCommandEvent&)
{
    activeUser_.clear();

    viewContainer_->Show(false);
    satPanel_->Show(false);
    passPanel_->Show(false);
    calSidebar_->Show(false);
    sidebarPanel_->Layout();
    GetSizer()->Layout();

    if (fabBtn_)     fabBtn_->Show(false);
    if (calTitle_)   calTitle_->Show(false);
    if (btnToday_)   btnToday_->Show(false);
    if (btnPrev_)    btnPrev_->Show(false);
    if (btnNext_)    btnNext_->Show(false);
    if (headerLabel_) headerLabel_->Show(false);
    if (btnDay_)     btnDay_->Show(false);
    if (btnWeek_)    btnWeek_->Show(false);
    if (btnTwoWeek_) btnTwoWeek_->Show(false);
    if (btnMonth_)   btnMonth_->Show(false);
    userLabel_->SetLabel(wxEmptyString);
    userLabel_->Show(false);
    logoutBtn_->Show(false);
    headerPanel_->GetSizer()->Layout();

    loginPanel_->Reset();
    loginPanel_->Show(true);
    sidebarPanel_->Layout();
}

// ── View area ─────────────────────────────────────────────────────────────────

wxPanel* CalendarFrame::BuildViewArea()
{
    wxPanel* area = new wxPanel(this, wxID_ANY);
    area->SetBackgroundColour(*wxWHITE);

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    area->SetSizer(vbox);

    monthView_ = new MonthView(area, &store_, &userStore_);
    weekView_  = new WeekView(area, &store_, &userStore_);
    dayView_   = new DayView(area, &store_, &userStore_);

    // Wire callbacks
    monthView_->OnDayActivated = [this](const wxDateTime& d) {
        SetDisplayDate(d);
        SetViewMode(ViewMode::Day);
    };
    monthView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };
    monthView_->OnNewEventRequested = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };

    weekView_->OnSlotActivated = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };
    weekView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };
    weekView_->OnNewEventRequested = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };

    dayView_->OnSlotActivated = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };
    dayView_->OnEventActivated = [this](const std::string& id) {
        EditEvent(id);
    };
    dayView_->OnNewEventRequested = [this](const wxDateTime& d) {
        CreateEventOnDate(d);
    };

    vbox->Add(monthView_, 1, wxEXPAND);
    vbox->Add(weekView_,  1, wxEXPAND);
    vbox->Add(dayView_,   1, wxEXPAND);

    monthView_->Show(true);
    weekView_->Show(false);
    dayView_->Show(false);

    return area;
}

// ── Satellite panel ───────────────────────────────────────────────────────────

SatellitePanel* CalendarFrame::BuildSatellitePanel()
{
    return new SatellitePanel(this, &satStore_);
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
    weekView_->Show(m == ViewMode::Week || m == ViewMode::TwoWeek);
    dayView_->Show(m == ViewMode::Day);

    if (m == ViewMode::Week)    weekView_->SetSpan(7);
    if (m == ViewMode::TwoWeek) weekView_->SetSpan(14);

    StyleToggleBtn(btnMonth_,   m == ViewMode::Month);
    StyleToggleBtn(btnWeek_,    m == ViewMode::Week);
    StyleToggleBtn(btnTwoWeek_, m == ViewMode::TwoWeek);
    StyleToggleBtn(btnDay_,     m == ViewMode::Day);

    if (viewContainer_) viewContainer_->Layout();

    SetDisplayDate(displayDate_);
    UpdateHeaderLabel();
}

void CalendarFrame::OnBtnMonth(wxCommandEvent&)   { SetViewMode(ViewMode::Month);   }
void CalendarFrame::OnBtnWeek(wxCommandEvent&)    { SetViewMode(ViewMode::Week);    }
void CalendarFrame::OnBtnTwoWeek(wxCommandEvent&) { SetViewMode(ViewMode::TwoWeek); }
void CalendarFrame::OnBtnDay(wxCommandEvent&)     { SetViewMode(ViewMode::Day);     }

// ── Date navigation ───────────────────────────────────────────────────────────

void CalendarFrame::SetDisplayDate(const wxDateTime& d)
{
    displayDate_ = d;

    switch (viewMode_) {
        case ViewMode::Month:
            monthView_->SetDate(d);
            break;
        case ViewMode::Week:
        case ViewMode::TwoWeek:
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

        case ViewMode::Week:
        case ViewMode::TwoWeek: {
            int span = (viewMode_ == ViewMode::TwoWeek) ? 13 : 6;
            wxDateTime ws = displayDate_;
            int dow = (int)ws.GetWeekDay();
            ws.Subtract(wxDateSpan::Days(dow));
            wxDateTime we = ws;
            we.Add(wxDateSpan::Days(span));
            if (ws.GetMonth() == we.GetMonth()) {
                label = wxString::Format("%s %d - %d, %d",
                    wxDateTime::GetMonthName(ws.GetMonth(), wxDateTime::Name_Abbr),
                    ws.GetDay(), we.GetDay(), ws.GetYear());
            } else {
                label = wxString::Format("%s %d - %s %d, %d",
                    wxDateTime::GetMonthName(ws.GetMonth(), wxDateTime::Name_Abbr),
                    ws.GetDay(),
                    wxDateTime::GetMonthName(we.GetMonth(), wxDateTime::Name_Abbr),
                    we.GetDay(), we.GetYear());
            }
            break;
        }

        case ViewMode::Day:
            label = displayDate_.Format("%A, %B %e %Y");
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
        case ViewMode::TwoWeek:
            displayDate_.Subtract(wxDateSpan::Days(14)); break;
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
        case ViewMode::TwoWeek:
            displayDate_.Add(wxDateSpan::Days(14)); break;
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

    EventDialog dlg(this, tmpl, &userStore_, &sourceStore_);
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

    EventDialog dlg(this, *found, &userStore_, &sourceStore_);
    int rc = dlg.ShowModal();
    if (rc == wxID_OK) {
        CalendarEvent updated = dlg.GetEvent();
        updated.id = id;
        store_.Update(updated);
        RefreshAllViews();
        if (miniCal_) miniCal_->Refresh();
    } else if (rc == wxID_DELETE) {
        store_.Remove(id);
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

void CalendarFrame::OnNLAdd(wxCommandEvent&)
{
    NLEventDialog dlg(this, &userStore_);
    if (dlg.ShowModal() == wxID_OK) {
        for (const auto& ev : dlg.GetEvents())
            store_.Add(ev);
        RefreshAllViews();
        if (miniCal_) miniCal_->Refresh();
    }
}

void CalendarFrame::OnMenuQuit(wxCommandEvent&)
{
    Close(true);
}

void CalendarFrame::OnMenuAllianceReport(wxCommandEvent&)
{
    AllianceReportDialog dlg(this, store_.Events(), satStore_, sourceStore_);
    dlg.ShowModal();
}

void CalendarFrame::OnMenuAbout(wxCommandEvent&)
{
    LicenseInfo li = License::Load();
    wxMessageBox(
        wxString::Format(
            "USSC Calendar\nBuilt with wxWidgets 3.3\n\n"
            "License: %s\n"
            "Network mode: %s",
            wxString::FromUTF8(li.message),
            netCfg_.mode == NetworkMode::Server ? "Server" :
            netCfg_.mode == NetworkMode::Client ? "Client" : "Standalone"),
        "About USSC Calendar", wxOK | wxICON_INFORMATION, this);
}

// ── Networking ────────────────────────────────────────────────────────────────

void CalendarFrame::ApplyNetworkMode(const NetworkConfig& cfg)
{
    syncTimer_.Stop();
    netInfoTimer_.Stop();

    // Tear down any existing server/client
    if (lanServer_) { lanServer_->Stop(); delete lanServer_; lanServer_ = nullptr; }
    if (lanClient_) {
        store_.SetLanClient(nullptr);
        satStore_.SetLanClient(nullptr);
        gsStore_.SetLanClient(nullptr);
        delete lanClient_;
        lanClient_ = nullptr;
        loginPanel_->RemoteAuth = nullptr;
    }

    netCfg_ = cfg;

    if (cfg.mode == NetworkMode::Server) {
        // Delegates to LoginPanel::Validate which uses the same BCrypt SHA-256 hash
        // comparison as the local login path — no separate credential store needed.
        auto credCheck = [this](const std::string& user, const std::string& pass) -> bool {
            return loginPanel_ && loginPanel_->Validate(user, pass);
        };

        lanServer_ = new LanServer(store_, satStore_, gsStore_, userStore_, tleStore_, credCheck);
        std::string err;
        if (!lanServer_->Start(cfg.port, err)) {
            wxMessageBox("Failed to start LAN server: " + wxString::FromUTF8(err),
                         "Network Error", wxOK | wxICON_ERROR, this);
            delete lanServer_;
            lanServer_ = nullptr;
            return;
        }
        // Show server IP and TLS status in the title bar
        std::string ip  = lanServer_->LocalIp();
        std::string tls = lanServer_->IsTlsEnabled() ? " [TLS]" : " [plaintext]";
        SetTitle(wxString::Format("USSC Calendar — SERVER  %s:%d%s",
                                   wxString::FromUTF8(ip), cfg.port,
                                   wxString::FromUTF8(tls)));
        // Refresh the IP/cert display every 5 minutes (DHCP lease may change)
        netInfoTimer_.Start(300000);

    } else if (cfg.mode == NetworkMode::Client) {
        lanClient_ = new LanClient();
        std::string err;
        if (!lanClient_->Connect(cfg.clientHost, cfg.clientPort, err)) {
            wxMessageBox("Cannot reach server at " +
                         wxString::FromUTF8(cfg.clientHost) + ":" +
                         std::to_string(cfg.clientPort) + "\n" +
                         wxString::FromUTF8(err),
                         "Network Error", wxOK | wxICON_ERROR, this);
            delete lanClient_;
            lanClient_ = nullptr;
            return;
        }

        // Wire remote auth into LoginPanel
        loginPanel_->RemoteAuth = [this](const std::string& user, const std::string& pass) -> bool {
            std::string err2;
            bool ok = lanClient_->Auth(user, pass, err2);
            if (!ok && !err2.empty())
                wxMessageBox(wxString::FromUTF8(err2), "Login Failed", wxOK | wxICON_ERROR, this);
            return ok;
        };

        // Wire stores to pull from remote
        store_.SetLanClient(lanClient_);
        satStore_.SetLanClient(lanClient_);
        gsStore_.SetLanClient(lanClient_);

        // Reload stores from server
        store_.Load();
        satStore_.Load();
        gsStore_.Load();

        SetTitle(wxString::Format("USSC Calendar — CLIENT  %s:%d",
                                   wxString::FromUTF8(cfg.clientHost), cfg.clientPort));

        // Poll server every 30 s to keep all operators in sync
        syncTimer_.Start(30000);
    }
}

void CalendarFrame::OnSyncTimer(wxTimerEvent&)
{
    if (!lanClient_ || !lanClient_->IsConnected()) return;
    store_.Load();
    RefreshAllViews();
    if (miniCal_) miniCal_->Refresh();
}

void CalendarFrame::OnNetInfoTimer(wxTimerEvent&)
{
    if (!lanServer_ || !lanServer_->IsRunning()) return;
    // Re-query the DHCP-assigned IP so the title stays correct after a lease renewal
    std::string ip  = lanServer_->LocalIp();
    std::string tls = lanServer_->IsTlsEnabled() ? " [TLS]" : " [plaintext]";
    SetTitle(wxString::Format("USSC Calendar — SERVER  %s:%d%s",
                               wxString::FromUTF8(ip), lanServer_->Port(),
                               wxString::FromUTF8(tls)));
}

void CalendarFrame::OnMenuNetworkSettings(wxCommandEvent&)
{
    NetworkModeDialog dlg(this, netCfg_);
    if (dlg.ShowModal() == wxID_OK) {
        ApplyNetworkMode(dlg.Result());
        if (lanClient_ && !activeUser_.empty()) {
            store_.Load();
            satStore_.Load();
            gsStore_.Load();
            RefreshAllViews();
        }
    }
}

void CalendarFrame::OnMenuLicense(wxCommandEvent&)
{
    wxTextEntryDialog dlg(this,
        "Enter your USSC Calendar license key:\n"
        "(Format: PREFIX-XXXXX-XXXXX-CCCCC)\n"
        "  SGL = Single-User    MLT = Multi-User    TRL = Trial",
        "License Key", "", wxOK | wxCANCEL);
    if (dlg.ShowModal() != wxID_OK) return;
    std::string key = std::string(dlg.GetValue().mb_str());
    LicenseInfo li  = License::Validate(key);
    if (li.valid) {
        License::Save(key);
        wxMessageBox("License accepted: " + wxString::FromUTF8(li.message),
                     "License Activated", wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox("Invalid key: " + wxString::FromUTF8(li.message),
                     "License Error", wxOK | wxICON_ERROR, this);
    }
}

// ── Accounts / connector ──────────────────────────────────────────────────────

void CalendarFrame::OnMenuAccounts(wxCommandEvent&)
{
    ConnectorDialog dlg(this, connectorSettings_);
    if (dlg.ShowModal() == wxID_OK) {
        connectorSettings_ = dlg.GetSettings();
        SaveConnectorSettings();
        for (const auto& ev : dlg.ImportedEvents())
            store_.Add(ev);
        if (!dlg.ImportedEvents().empty())
            RefreshAllViews();
    }
}

void CalendarFrame::OnMenuExportICS(wxCommandEvent&)
{
    wxFileDialog saveDlg(this, "Export Calendar", "",
                         "ussc_calendar.ics",
                         "iCalendar files (*.ics)|*.ics",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() != wxID_OK) return;

    std::string path = saveDlg.GetPath().ToStdString();
    std::string errMsg;
    IcsConnector ics;
    if (!ics.ExportToFile(store_.Events(), path, errMsg)) {
        wxMessageBox(wxString::FromUTF8(errMsg), "Export Failed",
                     wxOK | wxICON_ERROR, this);
    } else {
        wxMessageBox(
            wxString::Format("Exported %zu events to ICS file.", store_.Events().size()),
            "Export Complete", wxOK | wxICON_INFORMATION, this);
    }
}

// ── Connector settings persistence ───────────────────────────────────────────

static std::string ConnectorDataPath()
{
#ifdef _WIN32
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    if (!wxDirExists(dir)) wxMkdir(dir);
    return (dir + "\\cal_connector.json").ToStdString();
#else
    std::string dir = wxGetHomeDir().ToStdString() + "/.ussc_calendar";
    if (!wxDirExists(dir)) wxMkdir(dir);
    return dir + "/cal_connector.json";
#endif
}

void CalendarFrame::LoadConnectorSettings()
{
    std::ifstream f(ConnectorDataPath());
    if (!f.is_open()) return;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    auto extractStr = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\":\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                if      (json[pos] == '"')  val += '"';
                else if (json[pos] == '\\') val += '\\';
                else if (json[pos] == 'n')  val += '\n';
                else if (json[pos] == 'r')  val += '\r';
                else                        val += json[pos];
            } else {
                val += json[pos];
            }
            ++pos;
        }
        return val;
    };

    auto extractInt = [&](const std::string& key) -> int {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        pos += needle.size();
        if (json[pos] == '"') return 0;
        int val = 0;
        while (pos < json.size() && std::isdigit((unsigned char)json[pos]))
            val = val * 10 + (json[pos++] - '0');
        return val;
    };

    auto extractBool = [&](const std::string& key) -> bool {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos += needle.size();
        return json.compare(pos, 4, "true") == 0;
    };

    connectorSettings_.type              = static_cast<ConnectorType>(extractInt("type"));
    connectorSettings_.graphTenantId     = extractStr("graphTenantId");
    connectorSettings_.graphClientId     = extractStr("graphClientId");
    connectorSettings_.graphAccessToken  = extractStr("graphAccessToken");
    connectorSettings_.graphRefreshToken = extractStr("graphRefreshToken");
    connectorSettings_.ewsUrl            = extractStr("ewsUrl");
    connectorSettings_.ewsUsername       = extractStr("ewsUsername");
    connectorSettings_.ewsPassword       = extractStr("ewsPassword");
    connectorSettings_.icsImportPath     = extractStr("icsImportPath");
    connectorSettings_.icsExportPath     = extractStr("icsExportPath");
    connectorSettings_.autoSync          = extractBool("autoSync");
    connectorSettings_.syncIntervalMinutes = extractInt("syncIntervalMinutes");
}

void CalendarFrame::SaveConnectorSettings() const
{
    std::ofstream f(ConnectorDataPath());
    if (!f.is_open()) return;

    auto esc = [](const std::string& s) -> std::string {
        std::string out;
        for (char c : s) {
            if      (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else                out += c;
        }
        return out;
    };

    f << "{"
      << "\"type\":"                << (int)connectorSettings_.type           << ","
      << "\"graphTenantId\":\""     << esc(connectorSettings_.graphTenantId)     << "\","
      << "\"graphClientId\":\""     << esc(connectorSettings_.graphClientId)     << "\","
      << "\"graphAccessToken\":\""  << esc(connectorSettings_.graphAccessToken)  << "\","
      << "\"graphRefreshToken\":\"" << esc(connectorSettings_.graphRefreshToken) << "\","
      << "\"ewsUrl\":\""            << esc(connectorSettings_.ewsUrl)            << "\","
      << "\"ewsUsername\":\""       << esc(connectorSettings_.ewsUsername)       << "\","
      << "\"ewsPassword\":\""       << esc(connectorSettings_.ewsPassword)       << "\","
      << "\"icsImportPath\":\""     << esc(connectorSettings_.icsImportPath)     << "\","
      << "\"icsExportPath\":\""     << esc(connectorSettings_.icsExportPath)     << "\","
      << "\"autoSync\":"            << (connectorSettings_.autoSync ? "true" : "false") << ","
      << "\"syncIntervalMinutes\":" << connectorSettings_.syncIntervalMinutes
      << "}";
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
        case '2':
            SetViewMode(ViewMode::TwoWeek);
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
