#include "MiniCalendar.h"
#include "EventStore.h"
#include <wx/dcbuffer.h>

wxDEFINE_EVENT(wxEVT_MINI_CAL_DATE_SELECTED, wxCommandEvent);

enum {
    ID_MINI_PREV = wxID_HIGHEST + 200,
    ID_MINI_NEXT
};

wxBEGIN_EVENT_TABLE(MiniCalendar, wxPanel)
    EVT_BUTTON(ID_MINI_PREV, MiniCalendar::OnPrevMonth)
    EVT_BUTTON(ID_MINI_NEXT, MiniCalendar::OnNextMonth)
    EVT_SIZE(MiniCalendar::OnResize)
wxEND_EVENT_TABLE()

static const int CELL_W = 28;
static const int CELL_H = 26;
static const int DOW_H  = 20;   // day-of-week header row height

MiniCalendar::MiniCalendar(wxWindow* parent, EventStore* store)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(220, -1)),
      store_(store)
{
    today_    = wxDateTime::Today();
    display_  = today_;
    selected_ = today_;

    SetBackgroundColour(wxColour(248, 249, 250));

    // ── outer vertical sizer ──────────────────────────────────────────────
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

    // Navigation row
    wxBoxSizer* navRow = new wxBoxSizer(wxHORIZONTAL);

    monthLabel_ = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
    monthLabel_->SetFont(wxFont(wxFontInfo(9).Bold().Family(wxFONTFAMILY_SWISS)));

    btnPrev_ = new wxButton(this, ID_MINI_PREV, "<",
                            wxDefaultPosition, wxSize(24, 24), wxBORDER_NONE);
    btnNext_ = new wxButton(this, ID_MINI_NEXT, ">",
                            wxDefaultPosition, wxSize(24, 24), wxBORDER_NONE);
    btnPrev_->SetBackgroundColour(wxColour(248, 249, 250));
    btnNext_->SetBackgroundColour(wxColour(248, 249, 250));

    navRow->Add(btnPrev_, 0, wxALIGN_CENTER_VERTICAL);
    navRow->Add(monthLabel_, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
    navRow->Add(btnNext_, 0, wxALIGN_CENTER_VERTICAL);

    vbox->Add(navRow, 0, wxEXPAND | wxALL, 4);

    // Grid panel (custom painted)
    gridPanel_ = new wxPanel(this, wxID_ANY);
    gridPanel_->SetBackgroundColour(wxColour(248, 249, 250));
    gridPanel_->SetMinSize(wxSize(7 * CELL_W, DOW_H + 6 * CELL_H));
    gridPanel_->SetBackgroundStyle(wxBG_STYLE_PAINT);

    gridPanel_->Bind(wxEVT_PAINT,      &MiniCalendar::OnPaint,    this);
    gridPanel_->Bind(wxEVT_LEFT_DOWN,  &MiniCalendar::OnLeftDown, this);

    vbox->Add(gridPanel_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    SetSizer(vbox);
    SetDisplayDate(display_);
}

void MiniCalendar::SetDisplayDate(const wxDateTime& date)
{
    display_ = wxDateTime(1, date.GetMonth(), date.GetYear(), 0, 0, 0);
    wxString label;
    label.Printf("%s %d",
                 wxDateTime::GetMonthName(display_.GetMonth(), wxDateTime::Name_Abbr),
                 display_.GetYear());
    monthLabel_->SetLabel(label);
    Refresh();
}

void MiniCalendar::SetSelectedDate(const wxDateTime& date)
{
    selected_ = date;
    SetDisplayDate(date);
}

// Build cell geometry for the grid panel
static void BuildCells(const wxDateTime& display, int panelW,
                       std::vector<MiniCalendar::Cell>& cells)
{
    cells.clear();

    int cols = 7;
    int cw   = panelW / cols;

    wxDateTime first(1, display.GetMonth(), display.GetYear(), 0, 0, 0);
    int startDow = (int)first.GetWeekDay();   // 0=Sun

    wxDateTime cur = first;
    cur.Subtract(wxDateSpan::Days(startDow));

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            MiniCalendar::Cell c;
            c.rect  = wxRect(col * cw, DOW_H + row * CELL_H, cw, CELL_H);
            c.date  = cur;
            c.valid = true;
            cells.push_back(c);
            cur.Add(wxDateSpan::Day());
        }
    }
}

void MiniCalendar::DrawGrid(wxDC& dc)
{
    int pw = gridPanel_->GetClientSize().GetWidth();

    BuildCells(display_, pw, cells_);

    // Day-of-week header
    static const char* DOW[] = { "S","M","T","W","T","F","S" };
    int cols = 7;
    int cw   = pw / cols;

    dc.SetFont(wxFont(wxFontInfo(8).Bold().Family(wxFONTFAMILY_SWISS)));
    dc.SetTextForeground(wxColour(95, 99, 104));
    for (int i = 0; i < 7; ++i) {
        wxRect r(i * cw, 0, cw, DOW_H);
        dc.DrawLabel(DOW[i], r, wxALIGN_CENTER);
    }

    dc.SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));

    for (const auto& c : cells_) {
        bool inMonth = (c.date.GetMonth() == display_.GetMonth());
        bool isToday = (c.date.GetDay()   == today_.GetDay()   &&
                        c.date.GetMonth() == today_.GetMonth() &&
                        c.date.GetYear()  == today_.GetYear());
        bool isSel   = (c.date.GetDay()   == selected_.GetDay()   &&
                        c.date.GetMonth() == selected_.GetMonth() &&
                        c.date.GetYear()  == selected_.GetYear());

        wxRect r = c.rect;
        int cx = r.x + r.width / 2;
        int cy = r.y + r.height / 2;
        int radius = 11;

        if (isToday) {
            dc.SetBrush(wxBrush(wxColour(26, 115, 232)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawCircle(cx, cy - 2, radius);
            dc.SetTextForeground(*wxWHITE);
        } else if (isSel) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(wxColour(26, 115, 232), 1));
            dc.DrawCircle(cx, cy - 2, radius);
            dc.SetTextForeground(wxColour(26, 115, 232));
        } else {
            dc.SetTextForeground(inMonth ? wxColour(32, 33, 36) : wxColour(188, 190, 196));
        }

        wxString label = wxString::Format("%d", c.date.GetDay());
        wxSize   ts    = dc.GetTextExtent(label);
        dc.DrawText(label, cx - ts.x / 2, r.y + 2);

        // Dot if events exist
        if (store_) {
            auto evs = store_->EventsOnDate(c.date);
            if (!evs.empty()) {
                dc.SetBrush(wxBrush(wxColour(26, 115, 232)));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawCircle(cx, r.y + r.height - 4, 2);
            }
        }
    }
}

void MiniCalendar::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(gridPanel_);
    dc.SetBackground(wxBrush(wxColour(248, 249, 250)));
    dc.Clear();
    DrawGrid(dc);
}

void MiniCalendar::OnLeftDown(wxMouseEvent& e)
{
    wxPoint pt = e.GetPosition();
    wxDateTime hit = HitTest(pt);
    if (hit.IsValid()) {
        selected_ = hit;
        gridPanel_->Refresh();
        if (OnDateSelected) OnDateSelected(hit);
    }
}

wxDateTime MiniCalendar::HitTest(const wxPoint& pt) const
{
    for (const auto& c : cells_) {
        if (c.rect.Contains(pt)) return c.date;
    }
    return wxDateTime();
}

void MiniCalendar::OnPrevMonth(wxCommandEvent&)
{
    display_.Subtract(wxDateSpan::Month());
    SetDisplayDate(display_);
}

void MiniCalendar::OnNextMonth(wxCommandEvent&)
{
    display_.Add(wxDateSpan::Month());
    SetDisplayDate(display_);
}

void MiniCalendar::OnResize(wxSizeEvent& e)
{
    e.Skip();
    Refresh();
}
