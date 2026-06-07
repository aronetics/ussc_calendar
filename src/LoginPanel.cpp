#include "LoginPanel.h"
#include <wx/valtext.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

enum {
    ID_LP_SIGNIN = wxID_HIGHEST + 700,
    ID_LP_USER,
    ID_LP_PASS,
};

wxBEGIN_EVENT_TABLE(LoginPanel, wxPanel)
    EVT_BUTTON(ID_LP_SIGNIN,   LoginPanel::OnSignIn)
    EVT_TEXT_ENTER(ID_LP_USER, LoginPanel::OnTextEnter)
    EVT_TEXT_ENTER(ID_LP_PASS, LoginPanel::OnTextEnter)
wxEND_EVENT_TABLE()

// ── Construction ──────────────────────────────────────────────────────────────

LoginPanel::LoginPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(220, -1))
{
    SetBackgroundColour(wxColour(248, 249, 250));

    // Credential store path
    wxString home = wxGetHomeDir();
    wxString dir  = home + "/.ussc_calendar";
    wxMkdir(dir, 0700);
    usersPath_ = (dir + "/users.conf").ToStdString();
    LoadUsers();

    // ── Layout ───────────────────────────────────────────────────────────
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);

    vbox->AddSpacer(32);

    // App logo / title
    wxStaticText* logo = new wxStaticText(this, wxID_ANY, "USSC",
                                          wxDefaultPosition, wxDefaultSize,
                                          wxALIGN_CENTRE_HORIZONTAL);
    logo->SetFont(wxFont(wxFontInfo(28).Bold().Family(wxFONTFAMILY_SWISS)));
    logo->SetForegroundColour(wxColour(26, 115, 232));
    vbox->Add(logo, 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    wxStaticText* sub = new wxStaticText(this, wxID_ANY, "Calendar",
                                         wxDefaultPosition, wxDefaultSize,
                                         wxALIGN_CENTRE_HORIZONTAL);
    sub->SetFont(wxFont(wxFontInfo(13).Family(wxFONTFAMILY_SWISS)));
    sub->SetForegroundColour(wxColour(95, 99, 104));
    vbox->Add(sub, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    vbox->AddSpacer(16);

    // Separator
    wxPanel* sep = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    sep->SetBackgroundColour(wxColour(218, 220, 224));
    vbox->Add(sep, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);

    vbox->AddSpacer(24);

    // ── Username ──────────────────────────────────────────────────────────
    wxStaticText* userLbl = new wxStaticText(this, wxID_ANY, "Username");
    userLbl->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    userLbl->SetForegroundColour(wxColour(60, 64, 67));
    vbox->Add(userLbl, 0, wxLEFT | wxRIGHT, 16);

    userCtrl_ = new wxTextCtrl(this, ID_LP_USER, wxEmptyString,
                                wxDefaultPosition, wxSize(-1, 34),
                                wxTE_PROCESS_ENTER);
    userCtrl_->SetHint("Username");
    userCtrl_->SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));
    vbox->Add(userCtrl_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);

    vbox->AddSpacer(12);

    // ── Password ──────────────────────────────────────────────────────────
    wxStaticText* passLbl = new wxStaticText(this, wxID_ANY, "Password");
    passLbl->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    passLbl->SetForegroundColour(wxColour(60, 64, 67));
    vbox->Add(passLbl, 0, wxLEFT | wxRIGHT, 16);

    passCtrl_ = new wxTextCtrl(this, ID_LP_PASS, wxEmptyString,
                                wxDefaultPosition, wxSize(-1, 34),
                                wxTE_PASSWORD | wxTE_PROCESS_ENTER);
    passCtrl_->SetHint("Password");
    passCtrl_->SetFont(wxFont(wxFontInfo(11).Family(wxFONTFAMILY_SWISS)));
    vbox->Add(passCtrl_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);

    vbox->AddSpacer(8);

    // ── Error label (hidden until a failed attempt) ───────────────────────
    errorLabel_ = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE);
    errorLabel_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    errorLabel_->SetForegroundColour(wxColour(217, 48, 37));
    vbox->Add(errorLabel_, 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    vbox->AddSpacer(16);

    // ── Sign In button ────────────────────────────────────────────────────
    signInBtn_ = new wxButton(this, ID_LP_SIGNIN, "Sign In",
                              wxDefaultPosition, wxSize(-1, 38));
    signInBtn_->SetFont(wxFont(wxFontInfo(11).Bold().Family(wxFONTFAMILY_SWISS)));
    signInBtn_->SetBackgroundColour(wxColour(26, 115, 232));
    signInBtn_->SetForegroundColour(*wxWHITE);
    vbox->Add(signInBtn_, 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    vbox->AddStretchSpacer(1);

    // Version / build info at bottom
    wxStaticText* ver = new wxStaticText(this, wxID_ANY, "v" APP_VERSION,
                                         wxDefaultPosition, wxDefaultSize,
                                         wxALIGN_CENTRE_HORIZONTAL);
    ver->SetFont(wxFont(wxFontInfo(8).Family(wxFONTFAMILY_SWISS)));
    ver->SetForegroundColour(wxColour(189, 191, 196));
    vbox->Add(ver, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
}

// ── Event handlers ────────────────────────────────────────────────────────────

void LoginPanel::OnSignIn(wxCommandEvent&)
{
    std::string user = userCtrl_->GetValue().Trim(true).Trim(false).ToStdString();
    std::string pass = passCtrl_->GetValue().ToStdString();

    if (user.empty()) { SetError("Please enter your username."); return; }
    if (pass.empty()) { SetError("Please enter your password."); return; }

    if (Authenticate(user, pass)) {
        ClearError();
        passCtrl_->Clear();
        if (OnLoginSuccess) OnLoginSuccess(user);
    } else {
        SetError("Incorrect username or password.");
        passCtrl_->Clear();
        passCtrl_->SetFocus();
    }
}

void LoginPanel::OnTextEnter(wxCommandEvent&)
{
    wxCommandEvent dummy;
    OnSignIn(dummy);
}

// ── Auth ──────────────────────────────────────────────────────────────────────

bool LoginPanel::Authenticate(const std::string& user, const std::string& pass)
{
    auto it = users_.find(user);
    if (it == users_.end()) return false;
    return it->second == HashPassword(pass);
}

std::string LoginPanel::HashPassword(const std::string& password)
{
    // Write password to a mkstemp temp file (owner-only), hash it, unlink it.
    // Avoids OpenSSL dependency and keeps the password out of the process table.
    char tmp[] = "/tmp/ussc_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) return "";
    write(fd, password.c_str(), password.size());
    close(fd);

    std::string result;
    // mkstemp path is alphanumeric — safe to embed in shell command
    std::string cmd = std::string("sha256sum '") + tmp + "' | awk '{print $1}'";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[128] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            result = buf;
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                result.pop_back();
        }
        pclose(pipe);
    }
    unlink(tmp);
    return result;
}

// ── Credential store ──────────────────────────────────────────────────────────

void LoginPanel::LoadUsers()
{
    std::ifstream f(usersPath_);
    if (!f.is_open()) {
        CreateDefaultUsers();
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto sep = line.find(':');
        if (sep == std::string::npos) continue;
        std::string user = line.substr(0, sep);
        std::string hash = line.substr(sep + 1);
        users_[user] = hash;
    }
    if (users_.empty()) CreateDefaultUsers();
}

void LoginPanel::CreateDefaultUsers()
{
    // Default credential: admin / admin
    // Hash of "admin" via sha256sum
    std::string adminHash = HashPassword("admin");

    std::ofstream f(usersPath_);
    if (f.is_open()) {
        f << "# USSC Calendar users — username:sha256(password)\n";
        f << "# Change default credentials before deployment.\n";
        f << "admin:" << adminHash << "\n";
        f.close();
        // Restrict to owner-read-only
        chmod(usersPath_.c_str(), 0600);
    }
    users_["admin"] = adminHash;
}

// ── UI helpers ────────────────────────────────────────────────────────────────

void LoginPanel::SetError(const wxString& msg)
{
    errorLabel_->SetLabel(msg);
    errorLabel_->Wrap(188);
    Layout();
}

void LoginPanel::ClearError()
{
    errorLabel_->SetLabel(wxEmptyString);
}
