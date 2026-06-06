#pragma once
#include <wx/wx.h>
#include <wx/datetime.h>
#include "EventStore.h"
#include <memory>

class MiniCalendar;
class MonthView;
class WeekView;
class DayView;

enum class ViewMode { Month, Week, Day };

class CalendarFrame : public wxFrame {
public:
    CalendarFrame();
    ~CalendarFrame();

private:
    // Layout builders
    wxPanel* BuildHeader();
    wxPanel* BuildSidebar();
    wxPanel* BuildViewArea();

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
    wxPanel*      sidebarPanel_  = nullptr;
    wxPanel*      viewContainer_ = nullptr;

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
