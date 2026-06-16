#pragma once
#include <wx/wx.h>
#include <wx/simplebook.h>
#include <wx/filedlg.h>
#include "CalendarConnector.h"

class ConnectorDialog : public wxDialog {
public:
    // Pass current settings in; call GetSettings() after ShowModal()==wxID_OK
    ConnectorDialog(wxWindow* parent, const ConnectorSettings& current);

    ConnectorSettings GetSettings() const { return settings_; }

    // If the user chose ICS and clicked Import, these events are ready to add
    const std::vector<CalendarEvent>& ImportedEvents() const { return imported_; }

private:
    void BuildPage();
    void OnTypeChange(wxCommandEvent&);
    void OnTestConnect(wxCommandEvent&);
    void OnSignInGraph(wxCommandEvent&);
    void OnImportICS(wxCommandEvent&);
    void OnExportICS(wxCommandEvent&);
    void OnOK(wxCommandEvent&);

    void UpdatePageVisibility();
    void SetStatus(const wxString& msg, bool ok);
    ConnectorSettings ReadFields() const;

    ConnectorSettings         settings_;
    std::vector<CalendarEvent> imported_;

    // Type selection
    wxRadioButton* rbNone_    = nullptr;
    wxRadioButton* rbCOM_     = nullptr;
    wxRadioButton* rbGraph_   = nullptr;
    wxRadioButton* rbEWS_     = nullptr;
    wxRadioButton* rbICS_     = nullptr;

    // Page switcher
    wxSimplebook* book_       = nullptr;

    // Graph API page
    wxTextCtrl*   tenantCtrl_  = nullptr;
    wxTextCtrl*   clientCtrl_  = nullptr;
    wxStaticText* graphCode_   = nullptr;  // shows device code for sign-in

    // EWS page
    wxTextCtrl*   ewsUrlCtrl_  = nullptr;
    wxTextCtrl*   ewsUserCtrl_ = nullptr;
    wxTextCtrl*   ewsPassCtrl_ = nullptr;

    // ICS page
    wxTextCtrl*   icsImportPath_ = nullptr;
    wxTextCtrl*   icsExportPath_ = nullptr;

    wxStaticText* statusLabel_ = nullptr;

    wxDECLARE_EVENT_TABLE();
};
