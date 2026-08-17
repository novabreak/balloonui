#pragma once

/**
 *  DuiTextHost（无窗口排版引擎宿主）的单元测试。
 *  balloonwj@qq.com   2026-08-14
 */

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "../Controls/Input/DuiTextHost.h"

namespace balloonwjui {

// DuiTextHost 的自包含单元测试。
//
// 本组用例最重要的一条是**验证引擎能否在完全没有窗口的情况下工作**。
// 这条性质是整个无窗口路线的地基：若成立，则文本读写、选区、格式、
// 尺寸测量、查找、持久化等绝大部分行为都能写成纯单元测试；若不成立，
// 这些就只能靠人工核对界面。
//
// 因此这里刻意**不创建任何窗口，也不接控件**（回调接口传空），
// 直接把引擎创建出来灌文本、排版、量尺寸。能跑通就说明地基是实的。
//
// 返回多行报告，最后一行是汇总。
namespace DuiTextHostTests {
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
