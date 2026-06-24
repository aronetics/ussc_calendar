#pragma once
#include <wx/wx.h>
#include <string>

// Network mode saved to %APPDATA%\ussc_calendar\network.json
enum class NetworkMode { Standalone, Server, Client };

struct NetworkConfig {
    NetworkMode mode     = NetworkMode::Standalone;
    int         port     = 7743;
    std::string clientHost;      // only for Client mode
    int         clientPort = 7743;
};

NetworkConfig LoadNetworkConfig();
void          SaveNetworkConfig(const NetworkConfig& cfg);

// Dialog shown from Help → Network Settings…
// Also called at login time when mode is not yet configured.
class NetworkModeDialog : public wxDialog {
public:
    NetworkModeDialog(wxWindow* parent, const NetworkConfig& current);
    NetworkConfig Result() const { return result_; }

private:
    void OnRadioChanged(wxCommandEvent&);
    void OnOK(wxCommandEvent&);

    wxRadioButton* rbStandalone_ = nullptr;
    wxRadioButton* rbServer_     = nullptr;
    wxRadioButton* rbClient_     = nullptr;
    wxStaticText*  licWarn_      = nullptr;
    wxSpinCtrl*    portCtrl_     = nullptr;
    wxTextCtrl*    hostCtrl_     = nullptr;
    wxSpinCtrl*    cPortCtrl_    = nullptr;
    wxStaticText*  ipLabel_      = nullptr;

    NetworkConfig result_;
    wxDECLARE_EVENT_TABLE();
};
