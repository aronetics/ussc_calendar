#include "ConnectorDialog.h"
#include "OutlookComConnector.h"
#include "GraphApiConnector.h"
#include "EwsConnector.h"
#include "IcsConnector.h"
#include <wx/filedlg.h>
#include <wx/clipbrd.h>

enum {
    ID_RB_NONE    = wxID_HIGHEST + 800,
    ID_RB_COM,
    ID_RB_GRAPH,
    ID_RB_EWS,
    ID_RB_ICS,
    ID_BTN_TEST,
    ID_BTN_SIGN_IN_GRAPH,
    ID_BTN_ICS_IMPORT,
    ID_BTN_ICS_EXPORT,
    ID_BTN_ICS_BROWSE_IN,
    ID_BTN_ICS_BROWSE_OUT,
};

wxBEGIN_EVENT_TABLE(ConnectorDialog, wxDialog)
    EVT_RADIOBUTTON(ID_RB_NONE,  ConnectorDialog::OnTypeChange)
    EVT_RADIOBUTTON(ID_RB_COM,   ConnectorDialog::OnTypeChange)
    EVT_RADIOBUTTON(ID_RB_GRAPH, ConnectorDialog::OnTypeChange)
    EVT_RADIOBUTTON(ID_RB_EWS,   ConnectorDialog::OnTypeChange)
    EVT_RADIOBUTTON(ID_RB_ICS,   ConnectorDialog::OnTypeChange)
    EVT_BUTTON(ID_BTN_TEST,           ConnectorDialog::OnTestConnect)
    EVT_BUTTON(ID_BTN_SIGN_IN_GRAPH,  ConnectorDialog::OnSignInGraph)
    EVT_BUTTON(ID_BTN_ICS_IMPORT,     ConnectorDialog::OnImportICS)
    EVT_BUTTON(ID_BTN_ICS_EXPORT,     ConnectorDialog::OnExportICS)
    EVT_BUTTON(wxID_OK, ConnectorDialog::OnOK)
wxEND_EVENT_TABLE()

// ── Construction ──────────────────────────────────────────────────────────────

ConnectorDialog::ConnectorDialog(wxWindow* parent, const ConnectorSettings& current)
    : wxDialog(parent, wxID_ANY, "Calendar Account Setup",
               wxDefaultPosition, wxSize(500, 580),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      settings_(current)
{
    SetBackgroundColour(wxColour(255, 255, 255));

    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    SetSizer(root);

    // ── Header ────────────────────────────────────────────────────────────
    wxStaticText* hdr = new wxStaticText(this, wxID_ANY, "Add Calendar Account");
    hdr->SetFont(wxFont(wxFontInfo(14).Bold().Family(wxFONTFAMILY_SWISS)));
    hdr->SetForegroundColour(wxColour(32, 33, 36));
    root->Add(hdr, 0, wxALL, 16);

    wxStaticText* sub = new wxStaticText(this, wxID_ANY,
        "Choose how USSC Calendar connects to your external calendar service.");
    sub->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
    sub->SetForegroundColour(wxColour(95, 99, 104));
    root->Add(sub, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);

    // ── Option radio buttons ──────────────────────────────────────────────
    wxStaticBoxSizer* optBox = new wxStaticBoxSizer(wxVERTICAL,
                                                      this, "Connection type");

    auto makeRB = [&](int id, const wxString& label,
                       const wxString& desc) -> wxRadioButton* {
        wxRadioButton* rb = new wxRadioButton(this, id, label,
                                               wxDefaultPosition, wxDefaultSize,
                                               id == ID_RB_NONE ? wxRB_GROUP : 0);
        rb->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
        optBox->Add(rb, 0, wxLEFT | wxTOP, 8);
        wxStaticText* d = new wxStaticText(this, wxID_ANY, desc);
        d->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
        d->SetForegroundColour(wxColour(95, 99, 104));
        optBox->Add(d, 0, wxLEFT | wxBOTTOM, 24);
        return rb;
    };

    rbNone_  = makeRB(ID_RB_NONE,  "No external calendar",
                      "Events are stored locally in USSC Calendar only.");
    rbCOM_   = makeRB(ID_RB_COM,   "Local Outlook (COM automation)",
                      "Import from Microsoft Outlook installed on this computer. No internet required.");
    rbGraph_ = makeRB(ID_RB_GRAPH, "Microsoft 365 / Exchange Online (Graph API)",
                      "Connect via Microsoft cloud. Requires Azure AD app credentials from IT.");
    rbEWS_   = makeRB(ID_RB_EWS,   "Exchange On-Premises (EWS / SOAP)",
                      "Connect to an internal Exchange server by URL. Uses Windows credentials.");
    rbICS_   = makeRB(ID_RB_ICS,   "ICS File (iCalendar import / export)",
                      "Import .ics files from any calendar app, or export USSC events as .ics.");

    root->Add(optBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // ── Settings pages (one per option) ───────────────────────────────────
    book_ = new wxSimplebook(this);
    root->Add(book_, 1, wxEXPAND | wxALL, 16);

    // Page 0: None — empty
    book_->AddPage(new wxPanel(book_), "none");

    // Page 1: Outlook COM
    {
        wxPanel* pg = new wxPanel(book_);
        wxBoxSizer* v = new wxBoxSizer(wxVERTICAL);
        pg->SetSizer(v);
        wxStaticText* info = new wxStaticText(pg, wxID_ANY,
            "USSC Calendar will read events directly from Outlook's local data store.\n"
            "Outlook must be running. Click Test Connection to verify.");
        info->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
        info->Wrap(440);
        v->Add(info, 0, wxALL, 8);
        v->Add(new wxButton(pg, ID_BTN_TEST, "Test Connection"),
               0, wxALL, 8);
        book_->AddPage(pg, "com");
    }

    // Page 2: Graph API
    {
        wxPanel* pg = new wxPanel(book_);
        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 8, 8);
        grid->AddGrowableCol(1, 1);
        pg->SetSizer(grid);

        auto addField = [&](const wxString& lbl, wxTextCtrl** ctrl,
                             long style = 0) {
            grid->Add(new wxStaticText(pg, wxID_ANY, lbl),
                       0, wxALIGN_CENTER_VERTICAL);
            *ctrl = new wxTextCtrl(pg, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, style);
            (*ctrl)->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
            grid->Add(*ctrl, 1, wxEXPAND);
        };

        addField("Tenant ID:", &tenantCtrl_);
        addField("Client ID:", &clientCtrl_);

        tenantCtrl_->SetValue(current.graphTenantId);
        clientCtrl_->SetValue(current.graphClientId);

        grid->AddSpacer(0);
        wxButton* signIn = new wxButton(pg, ID_BTN_SIGN_IN_GRAPH,
                                         "Sign in with Microsoft...");
        grid->Add(signIn, 0, wxTOP, 4);

        grid->AddSpacer(0);
        graphCode_ = new wxStaticText(pg, wxID_ANY, wxEmptyString);
        graphCode_->SetFont(wxFont(wxFontInfo(10).Bold().Family(wxFONTFAMILY_SWISS)));
        graphCode_->SetForegroundColour(wxColour(26, 115, 232));
        grid->Add(graphCode_, 0, wxEXPAND);

        book_->AddPage(pg, "graph");
    }

    // Page 3: EWS
    {
        wxPanel* pg = new wxPanel(book_);
        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 8, 8);
        grid->AddGrowableCol(1, 1);
        pg->SetSizer(grid);

        auto addField = [&](const wxString& lbl, wxTextCtrl** ctrl,
                             long style = 0) {
            grid->Add(new wxStaticText(pg, wxID_ANY, lbl),
                       0, wxALIGN_CENTER_VERTICAL);
            *ctrl = new wxTextCtrl(pg, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, style);
            (*ctrl)->SetFont(wxFont(wxFontInfo(10).Family(wxFONTFAMILY_SWISS)));
            grid->Add(*ctrl, 1, wxEXPAND);
        };

        addField("Server URL:", &ewsUrlCtrl_);
        addField("Username:",   &ewsUserCtrl_);
        addField("Password:",   &ewsPassCtrl_, wxTE_PASSWORD);

        ewsUrlCtrl_->SetHint("https://mail.example.mil/EWS/Exchange.asmx");
        ewsUrlCtrl_->SetValue(current.ewsUrl);
        ewsUserCtrl_->SetValue(current.ewsUsername);

        grid->AddSpacer(0);
        grid->Add(new wxButton(pg, ID_BTN_TEST, "Test Connection"), 0, wxTOP, 4);

        book_->AddPage(pg, "ews");
    }

    // Page 4: ICS
    {
        wxPanel* pg = new wxPanel(book_);
        wxBoxSizer* v = new wxBoxSizer(wxVERTICAL);
        pg->SetSizer(v);

        // Import row
        wxBoxSizer* importRow = new wxBoxSizer(wxHORIZONTAL);
        icsImportPath_ = new wxTextCtrl(pg, wxID_ANY, current.icsImportPath,
                                         wxDefaultPosition, wxDefaultSize);
        icsImportPath_->SetHint("Path to .ics file to import");
        importRow->Add(icsImportPath_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        importRow->Add(new wxButton(pg, ID_BTN_ICS_BROWSE_IN, "Browse..."),
                        0, wxALIGN_CENTER_VERTICAL);
        v->Add(new wxStaticText(pg, wxID_ANY, "Import from ICS file:"),
               0, wxBOTTOM, 4);
        v->Add(importRow, 0, wxEXPAND | wxBOTTOM, 8);
        v->Add(new wxButton(pg, ID_BTN_ICS_IMPORT, "Import Events from File"),
               0, wxBOTTOM, 16);

        // Export row
        wxBoxSizer* exportRow = new wxBoxSizer(wxHORIZONTAL);
        icsExportPath_ = new wxTextCtrl(pg, wxID_ANY, current.icsExportPath,
                                         wxDefaultPosition, wxDefaultSize);
        icsExportPath_->SetHint("Path to save exported .ics file");
        exportRow->Add(icsExportPath_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        exportRow->Add(new wxButton(pg, ID_BTN_ICS_BROWSE_OUT, "Browse..."),
                        0, wxALIGN_CENTER_VERTICAL);
        v->Add(new wxStaticText(pg, wxID_ANY, "Export to ICS file:"),
               0, wxBOTTOM, 4);
        v->Add(exportRow, 0, wxEXPAND | wxBOTTOM, 8);
        v->Add(new wxButton(pg, ID_BTN_ICS_EXPORT, "Export All Events to File"),
               0);

        // Wire browse buttons with lambdas
        pg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            wxFileDialog fd(this, "Open ICS file", wxEmptyString, wxEmptyString,
                            "iCalendar files (*.ics)|*.ics|All files (*.*)|*.*",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (fd.ShowModal() == wxID_OK)
                icsImportPath_->SetValue(fd.GetPath());
        }, ID_BTN_ICS_BROWSE_IN);

        pg->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            wxFileDialog fd(this, "Save ICS file", wxEmptyString, "export.ics",
                            "iCalendar files (*.ics)|*.ics|All files (*.*)|*.*",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (fd.ShowModal() == wxID_OK)
                icsExportPath_->SetValue(fd.GetPath());
        }, ID_BTN_ICS_BROWSE_OUT);

        book_->AddPage(pg, "ics");
    }

    // ── Status label ──────────────────────────────────────────────────────
    statusLabel_ = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxST_NO_AUTORESIZE);
    statusLabel_->SetFont(wxFont(wxFontInfo(9).Family(wxFONTFAMILY_SWISS)));
    root->Add(statusLabel_, 0, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // ── Buttons ───────────────────────────────────────────────────────────
    wxStdDialogButtonSizer* btns = new wxStdDialogButtonSizer();
    btns->AddButton(new wxButton(this, wxID_OK,     "Save"));
    btns->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    btns->Realize();
    root->Add(btns, 0, wxEXPAND | wxALL, 12);

    // Select the right radio button for current type
    switch (settings_.type) {
        case ConnectorType::OutlookCOM: rbCOM_->SetValue(true);   break;
        case ConnectorType::GraphAPI:   rbGraph_->SetValue(true);  break;
        case ConnectorType::EWS:        rbEWS_->SetValue(true);    break;
        case ConnectorType::ICSFile:    rbICS_->SetValue(true);    break;
        default:                         rbNone_->SetValue(true);   break;
    }
    UpdatePageVisibility();

    Layout();
    Centre();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ConnectorDialog::UpdatePageVisibility()
{
    int page = 0;
    if (rbCOM_->GetValue())   page = 1;
    if (rbGraph_->GetValue()) page = 2;
    if (rbEWS_->GetValue())   page = 3;
    if (rbICS_->GetValue())   page = 4;
    book_->SetSelection(page);
}

void ConnectorDialog::SetStatus(const wxString& msg, bool ok)
{
    statusLabel_->SetLabel(msg);
    statusLabel_->SetForegroundColour(
        ok ? wxColour(11, 128, 67) : wxColour(217, 48, 37));
    Layout();
}

ConnectorSettings ConnectorDialog::ReadFields() const
{
    ConnectorSettings s = settings_;
    if (rbNone_->GetValue())   s.type = ConnectorType::None;
    if (rbCOM_->GetValue())    s.type = ConnectorType::OutlookCOM;
    if (rbGraph_->GetValue())  s.type = ConnectorType::GraphAPI;
    if (rbEWS_->GetValue())    s.type = ConnectorType::EWS;
    if (rbICS_->GetValue())    s.type = ConnectorType::ICSFile;

    if (tenantCtrl_)   s.graphTenantId  = tenantCtrl_->GetValue().ToStdString();
    if (clientCtrl_)   s.graphClientId  = clientCtrl_->GetValue().ToStdString();
    if (ewsUrlCtrl_)   s.ewsUrl         = ewsUrlCtrl_->GetValue().ToStdString();
    if (ewsUserCtrl_)  s.ewsUsername    = ewsUserCtrl_->GetValue().ToStdString();
    if (ewsPassCtrl_)  s.ewsPassword    = ewsPassCtrl_->GetValue().ToStdString();
    if (icsImportPath_) s.icsImportPath = icsImportPath_->GetValue().ToStdString();
    if (icsExportPath_) s.icsExportPath = icsExportPath_->GetValue().ToStdString();
    return s;
}

// ── Event handlers ────────────────────────────────────────────────────────────

void ConnectorDialog::OnTypeChange(wxCommandEvent&)
{
    UpdatePageVisibility();
    statusLabel_->SetLabel(wxEmptyString);
}

void ConnectorDialog::OnTestConnect(wxCommandEvent&)
{
    ConnectorSettings s = ReadFields();
    std::string err;

#ifdef _WIN32
    if (s.type == ConnectorType::OutlookCOM) {
        OutlookComConnector con;
        if (con.Connect(err))
            SetStatus("Connected to Outlook successfully.", true);
        else
            SetStatus(err, false);
        return;
    }
#endif
    if (s.type == ConnectorType::EWS) {
        EwsConnector con(s);
        if (con.Connect(err))
            SetStatus("EWS connection accepted (full validation requires network).", true);
        else
            SetStatus(err, false);
        return;
    }
    SetStatus("Test not available for this connection type.", false);
}

void ConnectorDialog::OnSignInGraph(wxCommandEvent&)
{
    ConnectorSettings s = ReadFields();
    if (s.graphTenantId.empty() || s.graphClientId.empty()) {
        SetStatus("Enter Tenant ID and Client ID before signing in.", false);
        return;
    }
    GraphApiConnector con(s);
    std::string userCode, url, err;
    if (!con.StartDeviceCodeFlow(userCode, url, err)) {
        SetStatus(wxString::FromUTF8(err), false);
        return;
    }
    // Show code + URL to user; copy code to clipboard for convenience
    wxString msg = wxString::Format(
        "Go to %s and enter code: %s",
        wxString::FromUTF8(url), wxString::FromUTF8(userCode));
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(userCode)));
        wxTheClipboard->Close();
        msg += "\n(Code copied to clipboard)";
    }
    graphCode_->SetLabel(msg);
    graphCode_->Wrap(430);
    Layout();
    SetStatus("Waiting for sign-in — complete in your browser, then save.", false);
}

void ConnectorDialog::OnImportICS(wxCommandEvent&)
{
    std::string path = icsImportPath_->GetValue().ToStdString();
    if (path.empty()) {
        SetStatus("Select an ICS file to import first.", false);
        return;
    }
    IcsConnector con;
    std::string err;
    imported_ = con.ImportFromFile(path, err);
    if (!err.empty()) {
        SetStatus(wxString::FromUTF8(err), false);
    } else {
        SetStatus(wxString::Format("Imported %d event(s) — click Save to add them.",
                                    (int)imported_.size()), true);
    }
}

void ConnectorDialog::OnExportICS(wxCommandEvent&)
{
    // CalendarFrame will call ExportToFile with the full event list.
    // We just save the path here; the frame handles the actual export.
    std::string path = icsExportPath_->GetValue().ToStdString();
    if (path.empty()) {
        SetStatus("Choose an export file path first.", false);
        return;
    }
    settings_ = ReadFields();
    SetStatus("Export path saved. Use File > Export ICS to write the file.", true);
}

void ConnectorDialog::OnOK(wxCommandEvent&)
{
    settings_ = ReadFields();
    EndModal(wxID_OK);
}
