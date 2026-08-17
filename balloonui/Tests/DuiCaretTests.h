#pragma once

/**
 *  DuiCaret（系统文本光标封装）的单元测试。
 *  balloonwj@qq.com   2026-08-14
 */

// BalloonUiFeatures.h 必须放在最前：库内测试头的既有约定，避免将来给本文件
// 加特性开关时，未定义的宏被静默求值为 0、整份用例无声地编译不进去。
#include "../BalloonUiFeatures.h"
#include "../DuiCaret.h"

namespace balloonwjui {

// DuiCaret 的自包含单元测试。
//
// 与库内多数测试不同，本组用例**需要一个真窗口**：系统的光标接口
// （::CreateCaret 等）要求传入一个属于当前线程的窗口句柄，没有窗口就调不通。
// 因此用例内部会临时建一个不可见的弹出窗口，用完即销毁。
//
// 覆盖范围是 DuiCaret 的状态机本身——是否占有光标、是否处于显示状态、
// 位置与尺寸是否被正确记录，以及未占有光标时各接口是否安全空转。
// 光标在屏幕上到底闪不闪、闪得好不好看，属于人工核对范畴，不在这里。
//
// 返回多行报告，最后一行是汇总。
namespace DuiCaretTests {
    CString RunAll();
}

} // namespace balloonwjui
