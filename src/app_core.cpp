#include <wx/wx.h>
#include "ui_login.h"
#include "main_frame.h"

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        wxInitAllImageHandlers();

        LoginDialog login(nullptr);
        if (login.ShowModal() == wxID_OK)
        {
            MainFrame *frame = new MainFrame(login.session);
            frame->Show();
            return true; // Control now belongs to MainFrame
        }

        return false; // User exited login initially
    }
};

wxIMPLEMENT_APP(MyApp);