/**
 *  画廊的页面注册表。所有演示页面按 balloonui 自己的目录分类归入若干分组，
 *  左侧导航树直接照这张表建出来。
 *
 *  每个分组的页面定义在各自的 Pages_<分组>.cpp 里，并由该文件导出一个
 *  GetXxxPages() 返回本分组的页面数组；本文件的 GetPageGroups() 只负责把
 *  各分组汇总成一张总表。新增页面时只需要改对应分组的那一个文件，不需要
 *  改这里，除非要新开一个分组。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <memory>
#include <tchar.h>

#include "DuiControl.h"

namespace Gallery {

// 构建一个页面的函数。返回该页面的根控件，所有权交给调用方。
typedef std::unique_ptr<balloonwjui::DuiControl> (*BuildPageFn)();

// 一个演示页面。
struct PageEntry
{
    // 页面的稳定标识，全小写英文加连字符，例如 "push-button"。
    // 它不随语言切换而变化，用途有三个：导航搜索时一并参与匹配、文档截图
    // 的文件名、以及单元测试里按名字定位页面。**已经发布的标识不要改动**。
    LPCTSTR idName;
    // 导航树里显示的中文标题。
    LPCTSTR titleZh;
    // 导航树里显示的英文标题。
    LPCTSTR titleEn;
    // 页面构建函数。
    BuildPageFn build;
    // 是否出现在左侧导航树里。
    // 取假的页面只有命令行截图模式会访问，例如专供生成文档配图的夹具页面 ——
    // 它的内容是别的页面的复制品，摆在导航里只会干扰阅读。
    bool showInNav;
};

// 一个页面分组，对应导航树里的一个可展开的顶层节点。
struct PageGroup
{
    // 分组的稳定标识，全小写英文，例如 "input"。
    LPCTSTR idName;
    // 导航树里显示的中文分组名。
    LPCTSTR titleZh;
    // 导航树里显示的英文分组名。
    LPCTSTR titleEn;
    // 本分组的页面数组。不持有所有权，指向对应 Pages_*.cpp 里的静态数组。
    const PageEntry* pages;
    // 本分组的页面个数。
    int pageCount;
};

// 取全部分组。
//   outCount：返回分组个数。
// 返回：分组数组的首地址，生命期与进程相同。
const PageGroup* GetPageGroups(int& outCount);

// 按标识查找一个页面。
//   idName：页面标识，即 PageEntry::idName。
// 返回：找到的页面；没有匹配时返回空指针。
const PageEntry* FindPageById(LPCTSTR idName);

// 取导航树里默认选中的页面标识。
LPCTSTR GetDefaultPageId();

// ---------------------------------------------------------------------
// 各分组导出的页面列表。实现分别在同名的 Pages_*.cpp 里。
// ---------------------------------------------------------------------

// 总览。介绍 balloonui 是什么、怎么用这个画廊。
const PageEntry* GetOverviewPages(int& outCount);
// 布局容器。竖直 / 水平 / 网格排列、分隔条、停靠、标签页容器。
const PageEntry* GetLayoutPages(int& outCount);
// 基础控件。标签、按钮、徽标、头像、分隔线、分组框、浮动提示条。
const PageEntry* GetBasicPages(int& outCount);
// 输入控件。各类输入框、滑块、开关、下拉框。
const PageEntry* GetInputPages(int& outCount);
// 列表与导航。列表框、虚拟列表、树、标签栏、菜单、菜单栏。
const PageEntry* GetListPages(int& outCount);
// 反馈与浮层。浮层宿主、工具提示、进度条、表情面板。
const PageEntry* GetFeedbackPages(int& outCount);
// 媒体。动图、富文本内嵌图片、异步图片加载。
const PageEntry* GetMediaPages(int& outCount);
// 窗口与宿主。滚动视图、框架窗口、九宫格背景。
const PageEntry* GetWindowPages(int& outCount);
// 引擎。主题、高 DPI、动画与缓动、运行期跟踪。
const PageEntry* GetEnginePages(int& outCount);
// 工具。控件树查看器、XML 自定义标签、键盘可达性、拖放、资源与皮肤。
const PageEntry* GetToolPages(int& outCount);
// 完整示例。用 balloonui 拼出来的几个完整界面。
const PageEntry* GetSamplePages(int& outCount);
// 文档配图夹具。不出现在导航里，只供命令行截图模式访问。
const PageEntry* GetDocCapturePages(int& outCount);

} // namespace Gallery
