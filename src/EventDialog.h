#pragma once
#include <wx/wx.h>
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include "CalendarEvent.h"
#include "UserStore.h"
#include "CalendarSourceStore.h"
#include <vector>

class EventDialog : public wxDialog {
public:
    EventDialog(wxWindow* parent, const CalendarEvent& tmpl,
                UserStore* users = nullptr,
                CalendarSourceStore* sources = nullptr);
    CalendarEvent GetEvent() const;

private:
    void BuildColorPicker(wxWindow* parent, wxSizer* sizer);
    void BuildParticipantPicker(wxWindow* parent, wxSizer* sizer);
    void UpdateTimeVisibility();

    void OnAllDayToggle(wxCommandEvent&);
    void OnColorChip(wxCommandEvent& e);
    void OnRecurFreqChange(wxCommandEvent&);
    void OnRecurEndsChange(wxCommandEvent&);
    void OnOK(wxCommandEvent&);

    void UpdateRecurVisibility();

    CalendarEvent     event_;
    wxColour          selectedColor_;
    std::vector<wxColour> palette_;

    wxTextCtrl*       titleCtrl_        = nullptr;
    wxCheckBox*       allDayChk_        = nullptr;
    wxDatePickerCtrl* startDatePicker_  = nullptr;
    wxTimePickerCtrl* startTimePicker_  = nullptr;
    wxDatePickerCtrl* endDatePicker_    = nullptr;
    wxTimePickerCtrl* endTimePicker_    = nullptr;
    wxTextCtrl*       descCtrl_         = nullptr;
    wxTextCtrl*       locCtrl_          = nullptr;
    wxSizer*          timeSizer_        = nullptr;

    std::vector<wxButton*>   colorBtns_;
    std::vector<wxCheckBox*> participantChks_;
    std::vector<std::string> participantIds_;

    UserStore*           users_   = nullptr;
    CalendarSourceStore* sources_ = nullptr;
    wxChoice*            srcChoice_ = nullptr;
    std::vector<std::string> srcIds_;  // parallel to srcChoice_ items

    // ── Recurrence ────────────────────────────────────────────────────────────
    wxChoice*         recurFreq_        = nullptr;
    wxPanel*          recurPanel_       = nullptr;
    wxSpinCtrl*       recurInterval_    = nullptr;
    wxStaticText*     recurUnitLbl_     = nullptr;
    wxChoice*         recurEnds_        = nullptr;
    wxDatePickerCtrl* recurUntilPicker_ = nullptr;
    wxSpinCtrl*       recurCountCtrl_   = nullptr;
    wxStaticText*     recurCountLbl_    = nullptr;

    wxDECLARE_EVENT_TABLE();
};
