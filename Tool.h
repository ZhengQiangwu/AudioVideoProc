#pragma once
#include <opencv2/opencv.hpp> // 因为函数参数用到了 cv::Mat
#include <string>

class Tool
{
public:
	/// <summary>
	/// 获取缩放比
	/// </summary>
	static double GetZoom();
	/// <summary>
	/// 获取桌面图像数据
	/// </summary>
	/// <param name="Width">存储返回的宽</param>
	/// <param name="Height">存储返回的高</param>
	/// <param name="Data">存储返回的数据</param>
	/// <param name="DataSize">存储返回的数据大小</param>
	/// <returns>是否成功获取</returns>
	static bool GetDesktopImgData(int& Width, int& Height, char* &Data,long& DataSize);

	/// <summary>
	/// 去除图像右侧和底部的黑边
	/// </summary>
	/// <param name="ImgMat">传入的图像</param>
	static void RemoveBlackEdge(cv::Mat& ImgMat);
};

