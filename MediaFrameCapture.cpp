#include "MediaFrameCapture.h"
#include "Log.h" // 假设Log.h是跨平台的

#include <iostream>

using namespace std;

MediaFrameCapture::MediaFrameCapture()
    : mCapture(nullptr), isOpen(false), mWidth(0), mHeight(0)
{
    LOG_INFO("MediaFrameCapture constructed for Linux.");
}

MediaFrameCapture::~MediaFrameCapture()
{
    release();
}

bool MediaFrameCapture::open(uint32_t index)
{
    // --- 修改开始 ---
    // 步骤 1: 在加锁之前，先调用 release() 释放旧资源。
    // 这样做是安全的，因为 release() 内部会管理自己的锁，不会和这里冲突。
    release(); // 先释放之前的资源
    // --- 修改结束 ---

    LOG_INFO("Attempting to open camera index: " + to_string(index));
    
    // --- 修改开始 ---
    // 步骤 2: 创建一个新的作用域，只在需要修改成员变量时才加锁
    {
        std::lock_guard<std::mutex> lock(mutexVar);

        // 检查 isOpen 标志位，防止在 release() 之后，有其他线程又打开了它
        // 这是一个额外的安全检查
        if (isOpen) {
            LOG_WARN("MediaFrameCapture::open - Camera " + to_string(index) + " was opened by another thread concurrently. Aborting this open attempt.");
            return false; // 另一个线程已经抢先打开了，本次操作失败
        }

        LOG_INFO("Attempting to open camera index: " + to_string(index));
        
        mCapture = new cv::VideoCapture();
        // 明确使用V4L2后端
        if (!mCapture->open(index, cv::CAP_V4L2)) {
            LOG_ERROR("Failed to open camera index: " + to_string(index));
            delete mCapture;
            mCapture = nullptr;
            // lock_guard 会在函数返回时自动解锁
            return false;
        }

        if (!mCapture->isOpened()) {
            LOG_ERROR("Camera index " + to_string(index) + " could not be opened.");
            delete mCapture;
            mCapture = nullptr;
            // lock_guard 会在函数返回时自动解锁
            return false;
        }

        // 在持有锁的情况下，安全地修改所有成员变量
        isOpen = true;
        mDeviceId = "/dev/video" + to_string(index);
        
        // 获取并存储默认的宽高
        mWidth = static_cast<uint32_t>(mCapture->get(cv::CAP_PROP_FRAME_WIDTH));
        mHeight = static_cast<uint32_t>(mCapture->get(cv::CAP_PROP_FRAME_HEIGHT));
        
        LOG_INFO("Successfully opened camera " + mDeviceId + " with default resolution " + to_string(mWidth) + "x" + to_string(mHeight));

    } // 锁(lock_guard)在这里结束作用域并自动释放

    // --- 修改结束 ---
    
    // open成功后，默认设置一个分辨率
    // setupDevice() 内部也有自己的锁，所以在锁外调用是安全的
    if (!setupDevice(0, 0)) { // 传入0, 0表示使用默认或最大分辨率
        LOG_ERROR("Failed to setup default resolution for camera " + to_string(index) + ". Releasing.");
        release(); // 如果设置分辨率失败，再次释放资源
        return false;
    }

    return true;
}

void MediaFrameCapture::release()
{
    std::lock_guard<std::mutex> lock(mutexVar);
    if (mCapture != nullptr) {
        LOG_INFO("Releasing camera.");
        mCapture->release();
        delete mCapture;
        mCapture = nullptr;
    }
    isOpen = false;
    mWidth = 0;
    mHeight = 0;
    mDeviceId = "";
}

bool MediaFrameCapture::setupDevice(int width, int height)
{
    std::lock_guard<std::mutex> lock(mutexVar);
    if (!mCapture || !mCapture->isOpened()) {
        LOG_ERROR("Cannot setup device, camera is not open.");
        return false;
    }
    //mCapture = new cv::VideoCapture(cameraIndex, cv::CAP_V4L2);

    // 如果width和height为0，则使用摄像头默认分辨率
    if (width > 0 && height > 0) {
        mCapture->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        LOG_INFO("Setting resolution to " + to_string(width) + "x" + to_string(height));
        mCapture->set(cv::CAP_PROP_FRAME_WIDTH, width);
        mCapture->set(cv::CAP_PROP_FRAME_HEIGHT, height);
    } else {
        LOG_INFO("Using default resolution.");
    }

    // 获取实际应用的分辨率
    mWidth = static_cast<uint32_t>(mCapture->get(cv::CAP_PROP_FRAME_WIDTH));
    mHeight = static_cast<uint32_t>(mCapture->get(cv::CAP_PROP_FRAME_HEIGHT));
    
    if (width > 0 && (mWidth != width || mHeight != height)) {
        LOG_WARN("Requested resolution " + to_string(width) + "x" + to_string(height) + 
                 " is not supported. Using " + to_string(mWidth) + "x" + to_string(mHeight) + " instead.");
    } else {
        LOG_INFO("Actual resolution set to " + to_string(mWidth) + "x" + to_string(mHeight));
    }
    
    return true;
}

bool MediaFrameCapture::read(cv::Mat& oneFrame)
{
    std::lock_guard<std::mutex> lock(mutexVar);
    if (!isOpened()) {
        return false;
    }
    try {
        if (!mCapture->read(oneFrame)) {
            LOG_WARN("Failed to read frame from camera.");
            return false;
        }
    } catch (const cv::Exception& e) {
        LOG_ERROR("OpenCV exception while reading frame: " + string(e.what()));
        return false;
    }
    return !oneFrame.empty();
}

bool MediaFrameCapture::isOpened() const
{
    return mCapture != nullptr && mCapture->isOpened();
}

// --- Linux Static Functions ---

std::vector<std::string> MediaFrameCapture::getDeviceList()
{
    vector<string> devices;
    for (int i = 0; i < 10; ++i) { // 检查前10个设备
        string path = "/dev/video" + to_string(i);
        cv::VideoCapture temp_cap(i, cv::CAP_V4L2);
        if (temp_cap.isOpened()) {
            devices.push_back(path);
            temp_cap.release();
        } else {
            if (i > 1) break; // 如果连续两个设备打不开，就停止搜索
        }
    }
    return devices;
}

std::vector<std::string> MediaFrameCapture::getDeviceListName()
{
    vector<string> names;
    auto deviceList = getDeviceList();
    for (size_t i = 0; i < deviceList.size(); ++i) {
        names.push_back("Camera " + to_string(i));
    }
    return names;
}

std::set<std::pair<int, int>> MediaFrameCapture::getDeviceWHList(int capNum)
{
    std::set<std::pair<int, int>> resolutions;
    cv::VideoCapture cap(capNum, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        return resolutions;
    }

    const std::vector<std::pair<int, int>> common_resolutions = {
        {320, 240}, {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1920, 1080}
    };

    for (const auto& res : common_resolutions) {
        cap.set(cv::CAP_PROP_FRAME_WIDTH, res.first);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.second);
        int actual_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        if (actual_width == res.first && actual_height == res.second) {
            resolutions.insert({actual_width, actual_height});
        }
    }
    cap.release();
    return resolutions;
}
