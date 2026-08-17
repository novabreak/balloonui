/**
 *  DuiEditHostTests —— 普通输入框兼容外壳 DuiEditHost 的单元测试。
 *
 *  2026-08-17 输入框改为无窗口实现之后，DuiEditHost 退化成 DuiEdit 之上薄薄
 *  一层兼容外壳，本组用例随之只覆盖这层外壳自己的那点东西：兼容用的空实现
 *  EnsureCreated、占位文字的显示开关与读写、以及两个旧通知码别名。
 *
 *  控件本体的用例在 DuiEditTests.cpp —— 文本读写与通知时机、密码模式与显隐
 *  切换按钮、左右内联图标、单行回车与 Esc、垂直居中等等，本文件一概不重复。
 *
 *  典型用法：由 DuiGallery 启动时统一调用 RunAll()，结果写进测试日志。
 *
 *  balloonwj@qq.com   2026-05-20
 */
#pragma once

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT

#include "../Controls/Input/DuiEditHost.h"

namespace balloonwjui {

namespace DuiEditHostTests
{
    // 运行本组全部用例。
    //   返回：逐条用例的结果，外加末尾一行汇总；行与行之间以回车换行分隔。
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
