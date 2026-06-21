#pragma once
#include <wx/wx.h>
#include <wx/datetime.h>
#include "EventStore.h"
#include "UserStore.h"
#include "CalendarSourceStore.h"
#include "CalendarConnector.h"
#include <memory>
#include <string>

class MiniCalendar;
class MonthView;
class WeekView;
class DayView;
class LoginPanel;
class UserPanel;
class CalendarSourcePanel;

enum class ViewMode { Month, Week, TwoWeek, Day };

class CalendarFrame : public wxFrame {
public:
    CalendarFrame();
    ~CalendarFrame();

private:
    // Layout builders
    wxPanel* BuildHeader();
    wxPanel* BuildSidebar();   // contains loginPanel_ + calSidebar_ stacked
    wxPanel* BuildCalSidebar();
    wxPanel* BuildViewArea();

    // Login
    void OnLoginSuccess(const std::string& username);

    // Navigation & view switching
    void NavigatePrev(wxCommandEvent&);
    void NavigateNext(wxCommandEvent&);
    void GoToday(wxCommandEvent&);
    void SetViewMode(ViewMode m);
    void SetDisplayDate(const wxDateTime& d);
    void UpdateHeaderLabel();

    // Event CRUD
    void OnNewEvent(wxCommandEvent&);
    void CreateEventOnDate(const wxDateTime& date);
    void EditEvent(const std::string& id);
    void RefreshAllViews();

    // Menu / toolbar handlers
    void OnMenuNewEvent(wxCommandEvent&);
    void OnMenuAccounts(wxCommandEvent&);
    void OnMenuExportICS(wxCommandEvent&);
    void OnMenuQuit(wxCommandEvent&);
    void OnMenuAbout(wxCommandEvent&);

    void LoadConnectorSettings();
    void SaveConnectorSettings() const;

    // Keyboard
    void OnKeyDown(wxKeyEvent&);

    // View mode buttons
    void OnBtnMonth(wxCommandEvent&);
    void OnBtnWeek(wxCommandEvent&);
    void OnBtnTwoWeek(wxCommandEvent&);
    void OnBtnDay(wxCommandEvent&);

    EventStore          store_;
    UserStore           userStore_;
    CalendarSourceStore sourceStore_;
    ConnectorSettings   connectorSettings_;
    ViewMode            viewMode_ = ViewMode::Month;
    wxDateTime    displayDate_;

    wxPanel*      headerPanel_   = nullptr;
    wxPanel*      sidebarPanel_  = nullptr;  // outer left column
    LoginPanel*   loginPanel_    = nullptr;  // shown before authentication
    wxPanel*      calSidebar_    = nullptr;  // mini-cal + my-cals (shown after auth)
    wxPanel*      viewContainer_ = nullptr;

    wxStaticText* userLabel_  = nullptr;     // shows "Signed in as …" in header
    std::string   activeUser_;

    MiniCalendar* miniCal_          = nullptr;
    wxButton*     sidebarActionBtn_ = nullptr;
    MonthView*    monthView_ = nullptr;
    WeekView*     weekView_  = nullptr;
    DayView*      dayView_   = nullptr;
    UserPanel*          userPanel_    = nullptr;
    CalendarSourcePanel* srcPanel_    = nullptr;

    wxStaticText* headerLabel_ = nullptr;

    wxButton* btnMonth_   = nullptr;
    wxButton* btnWeek_    = nullptr;
    wxButton* btnTwoWeek_ = nullptr;
    wxButton* btnDay_     = nullptr;

    wxDECLARE_EVENT_TABLE();
};
