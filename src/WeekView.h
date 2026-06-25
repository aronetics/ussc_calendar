#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/datetime.h>
#include "CalendarEvent.h"
#include "UserStore.h"
#include <functional>
#include <vector>

class EventStore;

class WeekView : public wxScrolledWindow {
public:
    WeekView(wxWindow* parent, EventStore* store, UserStore* users = nullptr);
    void SetDate(const wxDateTime& d);
    void SetSpan(int days);               // 7 (week) or 14 (two-week)
    int  GetSpan() const { return span_; }
    wxDateTime GetDate() const { return weekStart_; }
    void Refresh();

    std::function<void(const wxDateTime&)>  OnSlotActivated;
    std::function<void(const std::string&)> OnEventActivated;
    std::function<void(const wxDateTime&)>  OnNewEventRequested;

private:
    void OnPaint(wxPaintEvent&);
    void OnLeftDClick(wxMouseEvent&);
    void OnRightUp(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnTimer(wxTimerEvent&);
    void OnSize(wxSizeEvent&);

    void DrawAllDayBand(wxDC& dc, int x, int y, int w, int h);
    void DrawTimeColumn(wxDC& dc, int x, int y, int h);
    void DrawDayColumns(wxDC& dc, int x, int y, int w, int h);
    void DrawEventRect(wxDC& dc, const CalendarEvent& e, const wxRect& r);
    void DrawCurrentTimeLine(wxDC& dc, int dayCol, int timeColW, int colW, int baseY);

    wxDateTime SlotFromPoint(const wxPoint& pt, int timeColW, int colW,
                              int baseY) const;

    EventStore*  store_;
    UserStore*   users_;
    wxDateTime   weekStart_;
    wxDateTime   today_;
    wxTimer      timer_;
    int          span_ = 7;    // 7 = week, 14 = two-week

    static const int TIME_COL_W = 56;
    static const int HOUR_H     = 60;
    static const int ALLDAY_H   = 30;
    static const int HEADER_H   = 40;

    struct ERect { wxRect rect; std::string id; std::vector<std::string> participants; };
    std::vector<ERect> eRects_;

    wxDECLARE_EVENT_TABLE();
};
