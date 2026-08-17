/**
 *  画廊主窗口。它只装一个 DuiHost 子窗口，界面的其余部分全是 DUI 控件树：
 *  顶部一条工具条（标题、语言切换、主题切换），下面是一个竖向分隔条，
 *  左边是分组导航栏、右边是当前页面的滚动视图。
 *
 *  切换页面、切换语言、切换主题都走同一条路径：重建右侧内容区（切语言和
 *  切主题时连导航栏一起重建），因为页面上的文字与配色是在构建时取的。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <atlstr.h>

#include "DuiHost.h"
#include "Controls/Window/DuiScrollBar.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Layout/DuiSplitter.h"
#include "DuiNotify.h"

#include "GalleryNav.h"

// 画廊主窗口。
class GalleryFrame : public CWindowImpl<GalleryFrame, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiGalleryFrame__"), CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    GalleryFrame();
    ~GalleryFrame();

    // 本窗口私有的消息号。按仓库约定，窗口投给自己的消息一律走 WM_APP 段，
    // 不占用跨窗口投递的公共号段。偏移取 0x60，与 DuiHost 用的 0x120 错开，
    // 排查时一眼能看出号属于谁。
    enum
    {
        // 重建整棵控件树。切换语言或主题时用。
        //
        // 之所以要绕一道消息而不是就地重建：这两个动作都由工具条上的按钮
        // 触发，而通知是控件在自己的鼠标处理函数里同步发出来的，就地重建会
        // 在按钮自己的调用栈里把按钮析构掉。投一条消息让当前调用栈先返回，
        // 重建就发生在安全的时机。
        kMsgRebuildAll = WM_APP + 0x60,
    };

    BEGIN_MSG_MAP(GalleryFrame)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MESSAGE_HANDLER_EX(WM_DUI_NOTIFY, OnDuiNotify)
        MESSAGE_HANDLER_EX(kMsgRebuildAll, OnRebuildAllMsg)
    END_MSG_MAP()

protected:
    int     OnCreate(LPCREATESTRUCT lpcs);
    void    OnDestroy();
    void    OnSize(UINT nType, CSize size);
    BOOL    OnEraseBkgnd(CDCHandle dc);
    LRESULT OnDuiNotify(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnRebuildAllMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // 建出整棵控件树：工具条 + 分隔条（导航栏 / 内容区）。
    void    BuildRoot();

    // 建出顶部工具条。
    //   返回：工具条容器，所有权交给调用方。
    std::unique_ptr<balloonwjui::DuiControl> BuildToolBar();

    // 把右侧内容区换成指定页面。
    //   pageId：页面标识，即 PageEntry::idName。找不到时本函数不做任何事。
    void    SwitchToPage(LPCTSTR pageId);

    // 重建导航栏与当前页面。切换语言或切换主题之后调用 —— 两者都会改变
    // 构建时取用的文字或颜色，已经建好的控件不会自己跟着变。
    void    RebuildAll();

    // 把所有单元测试跑一遍，结果写到调试输出与临时目录下的日志文件。
    void    RunAllTests();

private:
    // 承载整棵 DUI 控件树的宿主窗口。
    balloonwjui::DuiHost m_host;
    // 左侧导航栏。所有权在分隔条里，这里只记指针。
    Gallery::GalleryNav* m_pNav;
    // 右侧内容区的滚动视图。所有权在分隔条里，这里只记指针。
    balloonwjui::DuiScrollView* m_pContent;
    // 分隔导航栏与内容区的竖向分隔条。所有权在根容器里，这里只记指针。
    balloonwjui::DuiSplitter* m_pSplitter;
    // 当前正在显示的页面标识。空串表示还没有显示任何页面。
    CString m_currentPageId;
};
