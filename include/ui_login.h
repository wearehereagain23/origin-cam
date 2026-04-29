#pragma once
#include <wx/wx.h>
#include <wx/hyperlink.h> // Required for wxHyperlinkCtrl
#include "internal_bridge.h"

class LoginDialog : public wxDialog
{
public:
    LoginDialog(wxWindow *parent);

    OriginSession session;

private:
    void OnLogin(wxCommandEvent &event);

    // Names matched to your .cpp file
    wxTextCtrl *emailInput;
    wxTextCtrl *passwordInput;
    wxStaticText *statusText;

    wxDECLARE_EVENT_TABLE();
};