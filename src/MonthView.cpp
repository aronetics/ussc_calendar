#include "MonthView.h"
#include "EventStore.h"
#include <wx/dcbuffer.h>
#include <algorithm>

wxBEGIN_EVENT_TABLE(MonthView, wxPanel)
    EVT_PAINT(MonthView::OnPaint)
    EVT_LEFT_DCLICK(MonthView::OnLeftDClick)
    EVT_RIGHT_UP(MonthView::OnRightUp)
    EVT_MOTION(MonthView::OnMotion)
    EVT_SIZE(MonthView::OnSize)
wxEND_EVENT_TABLE()

static const wxColour GRID_LINE (0xDA, 0xDC, 0xE0);
static const wxColour DAY_GRAY  (0x70, 0x75, 0x7A);
static const wxColour OUT_GRAY  (0xBD, 0xBF, 0xC4);
static const wxColour BG_WHITE  (0xFF, 0xFF, 0xFF);

MonthView::MonthView(wxWindow* parent, EventStore* store, UserStore* users)
    : wxPanel(parent, wxID_ANY),
      store_(store),
      users_(users)
{
    today_   = wxDateTime::Today();
    display_ = today_;
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(BG_WHITE);
}

void MonthView::SetDate(const wxDateTime& date)
{
    display_ = wxDateTime(1, date.GetMonth(), date.GetYear(), 0, 0, 0);
    Refresh();
}

void MonthView::OnSize(wxSizeEvent& e)
{
    e.Skip();
    Refresh();
}

void MonthView::DrawHeader(wxDC& dc, const wxRect& r)
{
    static const char* DOW[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    dc.SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(wxColour(95, 99, 104));
    dc.SetBrush(wxBrush(BG_WHITE));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(r);

    int colW = r.width / 7;
    for (int i = 0; i < 7; ++i) {
        wxRect cr(r.x + i * colW, r.y, colW, r.height);
        dc.DrawLabel(DOW[i], cr, wxALIGN_CENTER);
    }
    // Bottom border
    dc.SetPen(wxPen(GRID_LINE, 1));
    dc.DrawLine(r.x, r.GetBottom(), r.GetRight(), r.GetBottom());
}

void MonthView::DrawEventPill(wxDC& dc, const CalendarEvent& e,
                               const wxRect& cellRect, int yOff)
{
    const int PILL_H = 18;
    const int PAD    = 2;

    wxRect pr(cellRect.x + PAD, cellRect.y + 22 + yOff,
              cellRect.width - 2 * PAD, PILL_H);

    if (pr.GetBottom() > cellRect.GetBottom() - PAD) return;

    // Record for hit testing
    ERect er;
    er.rect         = pr;
    er.id           = e.id;
    er.participants = e.participants;
    eRects_.push_back(er);

    // Draw rounded pill
    dc.SetBrush(wxBrush(e.color));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRoundedRectangle(pr, 3);

    // Text
    dc.SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(*wxWHITE);
    dc.SetClippingRegion(pr);

    wxString label;
    if (!e.allDay && e.start.IsValid()) {
        label = e.start.Format("%I:%M %p ") + e.title;
    } else {
        label = e.title;
    }
    dc.DrawText(label, pr.x + 4, pr.y + 2);
    dc.DestroyClippingRegion();
}

void MonthView::DrawDayCell(wxDC& dc, const wxRect& r,
                             const wxDateTime& d, bool inMonth)
{
    // Background
    dc.SetBrush(wxBrush(BG_WHITE));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(r);

    // Grid border (right + bottom)
    dc.SetPen(wxPen(GRID_LINE, 1));
    dc.DrawLine(r.GetRight(), r.y, r.GetRight(), r.GetBottom());
    dc.DrawLine(r.x, r.GetBottom(), r.GetRight(), r.GetBottom());

    bool isToday = (d.GetDay()   == today_.GetDay()   &&
                    d.GetMonth() == today_.GetMonth() &&
                    d.GetYear()  == today_.GetYear());

    // Day number
    wxString num = wxString::Format("%d", d.GetDay());
    dc.SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));

    if (isToday) {
        // Blue filled circle
        dc.SetBrush(wxBrush(wxColour(26, 115, 232)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawCircle(r.x + 16, r.y + 14, 11);
        dc.SetTextForeground(*wxWHITE);
    } else {
        dc.SetTextForeground(inMonth ? wxColour(32, 33, 36) : OUT_GRAY);
    }

    wxSize ts = dc.GetTextExtent(num);
    dc.DrawText(num, r.x + 16 - ts.x / 2, r.y + 4);

    // Events
    if (store_) {
        auto evs = store_->EventsOnDate(d);
        // Sort: all-day first
        std::stable_sort(evs.begin(), evs.end(),
            [](const CalendarEvent* a, const CalendarEvent* b){
                if (a->allDay != b->allDay) return a->allDay > b->allDay;
                return a->start < b->start;
            });

        const int PILL_H  = 18;
        const int PILL_SP = 2;
        const int MAX_PILLS = (r.height - 24) / (PILL_H + PILL_SP);
        int shown = 0;
        for (int i = 0; i < (int)evs.size(); ++i) {
            if (shown < MAX_PILLS - (evs.size() > (size_t)MAX_PILLS ? 1 : 0)) {
                DrawEventPill(dc, *evs[i], r, shown * (PILL_H + PILL_SP));
                ++shown;
            } else {
                // "+N more" label
                int extra = (int)evs.size() - shown;
                wxRect mr(r.x + 2, r.y + 22 + shown * (PILL_H + PILL_SP),
                           r.width - 4, PILL_H);
                dc.SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
                dc.SetTextForeground(DAY_GRAY);
                dc.DrawLabel(wxString::Format("+%d more", extra), mr, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
                break;
            }
        }
    }
}

void MonthView::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(BG_WHITE));
    dc.Clear();

    wxSize sz  = GetClientSize();
    int    w   = sz.GetWidth();
    int    h   = sz.GetHeight();

    int hdrH = HEADER_H;
    wxRect hdrRect(0, 0, w, hdrH);
    DrawHeader(dc, hdrRect);

    // Build 6×7 grid
    cells_.clear();
    eRects_.clear();

    int colW  = w / 7;
    int rowH  = (h - hdrH) / 6;

    wxDateTime first(1, display_.GetMonth(), display_.GetYear(), 0, 0, 0);
    int startDow = (int)first.GetWeekDay();

    wxDateTime cur = first;
    cur.Subtract(wxDateSpan::Days(startDow));

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            bool inMonth = (cur.GetMonth() == display_.GetMonth());
            wxRect cr(col * colW, hdrH + row * rowH, colW, rowH);
            Cell c;
            c.rect    = cr;
            c.date    = cur;
            c.inMonth = inMonth;
            cells_.push_back(c);

            DrawDayCell(dc, cr, cur, inMonth);
            cur.Add(wxDateSpan::Day());
        }
    }

    // Left border on first column
    dc.SetPen(wxPen(GRID_LINE, 1));
    dc.DrawLine(0, hdrH, 0, h);
}

void MonthView::OnRightUp(wxMouseEvent& e)
{
    wxPoint pt = e.GetPosition();

    std::string eid = HitTestEvent(pt);
    wxMenu menu;
    if (!eid.empty()) {
        menu.Append(wxID_EDIT, "Edit Event");
        menu.AppendSeparator();
        menu.Append(wxID_DELETE, "Delete Event");
        int choice = GetPopupMenuSelectionFromUser(menu);
        if (choice == wxID_EDIT || choice == wxID_DELETE) {
            if (OnEventActivated) OnEventActivated(eid);
        }
    } else {
        wxDateTime d = HitTestDay(pt);
        if (!d.IsValid()) return;
        menu.Append(wxID_NEW, wxString::Format("New Event on %s %d",
            wxDateTime::GetMonthName(d.GetMonth(), wxDateTime::Name_Abbr),
            d.GetDay()));
        if (GetPopupMenuSelectionFromUser(menu) == wxID_NEW) {
            if (OnNewEventRequested) OnNewEventRequested(d);
        }
    }
}

void MonthView::OnLeftDClick(wxMouseEvent& e)
{
    wxPoint pt = e.GetPosition();

    // Check event pills first
    std::string eid = HitTestEvent(pt);
    if (!eid.empty()) {
        if (OnEventActivated) OnEventActivated(eid);
        return;
    }

    wxDateTime d = HitTestDay(pt);
    if (d.IsValid()) {
        if (OnDayActivated) OnDayActivated(d);
    }
}

wxDateTime MonthView::HitTestDay(const wxPoint& pt) const
{
    for (const auto& c : cells_) {
        if (c.rect.Contains(pt)) return c.date;
    }
    return wxDateTime();
}

std::string MonthView::HitTestEvent(const wxPoint& pt) const
{
    for (const auto& er : eRects_) {
        if (er.rect.Contains(pt)) return er.id;
    }
    return "";
}

void MonthView::OnMotion(wxMouseEvent& e)
{
    e.Skip();
    wxPoint pt = e.GetPosition();
    for (const auto& er : eRects_) {
        if (!er.rect.Contains(pt)) continue;
        if (er.participants.empty()) {
            SetToolTip(wxEmptyString);
            return;
        }
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
