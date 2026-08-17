/**
 *  DuiEditHost —— 普通输入框的兼容别名，本体是无窗口的 DuiEdit。
 *
 *  这个类名下曾经是一个内嵌真 Win32 输入框子窗口的控件。2026-08-17 起输入框
 *  改为无窗口实现（见同目录 DuiEdit.h），本类退化为一层薄薄的兼容外壳，只为
 *  让两个程序里几百处存量调用零改动通过编译。新代码一律直接用 DuiEdit。
 *
 *  balloonwj@qq.com   2026-08-17
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

#include "DuiEdit.h"

namespace balloonwjui {

// =================================================================
// DuiEditHost —— 兼容别名（等同于 DuiEdit）
// =================================================================
//
// 用途：承接存量代码。控件的全部能力都在基类 DuiEdit 上，本类自己只补两样
// 东西：
//
//   一、EnsureCreated —— 旧实现里"在宿主窗口就绪之后把子窗口创建出来"的那
//       一步。无窗口控件构造完就能用，本方法留成空实现并恒返回成功，存量
//       调用点因此不必立刻清理。
//   二、旧的两个图标点击通知码，作为新通知码的别名保留。
//
// 刻意<u>没有</u>保留的是 GetHostedHwnd（取内部子窗口句柄）。无窗口实现下它
// 无从返回有意义的值，若留成返回空句柄，所有调用点都会在运行期静默失效 ——
// 不报错、不崩溃、日志里什么都没有，只表现为某个功能点了没反应。删掉它，让
// 这些地方变成编译错误，逐个改写成无窗口的等价做法（取焦点用 SetFocus、全选
// 用 SelectAll、判断焦点用 IsFocused）。
//
// 迁移计划：调用方逐步改用 DuiEdit 之后，本类连同这个头文件一并删除。
class BUI_API DuiEditHost : public DuiEdit
{
public:
    // 旧的图标点击通知码，等值转发到新控件的通知码。
    //
    // 数值本身变了（新控件另开了取值段，避开库内十余个控件挤在同一档的老问题），
    // 但存量代码比较的是这两个符号而不是字面数值，因此不受影响。
    enum
    {
        DUIEN_LEFT_ICON_CLICK  = DuiEdit::DUIN_EDIT_LEFT_ICON_CLICK,    // 点击了左侧图标
        DUIEN_RIGHT_ICON_CLICK = DuiEdit::DUIN_EDIT_RIGHT_ICON_CLICK,   // 点击了右侧图标
    };

    DuiEditHost();
    ~DuiEditHost() override;

    // 兼容用的空实现。旧实现在这里创建内部子窗口，无窗口控件没有这一步。
    //   hwndParent：忽略。
    //   返回：恒为 true。
    // 说明：新代码不要再调用它。存量调用点可以就地删除，删除顺序不影响功能。
    bool    EnsureCreated(HWND hwndParent);
};

}   // namespace balloonwjui

#endif  // BUI_FEATURE_EDIT
