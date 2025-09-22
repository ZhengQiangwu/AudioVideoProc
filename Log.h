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
/// ��־��
/// </summary>
class Log {
private:
    static Log* defaultLog;	//Ĭ����־����
    void (*logCallBackFunVar)(const char* Message);	//��־�ص��������ڹ��˺����
    std::mutex* callBackMutex;	//�ص�������
public:
    /// <summary>
    /// Ĭ�Ϲ���
    /// </summary>
    Log();
    /// <summary>
    /// Ĭ������
    /// </summary>
    virtual ~Log();
    /// <summary>
    /// ��ȡĬ����־��ʹ��
    /// </summary>
    /// <returns>����һ���ɲ�����Ĭ����־��ʵ��</returns>
    static Log* Default();
    /// <summary>
    /// ��ӡDebug���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Debug(const char* Message);
    /// <summary>
    /// ��ӡDebug���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Debug(const std::string& Message);
    /// <summary>
    /// ��ӡInfo���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Info(const char* Message);
    /// <summary>
    /// ��ӡInfo���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Info(const std::string& Message);
    /// <summary>
    /// ��ӡWarn���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Warn(const char* Message);
    /// <summary>
    /// ��ӡWarn���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Warn(const std::string& Message);
    /// <summary>
    /// ��ӡError���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Error(const char* Message);
    /// <summary>
    /// ��ӡError���͵���Ϣ
    /// </summary>
    /// <param name="Message">������Ϣ</param>
    void Error(const std::string& Message);
    /// <summary>
    /// ������־��Ϣ�ص�����
    /// </summary>
    /// <param name="LogCallBackFunVar">��Ϣ�ص�������ַ</param>
    void SetCallBack(void (*LogCallBackFunVar)(const char* Message));
private:
    /// <summary>
    /// ��־������д��ͽ���Ϣ����ص�����Ҫ���ܷ���
    /// </summary>
    /// <param name="MessageType">��Ϣ����</param>
    /// <param name="Message">��Ϣ</param>
    inline void Write(const char* MessageType, const char* Message);
};
