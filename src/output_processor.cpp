#include "output_processor.h"

#include <wx/image.h>
#include <vector>

// =====================================================
// RECEIVE REMOTE FRAME (LATEST FRAME ONLY)
// =====================================================

void OutputProcessor::ProcessRemoteData(
    const char *data,
    size_t size)
{
    if (data == nullptr || size == 0)
        return;

    std::vector<uchar> buffer(data, data + size);

    cv::Mat decoded =
        cv::imdecode(buffer, cv::IMREAD_COLOR);

    if (decoded.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // overwrite old frame instantly
    m_latestFrame = decoded.clone();
}

// =====================================================
// GET LATEST OUTPUT
// =====================================================

cv::Mat OutputProcessor::GetLatestOutput()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_latestFrame.empty())
        return cv::Mat();

    return m_latestFrame.clone();
}

// =====================================================
// CLEAR BUFFER
// =====================================================

void OutputProcessor::ClearBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_latestFrame.release();
}

// =====================================================
// CONVERT TO WX BITMAP
// =====================================================

wxBitmap OutputProcessor::ConvertToWx(
    const cv::Mat &frame)
{
    if (frame.empty())
        return wxBitmap();

    cv::Mat rgb;

    if (frame.channels() == 3)
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    else if (frame.channels() == 4)
        cv::cvtColor(frame, rgb, cv::COLOR_BGRA2RGB);
    else
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);

    wxImage img(
        rgb.cols,
        rgb.rows,
        rgb.data,
        true);

    return wxBitmap(img.Copy());
}