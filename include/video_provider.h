#pragma once
#include <opencv2/opencv.hpp>
#include <wx/wx.h>

class VideoProvider
{
public:
    VideoProvider();
    ~VideoProvider();
    void Release();
    bool OpenCamera(int index);
    void CloseCamera();
    bool IsOpened() const;
    cv::Mat GetFrame();

private:
    cv::VideoCapture m_cap;
};