#include "main_frame.h"
#include "internal_bridge.h"
#include "constants.h"
#include "ui_login.h"

#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/statbmp.h>
#include <wx/mstream.h>
#include <wx/base64.h>
#include <wx/dcbuffer.h>

#include <thread>
#include <chrono>
#include <cstdlib>

// ======================================================
// DROP TARGET
// ======================================================
class ImageDropTarget : public wxFileDropTarget
{
    MainFrame *m_frame;

public:
    ImageDropTarget(MainFrame *frame)
        : m_frame(frame) {}

    bool OnDropFiles(wxCoord, wxCoord,
                     const wxArrayString &files) override
    {
        if (files.GetCount() > 0)
            m_frame->OnImageLoaded(files[0]);

        return true;
    }
};

// ======================================================
// MAIN FRAME
// ======================================================
MainFrame::MainFrame(const OriginSession &session)
    : wxFrame(nullptr,
              wxID_ANY,
              "Origin Cam",
              wxDefaultPosition,
              wxSize(1200, 850)),
      m_session(session),
      m_hasImage(false),
      m_cameraActive(false),
      m_aiRunning(false),
      m_aiConnected(false)
{
    m_bridgeService.SetUserId(m_session.userId);
    std::cout << "🧪 LOGIN DATA\n";
    std::cout << "User: " << m_session.name << "\n";
    std::cout << "Seconds: " << m_session.seconds << "\n";

    SetBackgroundColour(Theme::BACK);

    m_timer = new wxTimer(this);
    m_billingTimer = new wxTimer(this);

    // ==================================================
    // ROOT PANEL
    // ==================================================
    wxPanel *mainPanel = new wxPanel(this);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    // ==================================================
    // HEADER
    // ==================================================
    wxBoxSizer *header = new wxBoxSizer(wxHORIZONTAL);

    userLabel = new wxStaticText(
        mainPanel,
        wxID_ANY,
        "OPERATOR: " + wxString(m_session.name).Upper());

    userLabel->SetForegroundColour(Theme::TEXT_DIM);

    timerLabel = new wxStaticText(
        mainPanel,
        wxID_ANY,
        FormatTime(m_session.seconds));

    timerLabel->SetForegroundColour(Theme::ACCENT);

    wxButton *logoutBtn =
        new wxButton(mainPanel, wxID_ANY, "Logout");

    header->Add(userLabel, 0,
                wxALL | wxALIGN_CENTER_VERTICAL, 15);

    header->AddStretchSpacer(1);

    header->Add(timerLabel, 0,
                wxALL | wxALIGN_CENTER_VERTICAL, 15);

    header->Add(logoutBtn, 0,
                wxALL | wxALIGN_CENTER_VERTICAL, 15);

    mainSizer->Add(header, 0, wxEXPAND);

    // ==================================================
    // VIDEO AREA
    // ==================================================
    wxBoxSizer *videoSizer =
        new wxBoxSizer(wxHORIZONTAL);

    inputView =
        CreateVideoPanel(mainPanel,
                         "INPUT (LIVE)",
                         videoSizer);

    outputView =
        CreateVideoPanel(mainPanel,
                         "OUTPUT (AI)",
                         videoSizer);

    mainSizer->Add(videoSizer, 0,
                   wxEXPAND | wxTOP, 10);

    // ==================================================
    // REF PANEL
    // ==================================================
    refPanel = new wxPanel(mainPanel);
    refPanel->SetBackgroundColour(Theme::REF);

    wxBoxSizer *refSizer =
        new wxBoxSizer(wxHORIZONTAL);

    refPreview =
        new wxStaticBitmap(refPanel,
                           wxID_ANY,
                           wxBitmap(80, 80));

    wxBitmap blank(80, 80);

    {
        wxMemoryDC dc(blank);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
    }

    refPreview->SetBitmap(blank);

    wxStaticText *hint =
        new wxStaticText(
            refPanel,
            wxID_ANY,
            "Drag & Drop Reference Image");

    hint->SetForegroundColour(Theme::TEXT_DIM);

    refSizer->Add(refPreview, 0, wxALL, 15);
    refSizer->Add(hint, 0, wxALIGN_CENTER_VERTICAL);

    refPanel->SetSizer(refSizer);

    refPanel->SetDropTarget(
        new ImageDropTarget(this));

    refPanel->Bind(
        wxEVT_LEFT_DOWN,
        &MainFrame::OnUploadClick,
        this);

    mainSizer->Add(refPanel, 0,
                   wxEXPAND | wxALL, 20);

    mainSizer->AddStretchSpacer(1);

    // ==================================================
    // CONTROLS
    // ==================================================
    wxBoxSizer *ctrl =
        new wxBoxSizer(wxHORIZONTAL);

    cameraDropdown =
        new wxChoice(mainPanel, wxID_ANY);

    for (int i = 0; i < 4; i++)
    {
        cameraDropdown->Append(
            wxString::Format(
                "Camera %d", i));
    }

    cameraDropdown->SetSelection(0);

    startButton =
        new wxButton(mainPanel,
                     wxID_ANY,
                     "Start AI");

    stopButton =
        new wxButton(mainPanel,
                     wxID_ANY,
                     "Stop AI");

    ctrl->Add(cameraDropdown, 0, wxALL, 20);
    ctrl->AddStretchSpacer(1);
    ctrl->Add(startButton, 0, wxALL, 20);
    ctrl->Add(stopButton, 0, wxALL, 20);

    mainSizer->Add(ctrl, 0, wxEXPAND);

    mainPanel->SetSizer(mainSizer);

    wxBoxSizer *frameSizer =
        new wxBoxSizer(wxVERTICAL);

    frameSizer->Add(mainPanel, 1, wxEXPAND);

    SetSizer(frameSizer);

    // ==================================================
    // EVENTS
    // ==================================================
    startButton->Bind(
        wxEVT_BUTTON,
        &MainFrame::OnToggleAI,
        this);

    stopButton->Bind(
        wxEVT_BUTTON,
        &MainFrame::OnToggleAI,
        this);

    cameraDropdown->Bind(
        wxEVT_CHOICE,
        &MainFrame::OnCameraChange,
        this);

    logoutBtn->Bind(
        wxEVT_BUTTON,
        &MainFrame::OnLogout,
        this);

    Bind(wxEVT_TIMER,
         &MainFrame::OnFrame,
         this);

    Bind(wxEVT_TIMER,
         &MainFrame::OnBillingTick,
         this,
         m_billingTimer->GetId());

    // ==================================================
    // START CAMERA
    // ==================================================
    if (m_videoProvider.OpenCamera(0))
        m_timer->Start(33);

    UpdateButtonState();
}

// ======================================================
// VIDEO PANEL
// ======================================================
wxStaticBitmap *MainFrame::CreateVideoPanel(
    wxPanel *parent,
    const wxString &label,
    wxBoxSizer *target)
{
    wxPanel *p = new wxPanel(parent);

    wxBoxSizer *sz =
        new wxBoxSizer(wxVERTICAL);

    wxStaticText *txt =
        new wxStaticText(
            p,
            wxID_ANY,
            label);

    txt->SetForegroundColour(
        Theme::TEXT_DIM);

    wxStaticBitmap *view =
        new wxStaticBitmap(
            p,
            wxID_ANY,
            wxBitmap(580, 435));

    wxBitmap black(580, 435);

    {
        wxMemoryDC dc(black);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
    }

    view->SetBitmap(black);

    sz->Add(txt, 0, wxALL, 5);
    sz->Add(view, 1,
            wxEXPAND | wxALL, 5);

    p->SetSizer(sz);

    target->Add(p, 1,
                wxEXPAND | wxALL, 10);

    return view;
}

// ======================================================
// FRAME LOOP
// ======================================================
void MainFrame::OnFrame(wxTimerEvent &)
{
    cv::Mat frame =
        m_videoProvider.GetFrame();

    if (frame.empty())
        return;

    // ==================================================
    // CAMERA LIVE DETECTION
    // ==================================================
    if (!m_prevFrame.empty())
    {
        cv::Mat diff;
        cv::absdiff(frame, m_prevFrame, diff);

        double motion =
            cv::mean(diff)[0];

        m_cameraActive =
            motion > 0.5;
    }

    m_prevFrame = frame.clone();

    // ==================================================
    // THROTTLED AI SEND
    // ==================================================
    static auto lastSend =
        std::chrono::steady_clock::now();

    auto now =
        std::chrono::steady_clock::now();

    auto ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            now - lastSend)
            .count();

    if (m_aiRunning &&
        m_bridgeService.IsActive() &&
        ms >= 100)
    {
        m_bridgeService.SendFrame(frame);
        lastSend = now;
    }

    // ==================================================
    // INPUT VIEW
    // ==================================================
    cv::Mat rgb;

    cv::cvtColor(
        frame,
        rgb,
        cv::COLOR_BGR2RGB);

    cv::resize(
        rgb,
        rgb,
        cv::Size(580, 435));

    wxImage inputImg(
        rgb.cols,
        rgb.rows,
        rgb.data,
        true);

    inputView->SetBitmap(
        wxBitmap(inputImg.Copy()));

    // ==================================================
    // OUTPUT VIEW
    // ==================================================
    cv::Mat aiFrame =
        m_bridgeService
            .m_outputProcessor
            .GetLatestOutput();

    if (!aiFrame.empty() &&
        m_aiRunning)
    {
        cv::resize(
            aiFrame,
            aiFrame,
            cv::Size(580, 435));

        wxBitmap bmp =
            m_bridgeService
                .m_outputProcessor
                .ConvertToWx(aiFrame);

        outputView->SetBitmap(bmp);
    }

    UpdateButtonState();
}

// ======================================================
// START / STOP
// ======================================================
void MainFrame::OnToggleAI(wxCommandEvent &e)
{
    // ==================================================
    // START
    // ==================================================
    if (e.GetEventObject() == startButton)
    {
        if (m_aiRunning)
            return;

        if (!m_hasImage)
            return;

        if (!m_cameraActive)
            return;

        if (m_session.seconds <= 0)
        {
            wxMessageBox(
                "You have no remaining time.\n\nVisit:\nhttps://origin-ai-six.vercel.app/",
                "No Time Remaining",
                wxOK | wxICON_WARNING);
            return;
        }

        std::cout << "[START] Launching AI...\n";

        m_bridgeService.KillInternalServer();
        m_bridgeService.LaunchInternalServer();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1200));

        bool connected =
            m_bridgeService
                .EstablishConnection(
                    m_refImageBase64,
                    [this]()
                    {
                        std::cout
                            << "🔥 AI STREAM ACTIVE\n";

                        m_aiConnected = true;

                        wxTheApp->CallAfter(
                            [this]()
                            {
                                if (!m_billingTimer
                                         ->IsRunning())
                                {
                                    m_billingTimer
                                        ->Start(1000);
                                }
                            });
                    });

        if (!connected)
        {
            m_bridgeService
                .KillInternalServer();
            return;
        }

        m_aiRunning = true;

        UpdateButtonState();
        return;
    }

    // ==================================================
    // STOP
    // ==================================================
    if (e.GetEventObject() == stopButton)
    {
        if (!m_aiRunning)
            return;

        std::cout
            << "[STOP] Terminating Session...\n";

        m_bridgeService.Stop();
        m_bridgeService.KillInternalServer();

        if (m_billingTimer->IsRunning())
            m_billingTimer->Stop();

        UpdateUserSeconds(
            m_session.userId,
            m_session.seconds,
            m_session.token);

        m_aiRunning = false;
        m_aiConnected = false;

        UpdateButtonState();
        return;
    }
}

// ======================================================
// BILLING
// ======================================================
void MainFrame::OnBillingTick(wxTimerEvent &)
{
    if (!m_aiRunning)
        return;

    if (m_session.seconds <= 0)
    {
        m_billingTimer->Stop();

        m_bridgeService.Stop();
        m_bridgeService.KillInternalServer();

        m_aiRunning = false;
        m_aiConnected = false;

        UpdateButtonState();
        return;
    }

    m_session.seconds--;

    if (m_session.seconds < 0)
        m_session.seconds = 0;

    timerLabel->SetLabel(
        FormatTime(m_session.seconds));

    if (m_session.seconds == 0)
    {
        m_billingTimer->Stop();

        m_bridgeService.Stop();
        m_bridgeService.KillInternalServer();

        m_aiRunning = false;
        m_aiConnected = false;

        UpdateButtonState();
    }
}

// ======================================================
// BUTTON STATE
// ======================================================
void MainFrame::UpdateButtonState()
{
    bool canStart =
        m_hasImage &&
        m_cameraActive &&
        !m_aiRunning;

    if (m_aiRunning)
    {
        startButton->Hide();
        stopButton->Show();
    }
    else if (canStart)
    {
        startButton->Show();
        stopButton->Hide();
    }
    else
    {
        startButton->Hide();
        stopButton->Hide();
    }

    Layout();
}

// ======================================================
// CAMERA CHANGE
// ======================================================
void MainFrame::OnCameraChange(
    wxCommandEvent &e)
{
    m_timer->Stop();

    if (m_videoProvider.OpenCamera(
            e.GetSelection()))
    {
        m_timer->Start(33);
    }
}

// ======================================================
// IMAGE LOAD
// ======================================================
void MainFrame::OnImageLoaded(
    const wxString &path)
{
    wxImage img(path);

    if (!img.IsOk())
        return;

    const int maxSize = 512;

    int w = img.GetWidth();
    int h = img.GetHeight();

    if (w > maxSize || h > maxSize)
    {
        float scale =
            std::min(
                (float)maxSize / w,
                (float)maxSize / h);

        img = img.Scale(
            w * scale,
            h * scale);
    }

    refPreview->SetBitmap(
        wxBitmap(img.Scale(80, 80)));

    wxMemoryOutputStream mem;

    img.SaveFile(mem,
                 wxBITMAP_TYPE_JPEG);

    size_t size = mem.GetSize();

    std::vector<unsigned char> buffer(size);

    mem.CopyTo(buffer.data(), size);

    wxString base64 =
        wxBase64Encode(
            buffer.data(),
            buffer.size());

    m_refImageBase64 =
        "data:image/jpeg;base64," +
        std::string(base64.mb_str());

    m_hasImage = true;

    UpdateButtonState();
}

// ======================================================
// IMAGE CLICK
// ======================================================
void MainFrame::OnUploadClick(
    wxMouseEvent &)
{
    wxFileDialog dlg(
        this,
        "Select Image",
        "",
        "",
        "Images (*.jpg;*.png)|*.jpg;*.png");

    if (dlg.ShowModal() == wxID_OK)
        OnImageLoaded(
            dlg.GetPath());
}

// ======================================================
// LOGOUT
// ======================================================
void MainFrame::OnLogout(wxCommandEvent &)
{
    std::cout << "[LOGOUT] Cleaning up session...\n";

    // =========================
    // STOP BILLING TIMER
    // =========================
    if (m_billingTimer && m_billingTimer->IsRunning())
        m_billingTimer->Stop();

    // =========================
    // STOP CAMERA TIMER
    // =========================
    if (m_timer && m_timer->IsRunning())
        m_timer->Stop();

    // =========================
    // STOP AI SESSION
    // =========================
    if (m_aiRunning)
    {
        m_bridgeService.Stop();
        m_bridgeService.KillInternalServer();
        m_aiRunning = false;
        m_aiConnected = false;
    }

    // =========================
    // SAVE TIME
    // =========================
    UpdateUserSeconds(
        m_session.userId,
        m_session.seconds,
        m_session.token);

    // =========================
    // RELEASE CAMERA (FIX)
    // =========================
    m_videoProvider.Release();

    // =========================
    // CLEAR OUTPUT BUFFER
    // =========================
    m_bridgeService.m_outputProcessor.ClearBuffer();

    // =========================
    // HIDE OLD WINDOW
    // =========================
    Hide();

    // =========================
    // SHOW LOGIN
    // =========================
    LoginDialog dlg(nullptr);

    if (dlg.ShowModal() == wxID_OK)
    {
        MainFrame *newFrame =
            new MainFrame(dlg.session);

        newFrame->Show();
    }

    // =========================
    // DESTROY OLD FRAME
    // =========================
    Destroy();
}

// ======================================================
// FORMAT TIME
// ======================================================
wxString MainFrame::FormatTime(int sec)
{
    return wxString::Format(
        "%02d:%02d",
        sec / 60,
        sec % 60);
}