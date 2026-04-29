#pragma once

#include <opencv2/opencv.hpp>
#include <wx/bitmap.h>
#include <mutex>

class OutputProcessor
{
public:
    void ProcessRemoteData(const char *data, size_t size);

    cv::Mat GetLatestOutput();

    // ✅ FIXED SIGNATURE (must match cpp)
    wxBitmap ConvertToWx(const cv::Mat &frame);

    void ClearBuffer();

private:
    cv::Mat m_latestFrame;
    std::mutex m_mutex;
};