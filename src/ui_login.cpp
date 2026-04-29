#include <wx/wx.h>
#include <wx/hyperlink.h>
#include "internal_bridge.h"

// --- THIS PART WAS MISSING ---
class LoginDialog : public wxDialog
{
public:
    LoginDialog(wxWindow *parent);
    void OnLogin(wxCommandEvent &event);
    OriginSession session;

private:
    wxTextCtrl *emailInput;
    wxTextCtrl *passwordInput;
    wxStaticText *statusText;
    void OnClose(wxCloseEvent &event);
};
// -----------------------------

LoginDialog::LoginDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Origin Cam", wxDefaultPosition, wxSize(380, 420))
{
    // YOUR ORIGINAL UI CODE STARTS HERE
    SetBackgroundColour(wxColour(18, 18, 18));

    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->AddSpacer(40);

    wxStaticText *title = new wxStaticText(this, wxID_ANY, "Sign In");
    title->SetForegroundColour(*wxWHITE);
    title->SetFont(wxFont(18, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    mainSizer->Add(title, 0, wxALIGN_CENTER | wxBOTTOM, 25);

    emailInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(280, 35));
    emailInput->SetHint("Email");

    passwordInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(280, 35), wxTE_PASSWORD | wxTE_PROCESS_ENTER);
    passwordInput->SetHint("Password");

    mainSizer->Add(emailInput, 0, wxALIGN_CENTER | wxBOTTOM, 12);
    mainSizer->Add(passwordInput, 0, wxALIGN_CENTER | wxBOTTOM, 25);

    wxButton *loginBtn = new wxButton(this, wxID_OK, "Login");
    loginBtn->SetDefault();
    mainSizer->Add(loginBtn, 0, wxALIGN_CENTER | wxBOTTOM, 20);

    statusText = new wxStaticText(this, wxID_ANY, "");
    statusText->SetForegroundColour(wxColour(255, 100, 100));
    mainSizer->Add(statusText, 0, wxALIGN_CENTER | wxBOTTOM, 15);

    wxHyperlinkCtrl *link = new wxHyperlinkCtrl(this, wxID_ANY, "Create an account", "https://origin-ai-six.vercel.app/signup");
    link->SetNormalColour(wxColour(124, 58, 237));
    mainSizer->Add(link, 0, wxALIGN_CENTER);

    SetSizer(mainSizer);

    loginBtn->Bind(wxEVT_BUTTON, &LoginDialog::OnLogin, this);
    passwordInput->Bind(wxEVT_TEXT_ENTER, &LoginDialog::OnLogin, this);
    Bind(wxEVT_CLOSE_WINDOW, &LoginDialog::OnClose, this);
}

void LoginDialog::OnLogin(wxCommandEvent &event)
{
    std::string email = emailInput->GetValue().ToStdString();
    std::string password = passwordInput->GetValue().ToStdString();

    if (email.empty() || password.empty())
    {
        statusText->SetLabel("Required fields missing");
        return;
    }

    statusText->SetLabel("Authenticating...");
    this->Update();

    session = AuthenticateOrigin(email, password);

    if (session.isValid)
    {
        EndModal(wxID_OK);
    }
    else
    {
        statusText->SetLabel("Invalid email or password");
    }
}

void LoginDialog::OnClose(wxCloseEvent &event)
{
    EndModal(wxID_CANCEL);
}