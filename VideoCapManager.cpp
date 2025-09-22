#include <opencv2/opencv.hpp>
#include "VideoCapManager.h"
#include "MediaFrameCapture.h"
#include "Log.h"
// --- 修改开始 ---
// 包含 AudioVideoProc.h 以获取 ULONGLONG 的定义
#include "AudioVideoProc.h"
// --- 修改结束 ---
using namespace std;
using namespace cv;

// --- 修改开始: 移除静态指针的定义 ---
// VideoCapManager* VideoCapManager::self{ nullptr };
// --- 修改结束 ---

VideoCapManager::VideoCapManager()
	:isMindCapAttrWarn(true)
{
    // 构造函数可以打印日志，帮助我们看到它何时被创建
    LOG_INFO("VideoCapManager Singleton Constructed.");
}

VideoCapManager::~VideoCapManager()
{
}

// --- 修改开始: 使用 Magic Static 重写 Default() ---
VideoCapManager* VideoCapManager::Default()
{
    // C++11及以上标准保证这个初始化是线程安全的
	static VideoCapManager instance;
	return &instance;
}

bool VideoCapManager::OpenCamera(int CapNum, int W, int H)
{
	// --- 修改开始: 锁定整个函数以保护 videoCapMap ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---

	//如果没有这个摄像头序号对应，那么创建该键
	if (!videoCapMap.count(CapNum)) {  // videoCapMap.count(CapNum)，key是唯一的，如果有CapNum这个key,则返回1，没有则返回0
		LOG_INFO("未检测到该摄像头序号使用过，即将创建摄像头(" + to_string(CapNum) + ")的对应键值");
		// --- 修改开始: 创建一个智能指针包裹的 MediaFrameCapture 对象 ---
		videoCapMap[CapNum] = {0, std::make_shared<MediaFrameCapture>()};
        	// --- 修改结束 ---
	}

	// --- 修改开始 ---
	// 删除这一行，因为 MediaFrameCapture 的方法是线程安全的
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---

	//如果没有开启
	if (videoCapMap[CapNum].first == 0) {
		LOG_INFO("即将尝试打开摄像头(" + to_string(CapNum) + ")");
		// --- 修改开始: 通过智能指针访问对象 ---
		auto& videpCap = videoCapMap[CapNum].second;
		if (false == videpCap->open(CapNum)) { // 使用 -> 操作符
			LOG_ERROR("摄像头(" + to_string(CapNum) + ")打开失败");
			return false;
		}
		if (false == videpCap->setupDevice(W, H)) { // 使用 -> 操作符
			LOG_ERROR("摄像头(" + to_string(CapNum) + ")装载失败...");
			return false;
		}
		videoCapMap[CapNum].first = 1;
		LOG_INFO("摄像头(" + to_string(CapNum) + ")打开成功...分辨率为(" + to_string(videpCap->getWidth()) + "x" + to_string(videpCap->getHeight()) + ")");
        	// --- 修改结束 ---
		return true;
	}
	else {
		//如果已经开启，但分辨率不同，则尝试切换
		auto srcW = videoCapMap[CapNum].second->getWidth();
		auto srcH = videoCapMap[CapNum].second->getHeight();
		if (W != srcW || H != srcH) {
			//如果切换失败则还原
			if (false == videoCapMap[CapNum].second->setupDevice(W, H)) {
				//如果还原失败则去除摄像头
				if (false == videoCapMap[CapNum].second->setupDevice(srcW, srcH)) {
					RemoveCamera(CapNum);
					LOG_ERROR("摄像头(" + to_string(CapNum) + ")切换分辨率"
						+ "(" + to_string(W) + "x" + to_string(H) + ")" +
						"失败，保持原分辨率也失败" +
						"(" + to_string(srcW) + "x" + to_string(srcH) + ")"
					);
					return false;
				}
				//还原成功没事了
				++videoCapMap[CapNum].first;
				LOG_WARN("摄像头(" + to_string(CapNum) + ")切换分辨率"
					+ "(" + to_string(W) + "x" + to_string(H) + ")" +
					"失败，已保持原分辨率"+
					"(" + to_string(srcW) + "x" + to_string(srcH) + ")"
				);
				return false;
			}
			++videoCapMap[CapNum].first;
			LOG_INFO("摄像头(" + to_string(CapNum) + ")切换分辨率成功 " +
				"(" + to_string(srcW) + "x" + to_string(srcH) + ")->" +
				"(" + to_string(W) + "x" + to_string(H) + ")");
			return true;
		}
		else {//如果已经开启，则增加打开的权重
			++videoCapMap[CapNum].first;
			LOG_INFO("摄像头(" + to_string(CapNum) + ")共用权重增加，现在为 " + to_string(videoCapMap[CapNum].first));
		}
		return true;
	}
}

void VideoCapManager::CloseCamera(int CapNum)
{
	// --- 修改开始: 锁定整个函数 ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---
    
	//如果没有这个摄像头或者权重为0
	if (0 == videoCapMap.count(CapNum)) {
		LOG_WARN("没有摄像头(" + to_string(CapNum) + ")");
		return;
	}
	// --- 修改开始 ---
	// 删除这一行
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---
	if (videoCapMap[CapNum].first == 0) {
		LOG_WARN("摄像头(" + to_string(CapNum) + ")权重为0");
		return;
	}
	//如果关闭导致归零，那么释放摄像头
	if ((--videoCapMap[CapNum].first) == 0) {
		videoCapMap[CapNum].second->release();
		LOG_INFO("摄像头(" + to_string(CapNum) + ")权重归0，摄像头已释放");
	}
	else {
		LOG_INFO("摄像头(" + to_string(CapNum) + ")被通知关闭，权重减1");
	}
}

void VideoCapManager::RemoveCamera(int CapNum)
{
	// --- 修改开始: 锁定整个函数 ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---
    	
	//如果没有这个摄像头或者权重为0
	if (0 == videoCapMap.count(CapNum)) {
		LOG_WARN("没有摄像头(" + to_string(CapNum) + ")");
		return;
	}
	// --- 修改开始 ---
	// 删除这一行
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---
	if (videoCapMap[CapNum].first == 0) {
		LOG_WARN("摄像头(" + to_string(CapNum) + ")权重为0");
		return;
	}
	videoCapMap[CapNum].first = 0;
	videoCapMap[CapNum].second->release();
	LOG_INFO("摄像头(" + to_string(CapNum) + ")已经强行释放，权重归零");
}

bool VideoCapManager::GetMatFromCamera(int CapNum, cv::Mat& MatFromCap)
{
	return GetMatFromCamera(CapNum, 0, 0, MatFromCap);
}

bool VideoCapManager::GetMatFromCamera(int CapNum, int CapWidth, int CapHeight, cv::Mat& MatFromCap)
{
	// --- 修改开始: 锁定整个函数 ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---
    
	//如果没有这个摄像头或者权重为0
	if (0 == videoCapMap.count(CapNum)) {
		LOG_ERROR("没有摄像头(" + to_string(CapNum) + ")");
		return false;
	}
	// --- 修改开始 ---
	// 删除这一行
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---
	if (videoCapMap[CapNum].first == 0) {
		LOG_ERROR("摄像头(" + to_string(CapNum) + ")权重为0");
		return false;
	}
	auto& videoCap = videoCapMap[CapNum].second;
	//如果没打开
	if (!videoCap->isOpened()) {
		LOG_ERROR("摄像头(" + to_string(CapNum) + ")未打开");
		return false;
	}
	//如果亮度异常，即断联
	//if (isMindCapAttrWarn && -1 == (videoCap.get(cv::CAP_PROP_BRIGHTNESS))) {
	//	LOG_ERROR("摄像头(" + to_string(CapNum) + ")亮度异常");
	//	AudioVideoProcModuleNameSpace::ShowVideoCapInfo(&videoCap);
	//	return false;
	//}
	const int width = static_cast<int>(videoCap->getWidth());
	const int height = static_cast<int>(videoCap->getHeight());
	bool isChange = false;
	if (CapWidth == 0)
		CapWidth = width;
	if (CapHeight == 0)
		CapHeight = height;
	if (width != CapWidth || height != CapHeight) {
		videoCap->setupDevice(CapWidth, CapHeight);
		isChange = true;
	}
	const time_t&& startTime = time(NULL);
	do {
		videoCap->read(MatFromCap);
		if (MatFromCap.empty()) {
			LOG_ERROR("摄像头(" + to_string(CapNum) + ")返回Mat为空");
			return false;
		}
		//如果没有改变分辨率，那么直接返回当前的Mat
		if (!isChange)
			break;
		if (time(NULL) - startTime > 30 * 1000) {
			LOG_ERROR("摄像头(" + to_string(CapNum) + ")因持续获取无效Mat而失败");
			return false;
		}
		//随机获取像素，如果都为205或者0，那么就意味着摄像头还未完全激活
		int colorNum = MatFromCap.data[0];
		//不是问题像素，那么直接退出
		if (colorNum != 205 && colorNum != 0) {
			break;
		}
		//简易判断，如果存在一个不等于问题像素，那就过
		if (MatFromCap.data[0] != colorNum || MatFromCap.datastart[0] != colorNum || MatFromCap.dataend[0] != colorNum) {
			break;
		}
		//有可能有问题，进行进一步判断
		srand((unsigned int)time(NULL));
		// --- 修改开始 ---
		// ULONGLONG 现在已定义
		ULONGLONG dataSize = static_cast<ULONGLONG>(MatFromCap.total() * MatFromCap.elemSize());
		// --- 修改结束 ---
		uint i = 0;
		for (; i < 100; ++i) {
			if (MatFromCap.data[rand() % dataSize] != colorNum) {
				break;
			}
		}
		//没问题，有不一样的像素
		if (i != 100) {
			break;
		}
		//有问题，继续循环
	} while (true);
	//如果改变了分辨率，则重新设置回
	if (isChange) {
		LOG_INFO("分辨率指定后，摄像头(" + to_string(CapNum) + ")获取到的图的分辨率为 " + to_string(MatFromCap.cols) + " x " + to_string(MatFromCap.rows));
		if (width != CapWidth || height != CapHeight) {
			videoCap->setupDevice(width, height);
		}
	}
	return true;
}

bool VideoCapManager::GetCameraWH(int CapNum, int& W, int& H)
{
	// --- 修改开始: 锁定整个函数 ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---
    	
	//如果没有这个摄像头或者权重为0
	if (0 == videoCapMap.count(CapNum)) {
		LOG_ERROR("没有摄像头(" + to_string(CapNum) + ")");
		return false;
	}
	// --- 修改开始 ---
	// 删除这一行
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---
	if (videoCapMap[CapNum].first == 0) {
		LOG_ERROR("摄像头(" + to_string(CapNum) + ")权重为0");
		return false;
	}
	auto& videoCap = videoCapMap[CapNum].second;
	//如果没打开
	if (!videoCap->isOpened()) {
		LOG_ERROR("摄像头(" + to_string(CapNum) + ")未打开");
		return false;
	}
	W = static_cast<int>(videoCap->getWidth());
	H = static_cast<int>(videoCap->getHeight());
	return true;
}

int VideoCapManager::CheckCamera(int CapNum)
{
	// --- 修改开始: 锁定整个函数 ---
	std::lock_guard<std::mutex> autoMutex{ videoCapMutex };
    	// --- 修改结束 ---
    
	//如果没有这个摄像头
	if (0 == videoCapMap.count(CapNum)) {
		return 1;
	}
	// --- 修改开始 ---
	// 删除这一行
	// std::lock_guard<std::mutex> autoMutex{ videoCapMap[CapNum].second.mutexVar };
	// --- 修改结束 ---
	//如果权重为0
	if (videoCapMap[CapNum].first == 0) {
		return 2;
	}
	auto& videoCap = videoCapMap[CapNum].second;
	//如果摄像头未打开
	if (!videoCap->isOpened()) {

		return 3;
	}
	//摄像头亮度异常
	//if (isMindCapAttrWarn && -1 == (videoCap->get(cv::CAP_PROP_BRIGHTNESS))) {
	//	return 4;
	//}
	cv::Mat mat;
	videoCap->read(mat);
	//图像为空异常
	if (mat.empty()) {
		return 5;
	}
	//随机获取像素，如果都为205或者0，那么就意味着摄像头还未完全激活
	int colorNum = mat.data[0];
	//是问题像素，那么直接退出
	if (colorNum == 205 || colorNum == 0) {
		//简易判断，都是问题像素
		if (mat.data[0] == colorNum && mat.datastart[0] == colorNum && mat.dataend[0] == colorNum) {
			//有可能有问题，进行进一步判断
			srand((unsigned int)time(NULL));
			// --- 修改开始 ---
			// ULONGLONG 现在已定义
			ULONGLONG dataSize = static_cast<ULONGLONG>(mat.total() * mat.elemSize());
			// --- 修改结束 ---
			uint i = 0;
			for (; i < 100; ++i) {
				if (mat.data[rand() % dataSize] != colorNum) {
					break;
				}
			}
			//确定有问题，代表摄像头仍在激活中
			if (i == 100) {
				return 6;
			}
		}
	}
	return 0;
}

void VideoCapManager::SetMindCapAttrWarn(bool IsMind)
{
	isMindCapAttrWarn = IsMind;
}

bool VideoCapManager::GetMindCapAttrWarn()
{
	return isMindCapAttrWarn;
}
