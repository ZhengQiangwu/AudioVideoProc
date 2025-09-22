#pragma once
#include <iostream>
#include <string>
#include <mutex>

using std::to_string;
using std::string;
using std::mutex;

extern bool g_IsDebug;

#define LOG_DEBUG(MSG) if(g_IsDebug)Log::Default()->Debug((std::string(MSG)).c_str())
#define LOG_INFO(MSG) Log::Default()->Info((std::string(MSG)).c_str())
#define LOG_WARN(MSG) Log::Default()->Warn((std::string(__FUNCTION__) + "(" + std::to_string(__LINE__) + ") : " + std::string(MSG)).c_str())
#define LOG_ERROR(MSG) Log::Default()->Error((std::string(__FUNCTION__) + "(" + std::to_string(__LINE__) + ") : " + std::string(MSG)).c_str())

/// <summary>
/// 日志类
/// </summary>
class Log {
private:
    static Log* defaultLog;	//默认日志单例
    void (*logCallBackFunVar)(const char* Message);	//日志回调函数，在过滤后调用
    std::mutex* callBackMutex;	//回调更换锁
public:
    /// <summary>
    /// 默认构造
    /// </summary>
    Log();
    /// <summary>
    /// 默认析构
    /// </summary>
    virtual ~Log();
    /// <summary>
    /// 获取默认日志以使用
    /// </summary>
    /// <returns>返回一个可操作的默认日志类实例</returns>
    static Log* Default();
    /// <summary>
    /// 打印Debug类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Debug(const char* Message);
    /// <summary>
    /// 打印Debug类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Debug(const std::string& Message);
    /// <summary>
    /// 打印Info类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Info(const char* Message);
    /// <summary>
    /// 打印Info类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Info(const std::string& Message);
    /// <summary>
    /// 打印Warn类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Warn(const char* Message);
    /// <summary>
    /// 打印Warn类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Warn(const std::string& Message);
    /// <summary>
    /// 打印Error类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Error(const char* Message);
    /// <summary>
    /// 打印Error类型的信息
    /// </summary>
    /// <param name="Message">具体信息</param>
    void Error(const std::string& Message);
    /// <summary>
    /// 设置日志信息回调函数
    /// </summary>
    /// <param name="LogCallBackFunVar">信息回调函数地址</param>
    void SetCallBack(void (*LogCallBackFunVar)(const char* Message));
private:
    /// <summary>
    /// 日志类真正写入和将信息放入回调的主要功能方法
    /// </summary>
    /// <param name="MessageType">信息类型</param>
    /// <param name="Message">信息</param>
    inline void Write(const char* MessageType, const char* Message);
};
