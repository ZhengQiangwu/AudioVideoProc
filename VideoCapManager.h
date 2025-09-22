#pragma once
#include <map>
#include <mutex>
#include <opencv2/opencv.hpp> // ����OpenCVͷ�ļ���ʶ��cv::Mat

// MediaFrameCapture ֻ��Ҫǰ���������㹻�ˣ����Ա���ѭ������
class MediaFrameCapture;
class VideoCapManager
{
private:
	VideoCapManager();
	~VideoCapManager();

public:
	static VideoCapManager* Default();
	// --- �޸Ŀ�ʼ: ��ֹ�����͸�ֵ�����ǵ���ģʽ�����ʵ�� ---
    	VideoCapManager(const VideoCapManager&) = delete;
    	VideoCapManager& operator=(const VideoCapManager&) = delete;
    	// --- �޸Ľ��� ---
    
//private:
	//static VideoCapManager* self;

public:
	/// <summary>
	/// ������ͷ������Ѿ��У�Ȩ��+1
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
	/// <param name="W">�ֱ���W</param>
	/// <param name="H">�ֱ���H</param>
	/// <returns>����ͷ�Ƿ�򿪳ɹ�</returns>
	bool OpenCamera(int CapNum, int W = 0, int H = 0);

	/// <summary>
	/// �߼��ر�����ͷ��Ȩ��-1�����Ϊ0����Ĺر�����ͷ
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
	void CloseCamera(int CapNum);

	/// <summary>
	/// �Ƴ�����ͷ,����Ȩ�أ�������ͷ�Ƴ�
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
	void RemoveCamera(int CapNum);

	/// <summary>
	/// ���Ѿ��򿪵�����ͷ��ȡMat
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
	/// <param name="MatFromCap">Matͼ��</param>
	bool GetMatFromCamera(int CapNum, cv::Mat& MatFromCap);
	/// <summary>
	/// ���Ѿ��򿪵�����ͷ��ȡMat
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
		/// <param name="CapWidth">��ͼָ���ֱ��ʿ�(0����ǰ��)</param>
		/// <param name="CapHeight">��ͼָ���ֱ��ʸ�(0����ǰ��)</param>
	/// <param name="MatFromCap">Matͼ��</param>
	bool GetMatFromCamera(int CapNum, int CapWidth, int CapHeight, cv::Mat& MatFromCap);

	/// <summary>
	/// ��ȡ�Ѿ�����������ͷ���
	/// </summary>
	/// <param name="CapNum">����ͷ���</param>
	/// <param name="W">�洢W</param>
	/// <param name="H">�洢H</param>
	/// <returns>�Ƿ�ɹ���ȡ</returns>
	bool GetCameraWH(int CapNum, int& W, int& H);

	/// <summary>
	/// ���ָ������ͷ��״̬
	/// </summary>
	/// <param name="CameraIndex">����ͷ���</param>
	/// <returns>
	/// <para>0����ͷ������ʹ���� </para>
	/// <para>1����ͷδʹ��</para>
	/// <para>2����ͷ����ʹ�ù���������Ȩ��Ϊ0</para>
	/// <para>3����ͷӦ������ʹ�ã�����⵽δ��</para>
	/// <para>4����ͷ�����쳣�����ܶ���</para>
	/// <para>5����ͷ��ȡ����Ϊ��</para>
	/// <para>6����ͷ��Ȼ�ڼ������Ϊ�ڻ��߻�</para>
	/// </returns>
	int CheckCamera(int CameraIndex);
	/// <summary>
	/// �����Ƿ��������ͷ���Ծ��棬�����ᣬ������Ϊ�����ȡ������ͷ��
	/// </summary>
	/// <param name="IsMind">�Ƿ����</param>
	void SetMindCapAttrWarn(bool IsMind);
	/// <summary>
	/// ��ȡ��ǰ�Ƿ��������ͷ���Ծ��棬�����ᣬ������Ϊ�����ȡ������ͷ��
	/// </summary>
	bool GetMindCapAttrWarn();
private:
	// --- �޸Ŀ�ʼ: ��map�д洢����ָ�룬�����Ƕ����� ---
	std::map<int, std::pair<int, std::shared_ptr<MediaFrameCapture>>> videoCapMap;
    // --- �޸Ľ��� ---
	//std::mutex videoCapMutex;
	bool isMindCapAttrWarn;
	// --- �޸Ŀ�ʼ: ���һ������������Ļ����� ---
	std::mutex videoCapMutex;
	// --- �޸Ľ��� ---
};

