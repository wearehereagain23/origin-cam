#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <memory>
#include <atomic>
#include <functional>
#include <opencv2/opencv.hpp>
#include "output_processor.h"

class BridgeService
{
public:
    void LaunchInternalServer();
    void KillInternalServer();
    BridgeService();
    ~BridgeService();

    bool EstablishConnection(
        const std::string &refBase64,
        std::function<void()> onStartBilling);

    void StartStream(const std::string &refBase64);
    void Stop();
    void SetUserId(const std::string &userId);
    void SendFrame(const cv::Mat &frame);

    bool IsActive() const;

    OutputProcessor m_outputProcessor;

private:
    std::shared_ptr<ix::WebSocket> m_webSocket;
    std::string m_userId;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_isActive{false};

    std::function<void()> m_onStartBilling;
};