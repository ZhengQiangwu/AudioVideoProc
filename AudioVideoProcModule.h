#pragma once //ֻ����һ��

#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <thread>
#include <mutex>
// --- �޸Ŀ�ʼ ---
// ʹ�ñ�׼ͷ�ļ� <cstdint> ��ȷ�����Ͷ����ͳһ��׼ȷ��
#include <cstdint>
// --- �޸Ľ��� ---

// ΪFFmpeg��OpenCV�����ṩǰ��������������ͷ�ļ��������������
struct AVFormatContext;
struct AVCodec;
struct AVCodecContext;
struct AVStream;
struct AVFilterGraph;
struct AVFilterContext;
struct SwrContext;
struct AVAudioFifo;
namespace cv { class Mat; }

// --- �޸Ŀ�ʼ ---
// ͳһʹ�� <cstdint> �еı�׼����
typedef uint32_t UINT32;
typedef uint64_t ULONGLONG;
// --- �޸Ľ��� ---

/// <summary>
/// ����ͷ�쳣�ص�����
/// </summary>
typedef void (*VideoCapErrCallBack)(int CapErrType);
/// <summary>
/// ��Ƶ�쳣�ص�����
/// </summary>
typedef void (*AudioErrCallBack)(int AudioErrType);


/// <summary>
/// ����Ƶ����ģ����
/// </summary>
class  AudioVideoProcModule {
public:
    /// <summary>
    /// Ĭ�Ϲ���
    /// </summary>
    AudioVideoProcModule();
    /// <summary>
    /// Ĭ������
    /// </summary>
    ~AudioVideoProcModule();
public:
    /// <summary>
    /// ¼��/���� ����״̬
    /// </summary>
    enum class RecordType { Stop, Record, Pause };
private:
    // --- �޸Ŀ�ʼ: �������ͬ������ĳ�Ա���� ---

    // ����ȷ����Ƶ����һ֡��I֡�ı�־λ
    bool first_frame_sent = false;

    // ��������Ƶ������ʱ�����һ���ı���
    int64_t video_first_dts = -1; 
    int64_t audio_first_dts = -1;
    
    // ����ȷ��ʱ������������ı���
    int64_t last_video_pts = -1;
    int64_t last_audio_pts = -1;

    // --- �޸Ľ��� ---
    std::mutex video_pts_mutex;
    std::mutex audio_pts_mutex;
    
    #ifdef __linux__
    pid_t mic_arecord_pid = -1; // ���ڴ洢 arecord �ӽ��̵�ID
    // ʹ�� FILE* ָ�������� popen ���صĹܵ������
    //FILE* mic_pipe_handle = nullptr; 
    #endif
    bool isInit{};				        //ģ���Ѿ����أ�
    volatile bool isRecordVideo{};		//�Ƿ�¼����Ƶ
    volatile bool isRecordInner{};		//�Ƿ�¼������
    volatile bool isRecordMic{};	    //�Ƿ�¼����˷�
    bool isRtmp{};                      //�Ƿ���rtmp����
    int secondaryScreenLocation{};      //��Ҫ��Ļλ��
    bool isAcceptAppendFrame{};             //�Ƿ�����֡
    int frameRate{};					    //֡��
    int bitRate{};                          //��Ƶ����
    int gopSize{};                          //ͼ�����С
    int maxBFrames{};                       //���b֡��
    int threadCount{};                      //ʹ���߳�����
    std::map<std::string, std::string> privDataMap{};      //���ñ�������˽������
    int nbSample{};                         //��Ƶ������
    std::string mixFilterString;                 //�����˲��ַ���
    std::string micFilterString;                 //��˷��˲��ַ���
    std::list<int> frameAppendHistory;           //��ʷ��֡��
    volatile char isCanCap{};               //�Ƿ�׼����¼����
    volatile RecordType recordType{};       //¼��״̬
    VideoCapErrCallBack videoCapErr{};      //����ͷ����ص�����
    AudioErrCallBack audioErr{};            //��Ƶ�豸����ص�����
    std::unique_ptr<std::thread> recordThread{};	            //¼�����߳�
    std::unique_ptr<std::thread> recordThread_Video{};	    //¼����Ƶ���߳�
    std::unique_ptr<std::thread> recordThread_CapInner{};	    //¼����Ƶ֮�ɼ��߳�
    std::unique_ptr<std::thread> recordThread_CapMic{};	    //¼����˷�֮�ɼ��߳�
    std::unique_ptr<std::thread> recordThread_FilterMic{};	//¼����˷�֮�����߳�
    std::unique_ptr<std::thread> recordThread_Mix{};	        //��Ƶ֮�����߳�
    std::unique_ptr<std::thread> recordThread_Write{};	    //¼����Ƶ֮д���߳�

    int cameraNum{};		//��ǰ����¼����ʹ�õ�����ͷ���
    int secondaryCameraNum{};		//��ǰ��Ҫ����¼����ʹ�õ�����ͷ���
    int secondaryWPercent;          //��Ҫ�����ռ��Ҫ����İٷֱ�[0,100]
    int secondaryHPercent;          //��Ҫ�����ռ��Ҫ����İٷֱ�[0,100]
    int micPattern{};         //��ǰ��˷�ģʽ
    int micNum{};             //��ǰָ����˷����

    std::string recordFileName{};   //����ļ���
    std::string pushRtmpUrl{};       //rtmp��ַ
    int videoWidth{};       //��Ƶ��
    int videoHeight{};      //��Ƶ��
    int recordX{};
    int recordY{};
    int recordWidth{};           //¼�����������
    int recordHeight{};         //¼�����������
    int resizeWidth{};          //�ɼ�����Ӧ���γɵ�ָ����0��ʾ������
    int resizeHeight{};         //�ɼ�����Ӧ���γɵ�ָ���ߣ�0��ʾ������
    int screenW{};              //��Ļ���ڿ�ʼ¼��ǰ��Ԥ׼������ʼ��
    int screenH{};              //��Ļ�ߣ��ڿ�ʼ¼��ǰ��Ԥ׼������ʼ��

    AVFormatContext* pFormatCtxOut{};        //����ļ�������
    AVCodec* pCodecEncode_Video{};            //��Ƶ������
    AVCodec* pCodecEncode_Audio{};            //��Ƶ������
    AVCodecContext* pCodecEncodeCtx_Video{};  //��Ƶ������������
    AVCodecContext* pCodecEncodeCtx_Audio{};  //��Ƶ������������
    AVStream* pStream_Audio{};
    AVStream* pStream_Video{};
    int streamIndex_Video{};    //�����
    int streamIndex_Audio{};    //�����
    
    // ʹ��FFmpeg�������滻Windows��Ƶ�ӿ�
    AVFormatContext* pFormatCtxIn_Inner{};
    AVCodecContext* pCodecDecodeCtx_Inner{};
    int streamIndexIn_Inner = -1;
    AVFormatContext* pFormatCtxIn_Mic{};
    AVCodecContext* pCodecDecodeCtx_Mic{};
    int streamIndexIn_Mic = -1;

    //�ٽ��� (ʹ��pthread_mutex_t�滻CRITICAL_SECTION)
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

    SwrContext* pSwrCtx_Inner{};            //��Ƶ�ز���������
    SwrContext* pSwrCtx_Mic{};              //��˷��ز���������
    AVAudioFifo* pAudioFifo_Inner{};        //��Ƶ������
    AVAudioFifo* pAudioFifo_Mic{};          //��Ƶ������
    AVAudioFifo* pAudioFifo_Mic_Filter{};   //��Ƶ������
    AVAudioFifo* pAudioFifo_Mix{};          //��Ƶ������

    //��д��֡��
    ULONGLONG allVideoFrame{};
    ULONGLONG allAudioFrame{};

    //������
    pthread_mutex_t micMutex_pthread; //��˷��л�������
    //std::mutex micMutex;
    
    //ǿ��¼�ƻ�������
    char* mFixImgData{nullptr};
    bool mFixImgMatChange{ false };
    cv::Mat mFixImgMat;
    std::mutex mFixImgDataMutex; //¼�ƻ�����

public:
    /// <summary>
    /// װ��ģ���Գ�ʼ��
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool Init();
    /// <summary>
    /// ж��ģ���Ի�����Դ
    /// </summary>
    void UnInit();

    /// <summary>
    /// ����¼���ļ�����
    /// </summary>
    /// <param name="FileName">¼���ļ������ƣ�Ҳ������·��</param>
    void SetRecordFileName(const std::string& FileName);
    /// <summary>
    /// ����RTMP������ַ
    /// </summary>
    /// <param name="RtmpUrl">����Ŀ�ĵص�URL</param>
    void SetRtmpUrl(const std::string& RtmpUrl);
    /// <summary>
    /// ��ʼ¼��/����(��Ϯ����)��������Ĳ�������ֱ�ӿ�ʼ¼��
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool StartRecord();
    /// <summary>
    /// ��ʼ¼��/����(��Ϯ����)��������Ĳ�������ֱ�ӿ�ʼ¼��
    /// </summary>
    /// <param name="IsRtmp">����¼���Ƿ�Ӧ����������¼�Ƶ�����</param>
    /// <returns>true�ɹ�</returns>
    bool StartRecord(bool IsRtmp);
    /// <summary>
    /// ��ʼ¼��/����
    /// </summary>
    /// <param name="IsRtmp">����¼���Ƿ�Ӧ����������¼�Ƶ�����</param>
    /// <param name="FrameRate">¼�Ƶ���Ƶ֡��</param>
    /// <param name="IsRecordVideo">�Ƿ�¼����Ƶ</param>
    /// <param name="IsRecordSound">�Ƿ�¼��ϵͳ������</param>
    /// <param name="IsRecordMicro">�Ƿ�¼����˷�</param>
    /// <param name="SecondaryScreenLocation">��Ҫ����λ��(0�ޣ�[1-4][��������])</param>
    /// <returns>true�ɹ�</returns>
    bool StartRecord(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
    /// <summary>
    /// ����¼��/����
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool ContinueRecord();
    /// <summary>
    /// ��ͣ¼��/����
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool PauseRecord();
    /// <summary>
    /// ֹͣ¼��/�������ȼ���FinishRecord
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool StopRecord();
    /// <summary>
    /// ���¼��/�������ȼ���StopRecord
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool FinishRecord();
    /// <summary>
    /// ��������ͷ�쳣�ص�����
    /// </summary>
    /// <param name="VideoCapErrFun">�ص������������ṩ����ͷ�쳣��Ϣ</param>
    void SetVideoCapErrFun(VideoCapErrCallBack VideoCapErrFun);
    /// <summary>
    /// ������Ƶ�豸�쳣�ص�����
    /// </summary>
    /// <param name="AudioErrFun">�ص������������ṩ��Ƶ�豸�쳣��Ϣ</param>
    void SetAudioErrFun(AudioErrCallBack AudioErrFun);
    /// <summary>
    /// ���û��洫��:��Ļ����(-1);һ������ͷ����(0);��������ͷ����(1);������...
    /// </summary>
    /// <param name="CameraNum">����ͷ���</param>
    /// <param name="W">����ͷW�ֱ���</param>
    /// <param name="H">����ͷH�ֱ���</param>
    /// <returns>true�ɹ�</returns>
    bool SetCamera(const int& CameraNum, const int& W = 0, const int& H = 0);
    /// <summary>
    /// �������ͷ���:��Ļ����(-1);һ������ͷ����(0);��������ͷ����(1);������...
    /// </summary>
    /// <returns>��ǰ������ͷ���</returns>
    int GetCamera()const;
    /// <summary>
    /// ���ָ������ͷ��ŵĿ��:��Ļ����(-1);һ������ͷ����(0);��������ͷ����(1);������...
    /// </summary>
    /// <param name="CameraNum">����ͷ���</param>
    /// <param name="W">����ͷW�ֱ���</param>
    /// <param name="H">����ͷH�ֱ���</param>
    /// <returns>�Ƿ��ȡ�ɹ�</returns>
    bool GetCameraWH(const int& CameraNum, int& W, int& H)const;
    /// <summary>
    /// ���ô�Ҫ���洫��:��Ļ����(-1);һ������ͷ����(0);��������ͷ����(1);������...
    /// </summary>
    /// <param name="CameraNum">����ͷ���</param>
    /// <param name="W">����ͷW�ֱ���</param>
    /// <param name="H">����ͷH�ֱ���</param>
    /// <param name="WPercent">����ͷW��ռ������İٷֱ�[0,100]</param>
    /// <param name="HPercent">����ͷH��ռ������İٷֱ�[0,100]</param>
    /// <returns>true�ɹ�</returns>
    bool SetSecondaryCamera(const int& CameraNum, const int& W = 640, const int& H = 480, const int& WPercent = 40, const int& HPercent = 30);
    /// <summary>
    /// ��ô�Ҫ����ͷ���:��Ļ����(-1);һ������ͷ����(0);��������ͷ����(1);������...
    /// </summary>
    /// <returns>��ǰ������ͷ���</returns>
    int GetSecondaryCamera()const;
    /// <summary>
    /// �����ڴ����ӻ��������£��л�������ʹ�Ҫ�����λ��
    /// </summary>
    /// <returns>true�ɹ�</returns>
    bool SwapRecordSubScreen();
    /// <summary>
    /// ����¼��/��������
    /// </summary>
    /// <param name="IsRtmp">�Ƿ�����������¼��</param>
    /// <param name="FrameRate">��Ƶ¼��֡��</param>
    /// <param name="IsRecordVideo">�Ƿ�¼����Ƶ</param>
    /// <param name="IsRecordSound">�Ƿ�¼��ϵͳ������</param>
    /// <param name="IsRecordMicro">�Ƿ�¼����˷�</param>
    /// <param name="SecondaryScreenLocation">��Ҫ����λ��(0�ޣ�[1-4][�����Ͻǿ�ʼ��˳ʱ�����])</param>
    void SetRecordAttr(bool IsRtmp, int FrameRate, bool IsRecordVideo, bool IsRecordSound, bool IsRecordMicro, int SecondaryScreenLocation);
    /// <summary>
    /// ��ǰ�Ƿ�����������Ϊ¼��
    /// </summary>
    /// <returns>true����</returns>
    bool IsRtmp()const;
    /// <summary>
    /// �����Ƿ�ͬ�ⲹ֡
    /// </summary>
    /// <param name="IsAcceptAppendFrame"></param>
    void SetAcceptAppendFrame(bool IsAcceptAppendFrame);
    /// <summary>
    /// ��ǰ�Ƿ�ͬ�ⲹ֡
    /// </summary>
    /// <returns></returns>
    bool IsAcceptAppendFrame()const;
    /// <summary>
    /// �Ƿ�����¼��/������
    /// </summary>
    /// <returns>����һ����StartRecord�������ܹ�����Ƿ��Ѿ���¼�������Ĳ���ֵ</returns>
    bool IsRecording()const;
    /// <summary>
    /// ������Ƶ¼��֡��
    /// </summary>
    /// <returns></returns>
    int GetRecordAttrFrameRate()const;
    /// <summary>
    /// ���ص�ǰ�Ƿ�¼����Ƶ
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrRecordVideo()const;
    /// <summary>
    /// ���ص�ǰ�Ƿ�¼��ϵͳ������
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrRecordSound()const;
    /// <summary>
    /// ���ص�ǰ�Ƿ�¼����˷�
    /// </summary>
    /// <returns></returns>
    bool GetRecordAttrIsRecordMicro()const;
    /// <summary>
    /// ���ص�ǰ��¼���ļ�·��
    /// </summary>
    /// <returns></returns>
    std::string GetOutFileName()const;
    /// <summary>
    /// ���ص�ǰ������URL
    /// </summary>
    /// <returns></returns>
    std::string GetRtmpUrl()const;
    /// <summary>
    /// ��ȡװ��״̬
    /// </summary>
    /// <returns></returns>
    bool GetIsInit()const;
    /// <summary>
    /// ������Ƶ����
    /// </summary>
    /// <param name="BitRate">��Ƶ����</param>
    void SetBitRate(int BitRate);
    /// <summary>
    /// ��ȡ��Ƶ����
    /// </summary>
    /// <returns></returns>
    int GetBitRate()const;
    /// <summary>
    /// ������Ƶͼ�����С�����ô��ܽ�����Ƶ�������̫���˷���������
    /// </summary>
    /// <param name="GopSize">ͼ�����С</param>
    void SetGopSize(int GopSize);
    /// <summary>
    /// ��ȡ��Ƶͼ�����С
    /// </summary>
    /// <returns></returns>
    int GetGopSize()const;
    /// <summary>
    /// �������b֡��
    /// </summary>
    /// <param name="MaxBFrames">���b֡��</param>
    void SetMaxBFrames(int MaxBFrames);
    /// <summary>
    /// ��ȡ��Ƶ���b֡��
    /// </summary>
    /// <returns></returns>
    int GetMaxBFrames()const;
    /// <summary>
    /// ���ñ����ʹ���߳���
    /// </summary>
    /// <param name="ThreadCount">�����ʹ���߳���</param>
    void SetThreadCount(int ThreadCount);
    /// <summary>
    /// ��ȡ�����ʹ���߳���
    /// </summary>
    /// <returns></returns>
    int GetThreadCount()const;
    /// <summary>
    /// ���ñ�����˽������
    /// </summary>
    /// <param name="Key">��</param>
    /// <param name="Value">ֵ</param>
    void SetPrivData(const std::string& Key, const std::string& Value);
    /// <summary>
    /// ��ȡ������ָ�����µ�˽������ֵ
    /// </summary>
    std::string GetPrivData(const std::string& Key)const;
    /// <summary>
    /// ������Ƶ������
    /// </summary>
    /// <param name="NbSample">��Ƶ������</param>
    void SetNbSample(int NbSample);
    /// <summary>
    /// ��ȡ��Ƶ������
    /// </summary>
    /// <returns></returns>
    int GetNbSample()const;
    /// <summary>
    /// ���û������˲��ַ���
    /// </summary>
    void SetMixFilter(const std::string& MixFilterString);
    /// <summary>
    /// ��ȡ�������˲��ַ���
    /// </summary>
    std::string GetMixFilter()const;
    /// <summary>
    /// ������˷紦����˲��ַ���
    /// </summary>
    void SetMicFilter(const std::string& MicFilterString);
    /// <summary>
    /// ��ȡ��˷紦����˲��ַ���
    /// </summary>
    std::string GetMicFilter()const;
    /// <summary>
    /// ��ȡ30�βɼ��ڵ�ƽ����֡��
    /// </summary>
    int GetFrameAppendNum();
    /// <summary>
    /// ��ȡ�������豸�Ƿ��ѱ��ɹ�ʹ��
    /// </summary>
    bool GetInnerReadyOk();
    /// <summary>
    /// ��ȡ��˷��豸�Ƿ��ѱ��ɹ�ʹ��
    /// </summary>
    bool GetMicReadyOk();
    /// <summary>
    /// ��ȡ��˷��豸��ǰ���õ�ģʽ
    /// </summary>
    int GetMicPattern() const;
    /// <summary>
    /// ������˷��豸��ǰ���õ�ģʽ
    /// </summary>
    bool SetMicPattern(const int& MicPattern, const int& MicNum = 0);
    /// <summary>
    /// ����¼�������XYWH
    /// </summary>
    void SetRecordXYWH(const int& X, const int& Y, const int& Width, const int& Height);
    /// <summary>
    /// ��ȡ¼������
    /// </summary>
    void GetRecordXYWH(int& X, int& Y, int& Width, int& Height);
    /// <summary>
    /// ����¼����Ƶ�Ĺ̶����
    /// </summary>
    void SetRecordFixSize(const int& Width, const int& Height);
    /// <summary>
    /// ��ȡ¼����Ƶ�Ĺ̶����
    /// </summary>
    void GetRecordFixSize(int& Width, int& Height);
    /// <summary>
    /// ǿ�ƽ���ǰ¼�ƵĻ�����������õ�ͼ������
    /// </summary>
    void SetCurrentRecoredImg(char *Data,int Width,int Height);
    /// <summary>
    /// ����¼��ʱ����˷����ݻص�
    /// </summary>
    void SetMicRecorderCallBack(void (*dataCallBack)(unsigned char* dataBuf, UINT32 numFramesToRead));

private:
    //=========================================��Ҫ��������=========================================//
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
    //=========================================��Ҫ��������=========================================//
    int find_audio_stream(AVFormatContext *fmt_ctx);
    //=========================================���û�������=========================================//
private:
    void (*dataCallBackVar)(unsigned char*, UINT32) = nullptr;
};
