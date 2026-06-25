#include "WeekView.h"
#include "EventStore.h"
#include <wx/dcbuffer.h>
#include <algorithm>

enum { ID_WEEK_TIMER = wxID_HIGHEST + 500 };

wxBEGIN_EVENT_TABLE(WeekView, wxScrolledWindow)
    EVT_PAINT(WeekView::OnPaint)
    EVT_LEFT_DCLICK(WeekView::OnLeftDClick)
    EVT_RIGHT_UP(WeekView::OnRightUp)
    EVT_MOTION(WeekView::OnMotion)
    EVT_TIMER(ID_WEEK_TIMER, WeekView::OnTimer)
    EVT_SIZE(WeekView::OnSize)
wxEND_EVENT_TABLE()

static const wxColour WV_BG       (0xFF, 0xFF, 0xFF);
static const wxColour WV_GRID     (0xDA, 0xDC, 0xE0);
static const wxColour WV_GRID_HALF(0xEC, 0xED, 0xEF);
static const wxColour WV_TIME_FG  (0x70, 0x75, 0x7A);
static const wxColour WV_HDR_BG   (0xF8, 0xF9, 0xFA);
static const wxColour WV_TODAY_BG (0xE8, 0xF0, 0xFE);
static const wxColour WV_BLUE     (0x1A, 0x73, 0xE8);

WeekView::WeekView(wxWindow* parent, EventStore* store, UserStore* users)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL),
      store_(store),
      users_(users),
      timer_(this, ID_WEEK_TIMER)
{
    today_     = wxDateTime::Today();
    weekStart_ = today_;
    // Set weekStart_ to Sunday of the current week
    int dow = (int)today_.GetWeekDay();
    weekStart_.Subtract(wxDateSpan::Days(dow));

    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(1, 1);

    int virtualW = TIME_COL_W + span_ * 120;  // approx
    int virtualH = 24 * HOUR_H;
    SetVirtualSize(virtualW, virtualH + HEADER_H + ALLDAY_H);

    // Scroll to 7 AM
    Scroll(0, 7 * HOUR_H);

    timer_.Start(60000);   // refresh every minute
}

void WeekView::SetDate(const wxDateTime& d)
{
    int dow = (int)d.GetWeekDay();
    weekStart_ = d;
    weekStart_.Subtract(wxDateSpan::Days(dow));
    Refresh();
}

void WeekView::Refresh()
{
    wxScrolledWindow::Refresh();
}

void WeekView::SetSpan(int days)
{
    span_ = std::max(1, days);
    wxSizeEvent dummy;
    OnSize(dummy);
}

void WeekView::OnSize(wxSizeEvent& e)
{
    e.Skip();
    int w = GetClientSize().GetWidth();
    int colW = std::max(1, (w - TIME_COL_W) / span_);
    SetVirtualSize(TIME_COL_W + span_ * colW, 24 * HOUR_H + HEADER_H + ALLDAY_H);
    Refresh();
}

void WeekView::OnTimer(wxTimerEvent&)
{
    today_ = wxDateTime::Today();
    Refresh();
}

void WeekView::DrawAllDayBand(wxDC& dc, int x, int y, int w, int h)
{
    // Background
    dc.SetBrush(wxBrush(WV_HDR_BG));
    dc.SetPen(wxPen(WV_GRID, 1));
    dc.DrawRectangle(x, y, w, h);

    int colW = std::max(1, (w - TIME_COL_W) / span_);

    for (int col = 0; col < span_; ++col) {
        wxDateTime d = weekStart_;
        d.Add(wxDateSpan::Days(col));

        int cx = x + TIME_COL_W + col * colW;

        // Draw vertical separator
        dc.SetPen(wxPen(WV_GRID, 1));
        dc.DrawLine(cx, y, cx, y + h);

        // All-day events for this column
        if (store_) {
            auto evs = store_->EventsOnDate(d);
            int yOff = 2;
            for (const auto* ep : evs) {
                if (!ep->allDay) continue;
                wxRect pr(cx + 2, y + yOff, colW - 4, h - 4);
                dc.SetBrush(wxBrush(ep->color));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRoundedRectangle(pr, 3);
                dc.SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
                dc.SetTextForeground(*wxWHITE);
                dc.SetClippingRegion(pr);
                dc.DrawText(ep->title, pr.x + 3, pr.y + 2);
                dc.DestroyClippingRegion();

                ERect er; er.rect = pr; er.id = ep->id; er.participants = ep->participants;
                eRects_.push_back(er);
                break;  // one pill per column for simplicity
            }
        }
    }
}

void WeekView::DrawTimeColumn(wxDC& dc, int x, int y, int h)
{
    dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(WV_TIME_FG);

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

void WeekView::DrawEventRect(wxDC& dc, const CalendarEvent& e, const wxRect& r)
{
    dc.SetBrush(wxBrush(e.color));
    dc.SetPen(wxPen(e.color.ChangeLightness(80), 1));
    dc.DrawRoundedRectangle(r, 2);

    dc.SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(*wxWHITE);
    dc.SetClippingRegion(r);

    wxString timeStr;
    if (e.start.IsValid())
        timeStr = e.start.Format("%I:%M %p");
    dc.DrawText(e.title, r.x + 3, r.y + 2);
    if (r.height > 24)
        dc.DrawText(timeStr, r.x + 3, r.y + 14);
    dc.DestroyClippingRegion();
}

void WeekView::DrawCurrentTimeLine(wxDC& dc, int dayCol, int timeColW,
                                    int colW, int baseY)
{
    wxDateTime now = wxDateTime::Now();
    int minutes = now.GetHour() * 60 + now.GetMinute();
    int y = baseY + minutes;

    int lx = timeColW + dayCol * colW;
    int rx = lx + colW;

    dc.SetPen(wxPen(wxColour(234, 67, 53), 2));
    dc.SetBrush(wxBrush(wxColour(234, 67, 53)));
    dc.DrawCircle(lx, y, 5);
    dc.DrawLine(lx, y, rx, y);
}

void WeekView::DrawDayColumns(wxDC& dc, int x, int y, int w, int h)
{
    int colW = std::max(1, (w - TIME_COL_W) / span_);

    // Hour grid lines
    for (int hour = 0; hour <= 24; ++hour) {
        int hy = y + hour * HOUR_H;
        if (hour < 24) {
            dc.SetPen(wxPen(WV_GRID, 1));
            dc.DrawLine(x, hy, x + w, hy);
            // Half-hour
            dc.SetPen(wxPen(WV_GRID_HALF, 1));
            dc.DrawLine(x + TIME_COL_W, hy + HOUR_H / 2, x + w, hy + HOUR_H / 2);
        }
    }

    // Day columns
    for (int col = 0; col < span_; ++col) {
        wxDateTime d = weekStart_;
        d.Add(wxDateSpan::Days(col));

        int cx = x + TIME_COL_W + col * colW;

        // Vertical separator
        dc.SetPen(wxPen(WV_GRID, 1));
        dc.DrawLine(cx, y, cx, y + h);

        // Today highlight
        bool isToday = (d.GetDay()   == today_.GetDay()   &&
                        d.GetMonth() == today_.GetMonth() &&
                        d.GetYear()  == today_.GetYear());
        if (isToday) {
            dc.SetBrush(wxBrush(wxColour(232, 240, 254, 80)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(cx, y, colW, h);
        }

        // Events
        if (store_) {
            auto evs = store_->EventsOnDate(d);
            for (const auto* ep : evs) {
                if (ep->allDay || !ep->start.IsValid()) continue;

                int startMin = ep->start.GetHour() * 60 + ep->start.GetMinute();
                int endMin   = ep->end.IsValid()
                             ? ep->end.GetHour() * 60 + ep->end.GetMinute()
                             : startMin + 60;
                if (endMin <= startMin) endMin = startMin + 30;

                int ey = y + startMin;
                int eh = std::max(15, endMin - startMin);

                wxRect er(cx + 2, ey, colW - 4, eh);

                ERect rec; rec.rect = er; rec.id = ep->id; rec.participants = ep->participants;
                eRects_.push_back(rec);

                DrawEventRect(dc, *ep, er);
            }
        }

        // Current time line
        if (isToday) {
            DrawCurrentTimeLine(dc, col, x + TIME_COL_W, colW, y);
        }
    }
}

void WeekView::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    DoPrepareDC(dc);

    wxSize  sz  = GetVirtualSize();
    int     w   = sz.GetWidth();

    dc.SetBackground(wxBrush(WV_BG));
    dc.Clear();

    eRects_.clear();

    int colW   = std::max(1, (w - TIME_COL_W) / span_);
    int bodyH  = 24 * HOUR_H;

    // ── Fixed header (scrolled along with content) ────────────────────────
    // We draw it at scroll-adjusted position so it stays at virtual top
    int scrollX, scrollY;
    GetViewStart(&scrollX, &scrollY);

    // Header strip at top of virtual canvas
    dc.SetBrush(wxBrush(WV_HDR_BG));
    dc.SetPen(wxPen(WV_GRID, 1));
    dc.DrawRectangle(0, 0, w, HEADER_H);

    dc.SetFont(wxFont(wxFontInfo(11).Bold().Family(wxFONTFAMILY_SWISS)));
    static const char* DOW_SHORT[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

    for (int col = 0; col < span_; ++col) {
        wxDateTime d = weekStart_;
        d.Add(wxDateSpan::Days(col));

        bool isToday = (d.GetDay()   == today_.GetDay()   &&
                        d.GetMonth() == today_.GetMonth() &&
                        d.GetYear()  == today_.GetYear());

        int cx = TIME_COL_W + col * colW;
        wxRect hr(cx, 0, colW, HEADER_H);

        if (isToday) {
            dc.SetTextForeground(WV_BLUE);
        } else {
            dc.SetTextForeground(wxColour(60, 64, 67));
        }

        wxString label = wxString::Format("%s %d",
                                           DOW_SHORT[(int)d.GetWeekDay()], d.GetDay());
        dc.DrawLabel(label, hr, wxALIGN_CENTER);

        dc.SetPen(wxPen(WV_GRID, 1));
        dc.DrawLine(cx, 0, cx, HEADER_H);
    }
    dc.SetPen(wxPen(WV_GRID, 1));
    dc.DrawLine(0, HEADER_H - 1, w, HEADER_H - 1);

    // ── All-day band ──────────────────────────────────────────────────────
    DrawAllDayBand(dc, 0, HEADER_H, w, ALLDAY_H);

    // ── Time column + day body ────────────────────────────────────────────
    int bodyY = HEADER_H + ALLDAY_H;
    DrawTimeColumn(dc, 0, bodyY, bodyH);
    DrawDayColumns(dc, 0, bodyY, w, bodyH);
}

wxDateTime WeekView::SlotFromPoint(const wxPoint& pt, int timeColW, int colW,
                                    int baseY) const
{
    if (pt.x < timeColW) return wxDateTime();
    int col = (pt.x - timeColW) / std::max(1, colW);
    if (col < 0 || col >= span_) return wxDateTime();

    int minutes = pt.y - baseY;
    if (minutes < 0) return wxDateTime();

    wxDateTime d = weekStart_;
    d.Add(wxDateSpan::Days(col));
    d.SetHour(minutes / 60);
    d.SetMinute(minutes % 60);
    d.SetSecond(0);
    return d;
}

void WeekView::OnMotion(wxMouseEvent& e)
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

void WeekView::OnRightUp(wxMouseEvent& e)
{
    int ux, uy;
    CalcUnscrolledPosition(e.GetX(), e.GetY(), &ux, &uy);
    wxPoint upt(ux, uy);

    for (const auto& er : eRects_) {
        if (!er.rect.Contains(upt)) continue;
        wxMenu menu;
        menu.Append(wxID_EDIT, "Edit Event");
        menu.AppendSeparator();
        menu.Append(wxID_DELETE, "Delete Event");
        int choice = GetPopupMenuSelectionFromUser(menu);
        if (choice == wxID_EDIT || choice == wxID_DELETE) {
            if (OnEventActivated) OnEventActivated(er.id);
        }
        return;
    }

    int sz   = GetVirtualSize().GetWidth();
    int colW = std::max(1, (sz - TIME_COL_W) / span_);
    int baseY = HEADER_H + ALLDAY_H;
    wxDateTime slot = SlotFromPoint(upt, TIME_COL_W, colW, baseY);
    if (slot.IsValid()) {
        wxMenu menu;
        menu.Append(wxID_NEW, wxString::Format("New Event on %s %d at %s",
            wxDateTime::GetMonthName(slot.GetMonth(), wxDateTime::Name_Abbr),
            slot.GetDay(), slot.Format("%I:%M %p")));
        if (GetPopupMenuSelectionFromUser(menu) == wxID_NEW) {
            if (OnNewEventRequested) OnNewEventRequested(slot);
        }
    }
}

void WeekView::OnLeftDClick(wxMouseEvent& e)
{
    int ux, uy;
    CalcUnscrolledPosition(e.GetX(), e.GetY(), &ux, &uy);
    wxPoint upt(ux, uy);

    // Hit test events
    for (const auto& er : eRects_) {
        if (er.rect.Contains(upt)) {
            if (OnEventActivated) OnEventActivated(er.id);
            return;
        }
    }

    // Hit test time slots
    int sz  = GetVirtualSize().GetWidth();
    int colW = std::max(1, (sz - TIME_COL_W) / span_);
    int baseY = HEADER_H + ALLDAY_H;
    wxDateTime slot = SlotFromPoint(upt, TIME_COL_W, colW, baseY);
    if (slot.IsValid()) {
        if (OnSlotActivated) OnSlotActivated(slot);
    }
}
