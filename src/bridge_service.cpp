#include "bridge_service.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <wx/stdpaths.h>
#include <wx/filename.h>

using json = nlohmann::json;

// ============================
// CONSTRUCTOR
// ============================
BridgeService::BridgeService()
{
    m_webSocket = nullptr;
    m_isActive = false;
}

// ============================
// DESTRUCTOR
// ============================
BridgeService::~BridgeService()
{
    std::cout << "[SYSTEM] Destroying BridgeService...\n";

    try
    {
        if (m_webSocket)
        {
            m_webSocket->stop();
            m_webSocket.reset();
        }
    }
    catch (...)
    {
        std::cout << "⚠️ Destructor cleanup error\n";
    }
}

// ============================
// CONNECT
// ============================
bool BridgeService::EstablishConnection(
    const std::string &refBase64,
    std::function<void()> onStartBilling)
{
    std::cout << "[DEBUG] Connecting to Node...\n";

    m_onStartBilling = onStartBilling;
    m_isActive = false;

    // 🔥 ALWAYS CREATE FRESH SOCKET
    if (m_webSocket)
    {
        m_webSocket->stop();
        m_webSocket.reset();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    m_webSocket = std::make_unique<ix::WebSocket>();
    m_webSocket->setUrl("ws://127.0.0.1:8080");

    // ================= CALLBACK =================
    m_webSocket->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr &msg)
        {
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "✅ Connected to Node\n";
            }

            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                // JSON control messages
                if (!msg->str.empty() && msg->str[0] == '{')
                {
                    if (msg->str.find("\"type\":\"connected\"") != std::string::npos)
                    {
                        std::cout << "🔥 AI READY\n";

                        m_isActive = true;

                        if (m_onStartBilling)
                            m_onStartBilling();
                    }

                    return;
                }

                // EVERYTHING ELSE = frame data
                if (!msg->str.empty())
                {
                    m_outputProcessor.ProcessRemoteData(
                        msg->str.data(),
                        msg->str.size());
                }
            }

            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                std::cout << "❌ WebSocket Error: "
                          << msg->errorInfo.reason << "\n";
            }

            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::cout << "🔌 Connection closed\n";
                m_isActive = false;
            }
        });

    m_webSocket->start();

    // ================= SEND START =================
    std::thread([this, refBase64]()
                {
        int attempts = 0;

        while (attempts < 50)
        {
            if (m_webSocket &&
                m_webSocket->getReadyState() == ix::ReadyState::Open)
            {
                json init;
                init["type"] = "start";
               init["image"] = refBase64;
               init["user_id"] = m_userId;

                std::cout << "[DEBUG] Sending start\n";

                m_webSocket->send(init.dump());
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            attempts++;
        }

        std::cout << "❌ Failed to connect to Node\n"; })
        .detach();

    return true;
}

// ============================
// STOP
// ============================
void BridgeService::Stop()
{
    std::cout << "[STOP] Closing AI session...\n";

    try
    {
        if (m_webSocket &&
            m_webSocket->getReadyState() == ix::ReadyState::Open)
        {
            m_webSocket->send(R"({"type":"stop"})");
        }

        if (m_webSocket)
        {
            m_webSocket->stop();
            m_webSocket.reset();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        m_outputProcessor.ClearBuffer();
        m_isActive = false;

        std::cout << "[INFO] Bridge stopped\n";
    }
    catch (...)
    {
        std::cout << "❌ Stop error\n";
    }
}

// ============================
// SEND FRAME
// ============================
void BridgeService::SendFrame(const cv::Mat &frame)
{
    if (!m_isActive ||
        !m_webSocket ||
        m_webSocket->getReadyState() != ix::ReadyState::Open)
        return;

    if (frame.empty())
        return;

    // ==========================================
    // RESIZE FOR LOWER CPU (AI INPUT ONLY)
    // ==========================================
    cv::Mat sendFrame;

    const int maxWidth = 640;

    if (frame.cols > maxWidth)
    {
        float scale =
            (float)maxWidth / frame.cols;

        int newHeight =
            (int)(frame.rows * scale);

        cv::resize(
            frame,
            sendFrame,
            cv::Size(maxWidth, newHeight));
    }
    else
    {
        sendFrame = frame;
    }

    // ==========================================
    // JPEG ENCODE (BALANCED QUALITY)
    // ==========================================
    std::vector<uchar> buf;

    std::vector<int> params = {
        cv::IMWRITE_JPEG_QUALITY, 80};

    cv::imencode(
        ".jpg",
        sendFrame,
        buf,
        params);

    // ==========================================
    // SEND
    // ==========================================
    m_webSocket->sendBinary(
        std::string(buf.begin(), buf.end()));
}

// ============================
// PROCESS CONTROL
// ============================

void BridgeService::LaunchInternalServer()
{
    // ==========================================
    // GET APP ROOT
    // ==========================================
    wxString exePath =
        wxStandardPaths::Get().GetExecutablePath();

    wxFileName fn(exePath);

    wxString root =
        fn.GetPath();

    root = wxFileName(root).GetPath(); // one level up

    wxString bridgePath =
        root +
        wxFileName::GetPathSeparator() +
        "resources" +
        wxFileName::GetPathSeparator() +
        "bridge";

    std::cout << "[SYSTEM] Bridge path: "
              << bridgePath << "\n";

    std::string cmd;

#ifdef _WIN32

    // ==========================================
    // WINDOWS
    // ==========================================
    cmd =
        "start /B cmd /c cd /d \"" +
        std::string(bridgePath.mb_str()) +
        "\" && node bridge.js";

#else

    // ==========================================
    // MAC / LINUX
    // ==========================================
    cmd =
        "cd \"" +
        std::string(bridgePath.mb_str()) +
        "\" && node bridge.js > /dev/null 2>&1 &";

#endif

    std::cout << "[SYSTEM] Running: "
              << cmd << "\n";

    system(cmd.c_str());

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1200));
}

void BridgeService::KillInternalServer()
{
    std::cout << "[SYSTEM] Killing Node...\n";

#ifdef _WIN32

    // ==========================================
    // WINDOWS
    // ==========================================
    system("taskkill /F /IM node.exe >nul 2>&1");

#else

    // ==========================================
    // MAC / LINUX
    // ==========================================
    system("pkill -f bridge.js > /dev/null 2>&1");
    system("lsof -ti:8080 | xargs kill -9 > /dev/null 2>&1");

#endif
}

bool BridgeService::IsActive() const
{
    return m_isActive;
}

void BridgeService::SetUserId(const std::string &userId)
{
    m_userId = userId;
}