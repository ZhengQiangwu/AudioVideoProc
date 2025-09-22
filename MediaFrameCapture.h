#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <set>
#include <mutex>

class MediaFrameCapture
{
public:
    MediaFrameCapture();
    ~MediaFrameCapture();

    /// <summary>
    /// 以默认分辨率打开指定摄像头
    /// </summary>
    /// <param name="index">摄像头序号</param>
    /// <returns>是否打开成功</returns>
    bool open(uint32_t index);

    /// <summary>
    /// 释放摄像头资源
    /// </summary>
    void release();

    /// <summary>
    /// 设置设备分辨率，并重新初始化
    /// </summary>
    /// <param name="width">分辨率宽，0则使用默认值</param>
    /// <param name="height">分辨率高，0则使用默认值</param>
    /// <returns>返回设置是否成功</returns>
    bool setupDevice(int width, int height);

    /// <summary>
    /// 读取一帧
    /// </summary>
    /// <param name="mat">取得的一帧</param>
    /// <returns>是否成功读取</returns>
    bool read(cv::Mat& mat);
    
    uint32_t getWidth() const { return mWidth; }
    uint32_t getHeight() const { return mHeight; }
    bool isOpened() const;

    /// <summary>
    /// 获取设备路径列表，如 "/dev/video0"
    /// </summary>
    static std::vector<std::string> getDeviceList(); 
    
    /// <summary>
    /// 获取设备名称列表，如 "Camera 0", "Camera 1"
    /// </summary>
    static std::vector<std::string> getDeviceListName();
    
    /// <summary>
    /// 获取指定摄像头支持的分辨率集合
    /// </summary>
    static std::set<std::pair<int, int>> getDeviceWHList(int capNum);

private:
    // OpenCV 视频捕获对象指针
    cv::VideoCapture* mCapture{ nullptr };

    // 共享的成员变量
    std::mutex mutexVar;
    bool isOpen{ false };
    uint32_t mWidth{ 0 };
    uint32_t mHeight{ 0 };
    std::string mDeviceId{ "" };
};
