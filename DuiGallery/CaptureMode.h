/**
 *  命令行截图模式：不显示界面，把画廊里登记过截图标记的演示行逐个截成 PNG，
 *  供 docs/guides.md 引用。
 *  balloonwj@qq.com   2026-05-21
 */

#pragma once

#include <atlstr.h>

namespace CaptureMode {

// 以截图模式跑一遍画廊。
//   outDir：输出目录。不存在时本函数会创建它。
// 返回：成功写出的 PNG 张数；无法创建窗口或输出目录不可写时返回 -1。
//
// 流程是遍历 Gallery::GetPageGroups() 下的每一个页面（**包括不出现在导航里
// 的文档配图夹具**），逐页构建、强制排列与绘制到宿主的后台缓冲，然后按每个
// Gallery::CaptureMark 记下的控件矩形裁出一块 32 位位图存成
// <outDir>\ctl-<标记名>.png。
//
// 窗口建在屏幕外的 (-32000, -32000) 处并且是禁用状态，所以截图过程不会抢
// 焦点、也不会在屏幕上闪一下。
//
// 调用方必须事先完成 OleInitialize、_Module.Init 与
// DuiDpi::OptInPerMonitorV2 三项初始化。
int RunCaptureAll(LPCTSTR outDir);

} // namespace CaptureMode
