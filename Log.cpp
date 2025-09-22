#include "Log.h"// 包含Log类的声明。  
// --- 修改开始 ---
// 为了使用 localtime_r 和 snprintf，需要包含 <ctime> 和 <cstdio>
#include <ctime>
#include <cstdio>
// --- 修改结束 ---

using namespace std;// 使用C++标准库命名空间。  

bool g_IsDebug = false; //控制是否开启调试模式。

Log* Log::defaultLog{ nullptr };// 初始化Log类的静态成员变量defaultLog为nullptr。

// Log类的构造函数  
Log::Log() :
    logCallBackFunVar{ nullptr }  // 初始化列表，将logCallBackFunVar成员变量初始化为nullptr。  
    , callBackMutex{ new mutex }  // 初始化callBackMutex成员变量，为其分配一个新的互斥锁。  
{
    //setlocale(LC_ALL, "");// 本用于设置本地化信息，以便支持多语言环境。  
    //logStream->imbue(std::locale(""));
}

Log::~Log()
{// 析构函数体为空，但通常这里应该释放动态分配的资源，如callBackMutex。  
}

Log* Log::Default()// 获取默认的Log实例  
{
    if (defaultLog)// 如果defaultLog已经初始化 
        return defaultLog; // 直接返回defaultLog  
    return defaultLog = new Log();// 否则，创建一个新的Log实例，并将其赋值给defaultLog，然后返回。  
}

// 以下是一系列的重载函数，用于记录不同级别的日志信息。 
void Log::Debug(const char* Message)
{
    Write("Debug", Message);
}

void Log::Debug(const std::string& Message)
{
    Debug(Message.c_str());
}

void Log::Info(const char* Message)
{
    Write("Info", Message);
}

void Log::Info(const std::string& Message)
{
    Info(Message.c_str());
}

void Log::Warn(const char* Message)
{
    Write("Warn", Message);
}

void Log::Warn(const std::string& Message)
{
    Warn(Message.c_str());
}

void Log::Error(const char* Message)
{
    Write("Error", Message);
}

void Log::Error(const std::string& Message)
{
    Error(Message.c_str());
}

// 设置日志回调函数  
void Log::SetCallBack(void (*LogCallBackFunVar)(const char* Message))
{
    callBackMutex->lock();// 加锁，以保护共享资源logCallBackFunVar。
    logCallBackFunVar = LogCallBackFunVar;  // 更新回调函数指针。  
    callBackMutex->unlock();// 解锁。  
}

// 写入日志信息的函数（内联函数，可能在头文件中定义以提高性能）  
inline void Log::Write(const char* FunName, const char* Message)
{
    time_t curTime;// 定义时间变量。
    tm timeInfo; // 定义时间结构。  
    char timeStr[20];// 定义时间字符串数组。 
     
     // --- 修改开始 ---
    // localtime_s(&timeInfo, &curTime);// 将时间转换为本地时间。 (Windows特有)
    // 使用 localtime_r 替换，这是POSIX线程安全版本
    localtime_r(&curTime, &timeInfo);
    
    // sprintf_s(timeStr, "[%02d:%02d]", timeInfo.tm_min, timeInfo.tm_sec);// 格式化时间字符串。 (Windows特有)
    // 使用 snprintf 替换，这是C标准的缓冲区安全版本
    snprintf(timeStr, sizeof(timeStr), "[%02d:%02d]", timeInfo.tm_min, timeInfo.tm_sec);
    // --- 修改结束 ---
    
    string logMessage(timeStr);// 创建日志消息字符串，并初始化为时间字符串。  
    logMessage.append("[").append(FunName).append("] ").append(Message).append("\n"); // 拼接完整的日志消息。

    callBackMutex->lock();// 加锁，以保护可能的共享资源（如回调函数或输出流）。 
    if (logCallBackFunVar) // 如果设置了回调函数  
        logCallBackFunVar(logMessage.c_str()); // 调用回调函数，传递日志消息。
    callBackMutex->unlock();// 解锁。  
    return;// 函数返回。注意，这里没有将日志消息写入到文件或控制台，可能是通过回调函数来处理这部分逻辑。  
}

