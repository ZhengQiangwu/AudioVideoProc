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
    /// ��Ĭ�Ϸֱ��ʴ�ָ������ͷ
    /// </summary>
    /// <param name="index">����ͷ���</param>
    /// <returns>�Ƿ�򿪳ɹ�</returns>
    bool open(uint32_t index);

    /// <summary>
    /// �ͷ�����ͷ��Դ
    /// </summary>
    void release();

    /// <summary>
    /// �����豸�ֱ��ʣ������³�ʼ��
    /// </summary>
    /// <param name="width">�ֱ��ʿ�0��ʹ��Ĭ��ֵ</param>
    /// <param name="height">�ֱ��ʸߣ�0��ʹ��Ĭ��ֵ</param>
    /// <returns>���������Ƿ�ɹ�</returns>
    bool setupDevice(int width, int height);

    /// <summary>
    /// ��ȡһ֡
    /// </summary>
    /// <param name="mat">ȡ�õ�һ֡</param>
    /// <returns>�Ƿ�ɹ���ȡ</returns>
    bool read(cv::Mat& mat);
    
    uint32_t getWidth() const { return mWidth; }
    uint32_t getHeight() const { return mHeight; }
    bool isOpened() const;

    /// <summary>
    /// ��ȡ�豸·���б��� "/dev/video0"
    /// </summary>
    static std::vector<std::string> getDeviceList(); 
    
    /// <summary>
    /// ��ȡ�豸�����б��� "Camera 0", "Camera 1"
    /// </summary>
    static std::vector<std::string> getDeviceListName();
    
    /// <summary>
    /// ��ȡָ������ͷ֧�ֵķֱ��ʼ���
    /// </summary>
    static std::set<std::pair<int, int>> getDeviceWHList(int capNum);

private:
    // OpenCV ��Ƶ�������ָ��
    cv::VideoCapture* mCapture{ nullptr };

    // ����ĳ�Ա����
    std::mutex mutexVar;
    bool isOpen{ false };
    uint32_t mWidth{ 0 };
    uint32_t mHeight{ 0 };
    std::string mDeviceId{ "" };
};
