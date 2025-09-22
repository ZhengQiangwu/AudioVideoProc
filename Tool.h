#pragma once
#include <opencv2/opencv.hpp> // ��Ϊ���������õ��� cv::Mat
#include <string>

class Tool
{
public:
	/// <summary>
	/// ��ȡ���ű�
	/// </summary>
	static double GetZoom();
	/// <summary>
	/// ��ȡ����ͼ������
	/// </summary>
	/// <param name="Width">�洢���صĿ�</param>
	/// <param name="Height">�洢���صĸ�</param>
	/// <param name="Data">�洢���ص�����</param>
	/// <param name="DataSize">�洢���ص����ݴ�С</param>
	/// <returns>�Ƿ�ɹ���ȡ</returns>
	static bool GetDesktopImgData(int& Width, int& Height, char* &Data,long& DataSize);

	/// <summary>
	/// ȥ��ͼ���Ҳ�͵ײ��ĺڱ�
	/// </summary>
	/// <param name="ImgMat">�����ͼ��</param>
	static void RemoveBlackEdge(cv::Mat& ImgMat);
};

