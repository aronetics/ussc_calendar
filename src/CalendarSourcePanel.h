#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include "CalendarSourceStore.h"
#include <functional>

class CalendarSourcePanel : public wxScrolledWindow {
public:
    CalendarSourcePanel(wxWindow* parent, CalendarSourceStore* store);
    void Rebuild();

    std::function<void()> OnVisibilityChanged;

private:
    CalendarSourceStore* store_;
};
