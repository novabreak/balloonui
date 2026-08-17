/**
 *  DuiTrace 的实现。用途与开启方式见 DuiTrace.h。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "DuiTrace.h"

#include <stdio.h>
#include <stdarg.h>
#include <share.h>   // _SH_DENYNO：以允许他人同时读写的方式打开日志文件

namespace balloonwjui {

namespace DuiTrace {

namespace {

// 控制开关的环境变量名与开启值。
const TCHAR* const kEnvVarName = _T("BUI_DUI_TRACE");
const TCHAR* const kEnvVarOn   = _T("1");

// 日志文件名（放在临时目录下）。
const TCHAR* const kLogFileName = _T("DuiTrace.log");

// 单条日志的最大长度（字符）。跟踪日志都是短句，给足余量即可。
const int kMaxLineChars = 1024;

// 一秒有多少毫秒 / 微秒，用于把性能计数器的刻度换算成时间。
const double kMsPerSecond = 1000.0;

// 开关的三态缓存：尚未读取 / 已开启 / 已关闭。用三态而不是布尔，
// 是为了区分"还没读过环境变量"与"读过且是关闭"，避免每次调用都去读一遍。
enum EnableState
{
    kEnableUnknown = 0,
    kEnableOn      = 1,
    kEnableOff     = 2
};

EnableState s_state = kEnableUnknown;

// 性能计数器每秒的刻度数，用于换算成毫秒。为 0 表示本机不支持。
LARGE_INTEGER s_freq = { 0 };

// 进程开启跟踪那一刻的计数值，作为时间原点。
LARGE_INTEGER s_base = { 0 };

// 上一条日志的计数值，用于算与上一条的间隔 ——
// 排查时序问题时，间隔比绝对时间有用得多。
LARGE_INTEGER s_last = { 0 };

// 日志文件句柄。**全程保持打开**，每条写完只 fflush。
//
// 为什么不像通常那样每条都开关一次文件：本设施是用来测时序的，而反复
// 开关文件本身就要花上毫秒量级的时间，会把被测对象的时序搅乱 ——
// 测量工具的开销大于被测间隔，数据就没有意义了。保持打开加刷新既快，
// 又能保证进程被强杀时已写入的内容不丢。
FILE* s_fp = nullptr;

// 把性能计数器的差值换算成毫秒。
double TicksToMs(const LARGE_INTEGER& from, const LARGE_INTEGER& to)
{
    if (s_freq.QuadPart == 0)
    {
        return 0.0;
    }
    double ticks = (double)(to.QuadPart - from.QuadPart);
    return ticks * kMsPerSecond / (double)s_freq.QuadPart;
}

} // 匿名命名空间

bool IsEnabled()
{
    if (s_state != kEnableUnknown)
    {
        return s_state == kEnableOn;
    }

    TCHAR szValue[16] = { 0 };
    DWORD n = ::GetEnvironmentVariable(kEnvVarName, szValue, 16);
    bool bOn = (n > 0 && n < 16 && ::_tcscmp(szValue, kEnvVarOn) == 0);

    s_state = bOn ? kEnableOn : kEnableOff;
    if (!bOn)
    {
        return false;
    }

    // 取计数器频率。取不到说明本机不支持高精度计时，此时时间列会恒为 0，
    // 但事件顺序仍然有效，不影响判断谁先谁后。
    if (!::QueryPerformanceFrequency(&s_freq))
    {
        s_freq.QuadPart = 0;
    }
    ::QueryPerformanceCounter(&s_base);
    s_last = s_base;

    TCHAR szTemp[MAX_PATH] = { 0 };
    DWORD nTemp = ::GetTempPath(MAX_PATH, szTemp);
    if (nTemp == 0 || nTemp >= MAX_PATH)
    {
        s_state = kEnableOff;
        return false;
    }
    TCHAR szPath[MAX_PATH] = { 0 };
    ::_tcsncpy_s(szPath, MAX_PATH, szTemp, _TRUNCATE);
    ::_tcsncat_s(szPath, MAX_PATH, kLogFileName, _TRUNCATE);

    // 用允许共享的方式打开。**这一点不能省**：本设施全程持有文件句柄，
    // 若按默认的独占方式打开，排查者在程序运行期间连日志都读不了，只能
    // 先把程序关掉 —— 而很多现象恰恰要边跑边看。允许共享读写之后，
    // 别的进程可以随时打开这个文件查看当前进度。
    s_fp = ::_tfsopen(szPath, _T("w"), _SH_DENYNO);
    if (s_fp == nullptr)
    {
        s_state = kEnableOff;
        return false;
    }
    ::fprintf(s_fp, "=== DuiTrace ===\n");
    ::fflush(s_fp);
    return true;
}

void Write(const char* fmt, ...)
{
    if (!IsEnabled() || fmt == nullptr || s_fp == nullptr)
    {
        return;
    }

    // 先取时间再格式化字符串，让时间戳尽量贴近事件发生的那一刻，
    // 而不是把格式化本身的耗时也算进去。
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    double sinceStart = TicksToMs(s_base, now);
    double sinceLast  = TicksToMs(s_last, now);
    s_last = now;

    char szLine[kMaxLineChars] = { 0 };
    va_list args;
    va_start(args, fmt);
    ::vsnprintf(szLine, kMaxLineChars - 1, fmt, args);
    va_end(args);

    ::fprintf(s_fp, "[%10.3fms] +%-9.3f %s\n", sinceStart, sinceLast, szLine);
    ::fflush(s_fp);
}

} // namespace DuiTrace

} // namespace balloonwjui
