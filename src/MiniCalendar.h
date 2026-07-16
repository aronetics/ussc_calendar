#pragma once
#include <wx/wx.h>
#include <wx/datetime.h>
#include <functional>
#include <vector>

wxDECLARE_EVENT(wxEVT_MINI_CAL_DATE_SELECTED, wxCommandEvent);

class EventStore;

class MiniCalendar : public wxPanel {
public:
    MiniCalendar(wxWindow* parent, EventStore* store);
    void SetDisplayDate(const wxDateTime& date);
    void SetSelectedDate(const wxDateTime& date);
    wxDateTime GetSelectedDate() const { return selected_; }

    std::function<void(const wxDateTime&)> OnDateSelected;

private:
    void OnPaint(wxPaintEvent&);
    void OnLeftDown(wxMouseEvent&);
    void OnPrevMonth(wxCommandEvent&);
    void OnNextMonth(wxCommandEvent&);
    void OnResize(wxSizeEvent&);

    void DrawGrid(wxDC& dc);
    wxDateTime HitTest(const wxPoint& pt) const;

    EventStore* store_;
    wxDateTime  display_;   // any date in the displayed month
    wxDateTime  selected_;  // currently highlighted date
    wxDateTime  today_;

public:
    struct Cell { wxRect rect; wxDateTime date; bool valid = false; };
private:
    std::vector<Cell> cells_;

    wxButton*  btnPrev_;
    wxButton*  btnNext_;
    wxStaticText* monthLabel_;
    wxPanel*   gridPanel_;

    wxDECLARE_EVENT_TABLE();
};
