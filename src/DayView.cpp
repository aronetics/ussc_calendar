#include "DayView.h"
#include "EventStore.h"
#include <wx/dcbuffer.h>
#include <algorithm>

enum { ID_DAY_TIMER = wxID_HIGHEST + 600 };

wxBEGIN_EVENT_TABLE(DayView, wxScrolledWindow)
    EVT_PAINT(DayView::OnPaint)
    EVT_LEFT_DCLICK(DayView::OnLeftDClick)
    EVT_MOTION(DayView::OnMotion)
    EVT_TIMER(ID_DAY_TIMER, DayView::OnTimer)
    EVT_SIZE(DayView::OnSize)
wxEND_EVENT_TABLE()

static const wxColour DV_BG      (0xFF, 0xFF, 0xFF);
static const wxColour DV_GRID    (0xDA, 0xDC, 0xE0);
static const wxColour DV_GRID_H  (0xEC, 0xED, 0xEF);
static const wxColour DV_TIME_FG (0x70, 0x75, 0x7A);
static const wxColour DV_HDR_BG  (0xF8, 0xF9, 0xFA);
static const wxColour DV_BLUE    (0x1A, 0x73, 0xE8);

DayView::DayView(wxWindow* parent, EventStore* store, UserStore* users)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL),
      store_(store),
      users_(users),
      timer_(this, ID_DAY_TIMER)
{
    today_ = wxDateTime::Today();
    date_  = today_;

    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(1, 1);

    int virtualH = 24 * HOUR_H + HEADER_H + ALLDAY_H;
    SetVirtualSize(600, virtualH);

    Scroll(0, 7 * HOUR_H);
    timer_.Start(60000);
}

void DayView::SetDate(const wxDateTime& d)
{
    date_ = wxDateTime(d.GetDay(), d.GetMonth(), d.GetYear(), 0, 0, 0);
    Refresh();
}

void DayView::Refresh()
{
    wxScrolledWindow::Refresh();
}

void DayView::OnSize(wxSizeEvent& e)
{
    e.Skip();
    int w = GetClientSize().GetWidth();
    SetVirtualSize(w, 24 * HOUR_H + HEADER_H + ALLDAY_H);
    Refresh();
}

void DayView::OnTimer(wxTimerEvent&)
{
    today_ = wxDateTime::Today();
    Refresh();
}

void DayView::DrawHeader(wxDC& dc, int x, int y, int w)
{
    dc.SetBrush(wxBrush(DV_HDR_BG));
    dc.SetPen(wxPen(DV_GRID, 1));
    dc.DrawRectangle(x, y, w, HEADER_H);

    bool isToday = (date_.GetDay()   == today_.GetDay()   &&
                    date_.GetMonth() == today_.GetMonth() &&
                    date_.GetYear()  == today_.GetYear());

    // Weekday name
    wxString dow  = date_.GetWeekDayName(date_.GetWeekDay(), wxDateTime::Name_Full);
    // Date line: "Wednesday, June 4"
    wxString full = date_.Format("%A, %B %e");

    dc.SetFont(wxFont(wxFontInfo(16).Bold().Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(isToday ? DV_BLUE : wxColour(32, 33, 36));
    wxSize ts = dc.GetTextExtent(full);
    dc.DrawText(full, x + TIME_COL_W + 8, y + (HEADER_H - ts.y) / 2);

    // Bottom border
    dc.SetPen(wxPen(DV_GRID, 1));
    dc.DrawLine(x, y + HEADER_H - 1, x + w, y + HEADER_H - 1);

    // All-day band below header
    dc.SetBrush(wxBrush(DV_HDR_BG));
    dc.DrawRectangle(x, y + HEADER_H, w, ALLDAY_H);

    // All-day events
    if (store_) {
        auto evs = store_->EventsOnDate(date_);
        int xOff = x + TIME_COL_W + 4;
        for (const auto* ep : evs) {
            if (!ep->allDay) continue;
            int ew = std::min(200, w - TIME_COL_W - 8);
            wxRect pr(xOff, y + HEADER_H + 4, ew, ALLDAY_H - 8);
            dc.SetBrush(wxBrush(ep->color));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRoundedRectangle(pr, 3);
            dc.SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
            dc.SetTextForeground(*wxWHITE);
            dc.SetClippingRegion(pr);
            dc.DrawText(ep->title, pr.x + 4, pr.y + 3);
            dc.DestroyClippingRegion();
            xOff += ew + 4;
        }
    }

    dc.SetPen(wxPen(DV_GRID, 1));
    dc.DrawLine(x, y + HEADER_H + ALLDAY_H - 1, x + w, y + HEADER_H + ALLDAY_H - 1);
}

void DayView::DrawTimeColumn(wxDC& dc, int x, int y, int h)
{
    dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(DV_TIME_FG);

    for (int hour = 0; hour <= 23; ++hour) {
        int hy = y + hour * HOUR_H;
        if (hy > y + h) break;

        wxString label;
        if (hour == 0)       label = "12 AM";
        else if (hour < 12)  label = wxString::Format("%d AM", hour);
        else if (hour == 12) label = "12 PM";
        else                 label = wxString::Format("%d PM", hour - 12);

        wxSize ts = dc.GetTextExtent(label);
        dc.DrawText(label, x + TIME_COL_W - ts.x - 6, hy - ts.y / 2);
    }
}

void DayView::DrawCurrentTimeLine(wxDC& dc, int x, int w, int baseY)
{
    wxDateTime now = wxDateTime::Now();
    int minutes = now.GetHour() * 60 + now.GetMinute();
    int y = baseY + minutes;

    dc.SetPen(wxPen(wxColour(234, 67, 53), 2));
    dc.SetBrush(wxBrush(wxColour(234, 67, 53)));
    dc.DrawCircle(x, y, 5);
    dc.DrawLine(x, y, x + w, y);
}

void DayView::DrawEvents(wxDC& dc, int x, int y, int w)
{
    if (!store_) return;

    eRects_.clear();
    auto evs = store_->EventsOnDate(date_);

    for (const auto* ep : evs) {
        if (ep->allDay || !ep->start.IsValid()) continue;

        int startMin = ep->start.GetHour() * 60 + ep->start.GetMinute();
        int endMin   = ep->end.IsValid()
                     ? ep->end.GetHour() * 60 + ep->end.GetMinute()
                     : startMin + 60;
        if (endMin <= startMin) endMin = startMin + 30;

        int ey = y + startMin;
        int eh = std::max(15, endMin - startMin);

        wxRect er(x + 2, ey, w - 4, eh);

        ERect rec; rec.rect = er; rec.id = ep->id; rec.participants = ep->participants;
        eRects_.push_back(rec);

        dc.SetBrush(wxBrush(ep->color));
        dc.SetPen(wxPen(ep->color.ChangeLightness(80), 1));
        dc.DrawRoundedRectangle(er, 3);

        dc.SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
        dc.SetTextForeground(*wxWHITE);
        dc.SetClippingRegion(er);

        wxString timeStr;
        if (ep->start.IsValid())
            timeStr = ep->start.Format("%I:%M %p");
        dc.DrawText(ep->title, er.x + 4, er.y + 3);
        if (er.height > 24)
            dc.DrawText(timeStr, er.x + 4, er.y + 16);
        dc.DestroyClippingRegion();
    }
}

void DayView::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    DoPrepareDC(dc);

    int vw = GetVirtualSize().GetWidth();

    dc.SetBackground(wxBrush(DV_BG));
    dc.Clear();

    // Header + all-day
    DrawHeader(dc, 0, 0, vw);

    int bodyY = HEADER_H + ALLDAY_H;
    int bodyH = 24 * HOUR_H;

    // Hour grid lines
    for (int hour = 0; hour <= 24; ++hour) {
        int hy = bodyY + hour * HOUR_H;
        if (hour < 24) {
            dc.SetPen(wxPen(DV_GRID, 1));
            dc.DrawLine(TIME_COL_W, hy, vw, hy);
            // Half-hour dashed
            dc.SetPen(wxPen(DV_GRID_H, 1));
            dc.DrawLine(TIME_COL_W, hy + HOUR_H / 2, vw, hy + HOUR_H / 2);
        }
    }

    DrawTimeColumn(dc, 0, bodyY, bodyH);
    DrawEvents(dc, TIME_COL_W, bodyY, vw - TIME_COL_W);

    bool isToday = (date_.GetDay()   == today_.GetDay()   &&
                    date_.GetMonth() == today_.GetMonth() &&
                    date_.GetYear()  == today_.GetYear());
    if (isToday) {
        DrawCurrentTimeLine(dc, TIME_COL_W, vw - TIME_COL_W, bodyY);
    }
}

void DayView::OnMotion(wxMouseEvent& e)
{
    e.Skip();
    int ux, uy;
    CalcUnscrolledPosition(e.GetX(), e.GetY(), &ux, &uy);
    wxPoint upt(ux, uy);
    for (const auto& er : eRects_) {
        if (!er.rect.Contains(upt)) continue;
        if (er.participants.empty()) { SetToolTip(wxEmptyString); return; }
        wxString tip;
        for (const auto& pid : er.participants) {
            const UserEntry* u = users_ ? users_->FindUser(pid) : nullptr;
            if (!tip.IsEmpty()) tip += "\n";
            tip += u ? wxString::FromUTF8(u->name) : wxString::FromUTF8(pid);
        }
        SetToolTip(tip);
        return;
    }
    SetToolTip(wxEmptyString);
}

void DayView::OnLeftDClick(wxMouseEvent& e)
{
    int ux, uy;
    CalcUnscrolledPosition(e.GetX(), e.GetY(), &ux, &uy);
    wxPoint upt(ux, uy);

    for (const auto& er : eRects_) {
        if (er.rect.Contains(upt)) {
            if (OnEventActivated) OnEventActivated(er.id);
            return;
        }
    }

    int bodyY = HEADER_H + ALLDAY_H;
    if (uy >= bodyY && ux >= TIME_COL_W) {
        int minutes = uy - bodyY;
        wxDateTime slot = date_;
        slot.SetHour(minutes / 60);
        slot.SetMinute(minutes % 60);
        slot.SetSecond(0);
        if (OnSlotActivated) OnSlotActivated(slot);
    }
}
