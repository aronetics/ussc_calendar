#pragma once
#include <wx/wx.h>
#include <vector>
#include "CalendarEvent.h"
#include "UserStore.h"

// Dialog that accepts a free-text natural-language event description and
// converts it into one or more CalendarEvents.
class NLEventDialog : public wxDialog {
public:
    NLEventDialog(wxWindow* parent, UserStore* users);

    const std::vector<CalendarEvent>& GetEvents() const { return events_; }

private:
    void OnTextChanged(wxCommandEvent&);
    void OnCreate(wxCommandEvent&);
    void Parse(const wxString& text);

    wxTextCtrl*   inputCtrl_  = nullptr;
    wxStaticText* previewLbl_ = nullptr;
    wxButton*     createBtn_  = nullptr;

    UserStore*                 users_;
    std::vector<CalendarEvent> events_;

    wxDECLARE_EVENT_TABLE();
};
