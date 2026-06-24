#include "NetworkModeDialog.h"
#include "License.h"
#include "NetworkInfo.h"
#include <wx/stdpaths.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <fstream>
#include <sstream>

// ── Config persistence ───────────────────────────────────────────────────────

static std::string CfgPath()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    if (!wxDirExists(dir)) wxMkdir(dir);
    return std::string((dir + "/network.json").mb_str());
}

NetworkConfig LoadNetworkConfig()
{
    NetworkConfig cfg;
    std::ifstream f(CfgPath());
    if (!f.is_open()) return cfg;
    std::ostringstream ss; ss << f.rdbuf();
    std::string j = ss.str();
    auto ext = [&](const std::string& k) -> std::string {
        std::string srch = "\"" + k + "\":\"";
        size_t p = j.find(srch);
        if (p == std::string::npos) return {};
        p += srch.size();
        std::string v;
        while (p < j.size() && j[p] != '"') v += j[p++];
        return v;
    };
    auto extI = [&](const std::string& k, int def) -> int {
        std::string srch = "\"" + k + "\":";
        size_t p = j.find(srch);
        if (p == std::string::npos) return def;
        return std::atoi(j.c_str() + p + srch.size());
    };
    std::string mode = ext("mode");
    if (mode == "server") cfg.mode = NetworkMode::Server;
    else if (mode == "client") cfg.mode = NetworkMode::Client;
    cfg.port       = extI("port",       7743);
    cfg.clientPort = extI("clientPort", 7743);
    cfg.clientHost = ext("clientHost");
    return cfg;
}

void SaveNetworkConfig(const NetworkConfig& cfg)
{
    std::string mode = "standalone";
    if (cfg.mode == NetworkMode::Server) mode = "server";
    else if (cfg.mode == NetworkMode::Client) mode = "client";

    std::ofstream f(CfgPath());
    if (!f.is_open()) return;
    f << "{\"mode\":\"" << mode << "\","
      << "\"port\":" << cfg.port << ","
      << "\"clientHost\":\"" << cfg.clientHost << "\","
      << "\"clientPort\":" << cfg.clientPort << "}\n";
}

// ── Dialog ───────────────────────────────────────────────────────────────────

enum { ID_RB_STANDALONE = wxID_HIGHEST + 200, ID_RB_SERVER, ID_RB_CLIENT };

wxBEGIN_EVENT_TABLE(NetworkModeDialog, wxDialog)
    EVT_RADIOBUTTON(ID_RB_STANDALONE, NetworkModeDialog::OnRadioChanged)
    EVT_RADIOBUTTON(ID_RB_SERVER,     NetworkModeDialog::OnRadioChanged)
    EVT_RADIOBUTTON(ID_RB_CLIENT,     NetworkModeDialog::OnRadioChanged)
    EVT_BUTTON(wxID_OK,               NetworkModeDialog::OnOK)
wxEND_EVENT_TABLE()

NetworkModeDialog::NetworkModeDialog(wxWindow* parent, const NetworkConfig& current)
    : wxDialog(parent, wxID_ANY, "Network Mode",
               wxDefaultPosition, wxSize(460, 500),
               wxDEFAULT_DIALOG_STYLE)
    , result_(current)
{
    LicenseInfo li = License::Load();
    bool lanOk     = License::AllowsLanServer(li);

    // Query LAN state (DHCP and AD CS cert) at open time
    AdapterInfo   adapter = GetPrimaryAdapter();
    MachineCredential mc  = FindMachineCert();

    auto* outer = new wxBoxSizer(wxVERTICAL);

    // ── Header ───────────────────────────────────────────────────────────
    auto* hdr = new wxPanel(this);
    hdr->SetBackgroundColour(wxColour(11, 61, 145));
    auto* hdrTxt = new wxStaticText(hdr, wxID_ANY, "Network / Sharing Mode");
    hdrTxt->SetForegroundColour(*wxWHITE);
    hdrTxt->SetFont(hdrTxt->GetFont().Bold().Larger());
    auto* hdrSz = new wxBoxSizer(wxHORIZONTAL);
    hdrSz->Add(hdrTxt, 0, wxALL, 12);
    hdr->SetSizer(hdrSz);
    outer->Add(hdr, 0, wxEXPAND);

    auto* body = new wxBoxSizer(wxVERTICAL);

    // License status line
    wxColour licCol = lanOk ? wxColour(0, 120, 0) : wxColour(160, 0, 0);
    licWarn_ = new wxStaticText(this, wxID_ANY,
        "License: " + li.message + (lanOk ? "" : " — Multi-user requires MLT or TRL key"));
    licWarn_->SetForegroundColour(licCol);
    licWarn_->SetFont(licWarn_->GetFont().Italic());
    body->Add(licWarn_, 0, wxEXPAND | wxALL, 10);

    // ── LAN / DHCP info ──────────────────────────────────────────────────
    {
        std::string netLine;
        if (adapter.isConnected) {
            netLine  = "LAN: " + adapter.ip;
            if (!adapter.subnet.empty())    netLine += " / " + adapter.subnet;
            if (!adapter.gateway.empty())   netLine += "  GW " + adapter.gateway;
            if (!adapter.dhcpServer.empty())netLine += "  DHCP " + adapter.dhcpServer;
            if (!adapter.dnsSuffix.empty()) netLine += "  [" + adapter.dnsSuffix + "]";
        } else {
            netLine = "LAN: no connected adapter detected";
        }
        netInfoLabel_ = new wxStaticText(this, wxID_ANY, netLine);
        netInfoLabel_->SetForegroundColour(wxColour(30, 80, 150));
        netInfoLabel_->SetFont(netInfoLabel_->GetFont().Smaller());
        body->Add(netInfoLabel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    // ── AD CS certificate status ─────────────────────────────────────────
    {
        std::string certLine;
        if (mc.found) {
            certLine = "TLS cert: " + mc.subjectCN +
                       "  (issued by " + mc.issuerCN +
                       ", valid until " + mc.validUntil + ")";
            FreeMachineCert(mc);
        } else {
            certLine = "TLS cert: none found in MY store — server will use plaintext HTTP";
        }
        certLabel_ = new wxStaticText(this, wxID_ANY, certLine);
        certLabel_->SetForegroundColour(mc.found ? wxColour(0, 110, 0) : wxColour(130, 90, 0));
        certLabel_->SetFont(certLabel_->GetFont().Smaller());
        certLabel_->Wrap(420);
        body->Add(certLabel_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    }

    body->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // ── Standalone ───────────────────────────────────────────────────────
    rbStandalone_ = new wxRadioButton(this, ID_RB_STANDALONE, "Standalone  (single computer)",
                                       wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    body->Add(rbStandalone_, 0, wxALL, 10);
    body->Add(new wxStaticText(this, wxID_ANY,
        "   All data stored locally. No network required."),
        0, wxLEFT | wxBOTTOM, 8);

    body->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // ── Server ───────────────────────────────────────────────────────────
    rbServer_ = new wxRadioButton(this, ID_RB_SERVER, "Server  (host the shared calendar)");
    rbServer_->Enable(lanOk);
    body->Add(rbServer_, 0, wxALL, 10);

    auto* srvRow = new wxBoxSizer(wxHORIZONTAL);
    srvRow->Add(new wxStaticText(this, wxID_ANY, "   Listen port:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    portCtrl_ = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1),
                                wxSP_ARROW_KEYS, 1024, 65535, 7743);
    portCtrl_->Enable(lanOk);
    srvRow->Add(portCtrl_, 0, wxALIGN_CENTER_VERTICAL);
    body->Add(srvRow, 0, wxLEFT | wxBOTTOM, 8);

    ipLabel_ = new wxStaticText(this, wxID_ANY, "   (server IP shown after start)");
    ipLabel_->SetForegroundColour(wxColour(80, 80, 80));
    body->Add(ipLabel_, 0, wxLEFT | wxBOTTOM, 4);

    body->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // ── Client ───────────────────────────────────────────────────────────
    rbClient_ = new wxRadioButton(this, ID_RB_CLIENT, "Client  (connect to a server)");
    rbClient_->Enable(lanOk);
    body->Add(rbClient_, 0, wxALL, 10);

    auto* cliRow = new wxBoxSizer(wxHORIZONTAL);
    cliRow->Add(new wxStaticText(this, wxID_ANY, "   Server:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    hostCtrl_ = new wxTextCtrl(this, wxID_ANY, current.clientHost, wxDefaultPosition, wxSize(160, -1));
    hostCtrl_->Enable(lanOk);
    cliRow->Add(hostCtrl_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    cliRow->Add(new wxStaticText(this, wxID_ANY, "Port:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    cPortCtrl_ = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(70, -1),
                                  wxSP_ARROW_KEYS, 1024, 65535, 7743);
    cPortCtrl_->Enable(lanOk);
    cliRow->Add(cPortCtrl_, 0, wxALIGN_CENTER_VERTICAL);
    body->Add(cliRow, 0, wxLEFT | wxBOTTOM, 8);

    outer->Add(body, 1, wxEXPAND);

    // ── Buttons ───────────────────────────────────────────────────────────
    outer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
    btnRow->AddStretchSpacer();
    btnRow->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, 8);
    btnRow->Add(new wxButton(this, wxID_OK,     "Apply"),  0);
    outer->Add(btnRow, 0, wxEXPAND | wxALL, 10);

    SetSizer(outer);

    // Set initial radio selection
    if (current.mode == NetworkMode::Server && lanOk)       rbServer_->SetValue(true);
    else if (current.mode == NetworkMode::Client && lanOk)  rbClient_->SetValue(true);
    else                                                     rbStandalone_->SetValue(true);

    portCtrl_->SetValue(current.port);
    cPortCtrl_->SetValue(current.clientPort);

    wxCommandEvent dummy;
    OnRadioChanged(dummy);
}

void NetworkModeDialog::OnRadioChanged(wxCommandEvent&)
{
    bool srv = rbServer_->GetValue();
    bool cli = rbClient_->GetValue();
    portCtrl_->Enable(srv);
    hostCtrl_->Enable(cli);
    cPortCtrl_->Enable(cli);
}

void NetworkModeDialog::OnOK(wxCommandEvent&)
{
    if (rbServer_->GetValue())      result_.mode = NetworkMode::Server;
    else if (rbClient_->GetValue()) result_.mode = NetworkMode::Client;
    else                            result_.mode = NetworkMode::Standalone;

    result_.port       = portCtrl_->GetValue();
    result_.clientHost = std::string(hostCtrl_->GetValue().mb_str());
    result_.clientPort = cPortCtrl_->GetValue();

    SaveNetworkConfig(result_);
    EndModal(wxID_OK);
}
