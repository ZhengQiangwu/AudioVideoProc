#include <opencv2/opencv.hpp>
#include <cinttypes>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/fifo.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavutil/log.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}
extern "C" {
#include <libyuv.h>
}
#include "VideoCapManager.h"
#include "Tool.h"
#include "MediaFrameCapture.h"
#include "Log.h"
#include "AudioVideoProc.h"
#include "AudioVideoProcModule.h"

// Linux平台特定的头文件
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <chrono>

#define IS_NULL(POINTER) (nullptr == (POINTER))
// 安全地获取音频帧大小，避免在上下文未初始化时崩溃
#define AUDIO_FRAME_SIZE (pCodecEncodeCtx_Audio ? pCodecEncodeCtx_Audio->frame_size : 1024)
#define FINALE_WIDTH (resizeWidth==0?videoWidth:resizeWidth)
#define FINALE_HEIGHT (resizeHeight==0?videoHeight:resizeHeight)

using namespace std;
using namespace cv;

// 辅助函数：执行一个shell命令并返回其输出
std::string exec_shell_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
// C++风格的FFmpeg错误信息转换函数
static string av_err2str_cpp(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return string(errbuf);
}

// 假设g_IsDebug在其他地方定义（例如Log.h或Tool.h）
extern bool g_IsDebug;

AudioVideoProcModule::AudioVideoProcModule()
    :isInit(false),
     last_video_pts(-1),
     last_audio_pts(-1)
{}

AudioVideoProcModule::~AudioVideoProcModule() {
    if (isInit)
        UnInit();
}

bool AudioVideoProcModule::Init() {
    if (isInit) {
        LOG_WARN("已装载, 无需再装载");
        return false;
    }
// --- 关键修改: 在所有FFmpeg调用之前，首先设置日志回调 ---
    static bool ffmpeg_global_inited = false;
    if (!ffmpeg_global_inited) {
        LOG_INFO("Init: 正在设置FFmpeg日志回调...");
        av_log_set_level(AV_LOG_WARNING); // 设置我们关心的日志级别
        
        LOG_INFO("Init: 正在执行FFmpeg全局初始化...");
        avdevice_register_all(); 
        avformat_network_init();

        ffmpeg_global_inited = true;
        LOG_INFO("Init: FFmpeg全局初始化完成。");
    }
    // --- 修改结束 ---
    videoCapErr = nullptr;
    audioErr = nullptr;

    //参数初始化
    LOG_INFO("开始初始化参数");
    frameRate = 20;
    bitRate = 4992000;
    gopSize = 50;
    maxBFrames = 0;
    threadCount = 4;
    privDataMap.clear();
    privDataMap["preset"] = "superfast";
    privDataMap["tune"] = "zerolatency";
    nbSample = 48000;
    mixFilterString = "[in0][in1]amix=inputs=2:duration=longest:dropout_transition=0:weights=0.5 2[out]";
    micFilterString = "[in]highpass=200,lowpass=3000,afftdn[out]";
    recordType = RecordType::Stop;
    isRecordVideo = true;
    isRecordInner = true;
    isRecordMic = true;
    cameraNum = -1;
    secondaryCameraNum = -1;
    secondaryWPercent = 25;
    secondaryHPercent = 25;
    micPattern = 0; //默认采用默认麦克风
    micNum = 0;
    isRtmp = false;
    secondaryScreenLocation = 0;//默认不录制次要屏幕
    isAcceptAppendFrame = true;
    recordThread.reset(nullptr);
    recordThread_Video.reset(nullptr);
    recordThread_CapInner.reset(nullptr);
    recordThread_Write.reset(nullptr);
    recordThread_CapMic.reset(nullptr);
    recordThread_FilterMic.reset(nullptr);
    recordThread_Mix.reset(nullptr);
    recordFileName = "output.mp4";
    pushRtmpUrl = "rtmp://127.0.0.1/live";
    recordX = 0;
    recordY = 0;
    recordWidth = 100;
    recordHeight = 100;
    resizeWidth = 0;
    resizeHeight = 0;
    dataCallBackVar = nullptr;
    mFixImgData = nullptr;

    LOG_INFO("正在装载视频");
    if (!InitVideo()) {
        LOG_ERROR("视频装载失败");
        return false;
    }
    //创建临界区对象 (使用pthread_mutex替换)
    LOG_INFO("正在创建临界区对象");
    pthread_mutex_init(&csVideo, nullptr);
    pthread_mutex_init(&csInner, nullptr);
    pthread_mutex_init(&csMic, nullptr);
    pthread_mutex_init(&csMicFilter, nullptr);
    pthread_mutex_init(&csMix, nullptr);
    pthread_mutex_init(&csWrite, nullptr);

// --- 修改开始 ---
    // 初始化新的micMutex
    pthread_mutex_init(&micMutex_pthread, nullptr);
    // --- 修改结束 ---
    
    LOG_INFO("装载成功");
    isInit = true;
    return isInit;
}

void AudioVideoProcModule::UnInit() {
    if (!isInit) {
        LOG_WARN("已卸载,无需再卸载");
        return;
    }

    // 可选但建议：退出前屏蔽回调，避免退出路径继续往 Electron/JS 投递
    try {
        AudioVideoProcNameSpace::SetCallBack(nullptr);
    } catch (...) {
        cout<<"屏蔽回调失败"<<endl;
    }

    // 发出停止信号
    recordType = RecordType::Stop;

    // 线程卸载：为防止 Electron 下 join 卡死/自我 join，这里与 StopRecord 同策略
    LOG_INFO("已发出线程停止请求，正在等待所有线程退出");

    // 把线程句柄挪到本地，避免重复使用
    std::unique_ptr<std::thread> th;
    if (recordThread) {
        th = std::move(recordThread);   // 置空成员
    }

    if (th && th->joinable()) {
        const std::thread::id self_id = std::this_thread::get_id();
        if (th->get_id() == self_id) {
            // 防止在录制线程内部调用 UnInit 导致自我 join
            LOG_WARN("UnInit 在录制线程内被调用，改为 detach 防止自我 join");
            th->detach();
        } else {
            th->join();
        }
    }
    th.reset();

    LOG_INFO("所有线程成功退出");

    // 关闭摄像头（主/副）
    if (cameraNum > -1) {
        LOG_INFO("尝试卸载摄像头(" + to_string(cameraNum) + ")");
        VideoCapManager::Default()->CloseCamera(cameraNum);
        cameraNum = -1;
    }
    if (secondaryCameraNum > -1) {
        LOG_INFO("尝试卸载摄像头(" + to_string(secondaryCameraNum) + ")");
        VideoCapManager::Default()->CloseCamera(secondaryCameraNum);
        secondaryCameraNum = -1;
    }

    // 释放音视频资源（内部应当容错“多次调用/未初始化”）
    UnInitVideo();
    UnInitAudio();
    UnInitAudioMic();

    // 销毁互斥量（确保此时没有线程再使用它们）
    LOG_INFO("尝试销毁临界区对象");
    pthread_mutex_destroy(&csVideo);
    pthread_mutex_destroy(&csInner);
    pthread_mutex_destroy(&csMic);
    pthread_mutex_destroy(&csMicFilter);
    pthread_mutex_destroy(&csMix);
    pthread_mutex_destroy(&csWrite);
    // 你的新增 micMutex
    pthread_mutex_destroy(&micMutex_pthread);

    isInit = false;
    LOG_INFO("卸载成功");
}


bool AudioVideoProcModule::StartRecord()
{
    return StartRecord(isRtmp, frameRate, isRecordVideo, isRecordInner, isRecordMic, secondaryScreenLocation);
}

bool AudioVideoProcModule::StartRecord(bool IsRtmp) {
    return StartRecord(IsRtmp, frameRate, isRecordVideo, isRecordInner, isRecordMic, secondaryScreenLocation);
}

bool AudioVideoProcModule::StartRecord(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation) {
    SetRecordAttr(IsRtmp, FrameRate, IsRecordVideo, IsRecordSound, IsRecordMicro, SecondaryScreenLocation);
    switch (recordType) {
    case RecordType::Stop: {
        LOG_INFO("进行预启动");
        return StartThreadPre();
    }
    case RecordType::Record: {
        LOG_WARN("已经在录制");
        return false;
    }
    case RecordType::Pause: {
        recordType = RecordType::Record;
        LOG_INFO("继续录制");
        return true;
    }
    }
    LOG_ERROR("录制状态异常");
    return false;
}

bool AudioVideoProcModule::ContinueRecord() {
    return StartRecord();
}

bool AudioVideoProcModule::PauseRecord() {
    switch (recordType) {
    case RecordType::Stop: {
        LOG_WARN("未录制，无需暂停");
        return false;
    }
    case RecordType::Record: {
        recordType = RecordType::Pause;
        isCanCap = 0; // 在暂停时重置就绪标志
        LOG_INFO("已发出暂停请求");
        return true;
    }
    case RecordType::Pause: {
        LOG_WARN("已经处于暂停状态");
        return false;
    }
    }
    LOG_ERROR("录制状态异常");
    return false;
}

bool AudioVideoProcModule::StopRecord() {
    // 先把回调清掉（可选但强烈建议）：防止退出阶段继续回调到 Electron/JS
    // 如果你不想清掉，也可以注释掉这行
    try {
        AudioVideoProcNameSpace::SetCallBack(nullptr);
    } catch (...) {
        cout<<"屏蔽回调失败"<<endl;
    }
    // 发出停止信号
    recordType = RecordType::Stop;

    // 幂等：已是停止且没有可用线程
    if (recordType == RecordType::Stop && (!recordThread || !recordThread->joinable())) {
        LOG_WARN("当前未录制或已停止，无需再次停止");
        return false;
    }

    // 发出停止信号
    recordType = RecordType::Stop;
    LOG_INFO("已通知线程停止录制，等待响应中");

    // 将线程句柄移到本地，避免后续重复使用
    std::unique_ptr<std::thread> th;
    if (recordThread) {
        th = std::move(recordThread);   // 置空成员
    }

    if (!th) {
        LOG_INFO("录制线程句柄为空（可能已退出）");
        return true;
    }

    if (th->joinable()) {
        const std::thread::id self_id = std::this_thread::get_id();
        if (th->get_id() == self_id) {
            // 从录制线程内部触发 Stop → 不能 join 自己
            LOG_WARN("StopRecord 在录制线程内被调用，改为 detach 防止自我 join");
            th->detach();
        } else {
            // 正常从外部线程（主线程/Electron 主进程）调用 → 安全 join
            th->join();
        }
    }

    LOG_INFO("录制线程已停止");
    return true;
}


bool AudioVideoProcModule::FinishRecord() {
    return StopRecord();
}

void AudioVideoProcModule::SetVideoCapErrFun(VideoCapErrCallBack VideoCapErrFun) {
    videoCapErr = VideoCapErrFun;
}

void AudioVideoProcModule::SetAudioErrFun(AudioErrCallBack AudioErrFun) {
    audioErr = AudioErrFun;
}

bool AudioVideoProcModule::SetCamera(const int& CameraNum, const int& W, const int& H) {
    if (CameraNum < -1) {
        LOG_ERROR("传入数值过小，应当>=-1");
        return false;
    }
    if (recordType != RecordType::Stop) {
        LOG_WARN("正在录制，无法设置摄像头或桌面");
        return false;
    }
    bool returnVal = false;
    //如果原来是桌面
    if (cameraNum == -1) {
        //并且部署的也是桌面
        if (CameraNum == -1) {
            //那就没有变动
            returnVal = true;
        }
        else {
            //部署的是摄像头
            if (VideoCapManager::Default()->OpenCamera(CameraNum, W, H)) {
                cameraNum = CameraNum;
                returnVal = true;
            }
        }
    }
    //如果原来是摄像头，那么换个摄像头或者回到桌面
    else {
        //如果要换个摄像头，否则就保持桌面
        if (CameraNum != -1) {
            //打开成功
            if (VideoCapManager::Default()->OpenCamera(CameraNum, W, H)) {
                VideoCapManager::Default()->CloseCamera(cameraNum); // 先关闭旧的
                cameraNum = CameraNum;
                returnVal = true;
            }
            else {
                LOG_ERROR("摄像头(" + to_string(CameraNum) + ")打开失败，本次切换取消");
            }
        }
        else {
            //关闭原本摄像头
            VideoCapManager::Default()->CloseCamera(cameraNum);
            //设置桌面
            cameraNum = -1;
            returnVal = true;
        }
    }
    if (returnVal)
        LOG_INFO("设置录制对象已完成，当前录制对象为 " + string((cameraNum == -1) ? "桌面" : "摄像头(" + to_string(cameraNum) + ")"));
    return returnVal;
}

int AudioVideoProcModule::GetCamera()const {
    return cameraNum;
}

bool AudioVideoProcModule::GetCameraWH(const int& CameraNum, int& W, int& H) const
{
    return VideoCapManager::Default()->GetCameraWH(CameraNum, W, H);
}

bool AudioVideoProcModule::SetSecondaryCamera(const int& CameraNum, const int& W, const int& H, const int& WPercent, const int& HPercent) {
    if (CameraNum < -1) {
        LOG_ERROR("传入数值过小，应当>=-1");
        return false;
    }
    secondaryWPercent = WPercent;
    secondaryHPercent = HPercent;
    bool returnVal = false;
    //如果原来是桌面
    if (secondaryCameraNum == -1) {
        if (CameraNum == -1) {
            returnVal = true;
        }
        else {
            if (VideoCapManager::Default()->OpenCamera(CameraNum, W, H)) {
                secondaryCameraNum = CameraNum;
                returnVal = true;
            }
        }
    }
    else {
        if (CameraNum != -1) {
            if (VideoCapManager::Default()->OpenCamera(CameraNum, W, H)) {
                VideoCapManager::Default()->CloseCamera(secondaryCameraNum);
                secondaryCameraNum = CameraNum;
                returnVal = true;
            }
            else {
                LOG_ERROR("摄像头(" + to_string(CameraNum) + ")打开失败，本次切换取消");
            }
        }
        else {
            VideoCapManager::Default()->CloseCamera(secondaryCameraNum);
            secondaryCameraNum = -1;
            returnVal = true;
        }
    }
    if(returnVal)
        LOG_INFO("设置次要录制对象已完成，当前次要录制对象为 " + string((secondaryCameraNum == -1) ? "桌面" : "摄像头(" + to_string(secondaryCameraNum) + ")"));
    return returnVal;
}

int AudioVideoProcModule::GetSecondaryCamera()const {
    return secondaryCameraNum;
}

bool AudioVideoProcModule::SwapRecordSubScreen() {
    if (0 == secondaryScreenLocation) {
        LOG_ERROR("次要屏幕必须要显示，其位置值不能为0");
        return false;
    }
    if (cameraNum == secondaryCameraNum) {
        LOG_INFO("主要屏幕和次要屏幕显示一致，无需切换");
        return true;
    }
    if (cameraNum == -1) {
        LOG_ERROR("主要屏幕必须是摄像头才可切换");
        return false;
    }
    if (secondaryCameraNum == -1) {
        LOG_ERROR("次要屏幕不能是桌面");
        return false;
    }
    std::swap(cameraNum, secondaryCameraNum);
    LOG_INFO("主要画面与次要画面切换完成");
    return true;
}

void AudioVideoProcModule::SetRecordAttr(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation) {
    if (FrameRate < 1 || FrameRate > 120) {
        LOG_ERROR("帧率不得小于1或大于120");
        return;
    }
    isRtmp = IsRtmp;
    frameRate = FrameRate;
    isRecordVideo = IsRecordVideo;
    isRecordInner = IsRecordSound;
    isRecordMic = IsRecordMicro;
    secondaryScreenLocation = SecondaryScreenLocation;
    LOG_INFO("当前设置后属性为:推流(" + to_string(isRtmp) + ") " + "帧率(" + to_string(frameRate) + ") " +
        "视频录制(" + to_string(isRecordVideo) + ") " + "扬声器录制(" + to_string(isRecordInner) + ") " + "麦克风录制(" + to_string(isRecordMic) + ") " +
        "次要屏幕录制状态码(" + to_string(secondaryScreenLocation) + ")");
}

bool AudioVideoProcModule::IsRtmp()const { return isRtmp; }
void AudioVideoProcModule::SetAcceptAppendFrame(bool IsAcceptAppendFrame) { isAcceptAppendFrame = IsAcceptAppendFrame; }
bool AudioVideoProcModule::IsAcceptAppendFrame()const { return isAcceptAppendFrame; }
bool AudioVideoProcModule::IsRecording()const { return isCanCap > 0 && recordType != RecordType::Stop; }
int AudioVideoProcModule::GetRecordAttrFrameRate()const { return frameRate; }
bool AudioVideoProcModule::GetRecordAttrRecordVideo() const { return isRecordVideo; }
bool AudioVideoProcModule::GetRecordAttrRecordSound() const { return isRecordInner; }
bool AudioVideoProcModule::GetRecordAttrIsRecordMicro()const { return isRecordMic; }
string AudioVideoProcModule::GetOutFileName() const { return recordFileName; }
string AudioVideoProcModule::GetRtmpUrl()const { return pushRtmpUrl; }
bool AudioVideoProcModule::GetIsInit()const { return isInit; }
void AudioVideoProcModule::SetBitRate(int BitRate) { bitRate = BitRate; }
int AudioVideoProcModule::GetBitRate()const { return bitRate; }
void AudioVideoProcModule::SetGopSize(int GopSize) { gopSize = GopSize; }
int AudioVideoProcModule::GetGopSize() const { return gopSize; }
void AudioVideoProcModule::SetMaxBFrames(int MaxBFrames) { maxBFrames = MaxBFrames; }
int AudioVideoProcModule::GetMaxBFrames() const { return maxBFrames; }
void AudioVideoProcModule::SetThreadCount(int ThreadCount) { threadCount = ThreadCount; }
int AudioVideoProcModule::GetThreadCount() const { return threadCount; }
void AudioVideoProcModule::SetPrivData(const string& Key, const string& Value) { privDataMap[Key] = Value; }
string AudioVideoProcModule::GetPrivData(const string& Key) const { return privDataMap.count(Key) ? privDataMap.at(Key) : ""; }
void AudioVideoProcModule::SetNbSample(int NbSample) { if (NbSample > 0) nbSample = NbSample; }
int AudioVideoProcModule::GetNbSample()const { return nbSample; }
void AudioVideoProcModule::SetMixFilter(const string& MixFilterString) { mixFilterString = MixFilterString; }
string AudioVideoProcModule::GetMixFilter() const { return mixFilterString; }
void AudioVideoProcModule::SetMicFilter(const string& MicFilterString) { micFilterString = MicFilterString; }
string AudioVideoProcModule::GetMicFilter() const { return micFilterString; }

int AudioVideoProcModule::GetFrameAppendNum() {
    if (frameAppendHistory.empty())
        return 0;
    int all = 0;
    for (int num : frameAppendHistory) {
        all += num;
    }
    return static_cast<int>(all / frameAppendHistory.size());
}

bool AudioVideoProcModule::GetInnerReadyOk() { return pFormatCtxIn_Inner != nullptr; }
bool AudioVideoProcModule::GetMicReadyOk() { return pFormatCtxIn_Mic != nullptr; }
int AudioVideoProcModule::GetMicPattern() const { return micPattern; }

bool AudioVideoProcModule::SetMicPattern(const int& MicPattern, const int& MicNum) {
    if (MicPattern < 0 || MicPattern > 2) {
        LOG_ERROR("MicPattern应该在[0,2]之间，0代表采用默认麦克风 1代表自动选择可用麦克风 2代表采用指定序号的麦克风");
        return false;
    }
    micPattern = MicPattern;
    if (2 == micPattern)
        micNum = MicNum;
    if (InitAudioMic()) {
        InitSwrMic();
        return true;
    }
    return false;
}

void AudioVideoProcModule::SetRecordXYWH(const int& X, const int& Y, const int& Width, const int& Height) {
    recordX = X;
    recordY = Y;
    recordWidth = Width;
    recordHeight = Height;
}

void AudioVideoProcModule::GetRecordXYWH(int& X, int& Y, int& Width, int& Height) {
    X = recordX;
    Y = recordY;
    Width = recordWidth;
    Height = recordHeight;
}

void AudioVideoProcModule::SetRecordFixSize(const int& Width, const int& Height) {
    resizeWidth = Width;
    resizeHeight = Height;
    if (resizeWidth % 2) --resizeWidth;
    if (resizeHeight % 2) --resizeHeight;
}

void AudioVideoProcModule::GetRecordFixSize(int& Width, int& Height) {
    Width = resizeWidth;
    Height = resizeHeight;
}

void AudioVideoProcModule::SetCurrentRecoredImg(char* Data, int Width, int Height)
{
    lock_guard<mutex> lock(mFixImgDataMutex);
    mFixImgData = Data;
    if (Data) {
        mFixImgMat = Mat(Height, Width, CV_8UC4, Data).clone();
        mFixImgMatChange = true;
    }
}

void AudioVideoProcModule::SetMicRecorderCallBack(void(*dataCallBack)(unsigned char* dataBuf, UINT32 numFramesToRead))
{
    dataCallBackVar = dataCallBack;
}

// ...[The rest of the file follows]...
//=========================================主要辅助函数=========================================//

bool AudioVideoProcModule::InitVideo() {
    UnInitVideo();
    return true;
}

void AudioVideoProcModule::UnInitVideo() {
    // Linux下此函数无特定内容
}

// 查找音频流的辅助函数
int AudioVideoProcModule::find_audio_stream(AVFormatContext *fmt_ctx) {
    if (!fmt_ctx) return -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            return i;
        }
    }
    return -1;
}

// 使用FFmpeg libavdevice重写
bool AudioVideoProcModule::InitAudio() {
    UnInitAudio();
    int ret = -1;
    AVInputFormat* informat = nullptr;
    std::string pulse_monitor_source = ""; // 用于存储我们找到的设备名
    
    LOG_INFO("InitAudio: 开始探测可用的系统声音捕获设备...");

    // --- 步骤 1: 尝试自动查找PulseAudio监听源 ---
    try {
        LOG_INFO("InitAudio: 正在执行 'pactl list sources' 来查找监听源...");
        std::string pactl_output = exec_shell_command("pactl list sources");
        std::istringstream stream(pactl_output);
        std::string line;
        std::string current_name = "";
        
        while (std::getline(stream, line)) {
            // 如果找到包含 "Name:" 的行，就记录下设备名
            if (line.find("Name:") != std::string::npos) {
                current_name = line.substr(line.find(":") + 1);
                // 去掉前后的空格
                current_name.erase(0, current_name.find_first_not_of(" \t\n\r"));
                current_name.erase(current_name.find_last_not_of(" \t\n\r") + 1);
            }
            // 如果找到包含 "Monitor of" 的描述，说明我们刚刚记录的那个Name就是正确的
            if (line.find("Monitor of") != std::string::npos) {
                pulse_monitor_source = current_name;
                LOG_INFO("InitAudio: 成功找到监听源设备: " + pulse_monitor_source);
                break; // 找到第一个就够了
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("InitAudio: 执行 'pactl list sources' 失败: " + std::string(e.what()));
    }
    
    // --- 步骤 2: 使用找到的设备名或回退到 'default' ---
    informat = av_find_input_format("pulse");
    if (informat) {
        // 如果我们成功找到了一个精确的监听源名字，就用它
        if (!pulse_monitor_source.empty()) {
            LOG_INFO("InitAudio: 找到了PulseAudio后端，尝试打开精确的监听源设备: " + pulse_monitor_source);
            ret = avformat_open_input(&pFormatCtxIn_Inner, pulse_monitor_source.c_str(), informat, nullptr);
        }

        // 如果没找到精确名字，或者用精确名字打开失败了，再尝试一次 'default'
        if (ret < 0) {
            LOG_WARN("InitAudio: 打开精确监听源失败或未找到，回退到尝试 'default' 设备...");
            ret = avformat_open_input(&pFormatCtxIn_Inner, "default", informat, nullptr);
        }

        if (ret == 0) {
            LOG_INFO("InitAudio: 成功使用 PulseAudio 捕获系统声音。");
        } else {
             LOG_WARN("InitAudio: 最终使用PulseAudio打开设备失败: " + av_err2str_cpp(ret));
        }

    } else {
        LOG_WARN("InitAudio: 当前FFmpeg版本不支持PulseAudio。");
    }

    // --- 步骤 3: 如果PulseAudio彻底失败，才尝试ALSA ---
    if (ret < 0) {
        LOG_WARN("InitAudio: 所有PulseAudio方法均失败，最后尝试ALSA 'default'...");
        informat = av_find_input_format("alsa");
        if(informat) {
            ret = avformat_open_input(&pFormatCtxIn_Inner, "default", informat, nullptr);
        }
    }
    
    // --- 最终检查 ---
    if (ret < 0) {
        LOG_ERROR("InitAudio: 尝试了所有可用方法后，仍然无法打开任何系统声音捕获设备。");
        if(audioErr) audioErr(3);
        return false;
    }

    // 如果成功打开了任何一个设备，就继续后续的初始化
    ret = avformat_find_stream_info(pFormatCtxIn_Inner, nullptr);
    if (ret < 0) {
        LOG_ERROR("InitAudio: 查找流信息失败: " + av_err2str_cpp(ret));
        UnInitAudio();
        return false;
    }

    streamIndexIn_Inner = find_audio_stream(pFormatCtxIn_Inner);
    if (streamIndexIn_Inner < 0) {
        LOG_ERROR("InitAudio: 在捕获到的流中找不到音频轨道。");
        UnInitAudio();
        return false;
    }

    AVStream* stream = pFormatCtxIn_Inner->streams[streamIndexIn_Inner];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        LOG_ERROR("InitAudio: 找不到解码器。");
        UnInitAudio();
        return false;
    }
    pCodecDecodeCtx_Inner = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(pCodecDecodeCtx_Inner, stream->codecpar);
    ret = avcodec_open2(pCodecDecodeCtx_Inner, decoder, nullptr);
    if (ret < 0) {
        LOG_ERROR("InitAudio: 打开解码器失败: " + av_err2str_cpp(ret));
        UnInitAudio();
        return false;
    }
    
    LOG_INFO("InitAudio: 系统声音捕获模块初始化成功。");
    return true;
}

void AudioVideoProcModule::UnInitAudio() {
    if (pCodecDecodeCtx_Inner) {
        avcodec_free_context(&pCodecDecodeCtx_Inner);
        pCodecDecodeCtx_Inner = nullptr;
    }
    if (pFormatCtxIn_Inner) {
        avformat_close_input(&pFormatCtxIn_Inner);
        pFormatCtxIn_Inner = nullptr;
    }
    streamIndexIn_Inner = -1;
}

bool AudioVideoProcModule::InitAudioMic() {
// --- 步骤 1: 加锁 ---
    pthread_mutex_lock(&micMutex_pthread);

    // 步骤 2: 清理旧资源 (将 UnInitAudioMic 的逻辑直接放在这里，避免死锁)
    if (mic_arecord_pid > 0) {
        kill(mic_arecord_pid, SIGTERM);
        waitpid(mic_arecord_pid, NULL, 0); // 阻塞式等待确保进程退出
        LOG_INFO("旧的 arecord 子进程 (PID: " + to_string(mic_arecord_pid) + ") 已被终止。");
        mic_arecord_pid = -1;
    }
    if (pCodecDecodeCtx_Mic) {
        avcodec_free_context(&pCodecDecodeCtx_Mic);
        pCodecDecodeCtx_Mic = nullptr;
    }
    if (pFormatCtxIn_Mic) {
        avformat_close_input(&pFormatCtxIn_Mic);
        pFormatCtxIn_Mic = nullptr;
    }
    streamIndexIn_Mic = -1;
    
    // 步骤 3: 准备并执行 arecord 命令
    const char* device = "hw:0,0"; // 你可以根据需要修改此默认设备
    if (micPattern == 2) {
        // (这部分逻辑可以后续添加，暂时使用默认)
        LOG_INFO("InitAudioMic: 指定麦克风序号 " + to_string(micNum) + " (暂未实现，使用默认设备 " + device + ")");
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "arecord -f S16_LE -r 48000 -c 2 -D %s", device);
    LOG_INFO("InitAudioMic: 准备执行管道命令: " + string(cmd));

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_ERROR("InitAudioMic: 创建麦克风管道失败 (pipe error)。");
        pthread_mutex_unlock(&micMutex_pthread); // 在返回前解锁
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) { // fork 失败
        LOG_ERROR("InitAudioMic: 创建arecord子进程失败 (fork error)。");
        close(pipefd[0]);
        close(pipefd[1]);
        pthread_mutex_unlock(&micMutex_pthread); // 在返回前解锁
        return false;
    }

    if (pid == 0) { // --- 子进程的代码 ---
        close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]); 
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        exit(127);
    }

    // --- 父进程的代码 ---
    mic_arecord_pid = pid;
    close(pipefd[1]); 
    
    // --- 修改开始: 将 "fd:%d" 改为 "pipe:%d" ---
    char input_pipe_url[64];
    snprintf(input_pipe_url, sizeof(input_pipe_url), "pipe:%d", pipefd[0]);
    LOG_INFO("InitAudioMic: 尝试使用管道URL: " + string(input_pipe_url));
    // --- 修改结束 ---
    
    pFormatCtxIn_Mic = avformat_alloc_context();
    if (!pFormatCtxIn_Mic) {
        LOG_ERROR("InitAudioMic: avformat_alloc_context 失败。");
        close(pipefd[0]);
        // 调用 UnInitAudioMic 来清理子进程
        pthread_mutex_unlock(&micMutex_pthread); // 解锁后才能调用
        UnInitAudioMic();
        return false;
    }
    
    AVInputFormat* informat = av_find_input_format("s16le"); 
    if (informat == nullptr) {
        // 如果失败，打印明确的错误信息并退出
        LOG_ERROR("InitAudioMic 失败: av_find_input_format 无法找到 's16le' demuxer。");
        LOG_ERROR("这通常意味着链接的FFmpeg库版本不完整或已损坏。");
        UnInitAudioMic();
        return false;
    }else{
        LOG_ERROR("找到 's16le' demuxer。");

}
    AVDictionary* options = nullptr;
    av_dict_set(&options, "ar", "48000", 0);
    av_dict_set(&options, "ac", "2", 0);

     // --- 修改开始: 使用新的URL ---
    int ret = avformat_open_input(&pFormatCtxIn_Mic, input_pipe_url, informat, &options);

    //close(pipefd[0]); 
    if (options) {
        av_dict_free(&options);
    }

    if (ret < 0) {
        LOG_ERROR("从管道打开FFmpeg麦克风输入失败: " + av_err2str_cpp(ret));
        close(pipefd[0]); // 如果 open 失败，我们需要手动关闭它
        pthread_mutex_unlock(&micMutex_pthread); // 解锁后才能调用
        UnInitAudioMic(); 
        return false;
    }
    
    // (后续逻辑保持不变)
    ret = avformat_find_stream_info(pFormatCtxIn_Mic, nullptr);
    if (ret < 0) {
        LOG_ERROR("查找麦克风流信息失败: " + av_err2str_cpp(ret));
        UnInitAudioMic();
        return false;
    }
    streamIndexIn_Mic = find_audio_stream(pFormatCtxIn_Mic);
    if (streamIndexIn_Mic < 0) {
        LOG_ERROR("在麦克风上找不到音频流");
        UnInitAudioMic();
        return false;
    }
    const AVCodec* decoder = avcodec_find_decoder(pFormatCtxIn_Mic->streams[streamIndexIn_Mic]->codecpar->codec_id);
    pCodecDecodeCtx_Mic = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(pCodecDecodeCtx_Mic, pFormatCtxIn_Mic->streams[streamIndexIn_Mic]->codecpar);
    ret = avcodec_open2(pCodecDecodeCtx_Mic, decoder, nullptr);
    if (ret < 0) {
        LOG_ERROR("打开麦克风解码器失败: " + av_err2str_cpp(ret));
        UnInitAudioMic();
        return false;
    }
    
    LOG_INFO("成功通过 arecord 管道初始化麦克风。");
    pthread_mutex_unlock(&micMutex_pthread); // 在成功返回前解锁
    return true;
}

void AudioVideoProcModule::UnInitAudioMic() {
    // 加锁
    pthread_mutex_lock(&micMutex_pthread);
    
    // 终止 arecord 子进程
    if (mic_arecord_pid > 0) {
        kill(mic_arecord_pid, SIGTERM);
        int status;
        waitpid(mic_arecord_pid, &status, 0); // 阻塞式等待
        LOG_INFO("arecord 子进程 (PID: " + to_string(mic_arecord_pid) + ") 已被终止。");
        mic_arecord_pid = -1;
    }

    // 释放FFmpeg资源
    if (pCodecDecodeCtx_Mic) {
        avcodec_free_context(&pCodecDecodeCtx_Mic);
        pCodecDecodeCtx_Mic = nullptr;
    }
    if (pFormatCtxIn_Mic) {
        avformat_close_input(&pFormatCtxIn_Mic);
        pFormatCtxIn_Mic = nullptr;
    }
    streamIndexIn_Mic = -1;
    
    // 解锁
    pthread_mutex_unlock(&micMutex_pthread);
}

bool AudioVideoProcModule::StartThreadPre() {
    isCanCap = 0;
    
    LOG_INFO("StartThreadPre: 函数开始执行。");

    //重新获取当前的视频宽高
    {
        LOG_INFO("StartThreadPre: 开始获取视频画面宽高。");

        // 使用X11获取屏幕尺寸
        Display* display = XOpenDisplay(NULL);
        if (!display) {
            LOG_ERROR("StartThreadPre 失败: 无法打开X Display获取屏幕尺寸。");
            return false; // 直接返回，避免段错误
        }
        
        LOG_INFO("StartThreadPre: X Display 打开成功。");

        int screen_num = DefaultScreen(display);
        screenW = DisplayWidth(display, screen_num);
        screenH = DisplayHeight(display, screen_num);
        XCloseDisplay(display);
        
        LOG_INFO("StartThreadPre: 获取到屏幕尺寸 " + to_string(screenW) + "x" + to_string(screenH));

        if (-1 != cameraNum) {
            LOG_INFO("StartThreadPre: 正在为摄像头 " + to_string(cameraNum) + " 计算尺寸。");
            if (!VideoCapManager::Default()->GetCameraWH(cameraNum, videoWidth, videoHeight)) {
                LOG_WARN("StartThreadPre: 摄像头宽高获取失败，即将回到桌面录制");
                VideoCapManager::Default()->CloseCamera(cameraNum);
                cameraNum = -1;
                videoWidth = recordWidth > 0 ? recordWidth : screenW;
                videoHeight = recordHeight > 0 ? recordHeight : screenH;
            } else {
                // 数据矫正
                if (recordX < 0 || recordX >= videoWidth) recordX = 0;
                if (recordY < 0 || recordY >= videoHeight) recordY = 0;
                if (recordWidth <= 0) recordWidth = INT32_MAX;
                if (recordHeight <= 0) recordHeight = INT32_MAX;
                videoWidth = (recordX + recordWidth > videoWidth) ? (videoWidth - recordX) : recordWidth;
                videoHeight = (recordY + recordHeight > videoHeight) ? (videoHeight - recordY) : recordHeight;
            }
        }
        if (-1 == cameraNum) {
             LOG_INFO("StartThreadPre: 正在为桌面计算尺寸。");
             videoWidth = recordWidth > 0 ? recordWidth : screenW;
             videoHeight = recordHeight > 0 ? recordHeight : screenH;
        }

        // 确保宽高为偶数
        if (videoWidth % 2 != 0) videoWidth--;
        if (videoHeight % 2 != 0) videoHeight--;
        // 这样 FINALE_WIDTH/HEIGHT 宏才能获取到正确的值
        resizeWidth = videoWidth;
        resizeHeight = videoHeight;
    	
        LOG_INFO("recordX=" + to_string(recordX));
        LOG_INFO("recordY=" + to_string(recordY));
        LOG_INFO("recordWidth=" + to_string(recordWidth));
        LOG_INFO("recordHeight=" + to_string(recordHeight));
        LOG_INFO("videoWidth=" + to_string(videoWidth));
        LOG_INFO("videoHeight=" + to_string(videoHeight));
        LOG_INFO("视频画面宽高已确定并固定为" + to_string(videoWidth) + "x" + to_string(videoHeight));
    }

    // --- 关键修改: 将所有输入初始化提到最前面 ---

    // 步骤 1: 初始化所有需要的音频输入设备
    if (isRecordInner) {
        LOG_INFO("StartThreadPre: 准备初始化系统声音...");
        if (!InitAudio()) {
            LOG_ERROR("StartThreadPre: InitAudio() 失败。");
            goto END_ERR; 
        }
    }
    if (isRecordMic) {
        LOG_INFO("StartThreadPre: 準備初始化麦克风...");
        if (!InitAudioMic()) {
            LOG_ERROR("StartThreadPre: InitAudioMic() 失败。");
            goto END_ERR;
        }
    }
    
    // 步骤 2: 在所有输入都成功后，再初始化输出环境
    LOG_INFO("尝试打开输出环境");
    if (OpenOutPut() != 0) {
        LOG_ERROR("输出环境打开失败");
        goto END_ERR;
    }

    // 步骤 3: 在输入和输出都就绪后，初始化连接它们的组件（重采样器、过滤器）
    if (isRecordInner) {
        LOG_INFO("StartThreadPre: 准备初始化扬声器重采样...");
        if (InitSwrInner() != 0) {
            LOG_ERROR("StartThreadPre: InitSwrInner() 失败。");
            goto END_ERR;
        }
    }
    if (isRecordMic) {
        LOG_INFO("StartThreadPre: 准备初始化麦克风重采样...");
        if (InitSwrMic() != 0) {
            LOG_ERROR("StartThreadPre: InitSwrMic() 失败。");
            goto END_ERR;
        }
    }

    if (isRecordInner || isRecordMic) {
        LOG_INFO("尝试预设混合过滤器");
        if (InitFilter() != 0) {
            LOG_ERROR("预设混合过滤器失败");
            goto END_ERR;
        }
    }
    if (isRecordMic) {
        LOG_INFO("尝试预设麦克风过滤器");
        if (InitFilterMic() != 0) {
            LOG_ERROR("预设麦克风过滤器失败");
            goto END_ERR;
        }
    }
    // --- 修改结束 ---

    if (isRecordInner || isRecordMic) {
        LOG_INFO("尝试装载音频缓冲区");
        if (InitFifo() != 0) {
            LOG_ERROR("装载音频缓冲区失败");
            goto END_ERR;
        }
    } else {
        LOG_INFO("未启用任何音频源，跳过音频缓冲区初始化。");
    }

    LOG_INFO("尝试停止录制线程");
    recordType = RecordType::Stop;
    if (recordThread && recordThread->joinable()) {
        recordThread->join();
    }
    LOG_INFO("即将设置新的录制线程");
    recordType = RecordType::Record;
    recordThread.reset(new thread(&AudioVideoProcModule::RecordThreadRun, this));
    //recording_start_time = std::chrono::steady_clock::now(); // <-- 记录开始时间
    LOG_INFO("录制线程的预开启完成");
    return true;

END_ERR:
    // 清理函数的顺序也最好和初始化相反
    UnInitFifo();
    if (isRecordMic) { UnInitFilterMic(); }
    if (isRecordInner || isRecordMic) { UnInitFilter(); }
    UnInitSwrInner();
    UnInitSwrMic();
    CloseOutPut(); // <-- 在 UnInitAudio 之前
    UnInitAudio();
    UnInitAudioMic();
    LOG_ERROR("录制线程的预开启失败");
    return false;
}

void AudioVideoProcModule::RecordThreadRun() {
    //准备记录总写入帧数
    allAudioFrame = 0;
    allVideoFrame = 0;
    LOG_INFO("录制线程就绪，正在展开子线程");
    if(isRecordVideo) recordThread_Video.reset(new thread(&AudioVideoProcModule::RecordThreadRun_Video, this));
    if(isRecordInner) recordThread_CapInner.reset(new thread(&AudioVideoProcModule::RecordThreadRun_CapInner, this));
    if(isRecordMic) {
        recordThread_CapMic.reset(new thread(&AudioVideoProcModule::RecordThreadRun_CapMic, this));
        recordThread_FilterMic.reset(new thread(&AudioVideoProcModule::RecordThreadRun_FilterMic, this));
    }else {
        LOG_INFO("麦克风录制已禁用，跳过相关线程的创建。");
    }
    if(isRecordInner || isRecordMic) {
        recordThread_Mix.reset(new thread(&AudioVideoProcModule::RecordThreadRun_Mix, this));
        recordThread_Write.reset(new thread(&AudioVideoProcModule::RecordThreadRun_Write, this));
    }

    LOG_INFO("录制线程展开子线程完毕");
    while (recordType != RecordType::Stop) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    LOG_INFO("录制线程收到停止信号，即将回收所有子线程资源");
    //回收资源
    if (recordThread_Video && recordThread_Video->joinable()) {
        recordThread_Video->join();
    }
    if (recordThread_CapInner && recordThread_CapInner->joinable()) {//
        recordThread_CapInner->join();
    }
    if (recordThread_CapMic && recordThread_CapMic->joinable()) {//
        recordThread_CapMic->join();
    }
    if (recordThread_FilterMic && recordThread_FilterMic->joinable()) {//
        recordThread_FilterMic->join();
    }
    if (recordThread_Mix && recordThread_Mix->joinable()) {//
        recordThread_Mix->join();
    }
    if (recordThread_Write && recordThread_Write->joinable()) {//
        recordThread_Write->join();
    }

    recordThread_Video.reset();
    recordThread_CapInner.reset();
    recordThread_CapMic.reset();
    recordThread_FilterMic.reset();
    recordThread_Mix.reset();
    recordThread_Write.reset();

    LOG_INFO("录制线程已完成所有子线程资源的回收，开始卸载录制变量");
    UnInitFifo();
    UnInitFilterMic();
    UnInitFilter();
    UnInitSwrInner();
    UnInitSwrMic();
    CloseOutPut();
    UnInitAudio();
    UnInitAudioMic();
    LOG_INFO("录制线程成功退出");
    LOG_INFO("本次共录制视频帧:" + to_string(allVideoFrame));
    LOG_INFO("本次共录制音频帧:" + to_string(allAudioFrame));
}

void AudioVideoProcModule::RecordThreadRun_Video() {
    LOG_INFO("录制子线程-视频就绪");

    // X11 屏幕捕获相关变量
    Display* display = nullptr;
    Window root_window;
    XImage* ximage = nullptr;

    const int videoFixWidth = FINALE_WIDTH;
    const int videoFixHeight = FINALE_HEIGHT;
    const long videoFixWH = videoFixWidth * videoFixHeight;
    const long videoFixWHOne = videoFixWH / 4;

    // --- 修改：明确变量用途 ---
    // colorMat 将作为送入libyuv前，统一格式(BGRA)的容器
    Mat colorMat;
    // --- 修改结束 ---
    Mat secondaryColorMat;
    Mat blackMat = Mat::zeros(videoFixHeight, videoFixWidth, CV_8UC4);

    // 使用智能指针确保内存在任何情况下都能被释放
    unique_ptr<uchar[]> ybuffer(new uchar[videoFixWH]);
    unique_ptr<uchar[]> ubuffer(new uchar[videoFixWHOne]);
    unique_ptr<uchar[]> vbuffer(new uchar[videoFixWHOne]);
    unique_ptr<uchar[]> frameBufferBlack(new uchar[videoFixWH * 3 / 2]);
    unique_ptr<uchar[]> frameBufferColor(new uchar[videoFixWH * 3 / 2]);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* yuvFrame = av_frame_alloc();

    // --- 你的原始代码 ---
    bool isCapPreNot = true;
    // --- 修改开始: 为视频线程创建自己的起始时间 ---
    std::chrono::steady_clock::time_point video_start_time;
    // --- 修改结束 ---
    auto dwBeginTime = chrono::steady_clock::now();
    //long long frameCount = 0;
    const chrono::milliseconds fps_duration((long long)(1000.0 / frameRate));
    int capErrNum = 0;//无异常
    bool isFixImgYuv = false;

    if (IS_NULL(pkt) || IS_NULL(yuvFrame)) {
        LOG_ERROR("分配pkt或yuvFrame内存失败");
        goto END;
    }
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = videoFixWidth;
    yuvFrame->height = videoFixHeight;
    if (av_frame_get_buffer(yuvFrame, 0) < 0) {
        LOG_ERROR("分配YUV帧的内存失败");
        goto END;
    }

    //转换黑屏帧
    libyuv::ARGBToI420(blackMat.data, videoFixWidth * 4, ybuffer.get(), videoFixWidth, ubuffer.get(), (videoFixWidth + 1) / 2, vbuffer.get(), (videoFixWidth + 1) / 2, videoFixWidth, videoFixHeight);
    //将上面的yuv数据保存到一个数组里面组成一帧yuv I420 数据 分辨率为w*h
    memcpy(frameBufferBlack.get(), ybuffer.get(), videoFixWH);
    memcpy(frameBufferBlack.get() + videoFixWH, ubuffer.get(), videoFixWHOne);
    memcpy(frameBufferBlack.get() + videoFixWH + videoFixWHOne, vbuffer.get(), videoFixWHOne);

    LOG_INFO("采集速率(每多少毫秒一帧):" + to_string(1000.0 / frameRate));
    LOG_INFO("videoWidth值为:" + to_string(videoWidth));
    LOG_INFO("videoHeight值为:" + to_string(videoHeight));
    LOG_INFO("videoFixWidth值为:" + to_string(videoFixWidth));
    LOG_INFO("videoFixHeight值为:" + to_string(videoFixHeight));

    while (recordType != RecordType::Stop) {
        while (recordType == RecordType::Record) {
            if (isCapPreNot) {
                LOG_INFO("视频已就绪，等待其余准备完毕");
                ++isCanCap;
                while (isCanCap < (isRecordVideo + isRecordInner + isRecordMic) && recordType != RecordType::Stop) {
                    this_thread::sleep_for(chrono::milliseconds(1));
                }
                //放在这里是为了初始化以及防止暂停后再启动的疯狂补帧
                dwBeginTime = chrono::steady_clock::now() - fps_duration;
                LOG_INFO("视频开始录制");
                // --- 修改开始: 在真正开始录制时，记录视频的起始时间 ---
                video_start_time = std::chrono::steady_clock::now();
                // --- 修改结束 ---
                isCapPreNot = false;
            }

            auto frameStartTime = chrono::steady_clock::now();
            LOG_DEBUG("开始采集一帧视频");
            bool isBlackMatUsed = true;

            if (isRecordVideo) {
                isFixImgYuv = false; // 你的mFixImgData逻辑目前未启用，保持此行为
                if (mFixImgData) {
                    // (你的 mFixImgData 逻辑保持原样)
                    lock_guard<mutex> lock(mFixImgDataMutex);
                    if (mFixImgData) {
                        if (mFixImgMatChange) {
                            colorMat = mFixImgMat(Range(recordY, recordY + videoHeight), Range(recordX, recordX + videoWidth));
                            if (colorMat.cols != videoFixWidth || colorMat.rows != videoFixHeight) {
                                resize(colorMat, colorMat, Size(videoFixWidth, videoFixHeight));
                            }
                            libyuv::ARGBToI420(colorMat.data, videoFixWidth * 4, ybuffer.get(), videoFixWidth, ubuffer.get(), (videoFixWidth + 1) / 2, vbuffer.get(), (videoFixWidth + 1) / 2, videoFixWidth, videoFixHeight);
                            memcpy(frameBufferColor.get(), ybuffer.get(), videoFixWH);
                            memcpy(frameBufferColor.get() + videoFixWH, ubuffer.get(), videoFixWHOne);
                            memcpy(frameBufferColor.get() + videoFixWH + videoFixWHOne, vbuffer.get(), videoFixWHOne);
                            av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, frameBufferColor.get(), AV_PIX_FMT_YUV420P, videoFixWidth, videoFixHeight, 1);
                            mFixImgMatChange = false;
                        }
                        isFixImgYuv = true;
                        isBlackMatUsed = false;
                    }
                }

                if (false == isFixImgYuv) {
                    //处理主要画面
                    if (-1 == cameraNum) {
                        //截屏获取 (X11)
                        if (!display) {
                            display = XOpenDisplay(NULL);
                            if (display) root_window = DefaultRootWindow(display);
                        }
                        if (display) {
                            ximage = XGetImage(display, root_window, recordX, recordY, videoWidth, videoHeight, AllPlanes, ZPixmap);
                            if (ximage) {
                                // XImage data is typically BGRA, which is compatible with CV_8UC4
                                colorMat = Mat(videoHeight, videoWidth, CV_8UC4, ximage->data);
                                if (colorMat.cols != videoFixWidth || colorMat.rows != videoFixHeight) {
                                    resize(colorMat, colorMat, Size(videoFixWidth, videoFixHeight));
                                }
                                capErrNum = 0;
                                isBlackMatUsed = false;
                            }
                        }
                    } else { // Camera capture
                        // --- 修改开始: 增加安全检查和统一数据格式 ---
                        Mat cameraFrame; // 创建一个临时的Mat来接收摄像头的原始BGR数据
                        if (VideoCapManager::Default()->GetMatFromCamera(cameraNum, videoWidth, videoHeight, cameraFrame)) {
                            // 安全检查：确保从摄像头获取的帧不是空的
                            if (!cameraFrame.empty()) {
                                // 将摄像头的 BGR (3通道) 转换为 BGRA (4通道)
                                cvtColor(cameraFrame, colorMat, COLOR_BGR2BGRA);
                                
                                // 现在 colorMat 是BGRA格式，可以继续处理
                                if (colorMat.cols != videoFixWidth || colorMat.rows != videoFixHeight) {
                                    resize(colorMat, colorMat, Size(videoFixWidth, videoFixHeight));
                                }
                                
                                if (capErrNum == 1) VideoCapManager::Default()->OpenCamera(cameraNum);
                                capErrNum = 0;
                                isBlackMatUsed = false;
                            } else {
                                LOG_WARN("摄像头(" + to_string(cameraNum) + ") GetMatFromCamera返回true但Mat为空，使用黑帧。");
                                // isBlackMatUsed 保持为 true
                            }
                        } else {
                            if (0 == capErrNum) {
                                LOG_WARN("摄像头获取Mat失败，即将释放摄像头");
                                VideoCapManager::Default()->CloseCamera(cameraNum);
                                capErrNum = 1;
                                if (videoCapErr) videoCapErr(capErrNum);
                            }
                            // isBlackMatUsed 保持为 true
                        }
                        // --- 修改结束 ---
                    }
                    //如果次要画面位置不为0 (logic remains the same, assuming secondary capture works)
                    // ... [secondary screen logic] ...
                }
            }

            //拷贝YUV数据到帧
            if (isBlackMatUsed) {
                LOG_DEBUG("拷贝黑帧");
                av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, frameBufferBlack.get(), AV_PIX_FMT_YUV420P, videoFixWidth, videoFixHeight, 1);
            } else if (!isFixImgYuv) {
                LOG_DEBUG("拷贝彩帧");
                // --- 修改开始: 增加最终极的安全检查 ---
                if (!colorMat.empty() && colorMat.isContinuous() && colorMat.channels() == 4) {
                    // 根据你之前的测试，桌面录制正常，说明X11的BGRA数据与BGRAToI420是匹配的
                    // 现在我们已经将摄像头数据也统一为了BGRA，所以这里应该可以正常工作
                    libyuv::ARGBToI420(colorMat.data, colorMat.step, 
                                       ybuffer.get(), videoFixWidth, 
                                       ubuffer.get(), (videoFixWidth + 1) / 2, 
                                       vbuffer.get(), (videoFixWidth + 1) / 2, 
                                       videoFixWidth, videoFixHeight);

                    memcpy(frameBufferColor.get(), ybuffer.get(), videoFixWH);
                    memcpy(frameBufferColor.get() + videoFixWH, ubuffer.get(), videoFixWHOne);
                    memcpy(frameBufferColor.get() + videoFixWH + videoFixWHOne, vbuffer.get(), videoFixWHOne);
                    av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, frameBufferColor.get(), AV_PIX_FMT_YUV420P, videoFixWidth, videoFixHeight, 1);
                } else {
                    LOG_ERROR("colorMat 为空、内存不连续或通道数不为4，无法进行YUV转换！使用黑帧替代。");
                    av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, frameBufferBlack.get(), AV_PIX_FMT_YUV420P, videoFixWidth, videoFixHeight, 1);
                }
                // --- 修改结束 ---
            }
            if (ximage) { XDestroyImage(ximage); ximage = nullptr; }

            int handleNum = -1;
            while (chrono::steady_clock::now() >= dwBeginTime && recordType == RecordType::Record) {
                if (handleNum >= 0 && !isAcceptAppendFrame) break;
                
                handleNum++;
                dwBeginTime += fps_duration;
                // --- 修改开始: 基于视频自己的起始时间来计算PTS ---
                auto now = std::chrono::steady_clock::now();
                auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(now - video_start_time).count();
                int64_t current_pts = av_rescale_q(elapsed_time, {1, 1000000}, pCodecEncodeCtx_Video->time_base);

                // --- 修改开始: 确保PTS严格递增 ---
                if (current_pts <= last_video_pts) {
                    // 如果当前计算出的PTS不比上一个大，就手动加1
                    current_pts = last_video_pts + 1;
                }
                yuvFrame->pts = current_pts;
                last_video_pts = current_pts; // 更新上一个PTS
                 // --- 修改结束 ---

                int ret = avcodec_send_frame(pCodecEncodeCtx_Video, yuvFrame);
                while (ret >= 0) {
                    ret = avcodec_receive_packet(pCodecEncodeCtx_Video, pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    else if (ret < 0) {
                        // 添加错误日志
                        LOG_WARN("avcodec_receive_packet 失败: " + av_err2str_cpp(ret));
                        break;
                    }
                    
                    av_packet_rescale_ts(pkt, pCodecEncodeCtx_Video->time_base, pStream_Video->time_base);
                    // --- 视频流独立归一化 ---
                    {
                        std::lock_guard<std::mutex> lock(video_pts_mutex);
                        if (video_first_dts == AV_NOPTS_VALUE) {
                            video_first_dts = pkt->dts;
                            LOG_INFO("视频流的第一个DTS被记录为: " + to_string(video_first_dts));
                        }
                    }
                    pkt->pts -= video_first_dts;
                    pkt->dts -= video_first_dts;
                    // --- 修改结束 ---
                    pkt->stream_index = streamIndex_Video;
                    
                    pthread_mutex_lock(&csWrite);
                    LOG_DEBUG("正在写入一个视频包，pts: " + to_string(pkt->pts)); 
                    av_interleaved_write_frame(pFormatCtxOut, pkt);
                    pthread_mutex_unlock(&csWrite);
                    allVideoFrame++;
                    av_packet_unref(pkt);
                }
            }
            if (handleNum > 0) {
                LOG_DEBUG("正常补帧:" + std::to_string(handleNum));
                frameAppendHistory.push_back(handleNum);
                if (frameAppendHistory.size() > 30) frameAppendHistory.pop_front();
            }

            auto sleep_for = dwBeginTime - chrono::steady_clock::now();
            if (sleep_for > chrono::milliseconds(1)) {
                this_thread::sleep_for(sleep_for);
            }
        }
        isCapPreNot = true;
        this_thread::sleep_for(chrono::milliseconds(100));
    }

END:
    LOG_INFO("录制子线程-视频即将停止并回收资源");
    if (display) { XCloseDisplay(display); }
    if (yuvFrame) av_frame_free(&yuvFrame);
    if (pkt) av_packet_free(&pkt);
    LOG_INFO("录制子线程-视频已退出");
}

// ... (The rest of the file follows) ...
void AudioVideoProcModule::RecordThreadRun_CapInner() {
    LOG_INFO("录制子线程-扬声器采集就绪");

    AVPacket* packet = av_packet_alloc();
    AVFrame* decoded_frame = av_frame_alloc();
    AVFrame* resampled_frame = av_frame_alloc();
    // --- 修改开始: 解决goto问题 ---
    bool isCapPreNot = true;
    // --- 修改结束 ---
    
    if (!packet || !decoded_frame || !resampled_frame) {
        LOG_ERROR("无法为音频采集分配内存");
        goto END;
    }

    while (recordType != RecordType::Stop) {
        while (recordType == RecordType::Record && isRecordInner) {
            //采集前设备检查
            if (!pFormatCtxIn_Inner) {
                if (InitAudio()) {
                    InitSwrInner();
                } else {
                    LOG_WARN("扬声器打开失败，将禁用扬声器录制");
                    isRecordInner = false;
                    if (audioErr) audioErr(3);
                    break;
                }
            }

            if (isCapPreNot) {
                if (av_audio_fifo_size(pAudioFifo_Inner) > 0) {
                    av_audio_fifo_drain(pAudioFifo_Inner, av_audio_fifo_size(pAudioFifo_Inner));
                    LOG_INFO("由于未录制，扬声器队列缓存已清空");
                }
                LOG_INFO("音频已就绪，等待其余准备完毕");
                ++isCanCap;
                while (isCanCap < (isRecordVideo + isRecordInner + isRecordMic) && recordType != RecordType::Stop) {
                    this_thread::sleep_for(chrono::milliseconds(1));
                }
                LOG_INFO("音频开始录制");
                isCapPreNot = false;
            }

            if (av_read_frame(pFormatCtxIn_Inner, packet) >= 0) {
                if (packet->stream_index == streamIndexIn_Inner) {
                    if (avcodec_send_packet(pCodecDecodeCtx_Inner, packet) >= 0) {
                        while (avcodec_receive_frame(pCodecDecodeCtx_Inner, decoded_frame) >= 0) {
                        	// --- 修改开始: 强制设置声道布局 ---
			    if (decoded_frame->channel_layout == 0) {
				//LOG_WARN("Decoded audio frame has no channel layout, forcing stereo.");
				decoded_frame->channel_layout = AV_CH_LAYOUT_STEREO;
			    }
                            if (pSwrCtx_Inner) {
                                resampled_frame->sample_rate = nbSample;
                                resampled_frame->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                                resampled_frame->format = pCodecEncodeCtx_Audio->sample_fmt;
                                // --- 修改开始: 添加返回值检查 ---
				 int ret = swr_convert_frame(pSwrCtx_Inner, resampled_frame, decoded_frame);
				 if (ret < 0) {
				     LOG_ERROR("swr_convert_frame in CapInner failed: " + av_err2str_cpp(ret));
				     // 发生错误，可能需要跳出循环
				     goto frame_loop_end; // 使用goto跳到循环末尾的清理部分
				 }
				 // --- 修改结束 ---
                                
                                pthread_mutex_lock(&csInner);
                                av_audio_fifo_write(pAudioFifo_Inner, (void**)resampled_frame->data, resampled_frame->nb_samples);
                                pthread_mutex_unlock(&csInner);
                            }
                        }
                        frame_loop_end: // 标签
		  	 av_packet_unref(packet);
                    }
                }
                av_packet_unref(packet);
            } else {
                LOG_WARN("读取扬声器数据失败，将禁用扬声器录制");
                isRecordInner = false;
                if (audioErr) audioErr(1);
            }
        }
        isCapPreNot = true;
        this_thread::sleep_for(chrono::milliseconds(100));
    }
END:
    LOG_INFO("录制子线程-扬声器采集即将停止并回收资源");
    if (decoded_frame) av_frame_free(&decoded_frame);
    if (resampled_frame) av_frame_free(&resampled_frame);
    if (packet) av_packet_free(&packet);
    LOG_INFO("录制子线程-扬声器采集已退出");
}

void AudioVideoProcModule::RecordThreadRun_CapMic() {
    LOG_INFO("录制子线程-麦克风采集就绪");
    
    AVPacket* packet = av_packet_alloc();
    AVFrame* resampled_frame = av_frame_alloc();
    SwrContext* pSwrCtx_Mic_local = nullptr; 

    bool isCapPreNot = true;

    if (!packet || !resampled_frame) {
        LOG_ERROR("麦克风采集线程无法分配packet或frame内存，即将退出。");
        goto END;
    }
    
    // --- 关键修改: 在线程启动时，只初始化一次 SwrContext ---
    // 我们不再需要动态重建，因为我们现在100%控制了输入参数
    {
        // 输入参数 (来自我们启动的 arecord 命令)
        int64_t in_ch_layout = AV_CH_LAYOUT_STEREO;
        AVSampleFormat in_sample_fmt = AV_SAMPLE_FMT_S16;
        int in_sample_rate = 48000;

        // 输出参数 (送往AAC编码器)
        int64_t out_ch_layout = pCodecEncodeCtx_Audio->channel_layout;
        AVSampleFormat out_sample_fmt = pCodecEncodeCtx_Audio->sample_fmt;
        int out_sample_rate = pCodecEncodeCtx_Audio->sample_rate;

        pSwrCtx_Mic_local = swr_alloc_set_opts(NULL,
            out_ch_layout, out_sample_fmt, out_sample_rate,
            in_ch_layout, in_sample_fmt, in_sample_rate,
            0, NULL);

        if (!pSwrCtx_Mic_local || swr_init(pSwrCtx_Mic_local) < 0) {
            LOG_ERROR("RecordThreadRun_CapMic: 无法初始化麦克风重采样上下文！线程退出。");
            if(pSwrCtx_Mic_local) swr_free(&pSwrCtx_Mic_local);
            goto END;
        }
        LOG_INFO("RecordThreadRun_CapMic: 麦克风重采样器已成功初始化。");
    }
    // --- 初始化结束 ---

    while (recordType == RecordType::Record && isRecordMic) {
        if (isCapPreNot) {
            LOG_INFO("麦克风已就绪，等待其余准备完毕");
            ++isCanCap;
            while (isCanCap < (isRecordVideo + isRecordInner + isRecordMic) && recordType != RecordType::Stop) {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
            LOG_INFO("麦克风开始录制");
            isCapPreNot = false;
        }

        // 直接从管道读取原始PCM数据包
        int ret = av_read_frame(pFormatCtxIn_Mic, packet);
        if (ret < 0) {
            LOG_WARN("av_read_frame from mic pipe failed or stream ended. Exiting thread.");
            break;
        }

        if (packet->stream_index == streamIndexIn_Mic) {
            // --- 关键修改: 绕过解码器，直接重采样 packet->data ---
            
            // 计算输入packet中有多少个样本
            // 每个样本 = 2声道 * 16位(2字节) = 4字节
            const int bytes_per_sample = 4;
            const int nb_samples_in_packet = packet->size / bytes_per_sample;

            // 准备输出帧
            resampled_frame->nb_samples = av_rescale_rnd(swr_get_delay(pSwrCtx_Mic_local, 48000) + nb_samples_in_packet, 48000, 48000, AV_ROUND_UP);
            resampled_frame->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
            resampled_frame->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
            resampled_frame->format = pCodecEncodeCtx_Audio->sample_fmt;
            av_frame_get_buffer(resampled_frame, 0);

            // 直接将 packet->data 指针传递给 swr_convert
            const uint8_t* in_data[AV_NUM_DATA_POINTERS] = { packet->data };
            
            ret = swr_convert(pSwrCtx_Mic_local, 
                              resampled_frame->data, resampled_frame->nb_samples,
                              in_data, nb_samples_in_packet);

            if (ret < 0) {
                LOG_ERROR("swr_convert in CapMic failed: " + av_err2str_cpp(ret));
                av_packet_unref(packet);
                continue; // 跳过这一帧
            }

            // ret 是输出的样本数
            resampled_frame->nb_samples = ret;

            // 将重采样后的数据写入FIFO
            pthread_mutex_lock(&csMic);
            av_audio_fifo_write(pAudioFifo_Mic, (void**)resampled_frame->data, resampled_frame->nb_samples);
            pthread_mutex_unlock(&csMic);
            
            av_frame_unref(resampled_frame); // 释放输出帧的数据缓冲区以备下次使用
            // --- 修改结束 ---
        }
        av_packet_unref(packet);
    }

END:
    LOG_INFO("录制子线程-麦克风采集即将停止并回收资源");
    if (packet) av_packet_free(&packet);
    if (resampled_frame) av_frame_free(&resampled_frame); // 这里只释放 AVFrame 结构体
    if (pSwrCtx_Mic_local) {
        swr_free(&pSwrCtx_Mic_local);
    }
    LOG_INFO("录制子线程-麦克风采集已退出");
}

void AudioVideoProcModule::RecordThreadRun_FilterMic() {
    LOG_INFO("录制子线程-麦克风降噪就绪");
    const int frameMicMinSize = AUDIO_FRAME_SIZE;
    AVFrame* frameAudioMic = av_frame_alloc();
    AVFrame* frameOut = av_frame_alloc();
    if (!frameAudioMic || !frameOut) {
        LOG_ERROR("无法为麦克风滤波分配帧内存");
        goto END;
    }

    while (recordType != RecordType::Stop) {
        while (recordType == RecordType::Record) {
            if (av_audio_fifo_size(pAudioFifo_Mic) >= frameMicMinSize) {
                frameAudioMic->nb_samples = frameMicMinSize;
                frameAudioMic->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frameAudioMic->format = pCodecEncodeCtx_Audio->sample_fmt;
                frameAudioMic->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                if (av_frame_get_buffer(frameAudioMic, 0) < 0) {
                    LOG_WARN("frameAudioMic申请内存失败，等待下一次处理重新申请");
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue;
                }
                pthread_mutex_lock(&csMic);
                av_audio_fifo_read(pAudioFifo_Mic, (void**)frameAudioMic->data, frameMicMinSize);
                pthread_mutex_unlock(&csMic);

                if (av_buffersrc_add_frame(pFilterCtxSrcMic_Mic, frameAudioMic) >= 0) {
                    while (av_buffersink_get_frame(pFilterCtxOutMic_Mic, frameOut) >= 0) {
                        pthread_mutex_lock(&csMicFilter);
                        av_audio_fifo_write(pAudioFifo_Mic_Filter, (void**)frameOut->data, frameOut->nb_samples);
                        pthread_mutex_unlock(&csMicFilter);
                        av_frame_unref(frameOut);
                    }
                }
                av_frame_unref(frameAudioMic);
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
END:
    LOG_INFO("录制子线程-麦克风降噪即将停止并回收资源");
    if (frameOut) av_frame_free(&frameOut);
    if (frameAudioMic) av_frame_free(&frameAudioMic);
    LOG_INFO("录制子线程-麦克风降噪已退出");
}

void AudioVideoProcModule::RecordThreadRun_Mix()
{
    LOG_INFO("录制子线程-混音就绪");
    const int frameMinSize = AUDIO_FRAME_SIZE;
    AVFrame* frameAudioInner = av_frame_alloc();
    AVFrame* frameAudioMic = av_frame_alloc();
    AVFrame* frameOut = av_frame_alloc();
    // --- 修改开始: 增加对内存分配失败的检查 ---
    if (!frameAudioInner || !frameAudioMic || !frameOut) {
        LOG_ERROR("混音线程无法分配帧内存，即将退出。");
        goto END;
    }
    // --- 修改结束 ---

    while (recordType != RecordType::Stop) {
        while (recordType == RecordType::Record) {
            // 检查是否有足够数据进行处理
            bool hasInnerData = isRecordInner && (av_audio_fifo_size(pAudioFifo_Inner) >= frameMinSize);
            bool hasMicData = isRecordMic && (av_audio_fifo_size(pAudioFifo_Mic_Filter) >= frameMinSize);
            
            // --- 修改开始: 重构整个处理逻辑 ---
            if (hasInnerData && hasMicData) {
                // 情况1：两个源都有数据，需要混合
                LOG_DEBUG("混音开始，队列数据为: 音频(" + to_string(av_audio_fifo_size(pAudioFifo_Inner)) + ") 麦克风(" + to_string(av_audio_fifo_size(pAudioFifo_Mic_Filter)) + ")");

                // 准备扬声器帧
                frameAudioInner->nb_samples = frameMinSize;
                frameAudioInner->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frameAudioInner->format = pCodecEncodeCtx_Audio->sample_fmt;
                frameAudioInner->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                if (av_frame_get_buffer(frameAudioInner, 0) < 0) {
                    LOG_WARN("frame_audio_inner申请内存失败");
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue;
                }
                
                // 准备麦克风帧
                frameAudioMic->nb_samples = frameMinSize;
                frameAudioMic->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frameAudioMic->format = pCodecEncodeCtx_Audio->sample_fmt;
                frameAudioMic->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                if (av_frame_get_buffer(frameAudioMic, 0) < 0) {
                    LOG_WARN("frame_audio_mic申请内存失败");
                    av_frame_unref(frameAudioInner); // 清理已分配的帧
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue;
                }

                // 从缓冲区读取数据
                pthread_mutex_lock(&csInner);
                av_audio_fifo_read(pAudioFifo_Inner, (void**)(frameAudioInner->data), frameMinSize);
                pthread_mutex_unlock(&csInner);
                pthread_mutex_lock(&csMicFilter);
                av_audio_fifo_read(pAudioFifo_Mic_Filter, (void**)frameAudioMic->data, frameMinSize);
                pthread_mutex_unlock(&csMicFilter);

                // 将两个帧都送入过滤器
                int ret = av_buffersrc_add_frame(pFilterCtxSrc_Inner, frameAudioInner);
                if (ret < 0) LOG_ERROR("向混音器添加扬声器帧失败: " + av_err2str_cpp(ret));
                
                ret = av_buffersrc_add_frame(pFilterCtxSrc_Mic, frameAudioMic);
                if (ret < 0) LOG_ERROR("向混音器添加麦克风帧失败: " + av_err2str_cpp(ret));
                
                // 从过滤器获取混合后的结果
                while (av_buffersink_get_frame(pFilterCtxOut_Mix, frameOut) >= 0) {
                    pthread_mutex_lock(&csMix);
                    av_audio_fifo_write(pAudioFifo_Mix, (void**)frameOut->data, frameOut->nb_samples);
                    pthread_mutex_unlock(&csMix);
                    av_frame_unref(frameOut);
                }
                
                // 清理两个输入帧
                av_frame_unref(frameAudioInner);
                av_frame_unref(frameAudioMic);
            }
            else if (hasInnerData) { // 情况2：只有扬声器数据，直接传递，不经过过滤器
                LOG_DEBUG("音频直通开始，队列数据为:音频(" + to_string(av_audio_fifo_size(pAudioFifo_Inner)) + ")");
                
                // 准备扬声器帧
                frameAudioInner->nb_samples = frameMinSize;
                frameAudioInner->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frameAudioInner->format = pCodecEncodeCtx_Audio->sample_fmt;
                frameAudioInner->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                if (av_frame_get_buffer(frameAudioInner, 0) < 0) {
                    LOG_WARN("frame_audio_inner申请内存失败");
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue;
                }
                
                // 从缓冲区读取数据
                pthread_mutex_lock(&csInner);
                av_audio_fifo_read(pAudioFifo_Inner, (void**)(frameAudioInner->data), frameMinSize);
                pthread_mutex_unlock(&csInner);

                // 直接将数据写入下一个缓冲区
                pthread_mutex_lock(&csMix);
                av_audio_fifo_write(pAudioFifo_Mix, (void**)frameAudioInner->data, frameAudioInner->nb_samples);
                pthread_mutex_unlock(&csMix);

                // 清理帧
                av_frame_unref(frameAudioInner);
            }
            else if (hasMicData) { // 情况3：只有麦克风数据，直接传递，不经过过滤器
                LOG_DEBUG("麦克风直通开始，队列数据为:麦克风(" + to_string(av_audio_fifo_size(pAudioFifo_Mic_Filter)) + ")");
                
                // 准备麦克风帧
                frameAudioMic->nb_samples = frameMinSize;
                frameAudioMic->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frameAudioMic->format = pCodecEncodeCtx_Audio->sample_fmt;
                frameAudioMic->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                if (av_frame_get_buffer(frameAudioMic, 0) < 0) {
                    LOG_WARN("frame_audio_mic申请内存失败");
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue;
                }
                
                // 从缓冲区读取数据
                pthread_mutex_lock(&csMicFilter);
                av_audio_fifo_read(pAudioFifo_Mic_Filter, (void**)frameAudioMic->data, frameMinSize);
                pthread_mutex_unlock(&csMicFilter);
                
                // 直接将数据写入下一个缓冲区
                pthread_mutex_lock(&csMix);
                av_audio_fifo_write(pAudioFifo_Mix, (void**)frameAudioMic->data, frameAudioMic->nb_samples);
                pthread_mutex_unlock(&csMix);

                // 清理帧
                av_frame_unref(frameAudioMic);
            }
            else {
                // 情况4：没有足够的数据，休眠
                this_thread::sleep_for(chrono::milliseconds(10));
            }
            // --- 修改结束 ---
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }

END: // 这是函数末尾的跳转标签和清理代码
    LOG_INFO("录制子线程-混音即将停止并回收资源");
    if (frameOut) { 
        av_frame_free(&frameOut);
    }
    if (frameAudioInner) {
        av_frame_free(&frameAudioInner);
    }
    if (frameAudioMic) {
        av_frame_free(&frameAudioMic);
    }
    LOG_INFO("录制子线程-混音已退出");
}

void AudioVideoProcModule::RecordThreadRun_Write() {
    LOG_INFO("录制子线程-音频写入就绪");
    int iRet = 0;
    // int64_t frameCount = 0; // 我们不再使用简单的帧计数来生成PTS
    const int frameMixMinSize = AUDIO_FRAME_SIZE;
    AVFrame* frame_mix = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    
    // --- 确保指针被成功分配 ---
    if (!frame_mix || !pkt) {
        LOG_ERROR("音频写入线程无法分配frame或packet内存，即将退出。");
        goto END;
    }

    while (recordType != RecordType::Stop) {
        while (recordType == RecordType::Record) {
            // 检查音频混合缓冲区中是否有足够的数据构成一个完整的音频帧
            if (av_audio_fifo_size(pAudioFifo_Mix) >= frameMixMinSize) {
                // 设置即将被填充的音频帧的参数
                frame_mix->nb_samples = frameMixMinSize;
                frame_mix->channel_layout = pCodecEncodeCtx_Audio->channel_layout;
                frame_mix->format = pCodecEncodeCtx_Audio->sample_fmt;
                frame_mix->sample_rate = pCodecEncodeCtx_Audio->sample_rate;
                
                // 为音频帧分配数据缓冲区
                iRet = av_frame_get_buffer(frame_mix, 0);
                if (iRet < 0) {
                    LOG_ERROR("音频写入线程 av_frame_get_buffer 失败。");
                    this_thread::sleep_for(chrono::milliseconds(10));
                    continue; // 跳过本次循环
                }

                // 从FIFO中读取数据到音频帧
                pthread_mutex_lock(&csMix);
                av_audio_fifo_read(pAudioFifo_Mix, (void**)frame_mix->data, frameMixMinSize);
                pthread_mutex_unlock(&csMix);

                // --- 你的原始代码：基于全局时钟的时间戳计算 ---
                // auto now = std::chrono::steady_clock::now();
            	// auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(now - recording_start_time).count();
            	// int64_t current_pts = av_rescale_q(elapsed_time, {1, 1000000}, pCodecEncodeCtx_Audio->time_base);

            	// if (current_pts <= last_audio_pts) {
             	//    current_pts = last_audio_pts + frame_mix->nb_samples; // 音频PTS通常按样本数增加
            	// }
            	// frame_mix->pts = current_pts;
            	// last_audio_pts = current_pts;
                
                // --- 新的、更简单的方案：基于样本数生成连续的PTS ---
                // 这种方法不依赖真实时间，但能保证音频流自身的流畅和连续性
                if (last_audio_pts < 0) { // 如果是第一帧
                    last_audio_pts = 0;
                }
                frame_mix->pts = last_audio_pts;
                last_audio_pts += frame_mix->nb_samples; // 为下一帧准备PTS

                // 将音频帧发送给编码器
                iRet = avcodec_send_frame(pCodecEncodeCtx_Audio, frame_mix);
                while (iRet >= 0) {
                    // 从编码器接收编码后的数据包
                    iRet = avcodec_receive_packet(pCodecEncodeCtx_Audio, pkt);
                    if (iRet == AVERROR(EAGAIN) || iRet == AVERROR_EOF) {
                        break;
                    } else if (iRet < 0) {
                        LOG_WARN("avcodec_receive_packet in audio writer failed: " + av_err2str_cpp(iRet));
                        break;
                    }
                    
                    // 将数据包的时间戳从编码器的时间基转换为输出流的时间基
                    av_packet_rescale_ts(pkt, pCodecEncodeCtx_Audio->time_base, pStream_Audio->time_base);
			
                    // --- 音频流独立归一化 ---
                    {
                        // 使用 std::lock_guard 来安全地访问共享变量
                        std::lock_guard<std::mutex> lock(audio_pts_mutex);
                        // 如果这是整个音频流的第一个数据包
                        if (audio_first_dts == AV_NOPTS_VALUE) {
                            // 记录下它的DTS作为时间戳的起点/偏移量
                            audio_first_dts = pkt->dts;
                            LOG_INFO("音频流的第一个DTS被记录为: " + to_string(audio_first_dts));
                        }
                    }
                    // 将所有后续数据包的时间戳都减去这个初始偏移量
                    // 这样可以确保音频流的时间戳从0开始
                    pkt->pts -= audio_first_dts;
                    pkt->dts -= audio_first_dts;
        
                    pkt->stream_index = streamIndex_Audio;

                    // 写入数据包到输出文件/流
                    pthread_mutex_lock(&csWrite);
                    av_interleaved_write_frame(pFormatCtxOut, pkt);
                    pthread_mutex_unlock(&csWrite);
                    allAudioFrame++;
                    av_packet_unref(pkt);
                }
                av_frame_unref(frame_mix); // 释放音频帧的数据缓冲区
            } else {
                // 如果缓冲区中没有足够的数据，则短暂休眠
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }

END: // 函数退出时的清理标签
    LOG_INFO("录制子线程-音频写入即将停止并回收资源");
    recordType = RecordType::Stop;
    if (frame_mix) {
        av_frame_free(&frame_mix);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    LOG_INFO("录制子线程-音频写入已退出");
}


int AudioVideoProcModule::OpenOutPut() {
    CloseOutPut();  // 先清理旧的

    // 避免 goto 跨越初始化
    AVDictionary* vopts = nullptr;

    int iRet = 0;
    const char* outFileName = isRtmp ? pushRtmpUrl.c_str() : recordFileName.c_str();
    streamIndex_Video = 0;

    // 三种模式：系统声 / 麦克风 / 无音频
    const bool wantAudio = (isRecordInner || isRecordMic);
    streamIndex_Audio = wantAudio ? 1 : -1;

    LOG_INFO("OpenOutPut: 准备打开输出: " + std::string(outFileName));

    // ========== 1) 输出上下文 ==========
    if (isRtmp) {
        iRet = avformat_alloc_output_context2(&pFormatCtxOut, nullptr, "flv", outFileName);
    } else {
        iRet = avformat_alloc_output_context2(&pFormatCtxOut, nullptr, nullptr, outFileName);
    }
    if (iRet < 0 || !pFormatCtxOut) {
        LOG_ERROR("输出上下文分配失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
        goto END_ERR;
    }

    // ========== 2) 选择视频编码器：libopenh264（只用它，不回退 x264） ==========
    pCodecEncode_Video = avcodec_find_encoder_by_name("libopenh264");
    if (!pCodecEncode_Video) {
        LOG_ERROR("未找到 libopenh264（请确认 FFmpeg 启用了 --enable-libopenh264 并正确安装 openh264）");
        iRet = AVERROR_ENCODER_NOT_FOUND;
        goto END_ERR;
    }
    LOG_INFO("使用视频编码器：libopenh264");

    // ========== 3) 分配编码器上下文 & 新建视频流 ==========
    pCodecEncodeCtx_Video = avcodec_alloc_context3(pCodecEncode_Video);
    if (!pCodecEncodeCtx_Video) {
        LOG_ERROR("分配视频编码器上下文失败");
        iRet = AVERROR(ENOMEM);
        goto END_ERR;
    }

    pStream_Video = avformat_new_stream(pFormatCtxOut, nullptr);
    if (!pStream_Video) {
        LOG_ERROR("创建输出视频流失败");
        iRet = AVERROR(ENOMEM);
        goto END_ERR;
    }
    pStream_Video->id = streamIndex_Video;

    // ========== 4) （可选）音频：AAC ==========
    if (wantAudio) {
        pCodecEncode_Audio = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!pCodecEncode_Audio) {
            LOG_ERROR("找不到 AAC 编码器");
            iRet = AVERROR_ENCODER_NOT_FOUND;
            goto END_ERR;
        }
        pCodecEncodeCtx_Audio = avcodec_alloc_context3(pCodecEncode_Audio);
        if (!pCodecEncodeCtx_Audio) {
            LOG_ERROR("分配音频编码器上下文失败");
            iRet = AVERROR(ENOMEM);
            goto END_ERR;
        }
        pStream_Audio = avformat_new_stream(pFormatCtxOut, nullptr);
        if (!pStream_Audio) {
            LOG_ERROR("创建输出音频流失败");
            iRet = AVERROR(ENOMEM);
            goto END_ERR;
        }
        pStream_Audio->id = streamIndex_Audio;
    } else {
        pCodecEncode_Audio = nullptr;
        pCodecEncodeCtx_Audio = nullptr;
        pStream_Audio = nullptr;
    }

    // ========== 5) 填写视频编码参数（CBR + 低延迟友好）==========
    pCodecEncodeCtx_Video->codec_id   = pCodecEncode_Video->id;
    pCodecEncodeCtx_Video->codec_type = AVMEDIA_TYPE_VIDEO;

    pCodecEncodeCtx_Video->width   = FINALE_WIDTH;
    pCodecEncodeCtx_Video->height  = FINALE_HEIGHT;
    pCodecEncodeCtx_Video->pix_fmt = AV_PIX_FMT_YUV420P;

    // 帧率/时间基
    pCodecEncodeCtx_Video->time_base = AVRational{1, frameRate};
    pStream_Video->time_base         = pCodecEncodeCtx_Video->time_base;

    // 固定码率（CBR）：用 AVCodecContext 字段控制
    pCodecEncodeCtx_Video->flags &= ~AV_CODEC_FLAG_QSCALE; // 不使用 QSCALE
    pCodecEncodeCtx_Video->bit_rate           = (bitRate > 0 ? bitRate : 2'000'000);
    pCodecEncodeCtx_Video->bit_rate_tolerance = pCodecEncodeCtx_Video->bit_rate / 2;
    pCodecEncodeCtx_Video->rc_max_rate        = pCodecEncodeCtx_Video->bit_rate;
    pCodecEncodeCtx_Video->rc_min_rate        = pCodecEncodeCtx_Video->bit_rate;
    pCodecEncodeCtx_Video->rc_buffer_size     = pCodecEncodeCtx_Video->bit_rate; // 与 maxrate 配对，消除 VBV 提示

    // GOP / B 帧（openh264 通常不使用 B 帧，保持 0）
    pCodecEncodeCtx_Video->gop_size     = (gopSize > 0 ? gopSize : frameRate); // 约 1s 一个关键帧
    pCodecEncodeCtx_Video->max_b_frames = 0;
    pCodecEncodeCtx_Video->thread_count = (threadCount > 0 ? threadCount : 1);

    if (pFormatCtxOut->oformat->flags & AVFMT_GLOBALHEADER) {
        pCodecEncodeCtx_Video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    LOG_INFO("视频参数: " + std::to_string(pCodecEncodeCtx_Video->width) + "x" +
             std::to_string(pCodecEncodeCtx_Video->height) + " @" +
             std::to_string(frameRate) + "fps, bitrate=" +
             std::to_string(pCodecEncodeCtx_Video->bit_rate) +
             ", gop=" + std::to_string(pCodecEncodeCtx_Video->gop_size) +
             ", threads=" + std::to_string(pCodecEncodeCtx_Video->thread_count));

    // ========== 6) 音频编码参数（仅当需要）==========
    if (wantAudio) {
        pCodecEncodeCtx_Audio->codec_type     = AVMEDIA_TYPE_AUDIO;
        pCodecEncodeCtx_Audio->sample_fmt     = pCodecEncode_Audio->sample_fmts ?
                                                pCodecEncode_Audio->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        pCodecEncodeCtx_Audio->sample_rate    = nbSample;   // 如 48000
        pCodecEncodeCtx_Audio->channel_layout = AV_CH_LAYOUT_STEREO;
        pCodecEncodeCtx_Audio->channels       = av_get_channel_layout_nb_channels(pCodecEncodeCtx_Audio->channel_layout);
        pCodecEncodeCtx_Audio->bit_rate       = 64000;
        pCodecEncodeCtx_Audio->time_base      = AVRational{1, pCodecEncodeCtx_Audio->sample_rate};
        pStream_Audio->time_base              = pCodecEncodeCtx_Audio->time_base;

        if (pFormatCtxOut->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecEncodeCtx_Audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        LOG_INFO("音频参数: " + std::to_string(pCodecEncodeCtx_Audio->channels) + "ch, " +
                 std::to_string(pCodecEncodeCtx_Audio->sample_rate) + "Hz, fmt=" +
                 std::to_string(pCodecEncodeCtx_Audio->sample_fmt) +
                 ", bitrate=" + std::to_string(pCodecEncodeCtx_Audio->bit_rate));
    }

    // ========== 7) 仅针对 libopenh264 的私有选项（保守且兼容）==========
    // 只设置通用且稳定的两个：profile/level；其它参数留空以避免 “unknown option”
    av_dict_set(&vopts, "profile", "baseline", 0);
    av_dict_set(&vopts, "level",   "3.1",      0);
    // （可选）如果想走 CRF/ABR 需查对应 FFmpeg 版本支持的 libopenh264 私有项，这里不强塞

    // ========== 8) 打开编码器 ==========
    iRet = avcodec_open2(pCodecEncodeCtx_Video, pCodecEncode_Video, &vopts);
    if (iRet < 0) {
        LOG_ERROR("打开视频编码器(libopenh264)失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
        goto END_ERR;
    }

    if (wantAudio) {
        iRet = avcodec_open2(pCodecEncodeCtx_Audio, pCodecEncode_Audio, nullptr);
        if (iRet < 0) {
            LOG_ERROR("打开音频编码器失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
            goto END_ERR;
        }
    }

    // ========== 9) 将编码参数写入流 ==========
    iRet = avcodec_parameters_from_context(pStream_Video->codecpar, pCodecEncodeCtx_Video);
    if (iRet < 0) {
        LOG_ERROR("从视频上下文中拷贝参数失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
        goto END_ERR;
    }
    if (wantAudio) {
        iRet = avcodec_parameters_from_context(pStream_Audio->codecpar, pCodecEncodeCtx_Audio);
        if (iRet < 0) {
            LOG_ERROR("从音频上下文中拷贝参数失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
            goto END_ERR;
        }
    }

    // ========== 10) 打开 IO & 写文件头 ==========
    if (!(pFormatCtxOut->oformat->flags & AVFMT_NOFILE)) {
        iRet = avio_open(&pFormatCtxOut->pb, outFileName, AVIO_FLAG_WRITE);
        if (iRet < 0) {
            LOG_ERROR("avio_open 打开输出失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
            goto END_ERR;
        }
    }

    iRet = avformat_write_header(pFormatCtxOut, nullptr);
    if (iRet < 0) {
        LOG_ERROR("写入文件头失败(" + std::to_string(iRet) + "): " + av_err2str_cpp(iRet));
        goto END_ERR;
    }

    LOG_INFO("OpenOutPut: 成功写入输出头（" + std::string(isRtmp ? "RTMP/FLV" : pFormatCtxOut->oformat->name) + "），使用 libopenh264。");

    if (vopts) { av_dict_free(&vopts); vopts = nullptr; }
    return 0;

END_ERR:
    if (vopts) { av_dict_free(&vopts); vopts = nullptr; }
    CloseOutPut();
    return (iRet < 0) ? iRet : AVERROR_UNKNOWN;
}


void AudioVideoProcModule::CloseOutPut() {
    if (pCodecEncodeCtx_Video)
    {
        LOG_INFO("释放视频编码器");
        if (avcodec_is_open(pCodecEncodeCtx_Video))
            avcodec_close(pCodecEncodeCtx_Video);
        avcodec_free_context(&pCodecEncodeCtx_Video);
        pCodecEncodeCtx_Video = nullptr;
    }
    if (pCodecEncodeCtx_Audio)
    {
        LOG_INFO("释放音频编码器");
        if (avcodec_is_open(pCodecEncodeCtx_Audio))
            avcodec_close(pCodecEncodeCtx_Audio);
        avcodec_free_context(&pCodecEncodeCtx_Audio);
        pCodecEncodeCtx_Audio = nullptr;
    }
    //编码器描述符不需要分配，所以不必释放，直接nullptr
    pCodecEncode_Video = nullptr;
    pCodecEncode_Audio = nullptr;
    //回收输出上下文，结束录制
    //同时此处av_write_trailer和avformat_free_context也已经做到将流回收
    if (pFormatCtxOut) {
        LOG_INFO("回收输出上下文");
        if (pFormatCtxOut->pb) {
            av_write_trailer(pFormatCtxOut);
            avio_close(pFormatCtxOut->pb);
        }
        avformat_free_context(pFormatCtxOut);
        pFormatCtxOut = nullptr;
        pStream_Video = nullptr;
        pStream_Audio = nullptr;
    }
    //如果没有录制的情况下申请了，那么这样子释放
    if (pStream_Video) {
        LOG_INFO("释放视频流");
        av_freep(&pStream_Video);
        pStream_Video = nullptr;
    }
    if (pStream_Audio) {
        LOG_INFO("释放音频流");
        av_freep(&pStream_Audio);
        pStream_Audio = nullptr;
    }
}
int AudioVideoProcModule::InitSwrInner() {
    UnInitSwrInner();
    
    // --- 步骤 1: 检查所有前提条件是否满足 ---
    if (!pFormatCtxIn_Inner) {
        LOG_ERROR("InitSwrInner 失败: 输入上下文 pFormatCtxIn_Inner 为空。");
        return -1;
    }
    if (streamIndexIn_Inner < 0) {
        LOG_ERROR("InitSwrInner 失败: 音频流索引 streamIndexIn_Inner 无效 (" + to_string(streamIndexIn_Inner) + ")。");
        return -1;
    }
    if (streamIndexIn_Inner >= (int)pFormatCtxIn_Inner->nb_streams) {
        LOG_ERROR("InitSwrInner 失败: 音频流索引 " + to_string(streamIndexIn_Inner) + " 超出范围 (总流数: " + to_string(pFormatCtxIn_Inner->nb_streams) + ")。");
        return -1;
    }
    
    AVStream* stream = pFormatCtxIn_Inner->streams[streamIndexIn_Inner];
    if (!stream) {
        LOG_ERROR("InitSwrInner 失败: 获取音频流对象失败。");
        return -1;
    }
    
    AVCodecParameters* codecpar = stream->codecpar;
    if (!codecpar) {
        LOG_ERROR("InitSwrInner 失败: 音频流的编解码参数 codecpar 为空。");
        return -1;
    }

    // --- 步骤 2: 安全地准备参数 ---
    int64_t in_ch_layout = codecpar->channel_layout;
    if (in_ch_layout == 0) {
        // 如果探测不到声道布局，根据声道数手动生成一个默认的
        in_ch_layout = av_get_default_channel_layout(codecpar->channels);
        LOG_WARN("InitSwrInner: 输入音频流未提供声道布局，根据声道数 " + to_string(codecpar->channels) + " 强制设置为默认布局。");
    }
    AVSampleFormat in_sample_fmt = (AVSampleFormat)codecpar->format;
    int in_sample_rate = codecpar->sample_rate;

    int64_t out_ch_layout = pCodecEncodeCtx_Audio->channel_layout;
    AVSampleFormat out_sample_fmt = pCodecEncodeCtx_Audio->sample_fmt;
    int out_sample_rate = pCodecEncodeCtx_Audio->sample_rate;

    LOG_INFO("InitSwrInner: 准备配置重采样: " +
             to_string(in_sample_rate) + "Hz " + av_get_sample_fmt_name(in_sample_fmt) + " -> " +
             to_string(out_sample_rate) + "Hz " + av_get_sample_fmt_name(out_sample_fmt));

    // --- 步骤 3: 调用FFmpeg函数 ---
    pSwrCtx_Inner = swr_alloc_set_opts(NULL,
        out_ch_layout, out_sample_fmt, out_sample_rate,
        in_ch_layout, in_sample_fmt, in_sample_rate,
        0, NULL);

    if (!pSwrCtx_Inner) {
        LOG_ERROR("InitSwrInner: swr_alloc_set_opts 失败。");
        return -1;
    }
    
    int ret = swr_init(pSwrCtx_Inner);
    if (ret < 0) {
        LOG_ERROR("InitSwrInner: swr_init 失败: " + av_err2str_cpp(ret));
        swr_free(&pSwrCtx_Inner);
        return ret;
    }

    LOG_INFO("InitSwrInner: 扬声器重采样上下文初始化成功。");
    return 0; // 成功返回0
}
void AudioVideoProcModule::UnInitSwrInner() { if(pSwrCtx_Inner) swr_free(&pSwrCtx_Inner); }
int AudioVideoProcModule::InitSwrMic() {
    UnInitSwrMic();
    
    // --- 关键修改：我们不再信任任何从管道探测到的参数 ---
    // 因为我们自己通过 arecord 启动了进程，我们100%知道输入的音频参数是什么。
    
    // 输入参数 (来自我们启动的 arecord 命令)
    // command: arecord -f S16_LE -r 48000 -c 2 -D hw:0,0
    int64_t in_ch_layout = AV_CH_LAYOUT_STEREO;         // 对应 -c 2 (立体声)
    AVSampleFormat in_sample_fmt = AV_SAMPLE_FMT_S16;   // 对应 -f S16_LE (16位有符号小端整数)
    int in_sample_rate = 48000;                         // 对应 -r 48000 (采样率)

    // 输出参数 (送往AAC编码器)
    int64_t out_ch_layout = pCodecEncodeCtx_Audio->channel_layout;
    AVSampleFormat out_sample_fmt = pCodecEncodeCtx_Audio->sample_fmt;
    int out_sample_rate = pCodecEncodeCtx_Audio->sample_rate;

    LOG_INFO("InitSwrMic: 准备配置麦克风重采样: " + 
             to_string(in_sample_rate) + "Hz " + av_get_sample_fmt_name(in_sample_fmt) + " (Stereo) -> " +
             to_string(out_sample_rate) + "Hz " + av_get_sample_fmt_name(out_sample_fmt));

    // 使用这些100%确定的参数来配置重采样器
    pSwrCtx_Mic = swr_alloc_set_opts(NULL,
        out_ch_layout, out_sample_fmt, out_sample_rate,
        in_ch_layout, in_sample_fmt, in_sample_rate,
        0, NULL);
    
    if (!pSwrCtx_Mic) {
        LOG_ERROR("InitSwrMic: swr_alloc_set_opts 失败。");
        return -1;
    }
    
    int ret = swr_init(pSwrCtx_Mic);
    if (ret < 0) {
        LOG_ERROR("InitSwrMic: swr_init 失败: " + av_err2str_cpp(ret));
        swr_free(&pSwrCtx_Mic);
        return ret;
    }

    LOG_INFO("InitSwrMic: 麦克风重采样上下文初始化成功。");
    return 0; 
}
void AudioVideoProcModule::UnInitSwrMic() { if(pSwrCtx_Mic) swr_free(&pSwrCtx_Mic); }

int AudioVideoProcModule::InitFilter()
{
    UnInitFilter();
    const char* filter_desc = mixFilterString.c_str();
    LOG_INFO("混音滤波字符串为:" + mixFilterString);
    int ret = 0;
    char args_inner[512];
    const char* pad_name_inner = "in0";
    char args_mic[512];
    const char* pad_name_mic = "in1";

    const AVFilter* filter_src_spk = avfilter_get_by_name("abuffer");
    const AVFilter* filter_src_mic = avfilter_get_by_name("abuffer");
    const AVFilter* filter_sink = avfilter_get_by_name("abuffersink");
    AVFilterInOut* filter_output_inner = avfilter_inout_alloc();
    AVFilterInOut* filter_output_mic = avfilter_inout_alloc();
    AVFilterInOut* filter_output = avfilter_inout_alloc();
    pFilterGraph = avfilter_graph_alloc();

    // 使用 PRIx64 宏来安全地打印 uint64_t
    snprintf(args_inner, sizeof(args_inner), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
        pCodecEncodeCtx_Audio->time_base.num, pCodecEncodeCtx_Audio->time_base.den, pCodecEncodeCtx_Audio->sample_rate,
        av_get_sample_fmt_name((AVSampleFormat)pCodecEncodeCtx_Audio->sample_fmt), pCodecEncodeCtx_Audio->channel_layout);

    snprintf(args_mic, sizeof(args_mic), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
        pCodecEncodeCtx_Audio->time_base.num, pCodecEncodeCtx_Audio->time_base.den, pCodecEncodeCtx_Audio->sample_rate,
        av_get_sample_fmt_name((AVSampleFormat)pCodecEncodeCtx_Audio->sample_fmt), pCodecEncodeCtx_Audio->channel_layout);

    AVFilterInOut* filter_inputs[2]{};
    do {
        ret = avfilter_graph_create_filter(&pFilterCtxSrc_Inner, filter_src_spk, pad_name_inner, args_inner, NULL, pFilterGraph);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_create_filter -- src inner");
            break;
        }

        ret = avfilter_graph_create_filter(&pFilterCtxSrc_Mic, filter_src_mic, pad_name_mic, args_mic, NULL, pFilterGraph);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_create_filter -- src mic");
            break;
        }

        ret = avfilter_graph_create_filter(&pFilterCtxOut_Mix, filter_sink, "out", NULL, NULL, pFilterGraph);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_create_filter -- sink");
            break;
        }

        ret = av_opt_set_bin(pFilterCtxOut_Mix, "sample_fmts", (uint8_t*)&pCodecEncodeCtx_Audio->sample_fmt, sizeof(pCodecEncodeCtx_Audio->sample_fmt), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- sample_fmts");
            break;
        }
        ret = av_opt_set_bin(pFilterCtxOut_Mix, "channel_layouts", (uint8_t*)&pCodecEncodeCtx_Audio->channel_layout, sizeof(pCodecEncodeCtx_Audio->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- channel_layouts");
            break;
        }
        ret = av_opt_set_bin(pFilterCtxOut_Mix, "sample_rates", (uint8_t*)&pCodecEncodeCtx_Audio->sample_rate, sizeof(pCodecEncodeCtx_Audio->sample_rate), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- sample_rates");
            break;
        }

        filter_output_inner->name = av_strdup(pad_name_inner);
        filter_output_inner->filter_ctx = pFilterCtxSrc_Inner;
        filter_output_inner->pad_idx = 0;
        filter_output_inner->next = filter_output_mic;

        filter_output_mic->name = av_strdup(pad_name_mic);
        filter_output_mic->filter_ctx = pFilterCtxSrc_Mic;
        filter_output_mic->pad_idx = 0;
        filter_output_mic->next = NULL;

        filter_output->name = av_strdup("out");
        filter_output->filter_ctx = pFilterCtxOut_Mix;
        filter_output->pad_idx = 0;
        filter_output->next = NULL;

        filter_inputs[0] = filter_output_inner;
        filter_inputs[1] = filter_output_mic;

        ret = avfilter_graph_parse_ptr(pFilterGraph, filter_desc, &filter_output, filter_inputs, NULL);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_parse_ptr");
            break;
        }

        ret = avfilter_graph_config(pFilterGraph, NULL);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_config");
            break;
        }

        ret = 0;
    } while (0);

    //char* temp = avfilter_graph_dump(pFilterGraph, NULL);

    avfilter_inout_free(filter_inputs);
    avfilter_inout_free(&filter_output);

    if (AVERROR(ret))
        UnInitFilter();
    return ret;
}
void AudioVideoProcModule::UnInitFilter() {
    if (pFilterCtxSrc_Inner) {
        LOG_INFO("释放pFilterCtxSrc_Inner");
        avfilter_free(pFilterCtxSrc_Inner);
        pFilterCtxSrc_Inner = nullptr;
    }
    if (pFilterCtxSrc_Mic) {
        LOG_INFO("释放pFilterCtxSrc_Mic");
        avfilter_free(pFilterCtxSrc_Mic);
        pFilterCtxSrc_Mic = nullptr;
    }
    if (pFilterCtxOut_Mix) {
        LOG_INFO("释放pFilterCtxOut_Mix");
        avfilter_free(pFilterCtxOut_Mix);
        pFilterCtxOut_Mix = nullptr;
    }
    if (pFilterGraph) {
        LOG_INFO("释放pFilterGraph");
        avfilter_graph_free(&pFilterGraph);
        pFilterGraph = nullptr;
    }
}

int AudioVideoProcModule::InitFilterMic()
{
    UnInitFilterMic();
    const char* filter_desc = micFilterString.c_str();//,afftdn,aresample=48000
    LOG_INFO("麦克过滤滤波字符串为:" + micFilterString);
    int ret = 0;
    char args[512];
    const char* pad_name = "in";

    const AVFilter* filter_src = avfilter_get_by_name("abuffer");
    const AVFilter* filter_dst = avfilter_get_by_name("abuffersink");
    AVFilterInOut* filter_output_mic = avfilter_inout_alloc();
    AVFilterInOut* filter_output = avfilter_inout_alloc();
    pFilterGraphMic = avfilter_graph_alloc();

    // --- 修改开始 ---
    // 使用 snprintf 替换 sprintf_s
    // 使用 PRIx64 替换 %I64x (或 %llx) 来安全打印 uint64_t
    snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
        pCodecEncodeCtx_Audio->time_base.num,
        pCodecEncodeCtx_Audio->time_base.den,
        pCodecEncodeCtx_Audio->sample_rate,
        av_get_sample_fmt_name((AVSampleFormat)pCodecEncodeCtx_Audio->sample_fmt),
        pCodecEncodeCtx_Audio->channel_layout);
    // --- 修改结束 ---

    do {
        ret = avfilter_graph_create_filter(&pFilterCtxSrcMic_Mic, filter_src, pad_name, args, NULL, pFilterGraphMic);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call avfilter_graph_create_filter -- src mic");
            break;
        }

        ret = avfilter_graph_create_filter(&pFilterCtxOutMic_Mic, filter_dst, "out", NULL, NULL, pFilterGraphMic);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call avfilter_graph_create_filter -- sink");
            break;
        }

        ret = av_opt_set_bin(pFilterCtxOutMic_Mic, "sample_fmts", (uint8_t*)&pCodecEncodeCtx_Audio->sample_fmt, sizeof(pCodecEncodeCtx_Audio->sample_fmt), AV_OPT_SEARCH_CHILDREN);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- sample_fmts");
            break;
        }
        ret = av_opt_set_bin(pFilterCtxOutMic_Mic, "channel_layouts", (uint8_t*)&pCodecEncodeCtx_Audio->channel_layout, sizeof(pCodecEncodeCtx_Audio->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- channel_layouts");
            break;
        }
        ret = av_opt_set_bin(pFilterCtxOutMic_Mic, "sample_rates", (uint8_t*)&pCodecEncodeCtx_Audio->sample_rate, sizeof(pCodecEncodeCtx_Audio->sample_rate), AV_OPT_SEARCH_CHILDREN);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call av_opt_set_bin -- sample_rates");
            break;
        }

        filter_output_mic->name = av_strdup(pad_name);
        filter_output_mic->filter_ctx = pFilterCtxSrcMic_Mic;
        filter_output_mic->pad_idx = 0;
        filter_output_mic->next = NULL;

        filter_output->name = av_strdup("out");
        filter_output->filter_ctx = pFilterCtxOutMic_Mic;
        filter_output->pad_idx = 0;
        filter_output->next = NULL;

        ret = avfilter_graph_parse_ptr(pFilterGraphMic, filter_desc, &filter_output, &filter_output_mic, NULL);
        if (ret < 0) {
            LOG_ERROR("Filter: failed to call avfilter_graph_parse_ptr");
            break;
        }

        ret = avfilter_graph_config(pFilterGraphMic, NULL);
        if (AVERROR(ret)) {
            LOG_ERROR("Filter: failed to call avfilter_graph_config");
            break;
        }
        ret = 0;
    } while (0);

    avfilter_inout_free(&filter_output_mic);
    avfilter_inout_free(&filter_output);
    //char* temp = avfilter_graph_dump(pFilterGraphMic, NULL);
    if (AVERROR(ret))
        UnInitFilterMic();

    return ret;
}

void AudioVideoProcModule::UnInitFilterMic() {
    if (pFilterCtxOutMic_Mic) {
        LOG_INFO("释放pFilterCtxOutMic_Mic");
        avfilter_free(pFilterCtxOutMic_Mic);
        pFilterCtxOutMic_Mic = nullptr;
    }
    if (pFilterCtxSrcMic_Mic) {
        LOG_INFO("释放pFilterCtxSrcMic_Mic");
        avfilter_free(pFilterCtxSrcMic_Mic);
        pFilterCtxSrcMic_Mic = nullptr;
    }
    if (pFilterGraphMic) {
        LOG_INFO("释放pFilterGraphMic");
        avfilter_graph_free(&pFilterGraphMic);
        pFilterGraphMic = nullptr;
    }
}

int AudioVideoProcModule::InitFifo() {
    // 1) 没有任何音频就直接返回成功
    if (!(isRecordInner || isRecordMic)) {
        return 0;
    }
    // 2) 保护性检查：输出上下文和音频流必须存在且可用
    if (!pFormatCtxOut || streamIndex_Audio < 0 ||
        streamIndex_Audio >= (int)pFormatCtxOut->nb_streams ||
        !pFormatCtxOut->streams[streamIndex_Audio]) {
        LOG_ERROR("InitFifo: 音频流无效或不存在。");
        return AVERROR(EINVAL);
    }

    int iRet = -1;
    UnInitFifo();
    do {
        AVStream* astream = pFormatCtxOut->streams[streamIndex_Audio];
        int fmt = astream->codecpar->format;
        int ch  = astream->codecpar->channels;

        pAudioFifo_Inner = av_audio_fifo_alloc((AVSampleFormat)fmt, ch, 3000 * AUDIO_FRAME_SIZE);
        if (!pAudioFifo_Inner) { LOG_ERROR("装载音频缓冲区失败"); break; }

        pAudioFifo_Mic = av_audio_fifo_alloc((AVSampleFormat)fmt, ch, 3000 * AUDIO_FRAME_SIZE);
        if (!pAudioFifo_Mic) { LOG_ERROR("装载麦克风缓冲区失败"); break; }

        pAudioFifo_Mic_Filter = av_audio_fifo_alloc((AVSampleFormat)fmt, ch, 3000 * AUDIO_FRAME_SIZE);
        if (!pAudioFifo_Mic_Filter) { LOG_ERROR("装载麦克风过滤缓冲区失败"); break; }

        pAudioFifo_Mix = av_audio_fifo_alloc((AVSampleFormat)fmt, ch, 3000 * AUDIO_FRAME_SIZE);
        if (!pAudioFifo_Mix) { LOG_ERROR("装载混音缓冲区失败"); break; }

        iRet = 0;
    } while (0);

    if (iRet != 0) UnInitFifo();
    return iRet;
}

void AudioVideoProcModule::UnInitFifo() {
    if (pAudioFifo_Inner) {
        LOG_INFO("正在卸载扬声器缓冲区");
        av_audio_fifo_drain(pAudioFifo_Inner, av_audio_fifo_size(pAudioFifo_Inner));
        av_audio_fifo_free(pAudioFifo_Inner);
        pAudioFifo_Inner = nullptr;
    }
    if (pAudioFifo_Mic) {
        LOG_INFO("正在卸载麦克风缓冲区");
        av_audio_fifo_drain(pAudioFifo_Mic, av_audio_fifo_size(pAudioFifo_Mic));
        av_audio_fifo_free(pAudioFifo_Mic);
        pAudioFifo_Mic = nullptr;
    }
    if (pAudioFifo_Mic_Filter) {
        LOG_INFO("正在卸载麦克风过滤缓冲区");
        av_audio_fifo_drain(pAudioFifo_Mic_Filter, av_audio_fifo_size(pAudioFifo_Mic_Filter));
        av_audio_fifo_free(pAudioFifo_Mic_Filter);
        pAudioFifo_Mic_Filter = nullptr;
    }
    if (pAudioFifo_Mix) {
        LOG_INFO("正在卸载混音缓冲区");
        av_audio_fifo_drain(pAudioFifo_Mix, av_audio_fifo_size(pAudioFifo_Mix));
        av_audio_fifo_free(pAudioFifo_Mix);
        pAudioFifo_Mix = nullptr;
    }
}

//=========================================次要辅助函数=========================================//

void AudioVideoProcModule::SetRecordFileName(const string& FileName) {
    recordFileName = FileName;
}
void AudioVideoProcModule::SetRtmpUrl(const string& RtmpUrl) {
    pushRtmpUrl = RtmpUrl;
    cout <<"pushRtmpUrl:"<<pushRtmpUrl<<endl;
}
