#include <opencv2/opencv.hpp>

// C++ Standard Libraries
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <memory> // For std::unique_ptr
#include <dirent.h>      // For DIR, opendir, readdir, closedir, struct dirent
#include <fcntl.h>       // For open, O_RDWR
#include <unistd.h>      // For close
#include <sys/ioctl.h>   // For ioctl
#include <linux/videodev2.h> // For v4l2_capability, VIDIOC_QUERYCAP, V4L2_CAP_VIDEO_CAPTURE, V4L2_CAP_STREAMING
#include <cstring>

// FFmpeg Headers
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/avutil.h"
#include "libavutil/fifo.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}

// YUV Header
extern "C" {
#include <libyuv.h>
}

// Project Headers
#include "VideoCapManager.h"
#include "Tool.h"
#include "MediaFrameCapture.h"
#include "Log.h"
#include "AudioVideoProc.h"
#include "AudioVideoProcModule.h"

// Linux-specific Headers
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace std;
using namespace cv;

// Global variables
static const string g_SplitStr = "MiGao";
static map<int, pair<thread*, bool>> g_CapForThread;
static map<int, AudioVideoProcModule*> g_MoudleVec;
extern bool g_IsDebug; // Assuming g_IsDebug is defined in Log.h or another common place

extern "C" {
    namespace AudioVideoProcNameSpace {
	 bool StartPush(int ModuleNum) {
            LOG_INFO("StartSimplePush: 收到简易推流请求，模块号: " + to_string(ModuleNum));

            // 1. 创建并初始化模块
            if (!Module_New(ModuleNum)) {
                LOG_ERROR("StartSimplePush: Module_New(" + to_string(ModuleNum) + ") 失败。");
                return false;
            }
            if (!Init(ModuleNum)) {
                LOG_ERROR("StartSimplePush: Init(" + to_string(ModuleNum) + ") 失败。");
                Module_Delete(ModuleNum); // 清理已创建的模块
                return false;
            }

            // 2. 在这里硬编码你的预设参数
            const int camIndex = 0;
            const int videoWidth = 1280;
            const int videoHeight = 720;
            const int frameRate = 16;
            const char* rtmpUrl = "rtmp://localhost/live/stream";

            LOG_INFO("StartSimplePush: 使用预设参数 - 摄像头: " + to_string(camIndex) + 
                     ", 分辨率: " + to_string(videoWidth) + "x" + to_string(videoHeight) +
                     ", 帧率: " + to_string(frameRate));
            LOG_INFO("StartSimplePush: 推流地址: " + string(rtmpUrl));


            // 3. 应用参数设置
            // 设置摄像头和分辨率
            SetCameraWH(ModuleNum, camIndex, videoWidth, videoHeight); 
            // 录制区域 (使用摄像头完整尺寸)
            SetRecordXYWH(ModuleNum, 0, 0, videoWidth, videoHeight);
            // 最终视频分辨率
            SetRecordFixSize(ModuleNum, videoWidth, videoHeight);
            
            // 设置录制属性：推流(true), 录视频(true), 录系统声音(false), 录麦克风(true)
            SetRecordAttr(ModuleNum, true, frameRate, true, false, true, 0); 
            
            // 设置RTMP推流地址
            SetRtmpUrl(ModuleNum, (char*)rtmpUrl);

            // 4. 开始推流
            if (!StartRecord(ModuleNum)) {
                LOG_ERROR("StartSimplePush: StartRecord(" + to_string(ModuleNum) + ") 失败。");
                UnInit(ModuleNum);
                Module_Delete(ModuleNum);
                return false;
            }
            
            LOG_INFO("StartSimplePush: 简易推流任务已成功启动。");
	     cout << "推流已启动，将持续30秒..." << endl;
		
	     this_thread::sleep_for(chrono::seconds(30));
	     cout << "30秒结束，准备停止推流..." << endl;

	     LOG_INFO("StopSimplePush: 收到停止简易推流请求，模块号: " + to_string(ModuleNum));
            
            // 1. 停止录制
            StopRecord(ModuleNum);
            
            // 2. 卸载模块
            UnInit(ModuleNum);
            
            // 3. 删除模块
            Module_Delete(ModuleNum);

            LOG_INFO("StopSimplePush: 简易推流任务已停止并清理。");
            return true;
        }
        void EnabelDebug()
        {
            g_IsDebug = true;
        }
        bool Module_New(int ModuleNum) {
            if (g_MoudleVec.count(ModuleNum)) {
                LOG_WARN(to_string(ModuleNum) + "号模块已存在");
                return false;
            }
            g_MoudleVec[ModuleNum] = new AudioVideoProcModule;
            LOG_INFO("新增" + to_string(ModuleNum) + "号模块成功");
            return true;
        }

        bool Module_Delete(int ModuleNum) {
            auto moduleOne = g_MoudleVec.find(ModuleNum);
            if (moduleOne != g_MoudleVec.end()) {
                delete moduleOne->second;
                g_MoudleVec.erase(moduleOne);
                LOG_INFO("回收" + to_string(ModuleNum) + "号模块成功");
                return true;
            }
            LOG_WARN(to_string(ModuleNum) + "号模块不存在");
            return false;
        }

        void Module_DeleteAll() {
            for (auto& moduleOne : g_MoudleVec) {
                delete moduleOne.second;
                moduleOne.second = nullptr;
                LOG_INFO("回收" + to_string(moduleOne.first) + "号模块成功");
            }
            g_MoudleVec.clear();
            LOG_INFO("回收所有模块成功");
        }

        int Module_Size() {
            return static_cast<int>(g_MoudleVec.size());
        }

        bool Init(int ModuleNum) {
            auto moduleOne = g_MoudleVec.find(ModuleNum);
            if (moduleOne == g_MoudleVec.end()) {
                LOG_ERROR(to_string(ModuleNum) + "号模块不存在");
                return false;
            }
            return g_MoudleVec[ModuleNum]->Init();
        }

        void UnInit(int ModuleNum) {
            auto moduleOne = g_MoudleVec.find(ModuleNum);
            if (moduleOne == g_MoudleVec.end()) {
                LOG_WARN(to_string(ModuleNum) + "号模块不存在");
                return;
            }
            g_MoudleVec[ModuleNum]->UnInit();
        }

        int Hello() {
            return 2333;
        }

        bool GetIsInit(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetIsInit();
        }

        void SetBitRate(int ModuleNum, int BitRate) {
            g_MoudleVec[ModuleNum]->SetBitRate(BitRate);
        }

        int GetBitRate(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetBitRate();
        }

        void SetGopSize(int ModuleNum, int GopSize)
        {
            g_MoudleVec[ModuleNum]->SetGopSize(GopSize);
        }

        int GetGopSize(int ModuleNum)
        {
            return g_MoudleVec[ModuleNum]->GetGopSize();
        }

        void SetMaxBFrames(int ModuleNum, int MaxBFrames)
        {
            g_MoudleVec[ModuleNum]->SetMaxBFrames(MaxBFrames);
        }

        int GetMaxBFrames(int ModuleNum)
        {
            return g_MoudleVec[ModuleNum]->GetMaxBFrames();
        }

        void SetThreadCount(int ModuleNum, int ThreadCount)
        {
            g_MoudleVec[ModuleNum]->SetThreadCount(ThreadCount);
        }

        int GetThreadCount(int ModuleNum)
        {
            return g_MoudleVec[ModuleNum]->GetThreadCount();
        }

        void SetPrivData(int ModuleNum, char* Key, char* Value)
        {
            g_MoudleVec[ModuleNum]->SetPrivData(Key, Value);
        }

        const char* GetPrivData(int ModuleNum, char* Key)
        {
            static string str;
            str = g_MoudleVec[ModuleNum]->GetPrivData(Key);
            return str.c_str();
        }

        void SetNbSample(int ModuleNum, int NbSample) {
            g_MoudleVec[ModuleNum]->SetNbSample(NbSample);
        }

        int GetNbSample(int ModuleNum) {
            return  g_MoudleVec[ModuleNum]->GetNbSample();
        }

        void SetMixFilter(int ModuleNum, char* MixFilterString)
        {
            g_MoudleVec[ModuleNum]->SetMixFilter(MixFilterString);
        }

        const char* GetMixFilter(int ModuleNum)
        {
            static string str;
            str = g_MoudleVec[ModuleNum]->GetMixFilter();
            return str.c_str();
        }

        void SetMicFilter(int ModuleNum, char* MicFilterString)
        {
            g_MoudleVec[ModuleNum]->SetMicFilter(MicFilterString);
        }

        const char* GetMicFilter(int ModuleNum)
        {
            static string str;
            str = g_MoudleVec[ModuleNum]->GetMicFilter();
            return str.c_str();
        }

        int GetFrameAppendNum(int ModuleNum) {
            return  g_MoudleVec[ModuleNum]->GetFrameAppendNum();
        }

        bool GetInnerReadyOk(int ModuleNum) {
            return  g_MoudleVec[ModuleNum]->GetInnerReadyOk();
        }

        bool GetMicReadyOk(int ModuleNum) {
            return  g_MoudleVec[ModuleNum]->GetMicReadyOk();
        }
        const char* GetCameraList() {
            static std::string result;
            result.clear();  // 清空上次的内容
        
            std::set<std::string> uniqueDevices;
            std::vector<std::string> deviceList;  // 用于存储设备名称
        
            DIR *dir = opendir("/dev");
            if (dir == nullptr) {
                std::cerr << "无法打开 /dev 目录" << std::endl;
                return "";
            }
        
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strncmp(entry->d_name, "video", 5) == 0) {
                    std::string device = "/dev/" + std::string(entry->d_name);
                    int fd = open(device.c_str(), O_RDWR);
                    if (fd < 0) continue;
        
                    struct v4l2_capability cap;
                    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
                            (cap.capabilities & V4L2_CAP_STREAMING)) {
                            
                            std::string fullDeviceName = reinterpret_cast<char*>(cap.card);
        
                            // 选择前半部分或后半部分
                            std::string filteredName;
                            size_t pos = fullDeviceName.find(":");  
                            if (pos != std::string::npos) {
                                filteredName = fullDeviceName.substr(0, pos);  // 获取前半部分
                            } else {
                                filteredName = fullDeviceName;  // 如果没有冒号，则直接使用整个名字
                            }
        
                            if (uniqueDevices.insert(filteredName).second) {
                                deviceList.push_back(filteredName);  // 存入 vector
                            }
                        }
                    }
                    close(fd);
                }
            }
            closedir(dir);
        
            for (auto it = deviceList.rbegin(); it != deviceList.rend(); ++it) {
                if (!result.empty()) {
                    result += "MiGao";  
                }
                result += *it;
            }
        
            std::cout << "检测到的摄像头: " << result << std::endl;
            return result.c_str();
        }

        int GetCameraIndex(const char* cameraName) {
            if (cameraName == nullptr) {
                std::cerr << "传入的摄像头名称为空" << std::endl;
                return -1;
            }
        
            std::string targetName(cameraName); // 转换为 std::string
            DIR *dir = opendir("/dev");
            if (dir == nullptr) {
                std::cerr << "无法打开 /dev 目录" << std::endl;
                return -1;
            }
        
            struct dirent *entry;
            std::vector<std::pair<int, std::string>> cameraList; // 存储索引和摄像头名称
        
            while ((entry = readdir(dir)) != nullptr) {
                if (strncmp(entry->d_name, "video", 5) == 0) {
                    std::string device = "/dev/" + std::string(entry->d_name);
                    int index = std::stoi(std::string(entry->d_name + 5)); // 提取索引号
                    int fd = open(device.c_str(), O_RDWR);
                    if (fd < 0) continue;
        
                    struct v4l2_capability cap;
                    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
                            (cap.capabilities & V4L2_CAP_STREAMING)) {
        
                            std::string fullDeviceName = reinterpret_cast<char*>(cap.card);
        
                            // 选择前半部分或后半部分
                            std::string filteredName;
                            size_t pos = fullDeviceName.find(":");
                            if (pos != std::string::npos) {
                                filteredName = fullDeviceName.substr(0, pos);  // 获取前半部分
                            } else {
                                filteredName = fullDeviceName;  // 如果没有冒号，则使用整个名字
                            }
        
                            cameraList.emplace_back(index, filteredName); // 记录索引和名称
                            //std::cout << "检测到摄像头: " << filteredName << " (索引: video" << index << ")" << std::endl;
                        }
                    }
                    close(fd);
                }
            }
            closedir(dir);
            std::sort(cameraList.begin(), cameraList.end());
            for (const auto& cam : cameraList) {
                //std::cout << "设备: " << cam.second << " -> video" << cam.first << std::endl;
            }
            // 查找对应的索引
            for (const auto& cam : cameraList) {
                if (cam.second == targetName) {
                    return cam.first;
                }
            }
            return -1;
        }

       const char* GetCameraWHList(int CameraNum) {
            static std::string resolutions;  // 用来存储分辨率列表
        
            // 打开摄像头设备
            std::string device = "/dev/video" + std::to_string(CameraNum);
            int fd = open(device.c_str(), O_RDWR);
            if (fd == -1) {
                std::cerr << "无法打开摄像头设备: " << device << std::endl;
                return nullptr;
            }
        
            // 枚举支持的格式
            struct v4l2_fmtdesc fmtDesc;
            memset(&fmtDesc, 0, sizeof(fmtDesc));
            fmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            
            if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtDesc) == -1) {
                std::cerr << "获取格式列表失败" << std::endl;
                close(fd);
                return nullptr;
            }
        
            // 如果支持 MJPEG 格式
            if (fmtDesc.pixelformat == V4L2_PIX_FMT_MJPEG) {
                resolutions.clear();  // 清空以前的分辨率信息
        
                // 枚举 MJPEG 格式下支持的分辨率
                struct v4l2_frmsizeenum frameSize;
                memset(&frameSize, 0, sizeof(frameSize));
                frameSize.pixel_format = V4L2_PIX_FMT_MJPEG;
        
                int index = 0;
                while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frameSize) == 0) {
                    if (frameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                        // 拼接分辨率信息，按照格式 "640x480MiGao"
                        resolutions += std::to_string(frameSize.discrete.width) + "x" + std::to_string(frameSize.discrete.height) + "MiGao";
                    }
                    index++;
                    frameSize.index = index;
                }
        
                // 如果最后一个分辨率有多余的 "MiGao" 要去掉
                if (!resolutions.empty() && resolutions.substr(resolutions.size() - 5) == "MiGao") {
                    resolutions = resolutions.substr(0, resolutions.size() - 5);
                }
            }
        
            close(fd);
            return resolutions.c_str();
        }

        bool SetCamera(int ModuleNum, int CameraNum) {
            return g_MoudleVec[ModuleNum]->SetCamera(CameraNum);
        }

        bool SetCameraWH(int ModuleNum, int CameraNum, int W, int H) {
            return g_MoudleVec[ModuleNum]->SetCamera(CameraNum, W, H);
        }

        int GetCamera(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetCamera();
        }

        bool GetCameraWH(int ModuleNum, int CameraNum, int* W, int* H)
        {
            return g_MoudleVec[ModuleNum]->GetCameraWH(CameraNum, *W, *H);
        }

        bool SetSecondaryCamera(int ModuleNum, int CameraNum, int W, int H, int WPercent, int HPercent) {
            return g_MoudleVec[ModuleNum]->SetSecondaryCamera(CameraNum, W, H, WPercent, HPercent);
        }

        int GetSecondaryCamera(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetSecondaryCamera();
        }

        bool SwapRecordSubScreen(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->SwapRecordSubScreen();
        }

        void SetVideoCapErrFun(int ModuleNum, VideoCapErrCallBack videoCapFun) {
            g_MoudleVec[ModuleNum]->SetVideoCapErrFun(videoCapFun);
        }

        void SetAudioErrFun(int ModuleNum, AudioErrCallBack AudioErrFun) {
            g_MoudleVec[ModuleNum]->SetAudioErrFun(AudioErrFun);
        }

        void SetRecordFileName(int ModuleNum, char* FileName) {
            g_MoudleVec[ModuleNum]->SetRecordFileName(FileName);
        }

        void SetRtmpUrl(int ModuleNum, char* RtmpUrl) {
            g_MoudleVec[ModuleNum]->SetRtmpUrl(RtmpUrl);
        }

        bool StartRecord(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->StartRecord();
        }

        bool StartRecordWithSet(int ModuleNum, bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation) {
            return g_MoudleVec[ModuleNum]->StartRecord(IsRtmp, FrameRate, IsRecordVideo, IsRecordSound, IsRecordMicro, SecondaryScreenLocation);
        }

        bool ContinueRecord(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->ContinueRecord();
        }

        bool PauseRecord(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->PauseRecord();
        }

        bool StopRecord(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->StopRecord();
        }

        bool FinishRecord(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->FinishRecord();
        }

        void SetRecordAttr(int ModuleNum, bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation) {
            g_MoudleVec[ModuleNum]->SetRecordAttr(IsRtmp, FrameRate, IsRecordVideo, IsRecordSound, IsRecordMicro, SecondaryScreenLocation);
        }

        bool IsRtmp(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->IsRtmp();
        }

        void SetAcceptAppendFrame(int ModuleNum, bool IsAcceptAppendFrame) {
            g_MoudleVec[ModuleNum]->SetAcceptAppendFrame(IsAcceptAppendFrame);
        }

        bool IsAcceptAppendFrame(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->IsAcceptAppendFrame();
        }

        bool IsRecording(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->IsRecording();
        }

        int GetRecordAttrFrameRate(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetRecordAttrFrameRate();
        }

        bool GetRecordAttrRecordVideo(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetRecordAttrRecordVideo();
        }

        bool GetRecordAttrRecordSound(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetRecordAttrRecordSound();
        }

        bool GetRecordAttrIsRecordMicro(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetRecordAttrIsRecordMicro();
        }

        const char* GetOutFileName(int ModuleNum) {
            static std::string outputFileStr;
            outputFileStr = g_MoudleVec[ModuleNum]->GetOutFileName();
            return outputFileStr.c_str();
        }

        const char* GetRtmpAddr(int ModuleNum) {
            static std::string rtmpStr;
            rtmpStr = g_MoudleVec[ModuleNum]->GetRtmpUrl();
            return rtmpStr.c_str();
        }

        const char* GetScreenInfo() {
            static string screenXY;
            screenXY.clear();
            
            Display* display = XOpenDisplay(NULL);
            if (!display) {
                LOG_ERROR("Cannot open X Display.");
                return "";
            }

            int screen_num = DefaultScreen(display);
            int width = DisplayWidth(display, screen_num);
            int height = DisplayHeight(display, screen_num);
            
            // Linux下全局缩放比例依赖具体桌面环境，难以统一获取，此处返回1.0
            double zoom = 1.0; 

            screenXY = to_string(width) + g_SplitStr;
            screenXY += to_string(height) + g_SplitStr;
            screenXY += to_string(zoom) + g_SplitStr;

            XCloseDisplay(display);
            return screenXY.c_str();
        }

        const char* GetMicList() {
            static string micListStr;
            micListStr.clear();

            avdevice_register_all();
            
            // --- 修改开始 ---
            // 去掉 const
            AVInputFormat* informat = av_find_input_format("alsa");
            // --- 修改结束 ---
            
            if (!informat) {
                LOG_WARN("Could not find ALSA input format. Trying PulseAudio.");
                informat = av_find_input_format("pulse");
                if(!informat) {
                    LOG_ERROR("Could not find ALSA or PulseAudio input format for device listing.");
                    return "";
                }
            }
            
            AVDeviceInfoList* device_list = nullptr;
            // --- 修改开始 ---
            // 现在 informat 是 AVInputFormat*，类型匹配
            int ret = avdevice_list_input_sources(informat, NULL, NULL, &device_list);
            // --- 修改结束 ---
            
            if (ret < 0) {
                LOG_ERROR("avdevice_list_input_sources failed: " + to_string(ret));
                return "";
            }

            for (int i = 0; i < device_list->nb_devices; i++) {
                micListStr += device_list->devices[i]->device_description;
                micListStr += g_SplitStr;
            }

            avdevice_free_list_devices(&device_list);
            return micListStr.c_str();
        }

        int GetMicPattern(int ModuleNum) {
            return g_MoudleVec[ModuleNum]->GetMicPattern();
        }

        bool SetMicPattern(int ModuleNum, int MicPattern, int MicNum) {
            return g_MoudleVec[ModuleNum]->SetMicPattern(MicPattern, MicNum);
        }

        void SetRecordXYWH(int ModuleNum, int X, int Y, int Width, int Height) {
            g_MoudleVec[ModuleNum]->SetRecordXYWH(X, Y, Width, Height);
        }

        const char* GetRecordXYWH(int ModuleNum) {
            static string whStr;
            int x, y, width, height;
            g_MoudleVec[ModuleNum]->GetRecordXYWH(x, y, width, height);
            whStr = (to_string(x) + g_SplitStr + to_string(y) + g_SplitStr + to_string(width) + g_SplitStr + to_string(height) + g_SplitStr);
            return whStr.c_str();
        }

        void SetRecordFixSize(int ModuleNum, int Width, int Height) {
            g_MoudleVec[ModuleNum]->SetRecordFixSize(Width, Height);
        }

        const char* GetRecordFixSize(int ModuleNum) {
            static string whStr;
            int width, height;
            g_MoudleVec[ModuleNum]->GetRecordFixSize(width, height);
            whStr = (to_string(width) + g_SplitStr + to_string(height) + g_SplitStr);
            return whStr.c_str();
        }

        bool SnapShoot(int CapNum, const char* FilePath,
            bool (*dataCallBack)(int capNum, int width, int height, long dataLen, char* data)) {
            return SnapShootWH(CapNum, 0, 0, FilePath, false, false, dataCallBack);
        }

        bool SnapShootWH(int CapNum, int CapWidth, int CapHeight, const  char* FilePath, bool IsReturnData, bool IsCutBlackEdge,
            bool (*dataCallBack)(int capNum, int width, int height, long dataLen, char* data)) {
            int Width = 0;
            int Height = 0;
            std::unique_ptr<char[]> DesktopData(nullptr);
            char* DataPtr = nullptr;
            bool isOk = false;
            Mat mat;
            long DataSize = 0;

            if (-1 == CapNum) {
                LOG_INFO("桌面即将进行截图");
                Display* display = XOpenDisplay(NULL);
                if (!display) {
                    LOG_ERROR("Cannot open X Display for screenshot.");
                    return false;
                }
                Window root = DefaultRootWindow(display);
                XWindowAttributes attributes = {0};
                XGetWindowAttributes(display, root, &attributes);
                Width = attributes.width;
                Height = attributes.height;

                XImage* img = XGetImage(display, root, 0, 0, Width, Height, AllPlanes, ZPixmap);
                if (!img) {
                    LOG_ERROR("XGetImage failed for screenshot.");
                    XCloseDisplay(display);
                    return false;
                }
                
                DataSize = Width * Height * 4;
                DesktopData = make_unique<char[]>(DataSize);
                memcpy(DesktopData.get(), img->data, DataSize);
                DataPtr = DesktopData.get();
                isOk = true;

                if (FilePath) {
                    LOG_DEBUG("写入到文件:" + string(FilePath));
                    Mat tempMat(Height, Width, CV_8UC4, DataPtr);
                    imwrite(FilePath, tempMat);
                }

                XDestroyImage(img);
                XCloseDisplay(display);
            }
            else {
                LOG_INFO("摄像头(" + to_string(CapNum) + ")即将进行截图 分辨率(" + to_string(CapWidth) + "x" + to_string(CapHeight) + ")");
                if (VideoCapManager::Default()->OpenCamera(CapNum)) {
                    if (VideoCapManager::Default()->GetMatFromCamera(CapNum, CapWidth, CapHeight, mat)) {
                        Height = mat.rows;
                        Width = mat.cols;
                        DataSize = static_cast<long>(mat.total() * mat.elemSize());
                        DataPtr = (char*)mat.data;

                        if (IsCutBlackEdge) {
                            LOG_DEBUG("即将去除黑边");
                            Tool::RemoveBlackEdge(mat);
                        }

                        if (FilePath) {
                            LOG_DEBUG("写入到文件:" + string(FilePath));
                            imwrite(FilePath, mat);
                        }
                        isOk = true;
                    }
                    else {
                        LOG_ERROR("采集截图失败");
	                LOG_INFO("桌面即将进行截图");
	                Display* display = XOpenDisplay(NULL);
	                if (!display) {
	                    LOG_ERROR("Cannot open X Display for screenshot.");
	                    return false;
	                }
	                Window root = DefaultRootWindow(display);
	                XWindowAttributes attributes = {0};
	                XGetWindowAttributes(display, root, &attributes);
	                Width = attributes.width;
	                Height = attributes.height;
	
	                XImage* img = XGetImage(display, root, 0, 0, Width, Height, AllPlanes, ZPixmap);
	                if (!img) {
	                    LOG_ERROR("XGetImage failed for screenshot.");
	                    XCloseDisplay(display);
	                    return false;
	                }
	                
	                DataSize = Width * Height * 4;
	                DesktopData = make_unique<char[]>(DataSize);
	                memcpy(DesktopData.get(), img->data, DataSize);
	                DataPtr = DesktopData.get();
	                isOk = true;
	
	                if (FilePath) {
	                    LOG_DEBUG("写入到文件:" + string(FilePath));
	                    Mat tempMat(Height, Width, CV_8UC4, DataPtr);
	                    imwrite(FilePath, tempMat);
	                }
	
	                XDestroyImage(img);
	                XCloseDisplay(display);
                    }
                    VideoCapManager::Default()->CloseCamera(CapNum);
                }else{
	                LOG_INFO("桌面即将进行截图");
	                Display* display = XOpenDisplay(NULL);
	                if (!display) {
	                    LOG_ERROR("Cannot open X Display for screenshot.");
	                    return false;
	                }
	                Window root = DefaultRootWindow(display);
	                XWindowAttributes attributes = {0};
	                XGetWindowAttributes(display, root, &attributes);
	                Width = attributes.width;
	                Height = attributes.height;
	
	                XImage* img = XGetImage(display, root, 0, 0, Width, Height, AllPlanes, ZPixmap);
	                if (!img) {
	                    LOG_ERROR("XGetImage failed for screenshot.");
	                    XCloseDisplay(display);
	                    return false;
	                }
	                
	                DataSize = Width * Height * 4;
	                DesktopData = make_unique<char[]>(DataSize);
	                memcpy(DesktopData.get(), img->data, DataSize);
	                DataPtr = DesktopData.get();
	                isOk = true;
	
	                if (FilePath) {
	                    LOG_DEBUG("写入到文件:" + string(FilePath));
	                    Mat tempMat(Height, Width, CV_8UC4, DataPtr);
	                    imwrite(FilePath, tempMat);
	                }
	                XDestroyImage(img);
	                XCloseDisplay(display);
			}
            	}
            if (isOk) {
                if (dataCallBack)
                    isOk = dataCallBack(CapNum, Width, Height, DataSize, DataPtr);
                return isOk;
            }
            else
                return false;
        }

        const char* GetSplitStr()
        {
            return g_SplitStr.c_str();
        }

        bool StartCapForCallBack(int CapNum, int CapW, int CapH, int CapTime, CapDataFuntion CallBackFun)
        {
            if (CapNum < 0) {
                LOG_ERROR("不支持桌面采集，只支持摄像头采集[0,1,...]");
                return false;
            }
            if (g_CapForThread.count(CapNum) && g_CapForThread[CapNum].first) {
                LOG_INFO("正在停止上次的采集");
                StopCapForCallBack(CapNum);
            }
            LOG_INFO("即将尝试以指定分辨率(" + to_string(CapW) + "×" + to_string(CapH) + ")打开摄像头(" + to_string(CapNum) + ")");
            if (!VideoCapManager::Default()->OpenCamera(CapNum, CapW, CapH)) {
                LOG_ERROR("摄像头未能就位，采集取消");
                return false;
            }
            if (nullptr == CallBackFun) {
                LOG_ERROR("发现回调函数为空，采集取消");
                return false;
            }
            g_CapForThread[CapNum] = {nullptr, true};
            g_CapForThread[CapNum].first = new thread([=]() {
                auto lastTime = std::chrono::steady_clock::now();
                Mat mat;
                int w, h;
                VideoCapManager::Default()->GetCameraWH(CapNum, w, h);
                while (g_CapForThread.count(CapNum) && g_CapForThread.at(CapNum).second) {
                    if (!VideoCapManager::Default()->GetMatFromCamera(CapNum, mat)) {
                        LOG_ERROR("摄像头采集失败，采集取消");
                        if(g_CapForThread.count(CapNum)) g_CapForThread.at(CapNum).second = false;
                        break;
                    }
                    LOG_DEBUG("采集到一帧，即将送往回调");
                    CallBackFun(CapNum, w, h, (const char*)mat.data, static_cast<int>(mat.total() * mat.elemSize()));
                    LOG_DEBUG("回调处理完成");
                    
                    auto nowTime = std::chrono::steady_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - lastTime).count();
                    
                    if (CapTime > elapsedTime) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(CapTime - elapsedTime));
                    }
                    lastTime = std::chrono::steady_clock::now();
                }
                LOG_INFO("即将关闭摄像头 " + to_string(CapNum));
                VideoCapManager::Default()->CloseCamera(CapNum);
                LOG_INFO("采集退出");
            });
            return true;
        }

        void StopCapForCallBack(int CapNum)
        {
            if (g_CapForThread.count(CapNum) && g_CapForThread[CapNum].first) {
                LOG_INFO("即将回收线程");
                g_CapForThread[CapNum].second = false;
                if (g_CapForThread[CapNum].first->joinable()) {
                    g_CapForThread[CapNum].first->join();
                }
                delete g_CapForThread[CapNum].first;
                g_CapForThread.erase(CapNum);
                LOG_INFO("线程回收完成");
            }
            else {
                LOG_INFO("没有采集线程等待回收");
            }
        }

        void FinishCapForCallBack(int CapNum)
        {
            StopCapForCallBack(CapNum);
        }

        void ShowMat(char* Data, int Width, int Height, int Channels) {
            ShowMatWithName("ShowMat", Data, Width, Height, Channels);
        }

        void ShowMatWithName(const char* WindowName, char* Data, int Width, int Height, int Channels)
        {
            int type = 0;
            switch (Channels) {
            case 1: type = CV_8UC1; break;
            case 2: type = CV_8UC2; break;
            case 3: type = CV_8UC3; break;
            case 4: type = CV_8UC4; break;
            default: return;
            }
            imshow(WindowName, Mat(Height, Width, type, Data));
            waitKey(1);
        }
        
        // --- 修改开始 ---
        // 明确使用 cv::VideoCapture 并提供函数体
        void ShowVideoCapInfo(cv::VideoCapture* videoCap) {
            if (videoCap && videoCap->isOpened()) {
                LOG_INFO("--- Video Capture Info ---");
                LOG_INFO("Width: " + std::to_string(videoCap->get(cv::CAP_PROP_FRAME_WIDTH)));
                LOG_INFO("Height: " + std::to_string(videoCap->get(cv::CAP_PROP_FRAME_HEIGHT)));
                LOG_INFO("FPS: " + std::to_string(videoCap->get(cv::CAP_PROP_FPS)));
                LOG_INFO("Backend: " + videoCap->getBackendName());
                LOG_INFO("--------------------------");
            } else {
                LOG_WARN("ShowVideoCapInfo: VideoCapture pointer is null or not opened.");
            }
        }
        // --- 修改结束 --

        int CheckCameraType(int CameraIndex)
        {
            return VideoCapManager::Default()->CheckCamera(CameraIndex);
        }

        void SetMindCapAttrWarn(bool IsMind)
        {
            VideoCapManager::Default()->SetMindCapAttrWarn(IsMind);
        }

        bool GetMindCapAttrWarn()
        {
            return VideoCapManager::Default()->GetMindCapAttrWarn();
        }
        void SetMicRecorderCallBack(int ModuleNum, void(*dataCallBack)(unsigned char* dataBuf, UINT32 numFramesToRead))
        {
            return g_MoudleVec[ModuleNum]->SetMicRecorderCallBack(dataCallBack);
        }
        void SetCallBack(void (*LogCallBackFunVar)(const char* Message)) {
            Log::Default()->SetCallBack(LogCallBackFunVar);
        }
        void SetCurrentRecoredImg(int ModuleNum, char* Data, int Width, int Height)
        {
            g_MoudleVec[ModuleNum]->SetCurrentRecoredImg(Data, Width, Height);
        }
        void ResizeImg(char* Data, int Width, int Height, int Channels, int OutWidth, int OutHeight, int OutChannels, void (*ResizeDataFun)(char* Data, int OutWidth, int OutHeight,int OutChannels))
        {
            int srcType = 0, cvtCode = -1;
            switch (Channels) {
            case 1: srcType = CV_8UC1; break;
            case 3: srcType = CV_8UC3; break;
            case 4: srcType = CV_8UC4; break;
            default: return;
            }
            
            Mat srcMat(Height, Width, srcType, Data);
            Mat resizedMat;
            resize(srcMat, resizedMat, Size(OutWidth, OutHeight));
            
            Mat finalMat = resizedMat;
            if (Channels != OutChannels) {
                if (Channels == 3 && OutChannels == 4)
                    cvtColor(resizedMat, finalMat, COLOR_RGB2RGBA);
                else if (Channels == 4 && OutChannels == 3)
                    cvtColor(resizedMat, finalMat, COLOR_RGBA2RGB);
                // Other conversions can be added here
            }

            if (ResizeDataFun)
                ResizeDataFun((char*)finalMat.data, finalMat.cols, finalMat.rows, OutChannels);
        }
    }
}
