#include "video_provider.h"

VideoProvider::VideoProvider() {}

VideoProvider::~VideoProvider()
{
    CloseCamera();
}

bool VideoProvider::OpenCamera(int index)
{
    if (m_cap.isOpened())
        m_cap.release();
    return m_cap.open(index);
}

void VideoProvider::CloseCamera()
{
    if (m_cap.isOpened())
        m_cap.release();
}

bool VideoProvider::IsOpened() const
{
    return m_cap.isOpened();
}

cv::Mat VideoProvider::GetFrame()
{
    cv::Mat frame;
    if (m_cap.isOpened())
    {
        m_cap.read(frame);
    }
    return frame;
}

void VideoProvider::Release()
{
    if (m_cap.isOpened())
    {
        m_cap.release();
    }
}