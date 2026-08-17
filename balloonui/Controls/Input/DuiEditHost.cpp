/**
 *  DuiEditHost 的实现 —— 普通输入框的兼容别名。
 *
 *  控件的全部能力都在基类 DuiEdit 上，本文件只有一个空实现的 EnsureCreated。
 *  背景与迁移计划见头文件。
 *
 *  balloonwj@qq.com   2026-08-17
 */
#include "stdafx.h"
#include "DuiEditHost.h"

#if BUI_FEATURE_EDIT

namespace balloonwjui {

DuiEditHost::DuiEditHost()
{
}

DuiEditHost::~DuiEditHost()
{
}

bool DuiEditHost::EnsureCreated(HWND /*hwndParent*/)
{
    // 无窗口控件构造完就能用，这里没有任何事情可做。恒返回成功是为了让存量
    // 调用点里那些"创建失败就记一条警告"的分支不会突然开始报警。
    return true;
}

}   // namespace balloonwjui

#endif  // BUI_FEATURE_EDIT
