#pragma once
#include <map>
#include <mutex>
#include <opencv2/opencv.hpp> // 包含OpenCV头文件以识别cv::Mat

// MediaFrameCapture 只需要前向声明就足够了，可以避免循环引用
class MediaFrameCapture;
class VideoCapManager
{
private:
	VideoCapManager();
	~VideoCapManager();

public:
	static VideoCapManager* Default();
	// --- 修改开始: 禁止拷贝和赋值，这是单例模式的最佳实践 ---
    	VideoCapManager(const VideoCapManager&) = delete;
    	VideoCapManager& operator=(const VideoCapManager&) = delete;
    	// --- 修改结束 ---
    
//private:
	//static VideoCapManager* self;

public:
	/// <summary>
	/// 打开摄像头，如果已经有，权重+1
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
	/// <param name="W">分辨率W</param>
	/// <param name="H">分辨率H</param>
	/// <returns>摄像头是否打开成功</returns>
	bool OpenCamera(int CapNum, int W = 0, int H = 0);

	/// <summary>
	/// 逻辑关闭摄像头，权重-1，如果为0则真的关闭摄像头
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
	void CloseCamera(int CapNum);

	/// <summary>
	/// 移除摄像头,无视权重，将摄像头移除
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
	void RemoveCamera(int CapNum);

	/// <summary>
	/// 从已经打开的摄像头获取Mat
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
	/// <param name="MatFromCap">Mat图像</param>
	bool GetMatFromCamera(int CapNum, cv::Mat& MatFromCap);
	/// <summary>
	/// 从已经打开的摄像头获取Mat
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
		/// <param name="CapWidth">截图指定分辨率宽(0代表当前宽)</param>
		/// <param name="CapHeight">截图指定分辨率高(0代表当前高)</param>
	/// <param name="MatFromCap">Mat图像</param>
	bool GetMatFromCamera(int CapNum, int CapWidth, int CapHeight, cv::Mat& MatFromCap);

	/// <summary>
	/// 获取已经开启的摄像头宽高
	/// </summary>
	/// <param name="CapNum">摄像头序号</param>
	/// <param name="W">存储W</param>
	/// <param name="H">存储H</param>
	/// <returns>是否成功获取</returns>
	bool GetCameraWH(int CapNum, int& W, int& H);

	/// <summary>
	/// 检查指定摄像头的状态
	/// </summary>
	/// <param name="CameraIndex">摄像头序号</param>
	/// <returns>
	/// <para>0摄像头正常打开使用中 </para>
	/// <para>1摄像头未使用</para>
	/// <para>2摄像头曾经使用过，但现在权重为0</para>
	/// <para>3摄像头应该正在使用，但检测到未打开</para>
	/// <para>4摄像头亮度异常，可能断联</para>
	/// <para>5摄像头获取画面为空</para>
	/// <para>6摄像头仍然在激活，画面为黑或者灰</para>
	/// </returns>
	int CheckCamera(int CameraIndex);
	/// <summary>
	/// 设置是否理会摄像头属性警告，如果理会，将会因为警告而取消摄像头打开
	/// </summary>
	/// <param name="IsMind">是否理会</param>
	void SetMindCapAttrWarn(bool IsMind);
	/// <summary>
	/// 获取当前是否理会摄像头属性警告，如果理会，将会因为警告而取消摄像头打开
	/// </summary>
	bool GetMindCapAttrWarn();
private:
	// --- 修改开始: 在map中存储智能指针，而不是对象本身 ---
	std::map<int, std::pair<int, std::shared_ptr<MediaFrameCapture>>> videoCapMap;
    // --- 修改结束 ---
	//std::mutex videoCapMutex;
	bool isMindCapAttrWarn;
	// --- 修改开始: 添加一个管理器级别的互斥锁 ---
	std::mutex videoCapMutex;
	// --- 修改结束 ---
};

