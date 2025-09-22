#pragma once //只编译一次

#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <thread>
#include <mutex>
// --- 修改开始 ---
// 使用标准头文件 <cstdint> 来确保类型定义的统一和准确性
#include <cstdint>
// --- 修改结束 ---

// 为FFmpeg和OpenCV类型提供前向声明，避免在头文件中引入过多依赖
struct AVFormatContext;
struct AVCodec;
struct AVCodecContext;
struct AVStream;
struct AVFilterGraph;
struct AVFilterContext;
struct SwrContext;
struct AVAudioFifo;
namespace cv { class Mat; }

// --- 修改开始 ---
// 统一使用 <cstdint> 中的标准类型
typedef uint32_t UINT32;
typedef uint64_t ULONGLONG;
// --- 修改结束 ---

/// <summary>
/// 摄像头异常回调函数
/// </summary>
typedef void (*VideoCapErrCallBack)(int CapErrType);
/// <summary>
/// 音频异常回调函数
/// </summary>
typedef void (*AudioErrCallBack)(int AudioErrType);


/// <summary>
/// 音视频处理模块类
/// </summary>
class  AudioVideoProcModule {
public:
    /// <summary>
    /// 默认构造
    /// </summary>
    AudioVideoProcModule();
    /// <summary>
    /// 默认析构
    /// </summary>
    ~AudioVideoProcModule();
public:
    /// <summary>
    /// 录制/推流 三种状态
    /// </summary>
    enum class RecordType { Stop, Record, Pause };
private:
    // --- 修改开始: 添加音画同步所需的成员变量 ---

    // 用于确保视频流第一帧是I帧的标志位
    bool first_frame_sent = false;

    // 用于音视频流独立时间戳归一化的变量
    int64_t video_first_dts = -1; 
    int64_t audio_first_dts = -1;
    
    // 用于确保时间戳单调递增的变量
    int64_t last_video_pts = -1;
    int64_t last_audio_pts = -1;

    // --- 修改结束 ---
    std::mutex video_pts_mutex;
    std::mutex audio_pts_mutex;
    
    #ifdef __linux__
    pid_t mic_arecord_pid = -1; // 用于存储 arecord 子进程的ID
    // 使用 FILE* 指针来管理 popen 返回的管道流句柄
    //FILE* mic_pipe_handle = nullptr; 
    #endif
    bool isInit{};				        //模块已经加载？
    volatile bool isRecordVideo{};		//是否录制视频
    volatile bool isRecordInner{};		//是否录制声音
    volatile bool isRecordMic{};	    //是否录制麦克风
    bool isRtmp{};                      //是否是rtmp推流
    int secondaryScreenLocation{};      //次要屏幕位置
    bool isAcceptAppendFrame{};             //是否允许补帧
    int frameRate{};					    //帧率
    int bitRate{};                          //视频码率
    int gopSize{};                          //图像组大小
    int maxBFrames{};                       //最大b帧数
    int threadCount{};                      //使用线程数量
    std::map<std::string, std::string> privDataMap{};      //设置编码器的私有属性
    int nbSample{};                         //音频采样率
    std::string mixFilterString;                 //混音滤波字符串
    std::string micFilterString;                 //麦克风滤波字符串
    std::list<int> frameAppendHistory;           //历史补帧数
    volatile char isCanCap{};               //是否都准备好录制了
    volatile RecordType recordType{};       //录制状态
    VideoCapErrCallBack videoCapErr{};      //摄像头错误回调函数
    AudioErrCallBack audioErr{};            //音频设备错误回调函数
    std::unique_ptr<std::thread> recordThread{};	            //录制用线程
    std::unique_ptr<std::thread> recordThread_Video{};	    //录制视频用线程
    std::unique_ptr<std::thread> recordThread_CapInner{};	    //录制音频之采集线程
    std::unique_ptr<std::thread> recordThread_CapMic{};	    //录制麦克风之采集线程
    std::unique_ptr<std::thread> recordThread_FilterMic{};	//录制麦克风之除杂线程
    std::unique_ptr<std::thread> recordThread_Mix{};	        //音频之混音线程
    std::unique_ptr<std::thread> recordThread_Write{};	    //录制音频之写入线程

    int cameraNum{};		//当前画面录制所使用的摄像头序号
    int secondaryCameraNum{};		//当前次要画面录制所使用的摄像头序号
    int secondaryWPercent;          //次要画面宽占主要画面的百分比[0,100]
    int secondaryHPercent;          //次要画面高占主要画面的百分比[0,100]
    int micPattern{};         //当前麦克风模式
    int micNum{};             //当前指定麦克风序号

    std::string recordFileName{};   //输出文件名
    std::string pushRtmpUrl{};       //rtmp地址
    int videoWidth{};       //视频宽
    int videoHeight{};      //视频高
    int recordX{};
    int recordY{};
    int recordWidth{};           //录制最大宽高设置
    int recordHeight{};         //录制最大宽高设置
    int resizeWidth{};          //采集后画面应变形成的指定宽，0表示不变形
    int resizeHeight{};         //采集后画面应变形成的指定高，0表示不变形
    int screenW{};              //屏幕宽，于开始录制前的预准备处初始化
    int screenH{};              //屏幕高，于开始录制前的预准备处初始化

    AVFormatContext* pFormatCtxOut{};        //输出文件上下文
    AVCodec* pCodecEncode_Video{};            //视频编码器
    AVCodec* pCodecEncode_Audio{};            //音频编码器
    AVCodecContext* pCodecEncodeCtx_Video{};  //视频编码器上下文
    AVCodecContext* pCodecEncodeCtx_Audio{};  //音频编码器上下文
    AVStream* pStream_Audio{};
    AVStream* pStream_Video{};
    int streamIndex_Video{};    //流序号
    int streamIndex_Audio{};    //流序号
    
    // 使用FFmpeg上下文替换Windows音频接口
    AVFormatContext* pFormatCtxIn_Inner{};
    AVCodecContext* pCodecDecodeCtx_Inner{};
    int streamIndexIn_Inner = -1;
    AVFormatContext* pFormatCtxIn_Mic{};
    AVCodecContext* pCodecDecodeCtx_Mic{};
    int streamIndexIn_Mic = -1;

    //临界区 (使用pthread_mutex_t替换CRITICAL_SECTION)
    pthread_mutex_t csVideo{};
    pthread_mutex_t csInner{};
    pthread_mutex_t csMic{};
    pthread_mutex_t csMicFilter{};
    pthread_mutex_t csMix{};
    pthread_mutex_t csWrite{};

    AVFilterGraph* pFilterGraph{};
    AVFilterContext* pFilterCtxSrc_Inner{};
    AVFilterContext* pFilterCtxSrc_Mic{};
    AVFilterContext* pFilterCtxOut_Mix{};

    AVFilterGraph* pFilterGraphMic{};
    AVFilterContext* pFilterCtxSrcMic_Mic{};
    AVFilterContext* pFilterCtxOutMic_Mic{};

    SwrContext* pSwrCtx_Inner{};            //音频重采样上下文
    SwrContext* pSwrCtx_Mic{};              //麦克风重采样上下文
    AVAudioFifo* pAudioFifo_Inner{};        //音频缓冲区
    AVAudioFifo* pAudioFifo_Mic{};          //音频缓冲区
    AVAudioFifo* pAudioFifo_Mic_Filter{};   //音频缓冲区
    AVAudioFifo* pAudioFifo_Mix{};          //音频缓冲区

    //总写入帧数
    ULONGLONG allVideoFrame{};
    ULONGLONG allAudioFrame{};

    //互斥锁
    pthread_mutex_t micMutex_pthread; //麦克风切换互斥锁
    //std::mutex micMutex;
    
    //强制录制画面数组
    char* mFixImgData{nullptr};
    bool mFixImgMatChange{ false };
    cv::Mat mFixImgMat;
    std::mutex mFixImgDataMutex; //录制画面锁

public:
    /// <summary>
    /// 装载模块以初始化
    /// </summary>
    /// <returns>true成功</returns>
    bool Init();
    /// <summary>
    /// 卸载模块以回收资源
    /// </summary>
    void UnInit();

    /// <summary>
    /// 设置录制文件名称
    /// </summary>
    /// <param name="FileName">录制文件的名称，也可以是路径</param>
    void SetRecordFileName(const std::string& FileName);
    /// <summary>
    /// 设置RTMP推流地址
    /// </summary>
    /// <param name="RtmpUrl">推流目的地的URL</param>
    void SetRtmpUrl(const std::string& RtmpUrl);
    /// <summary>
    /// 开始录制/推流(沿袭参数)，以最近的参数配置直接开始录制
    /// </summary>
    /// <returns>true成功</returns>
    bool StartRecord();
    /// <summary>
    /// 开始录制/推流(沿袭参数)，以最近的参数配置直接开始录制
    /// </summary>
    /// <param name="IsRtmp">本次录制是否应推流，否则录制到本地</param>
    /// <returns>true成功</returns>
    bool StartRecord(bool IsRtmp);
    /// <summary>
    /// 开始录制/推流
    /// </summary>
    /// <param name="IsRtmp">本次录制是否应推流，否则录制到本地</param>
    /// <param name="FrameRate">录制的视频帧率</param>
    /// <param name="IsRecordVideo">是否录制视频</param>
    /// <param name="IsRecordSound">是否录制系统内声音</param>
    /// <param name="IsRecordMicro">是否录制麦克风</param>
    /// <param name="SecondaryScreenLocation">次要画面位置(0无，[1-4][左上右下])</param>
    /// <returns>true成功</returns>
    bool StartRecord(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
    /// <summary>
    /// 继续录制/推流
    /// </summary>
    /// <returns>true成功</returns>
    bool ContinueRecord();
    /// <summary>
    /// 暂停录制/推流
    /// </summary>
    /// <returns>true成功</returns>
    bool PauseRecord();
    /// <summary>
    /// 停止录制/推流，等价于FinishRecord
    /// </summary>
    /// <returns>true成功</returns>
    bool StopRecord();
    /// <summary>
    /// 完成录制/推流，等价于StopRecord
    /// </summary>
    /// <returns>true成功</returns>
    bool FinishRecord();
    /// <summary>
    /// 设置摄像头异常回调函数
    /// </summary>
    /// <param name="VideoCapErrFun">回调函数，用于提供摄像头异常信息</param>
    void SetVideoCapErrFun(VideoCapErrCallBack VideoCapErrFun);
    /// <summary>
    /// 设置音频设备异常回调函数
    /// </summary>
    /// <param name="AudioErrFun">回调函数，用于提供音频设备异常信息</param>
    void SetAudioErrFun(AudioErrCallBack AudioErrFun);
    /// <summary>
    /// 设置画面传输:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
    /// </summary>
    /// <param name="CameraNum">摄像头序号</param>
    /// <param name="W">摄像头W分辨率</param>
    /// <param name="H">摄像头H分辨率</param>
    /// <returns>true成功</returns>
    bool SetCamera(const int& CameraNum, const int& W = 0, const int& H = 0);
    /// <summary>
    /// 获得摄像头序号:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
    /// </summary>
    /// <returns>当前的摄像头序号</returns>
    int GetCamera()const;
    /// <summary>
    /// 获得指定摄像头序号的宽高:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
    /// </summary>
    /// <param name="CameraNum">摄像头序号</param>
    /// <param name="W">摄像头W分辨率</param>
    /// <param name="H">摄像头H分辨率</param>
    /// <returns>是否获取成功</returns>
    bool GetCameraWH(const int& CameraNum, int& W, int& H)const;
    /// <summary>
    /// 设置次要画面传输:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
    /// </summary>
    /// <param name="CameraNum">摄像头序号</param>
    /// <param name="W">摄像头W分辨率</param>
    /// <param name="H">摄像头H分辨率</param>
    /// <param name="WPercent">摄像头W所占主画面的百分比[0,100]</param>
    /// <param name="HPercent">摄像头H所占主画面的百分比[0,100]</param>
    /// <returns>true成功</returns>
    bool SetSecondaryCamera(const int& CameraNum, const int& W = 640, const int& H = 480, const int& WPercent = 40, const int& HPercent = 30);
    /// <summary>
    /// 获得次要摄像头序号:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
    /// </summary>
    /// <returns>当前的摄像头序号</returns>
    int GetSecondaryCamera()const;
    /// <summary>
    /// 尝试在存在子画面的情况下，切换主画面和次要画面的位置
    /// </summary>
    /// <returns>true成功</returns>
    bool SwapRecordSubScreen();
    /// <summary>
    /// 设置录制/推流属性
    /// </summary>
    /// <param name="IsRtmp">是否推流，否则录制</param>
    /// <param name="FrameRate">视频录制帧率</param>
    /// <param name="IsRecordVideo">是否录制视频</param>
    /// <param name="IsRecordSound">是否录制系统内声音</param>
    /// <param name="IsRecordMicro">是否录制麦克风</param>
    /// <param name="SecondaryScreenLocation">次要画面位置(0无，[1-4][从左上角开始，顺时针放置])</param>
    void SetRecordAttr(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
    /// <summary>
    /// 当前是否推流，否则为录制
    /// </summary>
    /// <returns>true推流</returns>
    bool IsRtmp()const;
    /// <summary>
    /// 设置是否同意补帧
    /// </summary>
    /// <param name="IsAcceptAppendFrame"></param>
    void SetAcceptAppendFrame(bool IsAcceptAppendFrame);
    /// <summary>
    /// 当前是否同意补帧
    /// </summary>
    /// <returns></returns>
    bool IsAcceptAppendFrame()const;
    /// <summary>
    /// 是否正在录制/推流中
    /// </summary>
    /// <returns>返回一个在StartRecord函数后能够检测是否已经在录制推流的布尔值</returns>
    bool IsRecording()const;
    /// <summary>
    /// 返回视频录制帧率
    /// </summary>
    /// <returns></returns>
    int GetRecordAttrFrameRate()const;
    /// <summary>
    /// 返回当前是否录制视频
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrRecordVideo()const;
    /// <summary>
    /// 返回当前是否录制系统内声音
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrRecordSound()const;
    /// <summary>
    /// 返回当前是否录制麦克风
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrIsRecordMicro()const;
    /// <summary>
    /// 返回当前的录制文件路径
    /// </summary>
    /// <returns></returns>
    std::string GetOutFileName()const;
    /// <summary>
    /// 返回当前的推流URL
    /// </summary>
    /// <returns></returns>
    std::string GetRtmpUrl()const;
    /// <summary>
    /// 获取装载状态
    /// </summary>
    /// <returns></returns>
    bool GetIsInit()const;
    /// <summary>
    /// 设置视频码率
    /// </summary>
    /// <param name="BitRate">视频码率</param>
    void SetBitRate(int BitRate);
    /// <summary>
    /// 获取视频码率
    /// </summary>
    /// <returns></returns>
    int GetBitRate()const;
    /// <summary>
    /// 设置视频图像组大小，设置大能降低视频体积，但太大了反而会增加
    /// </summary>
    /// <param name="GopSize">图像组大小</param>
    void SetGopSize(int GopSize);
    /// <summary>
    /// 获取视频图像组大小
    /// </summary>
    /// <returns></returns>
    int GetGopSize()const;
    /// <summary>
    /// 设置最大b帧数
    /// </summary>
    /// <param name="MaxBFrames">最大b帧数</param>
    void SetMaxBFrames(int MaxBFrames);
    /// <summary>
    /// 获取视频最大b帧数
    /// </summary>
    /// <returns></returns>
    int GetMaxBFrames()const;
    /// <summary>
    /// 设置编解码使用线程数
    /// </summary>
    /// <param name="ThreadCount">编解码使用线程数</param>
    void SetThreadCount(int ThreadCount);
    /// <summary>
    /// 获取编解码使用线程数
    /// </summary>
    /// <returns></returns>
    int GetThreadCount()const;
    /// <summary>
    /// 设置编码器私有属性
    /// </summary>
    /// <param name="Key">键</param>
    /// <param name="Value">值</param>
    void SetPrivData(const std::string& Key, const std::string& Value);
    /// <summary>
    /// 获取编码器指定键下的私有属性值
    /// </summary>
    std::string GetPrivData(const std::string& Key)const;
    /// <summary>
    /// 设置音频采样率
    /// </summary>
    /// <param name="NbSample">音频采样率</param>
    void SetNbSample(int NbSample);
    /// <summary>
    /// 获取音频采样率
    /// </summary>
    /// <returns></returns>
    int GetNbSample()const;
    /// <summary>
    /// 设置混音的滤波字符串
    /// </summary>
    void SetMixFilter(const std::string& MixFilterString);
    /// <summary>
    /// 获取混音的滤波字符串
    /// </summary>
    std::string GetMixFilter()const;
    /// <summary>
    /// 设置麦克风处理的滤波字符串
    /// </summary>
    void SetMicFilter(const std::string& MicFilterString);
    /// <summary>
    /// 获取麦克风处理的滤波字符串
    /// </summary>
    std::string GetMicFilter()const;
    /// <summary>
    /// 获取30次采集内的平均补帧数
    /// </summary>
    int GetFrameAppendNum();
    /// <summary>
    /// 获取扬声器设备是否已被成功使用
    /// </summary>
    bool GetInnerReadyOk();
    /// <summary>
    /// 获取麦克风设备是否已被成功使用
    /// </summary>
    bool GetMicReadyOk();
    /// <summary>
    /// 获取麦克风设备当前启用的模式
    /// </summary>
    int GetMicPattern() const;
    /// <summary>
    /// 设置麦克风设备当前启用的模式
    /// </summary>
    bool SetMicPattern(const int& MicPattern, const int& MicNum = 0);
    /// <summary>
    /// 设置录制区域的XYWH
    /// </summary>
    void SetRecordXYWH(const int& X, const int& Y, const int& Width, const int& Height);
    /// <summary>
    /// 获取录制区域
    /// </summary>
    void GetRecordXYWH(int& X, int& Y, int& Width, int& Height);
    /// <summary>
    /// 设置录制视频的固定宽高
    /// </summary>
    void SetRecordFixSize(const int& Width, const int& Height);
    /// <summary>
    /// 获取录制视频的固定宽高
    /// </summary>
    void GetRecordFixSize(int& Width, int& Height);
    /// <summary>
    /// 强制将当前录制的画面更换成设置的图像数据
    /// </summary>
    void SetCurrentRecoredImg(char *Data,int Width,int Height);
    /// <summary>
    /// 设置录制时的麦克风数据回调
    /// </summary>
    void SetMicRecorderCallBack(void (*dataCallBack)(unsigned char* dataBuf, UINT32 numFramesToRead));

private:
    //=========================================主要辅助函数=========================================//
    bool InitVideo();
    void UnInitVideo();
    bool InitAudio();
    void UnInitAudio();
    bool InitAudioMic();
    void UnInitAudioMic();
    bool StartThreadPre();
    void RecordThreadRun();
    void RecordThreadRun_Video();
    void RecordThreadRun_CapInner();
    void RecordThreadRun_CapMic();
    void RecordThreadRun_FilterMic();
    void RecordThreadRun_Mix();
    void RecordThreadRun_Write();
    int OpenOutPut();
    void CloseOutPut();
    int InitSwrInner();
    void UnInitSwrInner();
    int InitSwrMic();
    void UnInitSwrMic();
    int InitFilter();
    void UnInitFilter();
    int InitFilterMic();
    void UnInitFilterMic();
    int InitFifo();
    void UnInitFifo();
    //=========================================次要辅助函数=========================================//
    int find_audio_stream(AVFormatContext *fmt_ctx);
    //=========================================调用回馈函数=========================================//
private:
    void (*dataCallBackVar)(unsigned char*, UINT32) = nullptr;
};
