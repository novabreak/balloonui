/**
 *  DuiTextServices —— Windows 无窗口文字排版引擎的加载入口。
 *  balloonwj@qq.com   2026-08-14
 */

#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiTextServices —— 排版引擎的加载与创建
// =================================================================
//
// 用途：把"从系统里取出一个无窗口文字排版引擎"这件事收在一处。使用者是
// 无窗口富文本控件 DuiRichEdit 及其宿主实现 DuiTextHost。
//
// ─────────────────────────────────────────────────────────────────
// 背景：两种用法，一个引擎
// ─────────────────────────────────────────────────────────────────
//
// Windows 的 RichEdit 排版引擎有两种取用方式：
//   · 真窗口用法 —— CreateWindowEx 建一个 RICHEDIT50W 窗口类的子窗口，
//     引擎藏在窗口过程后面。库内早先内嵌真子窗口的实现走的是这条路。
//   · 无窗口用法 —— 调 msftedit.dll 导出的 CreateTextServices，直接拿到
//     引擎对象，自己给它当宿主。本文件服务的是这一条。
//
// 两条路用的是**同一个 DLL、同一个引擎**，因此文本行为、RTF 格式、链接
// 识别规则完全一致，从真窗口用法迁过来不会出现行为差异。
//
// ─────────────────────────────────────────────────────────────────
// 为什么需要这么一个文件：两处"看起来该有、实际没有"的东西
// ─────────────────────────────────────────────────────────────────
//
// 一、**入口函数没有导入库可链接。** CreateTextServices 虽然在 SDK 头文件
//     里有声明，但没有哪个 .lib 导出它，直接调用会在链接期报找不到符号。
//     只能运行期 LoadLibrary + GetProcAddress 取函数指针。
//
// 二、**两个接口标识符也没有任何库导出。** IID_ITextServices 与 IID_ITextHost
//     在 SDK 头文件里只有 `EXTERN_C const IID` 的声明，没有定义。必须在
//     自己的代码里把这两个值写出来（本文件的 .cpp 就是干这个的），否则同样
//     是链接期报错。这一条尤其容易卡住——报错发生在链接期而不是编译期，
//     信息又只有一个符号名，一时不容易反应过来是缺定义而不是缺库。
//
// ─────────────────────────────────────────────────────────────────
// 代码用法
// ─────────────────────────────────────────────────────────────────
//
//     // 先确认引擎库可用（一次性懒加载，进程内只加载一次）：
//     if (!balloonwjui::DuiTextServices::IsAvailable())
//     {
//         return false;   // 系统缺少 msftedit.dll，控件退化处理
//     }
//
//     // 把自己的 ITextHost 实现交出去，换回引擎对象：
//     IUnknown* pUnk = nullptr;
//     HRESULT hr = balloonwjui::DuiTextServices::Create(pMyTextHost, &pUnk);
//     if (SUCCEEDED(hr) && pUnk != nullptr)
//     {
//         ITextServices* pServices = nullptr;
//         pUnk->QueryInterface(IID_ITextServices, (void**)&pServices);
//         pUnk->Release();          // 换到目标接口后本地这份引用即可释放
//         // ... 之后一切排版工作都通过 pServices 进行 ...
//     }
//
// XML 用法：N/A（不是控件，是引擎接入的基础设施）。

#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <textserv.h>

namespace balloonwjui {

namespace DuiTextServices {

// 引擎库是否可用。首次调用时懒加载 msftedit.dll 并解析入口函数，
// 之后直接返回缓存结果；加载失败时不会反复重试。
//   返回：true 表示引擎可用；false 表示系统缺少该库或入口函数解析失败，
//         调用方应当退化处理（例如控件只显示占位文字、不接受编辑）。
// 线程：首次调用应发生在 UI 线程；本函数不做加锁，不适合多线程同时首调。
bool IsAvailable();

// 创建一个引擎对象。
//   pHost：调用方实现的宿主接口，引擎会通过它反过来索取设备上下文、
//          客户区尺寸、光标、滚动条等。**所有权仍归调用方**，本函数不
//          增加它的引用计数；调用方必须保证 pHost 的生命周期长于引擎对象。
//   ppUnk：出参，成功时返回引擎对象的接口指针，**引用计数为 1，
//          调用方负责 Release**。通常紧接着 QueryInterface 换成
//          ITextServices，再把这一份释放掉。
//   返回：S_OK 表示成功；引擎库不可用时返回 E_NOTIMPL；其余错误码由
//         系统的创建函数原样返回。参数为空时返回 E_POINTER。
HRESULT Create(ITextHost* pHost, IUnknown** ppUnk);

} // namespace DuiTextServices

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
