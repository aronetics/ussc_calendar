#pragma once
#include <wx/wx.h>
#include <functional>
#include <map>
#include <string>

// Left-sidebar login panel.
// Fires OnLoginSuccess(username) when credentials are accepted;
// CalendarFrame swaps it out for the mini-calendar sidebar.
class LoginPanel : public wxPanel {
public:
    explicit LoginPanel(wxWindow* parent);

    std::function<void(const std::string& username)> OnLoginSuccess;

private:
    // UI events
    void OnSignIn(wxCommandEvent&);
    void OnTextEnter(wxCommandEvent&);

    // Auth helpers
    bool        Authenticate(const std::string& user, const std::string& pass);
    void        SetError(const wxString& msg);
    void        ClearError();
    std::string HashPassword(const std::string& password);

    // Credential store (~/.ussc_calendar/users.conf)
    void LoadUsers();
    void CreateDefaultUsers();    // writes admin/admin on first run
    std::string usersPath_;
    std::map<std::string, std::string> users_;  // username → sha256hex

    wxTextCtrl*   userCtrl_  = nullptr;
    wxTextCtrl*   passCtrl_  = nullptr;
    wxStaticText* errorLabel_= nullptr;
    wxButton*     signInBtn_ = nullptr;

    wxDECLARE_EVENT_TABLE();
};
