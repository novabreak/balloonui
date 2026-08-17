#pragma once

/**
 *  DuiRichEdit（无窗口富文本控件）的单元测试。
 *  balloonwj@qq.com   2026-08-14
 */

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "../Controls/Input/DuiRichEdit.h"

namespace balloonwjui {

// DuiRichEdit 的自包含单元测试。
//
// 全部用例都**不创建任何窗口**，也不挂进 DUI 树。这是可行的，因为排版
// 引擎本身不依赖窗口（已于 2026-08-14 实测确认，见 DuiTextHostTests）。
// 因此文本读写、属性开关、占位文字判定、布局换算都能在这里直接验证。
//
// 真正需要窗口的只有光标与输入法，那两块分别由 DuiCaretTests 和人工
// 核对清单负责，不在本文件内。
//
// 返回多行报告，最后一行是汇总。
namespace DuiRichEditTests {
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
