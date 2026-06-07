#pragma once
#include <wx/wx.h>
#include <wx/datetime.h>
#include "EventStore.h"
#include <memory>
#include <string>

class MiniCalendar;
class MonthView;
class WeekView;
class DayView;
class LoginPanel;

enum class ViewMode { Month, Week, Day };

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
    void OnMenuQuit(wxCommandEvent&);
    void OnMenuAbout(wxCommandEvent&);

    // Keyboard
    void OnKeyDown(wxKeyEvent&);

    // View mode buttons
    void OnBtnMonth(wxCommandEvent&);
    void OnBtnWeek(wxCommandEvent&);
    void OnBtnDay(wxCommandEvent&);

    EventStore    store_;
    ViewMode      viewMode_  = ViewMode::Month;
    wxDateTime    displayDate_;

    wxPanel*      headerPanel_   = nullptr;
    wxPanel*      sidebarPanel_  = nullptr;  // outer left column
    LoginPanel*   loginPanel_    = nullptr;  // shown before authentication
    wxPanel*      calSidebar_    = nullptr;  // mini-cal + my-cals (shown after auth)
    wxPanel*      viewContainer_ = nullptr;

    wxStaticText* userLabel_  = nullptr;     // shows "Signed in as …" in header
    std::string   activeUser_;

    MiniCalendar* miniCal_   = nullptr;
    MonthView*    monthView_ = nullptr;
    WeekView*     weekView_  = nullptr;
    DayView*      dayView_   = nullptr;

    wxStaticText* headerLabel_ = nullptr;

    wxButton* btnMonth_ = nullptr;
    wxButton* btnWeek_  = nullptr;
    wxButton* btnDay_   = nullptr;

    wxDECLARE_EVENT_TABLE();
};
