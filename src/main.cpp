#include <wx/wx.h>
#include "CalendarFrame.h"

class CalendarApp : public wxApp {
public:
    bool OnInit() override {
        SetAppName("USSC Calendar");
        auto* frame = new CalendarFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(CalendarApp);
