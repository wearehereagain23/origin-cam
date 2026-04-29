#pragma once

#include <wx/wx.h>
#include <wx/statbmp.h>
#include <opencv2/opencv.hpp>

#include "internal_bridge.h"
#include "bridge_service.h"
#include "video_provider.h"

class MainFrame : public wxFrame
{
public:
    MainFrame(const OriginSession &session);
    void OnImageLoaded(const wxString &path);

private:
    // =========================
    // SESSION
    // =========================
    OriginSession m_session;
    std::string m_refImageBase64;

    // =========================
    // TIMERS
    // =========================
    wxTimer *m_timer;
    wxTimer *m_billingTimer;

    // =========================
    // SERVICES
    // =========================
    BridgeService m_bridgeService;
    VideoProvider m_videoProvider;

    // =========================
    // STATE
    // =========================
    bool m_hasImage = false;
    bool m_cameraActive = false;
    bool m_aiRunning = false;
    bool m_aiConnected = false;

    int m_syncCounter = 0;

    cv::Mat m_prevFrame;

    // =========================
    // UI
    // =========================
    wxStaticBitmap *inputView;
    wxStaticBitmap *outputView;
    wxStaticBitmap *refPreview;

    wxStaticText *userLabel;
    wxStaticText *timerLabel;

    wxButton *startButton;
    wxButton *stopButton;

    wxChoice *cameraDropdown;
    wxPanel *refPanel;

    // =========================
    // EVENTS
    // =========================
    void OnFrame(wxTimerEvent &e);
    void OnToggleAI(wxCommandEvent &e);
    void OnBillingTick(wxTimerEvent &e);

    void OnLogout(wxCommandEvent &e);
    void OnCameraChange(wxCommandEvent &e);
    void OnUploadClick(wxMouseEvent &e);

    // =========================
    // HELPERS
    // =========================
    void UpdateButtonState();

    wxStaticBitmap *CreateVideoPanel(
        wxPanel *parent,
        const wxString &label,
        wxBoxSizer *target);

    wxString FormatTime(int sec);
};