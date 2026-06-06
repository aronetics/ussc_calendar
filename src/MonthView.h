#pragma once
#include <wx/wx.h>
#include <wx/datetime.h>
#include "CalendarEvent.h"
#include <functional>
#include <vector>

class EventStore;

class MonthView : public wxPanel {
public:
    MonthView(wxWindow* parent, EventStore* store);
    void SetDate(const wxDateTime& date);
    wxDateTime GetDate() const { return display_; }
    void Refresh() { wxPanel::Refresh(); }

    std::function<void(const wxDateTime&)>    OnDayActivated;
    std::function<void(const std::string&)>   OnEventActivated;

private:
    void OnPaint(wxPaintEvent&);
    void OnLeftDClick(wxMouseEvent&);
    void OnSize(wxSizeEvent&);
    void DrawHeader(wxDC& dc, const wxRect& r);
    void DrawDayCell(wxDC& dc, const wxRect& r, const wxDateTime& d, bool inMonth);
    void DrawEventPill(wxDC& dc, const CalendarEvent& e, const wxRect& r, int yOff);
    wxDateTime HitTestDay(const wxPoint& pt) const;
    std::string HitTestEvent(const wxPoint& pt) const;

    EventStore*  store_;
    wxDateTime   display_;
    wxDateTime   today_;

    static const int HEADER_H = 24;

    struct Cell { wxRect rect; wxDateTime date; bool inMonth; };
    std::vector<Cell> cells_;

    struct ERect { wxRect rect; std::string id; };
    std::vector<ERect> eRects_;

    wxDECLARE_EVENT_TABLE();
};
