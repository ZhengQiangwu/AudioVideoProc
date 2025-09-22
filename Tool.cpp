#include <opencv2/opencv.hpp>
#include "Tool.h"
#include "Log.h"

// --- Linux平台特定的头文件 ---
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// -----------------------------
using namespace std;
using namespace cv;

/// <summary>
/// // 返回当前显示器的缩放比例
/// </summary>
/// <returns></returns>
double Tool::GetZoom() {
    /*
    // 获取窗口当前显示的监视器
    HWND hWnd = GetDesktopWindow(); // 获取桌面窗口的句柄
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST); // 获取最接近当前显示器的句柄

    // 获取监视器逻辑宽度
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(hMonitor, &monitorInfo);
    int cxLogical = (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);

    // 获取监视器物理宽度
    DEVMODE dm;
    dm.dmSize = sizeof(dm);
    dm.dmDriverExtra = 0; //字段用于驱动程序特定的额外数据。如果你不打算使用它，你可以将其设置为0。
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &dm); 
    //获取指定显示器的当前显示设置。它有三个参数：
    // 指向设备名称的字符串的指针，它指定了要查询的显示器。
    // 这是一个标志，指定了要检索的显示设置类型。在这种情况下，它请求当前设置。  
    // 指向`DEVMODE`结构的指针，函数将把获取到的设置填充到这个结构中。
    int cxPhysical = dm.dmPelsWidth;

    return cxPhysical * 1.0 / cxLogical; // 返回缩放比例
    */
    return 1.0;
}

/// <summary>
/// 获取截图
/// </summary>
/// <param name="Width"></param>
/// <param name="Height"></param>
/// <param name="Data"></param>
/// <param name="DataSize"></param>
/// <returns></returns>
bool Tool::GetDesktopImgData(int& Width, int& Height, char* &Data, long& DataSize)
{
     // 使用X11库来实现在Linux下的桌面截图
    Display* display = XOpenDisplay(NULL);
    if (nullptr == display) {
        LOG_ERROR("无法打开X Display。");
        return false;
    }

    // 获取根窗口（即整个桌面）
    Window root = DefaultRootWindow(display);

    // 获取窗口属性，包括宽度和高度
    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, root, &attributes);

    Width = attributes.width;
    Height = attributes.height;

    // 从服务器获取窗口的图像
    XImage* img = XGetImage(display, root, 0, 0, Width, Height, AllPlanes, ZPixmap);
    if (nullptr == img) {
        LOG_ERROR("XGetImage失败，无法获取屏幕图像。");
        XCloseDisplay(display);
        return false;
    }

    // 计算数据大小。XImage返回的数据通常是32位的BGRA或RGBA格式，
    // 每个像素占4个字节。
    DataSize = Width * Height * 4;
    Data = new char[DataSize];
    if (nullptr == Data) {
        LOG_ERROR("为截图数据分配内存失败。");
        XDestroyImage(img);
        XCloseDisplay(display);
        return false;
    }

    // 将XImage中的像素数据复制到我们自己的缓冲区中
    memcpy(Data, img->data, DataSize);

    // 清理X11资源
    XDestroyImage(img);
    XCloseDisplay(display);
    
    return true;
}

/// <summary>
/// 去黑边，用于假的8K*6K摄像头产生的黑边，也可以用于其它去黑边
/// </summary>
/// <param name="ImgMat"></param>
void Tool::RemoveBlackEdge(cv::Mat& ImgMat)
{
    const int Width = ImgMat.cols;
    const int Height = ImgMat.rows;
    //黑边去除
    //计算出一行的最后一个像素
    int colsPix = Width - 1;
    for (; colsPix > 0; --colsPix) {
        if (ImgMat.datastart[colsPix * 3ull] + ImgMat.datastart[colsPix * 3ull + 1ull] + ImgMat.datastart[colsPix * 3ull + 2ull] != 0)
            break;
    }
    //计算出一列的最后一个像素
    int rowsPix = (Height - 1) * Width;
    for (; rowsPix > 0; rowsPix -= Width) {
        if (ImgMat.datastart[rowsPix * 3ull] + ImgMat.datastart[rowsPix * 3ull + 1ull] + ImgMat.datastart[rowsPix * 3ull + 2ull] != 0) {
            break;
        }
    }
    //截取并重变形
    resize(cv::Mat(ImgMat, cv::Rect(0, 0, colsPix, rowsPix / Width)), ImgMat, cv::Size(Width, Height));
}
