#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/datetime.h>
#include "CalendarEvent.h"
#include "UserStore.h"
#include <functional>
#include <vector>

class EventStore;

class DayView : public wxScrolledWindow {
public:
    DayView(wxWindow* parent, EventStore* store, UserStore* users = nullptr);
    void SetDate(const wxDateTime& d);
    wxDateTime GetDate() const { return date_; }
    void Refresh();

    std::function<void(const wxDateTime&)>                        OnSlotActivated;
    std::function<void(const std::string&)>                       OnEventActivated;
    std::function<void(const std::string&, const wxDateTime&)>    OnEventEditOccurrence;
    std::function<void(const std::string&)>                       OnEventDeleteRequested;
    std::function<void(const std::string&, const wxDateTime&)>    OnEventDeleteOccurrence;
    std::function<void(const wxDateTime&)>                        OnNewEventRequested;

private:
    void OnPaint(wxPaintEvent&);
    void OnLeftDClick(wxMouseEvent&);
    void OnRightUp(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnTimer(wxTimerEvent&);
    void OnSize(wxSizeEvent&);

    void DrawHeader(wxDC& dc, int x, int y, int w);
    void DrawTimeColumn(wxDC& dc, int x, int y, int h);
    void DrawEvents(wxDC& dc, int x, int y, int w);
    void DrawCurrentTimeLine(wxDC& dc, int x, int w, int baseY);

    EventStore* store_;
    UserStore*  users_;
    wxDateTime  date_;
    wxDateTime  today_;
    wxTimer     timer_;

    static const int TIME_COL_W = 56;
    static const int HOUR_H     = 60;
    static const int HEADER_H   = 56;
    static const int ALLDAY_H   = 30;

    struct ERect { wxRect rect; std::string id; std::vector<std::string> participants; wxDateTime occurrenceDate; };
    std::vector<ERect> eRects_;

    wxDECLARE_EVENT_TABLE();
};
