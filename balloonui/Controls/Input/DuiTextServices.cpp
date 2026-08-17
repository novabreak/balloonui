/**
 *  DuiTextServices 的实现：引擎库懒加载、入口函数解析，以及两个接口标识符的定义。
 *  设计背景见 DuiTextServices.h 的文件头注释。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiTextServices.h"

// ─────────────────────────────────────────────────────────────────
// 两个接口标识符的定义
// ─────────────────────────────────────────────────────────────────
//
// SDK 的 textserv.h 里只有 `EXTERN_C const IID IID_ITextServices;` 这样的
// **声明**，没有定义，也没有任何 .lib 导出它们。因此必须在自己的代码里把
// 这两个值写出来，否则链接期会报找不到符号。
//
// 取值是这两个接口自 RichEdit 诞生起就固定的公开标识符，注释里同时写出
// 可读形式便于核对。**不要改动这里的任何一个字节** —— 它们是与系统组件
// 对接的约定值，写错的表现是创建引擎时返回"接口不支持"，而不是崩溃，
// 排查起来会绕远路。
EXTERN_C const IID IID_ITextServices =   // 8d33f740-cf58-11ce-a89d-00aa006cadc5
{
    0x8d33f740,
    0xcf58,
    0x11ce,
    { 0xa8, 0x9d, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5 }
};

EXTERN_C const IID IID_ITextHost =       // c5bdd8d0-d26e-11ce-a89e-00aa006cadc5
{
    0xc5bdd8d0,
    0xd26e,
    0x11ce,
    { 0xa8, 0x9e, 0x00, 0xaa, 0x00, 0x6c, 0xad, 0xc5 }
};

namespace balloonwjui {

namespace DuiTextServices {

namespace {

// 引擎所在的系统库。早先内嵌真子窗口的实现包的也是这个库里的 RICHEDIT50W
// 窗口类，两种用法共用同一个引擎，行为才不会有差异。
const TCHAR* const kEngineDllName = _T("Msftedit.dll");

// 入口函数在 DLL 里的导出名。注意它是 **不带修饰的 ANSI 名字**，
// GetProcAddress 只接受窄字符，不能写成 _T("...")。
const char* const kCreateEntryName = "CreateTextServices";

// 加载状态。用三态而不是"函数指针是否为空"来表示，是为了区分
// "还没试过" 与 "试过并且失败了"：后者不应当每次调用都重试一遍
// LoadLibrary，那在缺库的系统上会反复吃到失败开销。
enum LoadState
{
    kLoadNotTried = 0,   // 尚未尝试加载
    kLoadOk       = 1,   // 加载成功，入口函数可用
    kLoadFailed   = 2    // 尝试过并且失败，不再重试
};

// 进程级缓存。控件全部在 UI 线程使用，这里不加锁；若将来出现多线程首调
// 的需求，需要另行加同步。
LoadState           s_state = kLoadNotTried;
PCreateTextServices s_pfnCreate = nullptr;

// 一次性加载引擎库并解析入口函数。返回是否可用。
bool EnsureLoaded()
{
    if (s_state != kLoadNotTried)
    {
        return s_state == kLoadOk;
    }

    // 刻意不调 FreeLibrary：引擎在整个进程生命周期内都可能被用到，
    // 卸载它没有收益，反而要处理"还有引擎对象活着时被卸载"的风险。
    // 这与库内其它进程级懒加载（如 GDI+ 启动）的处理方式一致。
    HMODULE hDll = ::LoadLibrary(kEngineDllName);
    if (hDll == nullptr)
    {
        s_state = kLoadFailed;
        return false;
    }

    s_pfnCreate = (PCreateTextServices)::GetProcAddress(hDll, kCreateEntryName);
    if (s_pfnCreate == nullptr)
    {
        // 库在但导出名对不上。理论上不该发生，真发生了说明系统组件异常，
        // 同样按不可用处理，让调用方走退化路径而不是崩溃。
        s_state = kLoadFailed;
        return false;
    }

    s_state = kLoadOk;
    return true;
}

} // 匿名命名空间

bool IsAvailable()
{
    return EnsureLoaded();
}

HRESULT Create(ITextHost* pHost, IUnknown** ppUnk)
{
    if (pHost == nullptr || ppUnk == nullptr)
    {
        return E_POINTER;
    }
    *ppUnk = nullptr;

    if (!EnsureLoaded())
    {
        return E_NOTIMPL;
    }

    // 第一个参数是 COM 聚合用的外部未知接口，我们不做聚合，传空即可。
    return s_pfnCreate(nullptr, pHost, ppUnk);
}

} // namespace DuiTextServices

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
