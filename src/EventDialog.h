#pragma once
#include <wx/wx.h>
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include "CalendarEvent.h"
#include <vector>

class EventDialog : public wxDialog {
public:
    explicit EventDialog(wxWindow* parent, const CalendarEvent& tmpl);
    CalendarEvent GetEvent() const;

private:
    void OnAllDayToggle(wxCommandEvent&);
    void OnColorChip(wxCommandEvent& e);
    void OnOK(wxCommandEvent&);
    void BuildColorPicker(wxWindow* parent, wxSizer* sizer);
    void UpdateTimeVisibility();

    wxTextCtrl*       titleCtrl_;
    wxTextCtrl*       descCtrl_;
    wxTextCtrl*       locCtrl_;
    wxCheckBox*       allDayChk_;
    wxDatePickerCtrl* startDatePicker_;
    wxDatePickerCtrl* endDatePicker_;
    wxTimePickerCtrl* startTimePicker_;
    wxTimePickerCtrl* endTimePicker_;
    wxSizer*          timeSizer_;       // sizer holding time pickers

    wxColour selectedColor_;
    std::vector<wxColour> palette_;
    std::vector<wxButton*> colorBtns_;

    CalendarEvent event_;   // filled in OnOK

    wxDECLARE_EVENT_TABLE();
};
