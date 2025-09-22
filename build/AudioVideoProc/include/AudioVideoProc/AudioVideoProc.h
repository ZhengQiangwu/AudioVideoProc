#ifndef AUDIOVIDEOPROC_H
#define AUDIOVIDEOPROC_H

// 在 Linux 环境下，为共享库(.so)定义符号导出
#define AUDIOVIDEOPROC_API __attribute__((visibility("default")))

#include <cstdint>

// 为Linux定义Windows兼容类型
typedef uint32_t UINT32;
typedef uint64_t ULONGLONG;

// --- 修改开始 ---
// 明确地使用带有命名空间的前向声明
namespace cv {
    class VideoCapture;
}
// --- 修改结束 ---

/// <summary>
/// 摄像头异常回调函数
/// 1代表如果打开的摄像头中获取到了空帧，则补充黑屏图
/// 2代表此处通常是因为选择了摄像头，但摄像头未被打开导致来到这里的
/// </summary>
typedef void (*VideoCapErrCallBack)(int CapErrType);
/// <summary>
/// 音频异常回调函数
/// 1代表扬声器设备突然异常
/// 2代表麦克风设备突然异常
/// 3代表扬声器设备打开失败
/// 4代表麦克风设备打开失败
/// </summary>
typedef void (*AudioErrCallBack)(int AudioErrType);
/// <summary>
/// 摄像头数据回调函数
/// 返回摄像头序号，画面长宽，以及捕获到的摄像头数据和他的长度
/// </summary>
typedef void (*CapDataFuntion)(int CapNum, int W, int H, const char* Data, int DataSize);

extern "C" {
    /// <summary>
    /// 音视频模块命名空间(C导出)
    /// </summary>
    namespace AudioVideoProcNameSpace {
	 /// <summary>
        /// 启动一个使用预设参数的简易推流任务。
        /// 内部会自动处理模块创建和所有参数设置。
        /// </summary>
        /// <param name="ModuleNum">为本次推流任务指定一个唯一的模块号</param>
        /// <returns>true表示成功启动，false表示失败</returns>
        AUDIOVIDEOPROC_API bool StartPush(int ModuleNum);
        /// <summary>
        /// 启用Debug模式
        /// </summary>
        AUDIOVIDEOPROC_API void EnabelDebug();
        /// <summary>
        /// 新增可用模块
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool Module_New(int ModuleNum);
        /// <summary>
        /// 删除并回收已有模块
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool Module_Delete(int ModuleNum);
        /// <summary>
        /// 删除并回收所有已有模块
        /// </summary>
        AUDIOVIDEOPROC_API void Module_DeleteAll();
        /// <summary>
        /// 返回当前可用模块数量
        /// </summary>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int Module_Size();
        /// <summary>
        /// 装载可用模块
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool Init(int ModuleNum);
        /// <summary>
        /// 卸载可用模块
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        AUDIOVIDEOPROC_API void UnInit(int ModuleNum);
        /// <summary>
        /// 测试DLL载入是否成功
        /// </summary>
        /// <returns>返回一个2开头的整数</returns>
        AUDIOVIDEOPROC_API int Hello();
        /// <summary>
        /// 获取装载状态
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetIsInit(int ModuleNum);
        /// <summary>
        /// 设置视频码率
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="BitRate">视频码率</param>
        AUDIOVIDEOPROC_API void SetBitRate(int ModuleNum, int BitRate);
        /// <summary>
        /// 获取视频码率
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetBitRate(int ModuleNum);
        /// <summary>
        /// 设置视频图像组大小，设置大能降低视频体积，但太大了反而会增加
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="GopSize">图像组大小</param>
        AUDIOVIDEOPROC_API void SetGopSize(int ModuleNum, int GopSize);
        /// <summary>
        /// 获取视频图像组大小
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetGopSize(int ModuleNum);
        /// <summary>
        /// 设置最大b帧数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="MaxBFrames">最大b帧数</param>
        AUDIOVIDEOPROC_API void SetMaxBFrames(int ModuleNum, int MaxBFrames);
        /// <summary>
        /// 获取视频最大b帧数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetMaxBFrames(int ModuleNum);
        /// <summary>
        /// 设置编解码使用线程数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="ThreadCount">编解码使用线程数</param>
        AUDIOVIDEOPROC_API void SetThreadCount(int ModuleNum, int ThreadCount);
        /// <summary>
        /// 获取编解码使用线程数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetThreadCount(int ModuleNum);
        /// <summary>
        /// 设置编码器私有属性
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="Key">键</param>
        /// <param name="Value">值</param>
        AUDIOVIDEOPROC_API void SetPrivData(int ModuleNum, char* Key, char* Value);
        /// <summary>
        /// <para>获取编码器指定键下的私有属性值</para>
        /// <para>当前的代码默认设置是</para>
        /// <para>privDataMap["b-pyramid"] = "none";</para>
        /// <para>privDataMap["preset"] = "superfast";</para>
        /// <para>privDataMap["tune"] = "zerolatency";</para>
        /// <para>可设置的其余值，举出的部分例子是</para>
        /// <para>"b-pyramid": 用于控制 B 帧金字塔的设置。</para>
        /// <para>——"none": 禁用 B 帧金字塔。</para>
        /// <para>——"strict" : 严格的 B 帧金字塔设置。</para>
        /// <para>"preset" : 用于设置预设编码器参数，以平衡编码速度和质量。</para>
        /// <para>——"ultrafast" : 极快编码速度，质量较低。</para>
        /// <para>——"superfast" : 超快编码速度，质量稍低。</para>
        /// <para>——"fast" : 快速编码速度，质量适中。</para>
        /// <para>——"medium" : 中等编码速度，质量适中。</para>
        /// <para>——"slow" : 较慢编码速度，较高质量。</para>
        /// <para>"tune" : 用于调整编码器以适应特定场景。</para>
        /// <para>——"zerolatency" : 低延迟模式。</para>
        /// <para>——"film" : 适用于电影场景的设置。</para>
        /// <para>——"animation" : 适用于动画场景的设置。</para>
        /// <para>——"grain" : 适用于带有颗粒噪声的场景。</para>
        /// <para>——"psnr" : 优化 PSNR（峰值信噪比）。</para>
        /// <para>"x264opts" : 用于 x264 编码器的设置。</para>
        /// <para>——"subme" : 子像素运动估计设置。</para>
        /// <para>——"me_range" : 运动估计搜索范围设置。</para>
        /// <para>"x265-params" : 用于 x265 编码器的设置。</para>
        /// <para>——"aq-mode" : 适应性量化模式。</para>
        /// <para>"qp" : 用于指定量化参数（QP，Quantization Parameter）。</para>
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="Key">键</param>
        /// <returns>对应的值</returns>
        AUDIOVIDEOPROC_API const char* GetPrivData(int ModuleNum, char* Key);
        /// <summary>
        /// 设置音频采样率
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="NbSample">音频采样率</param>
        AUDIOVIDEOPROC_API void SetNbSample(int ModuleNum, int NbSample);
        /// <summary>
        /// 获取音频采样率
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetNbSample(int ModuleNum);
        /// <summary>
        /// 设置混音的滤波字符串，默认为[in0][in1]amix=inputs=2:duration=longest:dropout_transition=0:weights="1 0.25":normalize=0[out]
        /// 其中[in0][in1][out]应该保持 [in0]是扬声器 [in1]是麦克风 [out]是混音输出
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="MixFilterString">滤波字符串</param>
        AUDIOVIDEOPROC_API void SetMixFilter(int ModuleNum, char* MixFilterString);
        /// <summary>
        /// 获取混音的滤波字符串
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API const char* GetMixFilter(int ModuleNum);
        /// <summary>
        /// 设置麦克风处理的滤波字符串，默认为[in]highpass=200,lowpass=3000,afftdn[out]
        /// 其中[in][out]应该保持 [in]是麦克风 [out]是输出
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="MicFilterString"></param>
        AUDIOVIDEOPROC_API void SetMicFilter(int ModuleNum, char* MicFilterString);
        /// <summary>
        /// 获取麦克风处理的滤波字符串
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API const char* GetMicFilter(int ModuleNum);
        /// <summary>
        /// 获取30次采集内的平均补帧数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetFrameAppendNum(int ModuleNum);
        /// <summary>
        /// 获取扬声器设备是否已被成功使用
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetInnerReadyOk(int ModuleNum);
        /// <summary>
        /// 获取麦克风设备是否已被成功使用
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetMicReadyOk(int ModuleNum);
        /// <summary>
        /// 获取摄像头列表列表字符串(Unicode)，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <returns>名字0?名字1?名字2?</returns>
        AUDIOVIDEOPROC_API const char* GetCameraList();
        /// <summary>
        /// 根据摄像头名称获取摄像头下标
        /// </summary>
        AUDIOVIDEOPROC_API int GetCameraIndex(const char* cameraName);
        /// <summary>
        /// 获取指定摄像头的分辨率列表，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <param name="CameraNum">摄像头序号</param>
        /// <returns>返回类似640x320?160x120?的分辨率列表</returns>
        AUDIOVIDEOPROC_API const char* GetCameraWHList(int CameraNum);
        /// <summary>
        /// 设置画面传输:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="CameraNum">摄像头序号</param>
        /// <returns>设置成功与否</returns>
        AUDIOVIDEOPROC_API bool SetCamera(int ModuleNum, int CameraNum);
        /// <summary>
        /// 设置画面传输:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="CameraNum">摄像头序号</param>
        /// <param name="W">摄像头W分辨率</param>
        /// <param name="H">摄像头H分辨率</param>
        /// <returns>设置成功与否</returns>
        AUDIOVIDEOPROC_API bool SetCameraWH(int ModuleNum, int CameraNum, int W, int H);
        /// <summary>
        /// 获取当前画面传输模式:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetCamera(int ModuleNum);
        /// <summary>
        /// 获得指定摄像头序号的宽高:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="CameraNum">摄像头序号</param>
        /// <param name="W">摄像头W分辨率</param>
        /// <param name="H">摄像头H分辨率</param>
        /// <returns>是否获取成功</returns>
        AUDIOVIDEOPROC_API bool GetCameraWH(int ModuleNum, const int CameraNum, int* W, int* H);
        /// <summary>
        /// 设置次要画面传输:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="CameraNum">摄像头序号</param>
        /// <param name="W">摄像头W分辨率</param>
        /// <param name="H">摄像头H分辨率</param>
        /// <param name="WPercent">摄像头W所占主画面的百分比[0,100]</param>
        /// <param name="HPercent">摄像头H所占主画面的百分比[0,100]</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool SetSecondaryCamera(int ModuleNum, int CameraNum, int W, int H, int WPercent, int HPercent);
        /// <summary>
        /// 获得次要摄像头序号:屏幕传输(-1);一号摄像头传输(0);二号摄像头传输(1);三号摄...
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>当前的摄像头序号</returns>
        AUDIOVIDEOPROC_API int GetSecondaryCamera(int ModuleNum);
        /// <summary>
        /// 尝试在存在子画面的情况下，切换主画面和次要画面的位置
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool SwapRecordSubScreen(int ModuleNum);
        /// <summary>
        /// 设置摄像头异常回调函数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="videoCapFun">回调函数地址，用于提供摄像头异常信息</param>
        AUDIOVIDEOPROC_API void SetVideoCapErrFun(int ModuleNum, VideoCapErrCallBack videoCapFun);
        /// <summary>
        /// 设置音频设备异常回调函数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="AudioErrFun">回调函数地址，用于提供音频设备异常信息</param>
        AUDIOVIDEOPROC_API void SetAudioErrFun(int ModuleNum, AudioErrCallBack AudioErrFun);
        /// <summary>
        /// 设置录制的视频文件名
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="FileName">录制的视频文件名</param>
        AUDIOVIDEOPROC_API void SetRecordFileName(int ModuleNum, char* FileName);
        /// <summary>
        /// 设置RTMP推流地址
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="RtmpUrl">RTMP推流地址</param>
        AUDIOVIDEOPROC_API void SetRtmpUrl(int ModuleNum, char* RtmpUrl);
        /// <summary>
        /// 开始录制(沿袭参数)
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool StartRecord(int ModuleNum);
        /// <summary>
        /// 开始录制
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="IsRtmp">本次录制是否应推流，否则录制到本地</param>
        /// <param name="FrameRate">录制的视频帧率</param>
        /// <param name="IsRecordVideo">是否录制视频</param>
        /// <param name="IsRecordSound">是否录制系统内声音</param>
        /// <param name="IsRecordMicro">是否录制麦克风</param>
        /// <param name="SecondaryScreenLocation">次要画面位置(0无，[1-4][左上右下])</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool StartRecordWithSet(int ModuleNum, bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
        /// <summary>
        /// 继续录制
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool ContinueRecord(int ModuleNum);
        /// <summary>
        /// 暂停录制
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool PauseRecord(int ModuleNum);
        /// <summary>
        /// 停止录制
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool StopRecord(int ModuleNum);
        /// <summary>
        /// 完成录制
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool FinishRecord(int ModuleNum);
        /// <summary>
        /// 设置录制参数
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="IsRtmp">本次录制是否应推流，否则录制到本地</param>
        /// <param name="FrameRate">录制的视频帧率</param>
        /// <param name="IsRecordVideo">是否录制视频</param>
        /// <param name="IsRecordSound">是否录制系统内声音</param>
        /// <param name="IsRecordMicro">是否录制麦克风</param>
        /// <param name="SecondaryScreenLocation">次要画面位置(0无，[1-4][从左上角开始，顺时针放置])</param>
        AUDIOVIDEOPROC_API void SetRecordAttr(int ModuleNum, bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
        /// <summary>
        /// 当前是否设置为推流
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool IsRtmp(int ModuleNum);
        /// <summary>
        /// 设置是否同意补帧
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="IsAcceptAppendFrame">是否同意补帧</param>
        AUDIOVIDEOPROC_API void SetAcceptAppendFrame(int ModuleNum, bool IsAcceptAppendFrame);
        /// <summary>
        /// 当前是否同意补帧
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool IsAcceptAppendFrame(int ModuleNum);
        /// <summary>
        /// 是否正在录制/推流中
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>返回一个在StartRecord函数后能够检测是否已经在录制推流的布尔值</returns>
        AUDIOVIDEOPROC_API bool IsRecording(int ModuleNum);
        /// <summary>
        /// 获取帧率
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API int GetRecordAttrFrameRate(int ModuleNum);
        /// <summary>
        /// 获取是否录制视频
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetRecordAttrRecordVideo(int ModuleNum);
        /// <summary>
        /// 获取是否录制系统内声音
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetRecordAttrRecordSound(int ModuleNum);
        /// <summary>
        /// 获取是否录制麦克风
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API bool GetRecordAttrIsRecordMicro(int ModuleNum);
        /// <summary>
        /// 获取当前录制文件名
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API const char* GetOutFileName(int ModuleNum);
        /// <summary>
        /// 获取当前推流地址
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns></returns>
        AUDIOVIDEOPROC_API const char* GetRtmpAddr(int ModuleNum);
        /// <summary>
        /// 获取真实屏幕分辨率(计算进了屏幕缩放比)，格式是X?Y?Zoom?，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <returns></returns>
        AUDIOVIDEOPROC_API const char* GetScreenInfo();
         /// <summary>
        /// 获取麦克风列表字符串，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <returns>名字0?名字1?名字2?</returns>
        AUDIOVIDEOPROC_API const char* GetMicList();
        /// <summary>
        /// 获取麦克风设备当前启用的模式
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>0代表采用默认麦克风,1代表自动选择可用麦克风,2代表采用指定序号的麦克风</returns>
        AUDIOVIDEOPROC_API int GetMicPattern(int ModuleNum);
        /// <summary>
        /// 设置麦克风设备当前启用的模式
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="MicPattern">采用的模式 0代表采用默认麦克风 1代表自动选择可用麦克风 2代表采用指定序号的麦克风</param>
        /// <param name="MicNum">麦克风序号，仅模式2时有效</param>
        /// <returns>true成功</returns>
        AUDIOVIDEOPROC_API bool SetMicPattern(int ModuleNum, int MicPattern, int MicNum);
        /// <summary>
        /// 设置录制区域XYWH
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="X">左上角X</param>
        /// <param name="Y">左上角Y</param>
        /// <param name="Width">录制区域W</param>
        /// <param name="Height">录制区域H</param>
        AUDIOVIDEOPROC_API void SetRecordXYWH(int ModuleNum, int X, int Y, int Width, int Height);
        /// <summary>
        /// 获取录制区域XYWH，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>X?Y?W?H?</returns>
        AUDIOVIDEOPROC_API const char* GetRecordXYWH(int ModuleNum);
        /// <summary>
        /// 设置录制视频的固定宽高
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="Width">视频宽</param>
        /// <param name="Height">视频高</param>
        AUDIOVIDEOPROC_API void SetRecordFixSize(int ModuleNum, int Width, int Height);
        /// <summary>
        /// 获取录制视频的固定宽高，其中?是分隔符，通过GetSplitStr函数获取
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <returns>W?H?</returns>
        AUDIOVIDEOPROC_API const char* GetRecordFixSize(int ModuleNum);
        /// <summary>
        /// 截图，其中?是分隔符，通过GetSplitStr函数获取
        /// 注意，只有成功了才会有后面的数据
        /// </summary>
        /// <param name="CapNum"> -1桌面 0摄像头 1摄像头...</param>
        /// <param name="FilePath">保存路径</param>
        /// <param name="dataCallBack">数据处理回调</param>
        /// <returns>dataCallBack回调函数的返回值</returns>
        AUDIOVIDEOPROC_API bool SnapShoot(int CapNum, const char* FilePath,
            bool (*dataCallBack)(int capNum, int width, int height, long dataLen, char* data));
        /// <summary>
        /// 截图，其中?是分隔符，通过GetSplitStr函数获取
        /// 注意，只有成功了才会有后面的数据
        /// </summary>
        /// <param name="CapNum"> -1桌面 0摄像头 1摄像头...</param>
        /// <param name="CapWidth">摄像头截图指定分辨率宽(0代表当前宽)</param>
        /// <param name="CapHeight">摄像头截图指定分辨率高(0代表当前高)</param>
        /// <param name="FilePath">保存路径(为空则不保存)</param>
        /// <param name="IsReturnData">是否需要在返回值中存放数据</param>
        /// <param name="IsCutBlackEdge">是否需要把结果图中可能的黑边去除（右边和下边）并拉伸回原貌</param>
        /// <param name="dataCallBack">数据处理回调</param>
        /// <returns>dataCallBack回调函数的返回值</returns>
        AUDIOVIDEOPROC_API bool SnapShootWH(int CapNum, int CapWidth, int CapHeight, const char* FilePath, bool IsReturnData, bool IsCutBlackEdge,
            bool (*dataCallBack)(int capNum, int width, int height, long dataLen, char* data));
        /// <summary>
        /// 获取分隔字符串
        /// </summary>
        /// <returns>分隔字符串</returns>
        AUDIOVIDEOPROC_API const char* GetSplitStr();
        /// <summary>
        /// 开始获取摄像头数据并返回
        /// 当数据为空，则表示采集异常，同时DataSize为0
        /// </summary>
        /// <param name="CapNum">要录制的摄像头序号</param>
        /// <param name="CapW">摄像头分辨率宽</param>
        /// <param name="CapH">摄像头分辨率高</param>
        /// <param name="CapTime">每多少毫秒采集一次</param>
        /// <param name="CallBackFun">回调函数地址，用以返回采集的数据和数据大小</param>
        /// <returns>是否成功开始获取</returns>
        AUDIOVIDEOPROC_API bool StartCapForCallBack(int CapNum, int CapW, int CapH, int CapTime, CapDataFuntion CallBackFun);
        /// <summary>
        /// 终止摄像头采集并回收线程，与FinishCapForCallBack相同
        /// </summary>
        /// <param name="CapNum">被录制的摄像头序号</param>
        AUDIOVIDEOPROC_API void StopCapForCallBack(int CapNum);
        /// <summary>
        /// 终止摄像头采集并回收线程，与StopCapForCallBack相同
        /// </summary>
        /// <param name="CapNum">被录制的摄像头序号</param>
        AUDIOVIDEOPROC_API void FinishCapForCallBack(int CapNum);
        /// <summary>
        /// 显示图像数据
        /// </summary>
        /// <param name="Data">要显示的图像数据</param>
        /// <param name="Width">图像宽</param>
        /// <param name="Height">图像高</param>
        /// <param name="Channels">通道数，摄像头填3，位图填4</param>
        AUDIOVIDEOPROC_API void ShowMat(char* Data, int Width, int Height, int Channels);
        /// <summary>
        /// 显示图像数据
        /// </summary>
        /// <param name="WindowName">窗口名称</param>
        /// <param name="Data">要显示的图像数据</param>
        /// <param name="Width">图像宽</param>
        /// <param name="Height">图像高</param>
        /// <param name="Channels">通道数，摄像头填3，位图填4</param>
        AUDIOVIDEOPROC_API void ShowMatWithName(const char* WindowName, char* Data, int Width, int Height, int Channels);
        /// <summary>
        /// 打印摄像头信息
        /// </summary>
        /// <param name="videoCap">摄像头指针</param>
        AUDIOVIDEOPROC_API void ShowVideoCapInfo(cv::VideoCapture* videoCap);
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
        AUDIOVIDEOPROC_API int CheckCameraType(int CameraIndex);
        /// <summary>
        /// 设置是否理会摄像头属性警告
        /// </summary>
        AUDIOVIDEOPROC_API void SetMindCapAttrWarn(bool IsMind);
        /// <summary>
        /// 获取当前是否理会摄像头属性警告，如果理会，将会因为警告而取消摄像头打开
        /// </summary>
        AUDIOVIDEOPROC_API bool GetMindCapAttrWarn();
        /// <summary>
        /// 设置录制时的麦克风数据回调
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="dataCallBack">回调方法</param>
        AUDIOVIDEOPROC_API void SetMicRecorderCallBack(int ModuleNum, void(*dataCallBack)(unsigned char* dataBuf, UINT32 numFramesToRead));
        /// <summary>
        /// 设置日志信息回调函数
        /// </summary>
        /// <param name="LogCallBackFunVar">信息回调函数地址</param>
        AUDIOVIDEOPROC_API void SetCallBack(void (*LogCallBackFunVar)(const char* Message));
        /// <summary>
        /// 强制将当前录制的画面更换成设置的图像数据，并在再次设置之前保持该更改，直至设置为nullptr
        /// </summary>
        /// <param name="ModuleNum">模块序号</param>
        /// <param name="Data">图像数据</param>
        /// <param name="Width">宽</param>
        /// <param name="Height">高</param>
        AUDIOVIDEOPROC_API void SetCurrentRecoredImg(int ModuleNum, char* Data, int Width, int Height);
        /// <summary>
        /// 对图像数据做宽高调整
        /// </summary>
        /// <param name="Data">原始图像数据</param>
        /// <param name="width">原始宽</param>
        /// <param name="height">原始高</param>
        /// <param name="Channels">原始通道数</param>
        /// <param name="outWidth">输出宽</param>
        /// <param name="outHeight">输出高</param>
        /// <param name="OutChannels">输出通道数</param>
        /// <param name="resizeDataFun">结果数据回调</param>
        AUDIOVIDEOPROC_API void ResizeImg(char* Data, int Width, int Height, int Channels, int OutWidth, int OutHeight, int OutChannels
            , void (*ResizeDataFun)(char* Data, int OutWidth, int OutHeight, int OutChannels));
    }
}

#endif // AUDIOVIDEOPROC_H
