/**
 *  「窗口与宿主」分组的演示页面，共三页：
 *    · 滚动视图（DuiScrollView）—— 裁剪一块比视口高得多的内容并滚动它。
 *    · 框架窗口（DuiFrameWindow）—— 抹掉系统非客户区、自绘标题栏的顶层窗口。
 *    · 九宫格背景（DuiNinePatch）—— 位图缩放到任意尺寸而四角不变形的画法。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Window/DuiScrollBar.h"
#include "Controls/Window/DuiFrameWindow.h"
#include "DuiHost.h"
#include "DuiNinePatch.h"
#include "DuiXmlBuilder.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace Gallery {

// =====================================================================
// 本文件公用的小工具
// =====================================================================

namespace {

// 同一段落里两组演示行之间的间距（像素）。
const int kInnerRowGap = 8;
// 演示行内相邻控件之间的间距（像素）。
const int kRowGap = 12;

// 说明性文字的颜色。比正文浅一档，避免抢演示控件的注意力。
const COLORREF kHintTextColor = RGB(110, 110, 110);
// 素材缺失一类的提醒文字颜色。
const COLORREF kWarnTextColor = RGB(180, 60, 60);

// 生成一张竖向双色渐变的 32 位位图，用作演示里的标题栏图标。
//
// 画廊里需要若干张互不相同、又不依赖任何素材文件的小图，直接合成比准备一堆
// PNG 更简便。返回的位图由调用方负责释放；本文件里的调用点都把它存进静态变量，
// 一直用到进程结束，因此没有释放代码。
//   r0 / g0 / b0：顶端颜色的三个通道分量，取值 0 ~ 255。
//   r1 / g1 / b1：底端颜色的三个通道分量，取值 0 ~ 255。
// 返回：新建的位图句柄；创建失败时返回 NULL。
HBITMAP MakeGradientIconBitmap(BYTE r0, BYTE g0, BYTE b0,
                               BYTE r1, BYTE g1, BYTE b1)
{
    // 图标位图的边长（像素）。标题栏会把它缩到 16×16 显示，这里取 32 是为了
    // 缩小时还有足够的采样精度。
    const int kIconBitmapSize = 32;
    // 一个像素占的字节数（32 位位图，四个通道各一字节）。
    const int kBytesPerPixel = 4;
    // 不透明的 alpha 通道取值。
    const BYTE kOpaqueAlpha = 255;

    BITMAPINFO bi;
    ::ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kIconBitmapSize;
    // 负高度表示自上而下排列的像素，行序与下面的循环一致。
    bi.bmiHeader.biHeight = -kIconBitmapSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBitmap == NULL || pBits == NULL)
    {
        return NULL;
    }

    BYTE* pBase = (BYTE*)pBits;
    for (int y = 0; y < kIconBitmapSize; ++y)
    {
        // 从上到下在两个端点颜色之间线性插值。分母取边长减一，使最后一行
        // 正好等于底端颜色。
        float fRatio = (float)y / (float)(kIconBitmapSize - 1);
        BYTE r = (BYTE)(r0 + (r1 - r0) * fRatio);
        BYTE g = (BYTE)(g0 + (g1 - g0) * fRatio);
        BYTE b = (BYTE)(b0 + (b1 - b0) * fRatio);
        for (int x = 0; x < kIconBitmapSize; ++x)
        {
            BYTE* pPixel = pBase + (y * kIconBitmapSize + x) * kBytesPerPixel;
            pPixel[0] = b;
            pPixel[1] = g;
            pPixel[2] = r;
            pPixel[3] = kOpaqueAlpha;
        }
    }
    return hBitmap;
}

// 新建一个只占位、什么都不画的控件，用来占住水平布局里剩下的空间，
// 使它左边的控件各自保持固定宽度。
// 返回：新建的空控件，所有权交给调用方。
std::unique_ptr<DuiControl> MakeSpacer()
{
    return std::unique_ptr<DuiControl>(new DuiControl());
}

// 新建一条说明性文字标签。
//   szText：文字内容。
//   clrText：文字颜色。
// 返回：新建的标签，所有权交给调用方。
std::unique_ptr<DuiLabel> MakeHintLabel(LPCTSTR szText, COLORREF clrText)
{
    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(szText);
    label->SetTextColor(clrText);
    return label;
}

} // 匿名命名空间

// =====================================================================
// 滚动视图（DuiScrollView）
// =====================================================================

namespace {

// 演示用的高内容控件：竖向排一列彩色横带，总高度远超视口高度。
//
// 用途：给 DuiScrollView 一块真正滚得动的内容。每条横带上写着自己的序号，
// 滚动时序号连续变化，一眼能看出滚到了哪里、有没有跳行或重影。
class TallColorColumn : public DuiControl
{
public:
    // 横带条数。
    static const int kBandCount = 30;
    // 每条横带的高度（像素）。
    static const int kBandHeight = 36;

    // 画满全部横带需要多高。
    // 返回：条数乘以单条高度，单位像素。调用方拿它去设 DuiScrollView 的内容高度。
    int DesiredHeight() const
    {
        return kBandCount * kBandHeight;
    }

    // 绘制：逐条横带填色并写上序号，与脏矩形无交集的横带跳过不画。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域，宿主窗口客户区坐标。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        for (int i = 0; i < kBandCount; ++i)
        {
            RECT rcBand;
            rcBand.left = m_rcItem.left;
            rcBand.top = m_rcItem.top + i * kBandHeight;
            rcBand.right = m_rcItem.right;
            rcBand.bottom = m_rcItem.top + (i + 1) * kBandHeight;

            RECT rcInter;
            if (!::IntersectRect(&rcInter, &rcBand, &rcDirty))
            {
                continue;
            }

            COLORREF clrBand = MakeBandColor(i);
            HBRUSH hBrush = ::CreateSolidBrush(clrBand);
            ::FillRect(hdc, &rcInter, hBrush);
            ::DeleteObject(hBrush);

            int nOldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            CString strLabel;
            strLabel.Format(Txt(_T("  横带 %02d"), _T("  band %02d")), i);
            ::DrawText(hdc, strLabel, -1, &rcBand,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ::SetBkMode(hdc, nOldBkMode);
        }
    }

private:
    // 按横带序号算出它的填充色。
    //   nIndex：横带序号，取值 [0, kBandCount)。
    // 返回：该条横带的颜色。
    static COLORREF MakeBandColor(int nIndex)
    {
        // 配色的取值方式：把序号乘一个步长再取模，得到一串分散开的基数，
        // 再分别落到红 / 绿 / 蓝三个通道各自的取值区间里。数值本身没有含义，
        // 只要相邻两条颜色明显不同、整体又都是浅色不喧宾夺主即可。
        const int kHueStride = 23;      // 相邻两条之间基数的步进
        const int kHueModulo = 256;     // 基数的取值上限
        const int kRedBase = 180;       // 红色通道的基准值
        const int kRedSpan = 60;        // 红色通道在基准值之上的浮动范围
        const int kGreenBase = 200;     // 绿色通道的基准值
        const int kGreenSpan = 80;      // 绿色通道在基准值之下的浮动范围
        const int kBlueBase = 220;      // 蓝色通道的基准值
        const int kBlueSpan = 90;       // 蓝色通道在基准值之下的浮动范围

        int nHue = (nIndex * kHueStride) % kHueModulo;
        int nRed = kRedBase + (nHue % kRedSpan);
        int nGreen = kGreenBase - (nHue % kGreenSpan);
        int nBlue = kBlueBase - (nHue % kBlueSpan);
        return RGB(nRed, nGreen, nBlue);
    }
};

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_ScrollView()
{
    // 滚动视图演示行的高度（像素）。取一个明显小于内容总高度的值，滚动条才会出现。
    const int kScrollDemoRowHeight = 360;

    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("竖直滚动视图"), _T("Vertical scroll view")),
               Txt(_T("DuiScrollView 把一块比自己高的内容控件裁进视口，多出来的部分靠滚动查看。")
                   _T("滚轮、拖动滑块、点击轨道空白处这三种操作都能滚动；滚动条平时隐藏，")
                   _T("滚动时淡入、停下约 800 毫秒后淡出。下面这块内容一共 30 条横带、总高 1080 像素，")
                   _T("视口只有 360 像素高。"),
                   _T("DuiScrollView clips a content control taller than itself and lets the user reach ")
                   _T("the rest by scrolling. The wheel, dragging the thumb and clicking the empty track ")
                   _T("all scroll it. The bar auto-hides: it fades in while scrolling and fades out about ")
                   _T("800 ms after the last move. The content below is 30 bands, 1080 px in total, ")
                   _T("inside a 360 px viewport.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        std::unique_ptr<TallColorColumn> column(new TallColorColumn());
        // 内容高度必须在把内容交出去之前问，交出去之后指针的所有权就不在这里了。
        int nContentHeight = column->DesiredHeight();
        scrollView->SetContent(std::move(column));
        scrollView->SetContentHeight(nContentHeight);

        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kScrollDemoRowHeight);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 框架窗口（DuiFrameWindow）
// =====================================================================

namespace {

// 演示窗口的尺寸限制与初始尺寸（像素）。段落说明里也会引用这几个值，
// 所以统一从这里取，避免文字与实际行为对不上。
const int kDemoFrameMinWidth = 360;
const int kDemoFrameMinHeight = 240;
const int kDemoFrameMaxWidth = 800;
const int kDemoFrameMaxHeight = 600;
// 「固定尺寸」演示里把上下限设成同一个值时用的尺寸（像素）。
const int kDemoFrameFixedWidth = 480;
const int kDemoFrameFixedHeight = 320;
// 演示窗口第一次弹出时的客户区尺寸（像素）。
const int kDemoFrameInitWidth = 480;
const int kDemoFrameInitHeight = 320;

// 演示窗口客户区里各行的高度（像素）。
const int kDemoFrameHeaderHeight = 28;
// 需要换到两行的说明文字的高度（像素）。窗口可以被拖窄，所以这两条说明开了
// 自动换行，高度按两行留。
const int kDemoFrameParagraphHeight = 44;
const int kDemoFrameButtonHeight = 36;
// 演示窗口客户区的内边距与行间距（像素）。
const int kDemoFrameContentPadding = 20;
const int kDemoFrameContentGap = 10;
// 演示窗口客户区里那一排按钮的个数。
const int kDemoFrameActionCount = 3;

// 传给 Create 的窗口名。它只作为操作系统层面的窗口标题（任务栏、Alt-Tab
// 里显示的那个），不面向画廊的读者，所以不做中英文切换；界面上看到的标题
// 由 SetTitle 另行设置。
const LPCTSTR kDemoFrameWindowName = _T("DuiGallery frame demo");

// 演示用的框架窗口。
//
// 它比基类多做一件事：在基类的消息映射之前先截下 WM_DUI_NOTIFY，认出标题栏
// 自定义图标发来的 DUIFW_CAPTION_ICON_CLICK，弹一个消息框把 captionId 显示
// 出来。其余通知一律交还给基类原有的路由。
//
// 标题栏图标不是客户区控件，它的通知不带有意义的 ctrlId，只能靠 extra 里
// 回带的 captionId 区分，所以这里只比较通知码。
class DemoFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(DemoFrame)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnFrameDuiNotify)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

private:
    // 处理 WM_DUI_NOTIFY。
    //   lp：LPARAM，实际是指向 DuiNotify 的指针。
    //   bHandled：置 TRUE 表示本函数已经处理完，不再往基类的映射传。
    // 返回：消息返回值，本处恒为 0。
    LRESULT OnFrameDuiNotify(UINT, WPARAM, LPARAM lp, BOOL& bHandled)
    {
        DuiNotify* pNotify = reinterpret_cast<DuiNotify*>(lp);
        if (pNotify != NULL
            && pNotify->code == (UINT)DuiFrameWindow::DUIFW_CAPTION_ICON_CLICK)
        {
            CString strMsg;
            strMsg.Format(Txt(_T("标题栏图标被点击：captionId = %d"),
                              _T("Caption icon clicked: captionId = %d")),
                          (int)pNotify->extra);
            ::MessageBox(m_hWnd, strMsg,
                         Txt(_T("标题栏图标"), _T("Caption icon")),
                         MB_OK | MB_ICONINFORMATION);
            bHandled = TRUE;
            return 0;
        }
        // 其余通知交给基类的 CHAIN_MSG_MAP 继续派发。
        bHandled = FALSE;
        return 0;
    }
};

// 演示窗口本身。做成静态对象是为了让它的窗口句柄跨页面重建存活 —— 画廊切换
// 页面时会销毁整棵页面控件树，已经弹出的窗口不应跟着消失。进程退出时随静态
// 对象一并析构。
DemoFrame s_demoFrame;

// 上一次完成配置时演示窗口的句柄。
//
// 关闭窗口会走 DestroyWindow，DuiFrameWindow 连同标题栏、标题栏图标、客户区
// 控件树一起释放，句柄随之失效。若只用一个「配置过没有」的布尔标志，再次弹出
// 得到的会是一个没有内容的空窗口。记住句柄之后，只要句柄与当前不一致就重新
// 配置一遍。NULL 表示还没有配置过。
HWND s_hDemoFrameConfigured = NULL;

// 组装演示窗口的客户区内容。
// 返回：新建的控件树根节点，所有权交给调用方。
std::unique_ptr<DuiControl> BuildDemoFrameContent()
{
    std::unique_ptr<DuiVBox> root(new DuiVBox());
    root->SetPadding(kDemoFrameContentPadding);
    root->SetGap(kDemoFrameContentGap);

    std::unique_ptr<DuiLabel> header(new DuiLabel());
    header->SetText(Txt(_T("按住标题栏拖动可以移动窗口。"),
                        _T("Drag the title bar to move the window.")));
    header->SetTextColor(RGB(45, 108, 223));
    root->AddChild(std::move(header), DuiLayout::Hint().Fixed(kDemoFrameHeaderHeight));

    CString strResizeHint;
    strResizeHint.Format(Txt(_T("拖动任意一条边或一个角改变大小，最小 %d×%d、最大 %d×%d。"),
                             _T("Drag any edge or corner to resize; stops at min %d×%d / max %d×%d.")),
                         kDemoFrameMinWidth, kDemoFrameMinHeight,
                         kDemoFrameMaxWidth, kDemoFrameMaxHeight);
    std::unique_ptr<DuiLabel> resizeHint(new DuiLabel());
    resizeHint->SetText(strResizeHint);
    resizeHint->SetTextColor(RGB(80, 80, 80));
    resizeHint->SetWordWrap(true);
    root->AddChild(std::move(resizeHint), DuiLayout::Hint().Fixed(kDemoFrameParagraphHeight));

    std::unique_ptr<DuiLabel> routeHint(new DuiLabel());
    routeHint->SetText(Txt(_T("最小化 / 最大化 / 关闭三个按钮都经 WM_SYSCOMMAND 走系统原有流程；")
                           _T("标题栏图标发的是 DUIFW_CAPTION_ICON_CLICK。"),
                           _T("Minimise / maximise / close all go through WM_SYSCOMMAND; ")
                           _T("caption icons fire DUIFW_CAPTION_ICON_CLICK.")));
    routeHint->SetTextColor(RGB(80, 80, 80));
    routeHint->SetWordWrap(true);
    root->AddChild(std::move(routeHint), DuiLayout::Hint().Fixed(kDemoFrameParagraphHeight));

    std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
    buttonRow->SetGap(kInnerRowGap);
    for (int i = 0; i < kDemoFrameActionCount; ++i)
    {
        std::unique_ptr<DuiButton> button(new DuiButton());
        CString strText;
        strText.Format(Txt(_T("操作 %d"), _T("Action %d")), i + 1);
        button->SetText(strText);
        buttonRow->AddChild(std::move(button), DuiLayout::Hint().Weight(1));
    }
    root->AddChild(std::move(buttonRow), DuiLayout::Hint().Fixed(kDemoFrameButtonHeight));

    // 余下的空间留白，把上面几行压在窗口顶部。
    root->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));

    return std::unique_ptr<DuiControl>(root.release());
}

// 给演示窗口装上标题、尺寸限制、标题栏图标与客户区内容。
// 仅在窗口刚被创建出来时调用一次。
void ApplyDemoFrameConfig()
{
    s_demoFrame.SetTitle(Txt(_T("自绘外观的框架窗口"), _T("Custom skin frame demo")));
    s_demoFrame.SetMinSize(kDemoFrameMinWidth, kDemoFrameMinHeight);
    s_demoFrame.SetMaxSize(kDemoFrameMaxWidth, kDemoFrameMaxHeight);

    // 标题栏图标用三张合成出来的渐变色块，各挂一条工具提示。位图存在静态变量
    // 里一直用到进程结束 —— 标题栏只按裸句柄引用它，不复制也不释放。
    static HBITMAP s_hIconSettings = MakeGradientIconBitmap(70, 130, 220, 30, 70, 160);
    static HBITMAP s_hIconBell = MakeGradientIconBitmap(255, 170, 60, 200, 90, 20);
    static HBITMAP s_hIconHelp = MakeGradientIconBitmap(80, 200, 110, 30, 130, 60);
    s_demoFrame.ClearCaptionIcons();
    s_demoFrame.AddCaptionIcon(s_hIconSettings, Txt(_T("设置"), _T("Settings")));
    s_demoFrame.AddCaptionIcon(s_hIconBell, Txt(_T("通知"), _T("Notifications")));
    s_demoFrame.AddCaptionIcon(s_hIconHelp, Txt(_T("帮助"), _T("Help")));

    s_demoFrame.SetClientContent(BuildDemoFrameContent());
}

// 确保演示窗口已经创建并配置完毕。
//   hOwner：作为所有者的窗口句柄，允许传 NULL。
// 返回：窗口可用时返回 true；创建失败时返回 false。
// 副作用：窗口不存在时会创建它（创建出来是隐藏的，显示由调用方决定）。
bool EnsureDemoFrame(HWND hOwner)
{
    if (s_demoFrame.m_hWnd == NULL)
    {
        s_demoFrame.Create(hOwner, CWindow::rcDefault,
                           kDemoFrameWindowName,
                           WS_OVERLAPPEDWINDOW,
                           0);
        if (s_demoFrame.m_hWnd == NULL)
        {
            return false;
        }
        s_demoFrame.ResizeClient(kDemoFrameInitWidth, kDemoFrameInitHeight);
        s_demoFrame.CenterWindow();
    }
    if (s_hDemoFrameConfigured != s_demoFrame.m_hWnd)
    {
        s_hDemoFrameConfigured = s_demoFrame.m_hWnd;
        ApplyDemoFrameConfig();
    }
    return true;
}

// 弹出演示窗口。
//   hOwner：作为所有者的窗口句柄，允许传 NULL。
void ShowDemoFrame(HWND hOwner)
{
    if (!EnsureDemoFrame(hOwner))
    {
        return;
    }
    s_demoFrame.ShowWindow(SW_SHOW);
    ::SetForegroundWindow(s_demoFrame.m_hWnd);
}

// 从一个演示按钮取它所在宿主窗口的句柄，用作弹出窗口的所有者。
//   pButton：被点击的按钮，允许传 NULL。
// 返回：宿主窗口句柄；取不到时返回 NULL（Create 允许所有者为空）。
HWND HostHwndOf(FnButton* pButton)
{
    if (pButton == NULL || pButton->GetHost() == NULL)
    {
        return NULL;
    }
    return pButton->GetHost()->m_hWnd;
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_FrameWindow()
{
    // 本页面里几个按钮的宽度（像素）。按钮文案长短不一，逐个给宽度。
    const int kPopButtonWidth = 160;
    const int kToggleButtonWidth = 116;
    const int kIconButtonWidth = 180;
    const int kSizeButtonWidth = 220;
    const int kUnlimitedButtonWidth = 180;

    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiFrameWindow —— 抹掉系统外框、自绘标题栏"),
                   _T("DuiFrameWindow - system chrome stripped, custom title bar")),
               Txt(_T("点「弹出窗口」开一个没有系统非客户区的顶层窗口：标题栏、三个按钮、四周的")
                   _T("拉伸边全部由 balloonui 自己画。按住标题栏可以移动，拖任意一条边或一个角可以")
                   _T("改变大小，最小化 / 最大化 / 关闭都是真正的 DUI 按钮，点下去经 WM_SYSCOMMAND ")
                   _T("走系统原有流程。标题栏上另外挂了三个自定义图标（设置 / 通知 / 帮助），")
                   _T("点它们会发 DUIFW_CAPTION_ICON_CLICK，本演示接住之后弹一个消息框显示 captionId。"),
                   _T("Click \"Pop frame\" to open a top-level window with no system non-client area: ")
                   _T("the title bar, its three buttons and the resize borders are all drawn by balloonui. ")
                   _T("Drag the title bar to move it, drag any edge or corner to resize it; minimise / ")
                   _T("maximise / close are real DUI buttons routed through WM_SYSCOMMAND. The title bar ")
                   _T("also carries three custom icons (Settings / Notifications / Help) that fire ")
                   _T("DUIFW_CAPTION_ICON_CLICK; this demo catches it and shows the captionId in a message box.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> popButton(new FnButton());
        popButton->SetText(Txt(_T("弹出窗口"), _T("Pop frame")));
        popButton->onClick = [](FnButton* b)
        {
            ShowDemoFrame(HostHwndOf(b));
        };
        row->AddChild(std::move(popButton), DuiLayout::Hint().Fixed(kPopButtonWidth));

        row->AddChild(MakeHintLabel(
                          Txt(_T("（关掉弹出的窗口之后再点一次，会重新建一个同样配置的窗口。）"),
                              _T("(Close the popped frame and click again to get a freshly built one.)")),
                          kHintTextColor),
                      DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("三个标题栏按钮 —— 用 SetButtons 逐个显隐"),
                   _T("Three caption buttons - show / hide via SetButtons")),
               Txt(_T("SetButtons(bool 最小化, bool 最大化, bool 关闭) 三个开关互相独立。")
                   _T("先把窗口弹出来，再点下面这几个按钮，标题栏会立刻跟着变。")
                   _T("窗口还没弹出时点也有效，配置会记在窗口上，下次弹出即生效。"),
                   _T("SetButtons(bool min, bool max, bool close) toggles each button independently. ")
                   _T("Pop the frame first, then click below and watch the title bar update live. ")
                   _T("Clicking before the frame is popped also works - the setting is applied to the ")
                   _T("window and shows up the next time it appears.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kInnerRowGap);

        std::unique_ptr<FnButton> btnAll(new FnButton());
        btnAll->SetText(Txt(_T("三个都要"), _T("All buttons")));
        btnAll->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetButtons(true, true, true);
            }
        };

        std::unique_ptr<FnButton> btnNoMin(new FnButton());
        btnNoMin->SetText(Txt(_T("去掉最小化"), _T("Hide min")));
        btnNoMin->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetButtons(false, true, true);
            }
        };

        std::unique_ptr<FnButton> btnNoMax(new FnButton());
        btnNoMax->SetText(Txt(_T("去掉最大化"), _T("Hide max")));
        btnNoMax->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetButtons(true, false, true);
            }
        };

        std::unique_ptr<FnButton> btnOnlyClose(new FnButton());
        btnOnlyClose->SetText(Txt(_T("只留关闭"), _T("Close only")));
        btnOnlyClose->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetButtons(false, false, true);
            }
        };

        std::unique_ptr<FnButton> btnNone(new FnButton());
        btnNone->SetText(Txt(_T("一个都不要"), _T("No buttons")));
        btnNone->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetButtons(false, false, false);
            }
        };

        row->AddChild(std::move(btnAll), DuiLayout::Hint().Fixed(kToggleButtonWidth));
        row->AddChild(std::move(btnNoMin), DuiLayout::Hint().Fixed(kToggleButtonWidth));
        row->AddChild(std::move(btnNoMax), DuiLayout::Hint().Fixed(kToggleButtonWidth));
        row->AddChild(std::move(btnOnlyClose), DuiLayout::Hint().Fixed(kToggleButtonWidth));
        row->AddChild(std::move(btnNone), DuiLayout::Hint().Fixed(kToggleButtonWidth));
        row->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("标题栏图标 —— 运行期增删"),
                   _T("Caption icons - add / remove at run time")),
               Txt(_T("AddCaptionIcon(HBITMAP, 提示文字) 在关闭按钮左侧追加一个可点击的图标，")
                   _T("返回一个 captionId 作为它的标识；ClearCaptionIcons() 一次清空全部。")
                   _T("窗口一建出来就带三个图标，点下面的按钮可以把它们清掉、再逐个加回来；")
                   _T("加完之后点标题栏上的图标，能看到 DUIFW_CAPTION_ICON_CLICK 带回来的 captionId。"),
                   _T("AddCaptionIcon(HBITMAP, tooltip) appends a clickable icon just left of the close ")
                   _T("button and returns a captionId identifying it; ClearCaptionIcons() removes them all. ")
                   _T("The frame starts with three icons; use the buttons below to clear them and add new ")
                   _T("ones, then click an icon on the title bar to see the captionId carried by ")
                   _T("DUIFW_CAPTION_ICON_CLICK.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kInnerRowGap);

        std::unique_ptr<FnButton> btnClear(new FnButton());
        btnClear->SetText(Txt(_T("清空标题栏图标"), _T("Clear caption icons")));
        btnClear->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.ClearCaptionIcons();
            }
        };

        std::unique_ptr<FnButton> btnAddOne(new FnButton());
        btnAddOne->SetText(Txt(_T("加一个新图标"), _T("Add one icon")));
        btnAddOne->onClick = [](FnButton* b)
        {
            if (!EnsureDemoFrame(HostHwndOf(b)))
            {
                return;
            }
            // 新图标的配色：用一个自增序号分别乘三个不同的步长再取模，落到
            // [基准值, 基准值 + 浮动范围) 区间里。步长取三个互不相同的奇数，
            // 连续几次生成的颜色就不会过于接近；数值本身没有别的含义。
            const int kColorBase = 60;
            const int kColorSpan = 180;
            const int kRedStride = 53;
            const int kGreenStride = 97;
            const int kBlueStride = 131;
            // 渐变底端颜色相对顶端的压暗倍数。
            const int kDarkenDivisor = 2;

            static int s_seq = 0;
            BYTE r = (BYTE)(kColorBase + (s_seq * kRedStride) % kColorSpan);
            BYTE g = (BYTE)(kColorBase + (s_seq * kGreenStride) % kColorSpan);
            BYTE bl = (BYTE)(kColorBase + (s_seq * kBlueStride) % kColorSpan);
            ++s_seq;

            HBITMAP hIcon = MakeGradientIconBitmap(r, g, bl,
                                                   r / kDarkenDivisor,
                                                   g / kDarkenDivisor,
                                                   bl / kDarkenDivisor);
            // 位图由标题栏按裸句柄引用，必须比窗口活得久。存进一个静态容器里
            // 留到进程结束，不在这里释放。
            static std::vector<HBITMAP> s_iconCache;
            s_iconCache.push_back(hIcon);

            CString strTip;
            strTip.Format(Txt(_T("新增图标 #%d"), _T("New icon #%d")), s_seq);
            s_demoFrame.AddCaptionIcon(hIcon, strTip);
        };

        row->AddChild(std::move(btnClear), DuiLayout::Hint().Fixed(kIconButtonWidth));
        row->AddChild(std::move(btnAddOne), DuiLayout::Hint().Fixed(kIconButtonWidth));
        row->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("拖动改变大小时的上下限"),
                   _T("Min / max drag size limits")),
               Txt(_T("SetMinSize / SetMaxSize 的值最终喂给 WM_GETMINMAXINFO，由操作系统在拖动过程中")
                   _T("强制执行，所以拖到边界就停住，不是拖完再弹回来。最大尺寸传 0 表示不限，")
                   _T("由操作系统沿用工作区大小作为上限；把上下限设成同一个值就等价于固定尺寸")
                   _T("（拉伸边的命中判定仍然在，要彻底禁止拖动得再调 SetResizable(false)）。")
                   _T("先把窗口弹出来，点下面的按钮换一组限制，再拖边验证。"),
                   _T("SetMinSize / SetMaxSize feed WM_GETMINMAXINFO, so the OS enforces them during the ")
                   _T("drag itself - the window stops at the limit instead of snapping back afterwards. ")
                   _T("A max size of 0 means \"no limit\" and lets the OS fall back to the work area. ")
                   _T("Setting min and max to the same value is equivalent to a fixed size (the resize ")
                   _T("edges still hit-test; call SetResizable(false) to disable dragging outright). ")
                   _T("Pop the frame first, click a preset below, then drag an edge to verify.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kInnerRowGap);

        CString strDefaultText;
        strDefaultText.Format(Txt(_T("默认 %d×%d ~ %d×%d"), _T("Default %d×%d - %d×%d")),
                              kDemoFrameMinWidth, kDemoFrameMinHeight,
                              kDemoFrameMaxWidth, kDemoFrameMaxHeight);
        std::unique_ptr<FnButton> btnDefault(new FnButton());
        btnDefault->SetText(strDefaultText);
        btnDefault->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetMinSize(kDemoFrameMinWidth, kDemoFrameMinHeight);
                s_demoFrame.SetMaxSize(kDemoFrameMaxWidth, kDemoFrameMaxHeight);
            }
        };

        std::unique_ptr<FnButton> btnUnlimited(new FnButton());
        btnUnlimited->SetText(Txt(_T("上限不限（最大传 0）"), _T("Unlimited (max = 0)")));
        btnUnlimited->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetMinSize(kDemoFrameMinWidth, kDemoFrameMinHeight);
                // 0 表示不限，由操作系统沿用工作区大小作为上限。
                s_demoFrame.SetMaxSize(0, 0);
            }
        };

        CString strFixedText;
        strFixedText.Format(Txt(_T("固定 %d×%d（上下限相同）"),
                                _T("Fixed %d×%d (min = max)")),
                            kDemoFrameFixedWidth, kDemoFrameFixedHeight);
        std::unique_ptr<FnButton> btnFixed(new FnButton());
        btnFixed->SetText(strFixedText);
        btnFixed->onClick = [](FnButton* b)
        {
            if (EnsureDemoFrame(HostHwndOf(b)))
            {
                s_demoFrame.SetMinSize(kDemoFrameFixedWidth, kDemoFrameFixedHeight);
                s_demoFrame.SetMaxSize(kDemoFrameFixedWidth, kDemoFrameFixedHeight);
            }
        };

        row->AddChild(std::move(btnDefault), DuiLayout::Hint().Fixed(kSizeButtonWidth));
        row->AddChild(std::move(btnUnlimited), DuiLayout::Hint().Fixed(kUnlimitedButtonWidth));
        row->AddChild(std::move(btnFixed), DuiLayout::Hint().Fixed(kSizeButtonWidth));
        row->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 九宫格背景（DuiNinePatch）
// =====================================================================

namespace {

// ---- 素材与内距 -----------------------------------------------------

// 演示用的背景图文件名。它与 DuiGallery.exe 放在同一个目录里，
// DuiXmlBuilder::ResolveAssetPath 会把它解析成绝对路径。
const LPCTSTR kDialogBgFileName = _T("BuddyInfoDlgBg.png");

// BuddyInfoDlgBg.png 的九宫格内距。这几个值是针对这张 587×535 的图设计的：
// 四角是圆角，顶部有一条渐变装饰带。
const int kDialogBgInsetLeft = 10;
const int kDialogBgInsetRight = 10;
const int kDialogBgInsetBottom = 10;
// 源图顶部渐变带的真实像素高度。
const int kDialogBgSrcGradientHeight = 69;
// 希望这条渐变带在目标里呈现成多高（像素），也就是标题栏的高度。
const int kDialogBgDstTitleBarHeight = 40;

// 单元测试 DuiNinePatchTests 里那张四角图的原始边长与内距（像素）。
const int kCornerTestBaseSize = 9;
const int kCornerTestBaseInset = 3;
// 演示时的放大倍数。原图只有 9×9，九块画到页面上每块不足两个像素，分界完全
// 看不出来；按整数倍放大之后配色与切分比例不变，每块却有几十像素。
const int kCornerTestScale = 10;
// 放大之后的边长与内距（像素）。
const int kCornerTestSize = kCornerTestBaseSize * kCornerTestScale;
const int kCornerTestInset = kCornerTestBaseInset * kCornerTestScale;

// 确保本模块的 GDI+ 已经初始化。
//
// GDI+ 要求每个使用它的模块自己初始化一次。画廊里只有命令行截图模式会在启动时
// 初始化，交互模式下没有别处做过，所以这里自己来。令牌记在静态变量里，进程退出
// 时由操作系统回收；不调 GdiplusShutdown —— 关闭之后别处再用 GDI+ 就会失败，
// 而这里无从知道别处是否还在用。
void EnsureGdiplusStarted()
{
    static ULONG_PTR s_token = 0;
    if (s_token != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&s_token, &input, NULL);
}

// 把一张 PNG 加载成完全不透明的位图。
//
// Gdiplus::Bitmap::GetHBITMAP(背景色, &hbm) 会先把图片合成到指定的背景色上，
// 输出的位图里 alpha 已经并进了三个颜色通道，后续 BitBlt / StretchBlt /
// DuiNinePatch::Draw 都可以按不透明位图处理。这样能同时绕开「alpha 全零的
// PNG 被 BitBlt 画成一片黑」和「位图字节序在不同路径下解释不一致」两个问题。
//   szPath：图片的绝对路径。
// 返回：新建的位图句柄；文件不存在或解码失败时返回 NULL。调用方负责释放，
//       本文件的调用点把它留到进程结束。
HBITMAP LoadOpaquePng(LPCTSTR szPath)
{
    EnsureGdiplusStarted();
    Gdiplus::Bitmap source(szPath);
    if (source.GetLastStatus() != Gdiplus::Ok)
    {
        return NULL;
    }
    HBITMAP hBitmap = NULL;
    if (source.GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &hBitmap) != Gdiplus::Ok)
    {
        return NULL;
    }
    return hBitmap;
}

// 取演示用的背景图位图，第一次调用时加载。
// 返回：位图句柄；素材文件缺失或解码失败时返回 NULL，调用方需要据此降级显示。
HBITMAP GetDialogBgBitmap()
{
    static bool s_loaded = false;
    static HBITMAP s_hBitmap = NULL;
    if (!s_loaded)
    {
        s_loaded = true;
        CString strPath = DuiXmlBuilder::ResolveAssetPath(kDialogBgFileName);
        if (!strPath.IsEmpty())
        {
            s_hBitmap = LoadOpaquePng(strPath);
        }
    }
    return s_hBitmap;
}

// 往一张自上而下排列的 32 位位图里填一块矩形色块。
//   pBits：像素首地址。
//   nStride：一行占多少个像素。
//   x0 / y0 / x1 / y1：要填充的矩形，左闭右开、上闭下开。
//   r / g / b：填充颜色的三个通道分量。
void FillBitmapRect(BYTE* pBits, int nStride,
                    int x0, int y0, int x1, int y1,
                    BYTE r, BYTE g, BYTE b)
{
    // 一个像素占的字节数（32 位位图，四个通道各一字节）。
    const int kBytesPerPixel = 4;
    // 不透明的 alpha 通道取值。
    const BYTE kOpaqueAlpha = 255;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            BYTE* pPixel = pBits + (y * nStride + x) * kBytesPerPixel;
            pPixel[0] = b;
            pPixel[1] = g;
            pPixel[2] = r;
            pPixel[3] = kOpaqueAlpha;
        }
    }
}

// 取「四角图」位图，第一次调用时合成。
//
// 配色与切分比例照搬单元测试 DuiNinePatchTests 里那张 9×9 的图：左上红、
// 右上绿、左下蓝、右下黄，其余留白，四角各占三分之一边长。这里只是按
// kCornerTestScale 整数倍放大了一遍，好让九块在页面上看得清。四个角颜色互不
// 相同，读者一眼就能看出哪一块被搬到了哪里、有没有被拉伸。
// 返回：位图句柄；创建失败时返回 NULL。位图留到进程结束，不释放。
HBITMAP GetCornerTestBitmap()
{
    static bool s_created = false;
    static HBITMAP s_hBitmap = NULL;
    if (s_created)
    {
        return s_hBitmap;
    }
    s_created = true;

    BITMAPINFO bi;
    ::ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kCornerTestSize;
    // 负高度表示自上而下排列的像素。
    bi.bmiHeader.biHeight = -kCornerTestSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    s_hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (s_hBitmap == NULL || pBits == NULL)
    {
        s_hBitmap = NULL;
        return NULL;
    }

    BYTE* pBase = (BYTE*)pBits;
    int nEdge = kCornerTestSize;
    int nInset = kCornerTestInset;
    // 先整张涂白，再覆盖四个角。
    FillBitmapRect(pBase, nEdge, 0, 0, nEdge, nEdge, 255, 255, 255);
    FillBitmapRect(pBase, nEdge, 0, 0, nInset, nInset, 255, 0, 0);
    FillBitmapRect(pBase, nEdge, nEdge - nInset, 0, nEdge, nInset, 0, 200, 0);
    FillBitmapRect(pBase, nEdge, 0, nEdge - nInset, nInset, nEdge, 0, 0, 255);
    FillBitmapRect(pBase, nEdge, nEdge - nInset, nEdge - nInset, nEdge, nEdge, 240, 220, 0);
    return s_hBitmap;
}

// ---- 演示控件 -------------------------------------------------------

// 在自己的矩形里画一张位图的演示控件，两种画法可切换。
//
//   · 九宫格：调 DuiNinePatch::Draw 把位图切成九块分别画 —— 四角原样复制、
//     四条边只沿一个方向拉伸、中央两个方向都拉伸。
//   · 朴素拉伸：直接 StretchBlt 整张图到目标矩形，作为反面对照。
//
// 为什么演示要用这种自绘控件，而不是直接调 DuiHost::SetBgImage？因为背景图铺满
// 的是整个宿主窗口的客户区，一个窗口同一时刻只能呈现一种尺寸，而这里要把几种
// 尺寸摆在同一个画面里对照。控件内部走的是与 DuiHost::SetBgImage 完全相同的
// DuiNinePatch::Draw，视觉上等价。真正调用 SetBgImage 的路径由本页面最后那个
// 「弹出真窗口」的段落演示。
class NinePatchTile : public DuiControl
{
public:
    // 画法。
    enum Mode
    {
        ModeNinePatch = 0,      // 九宫格：切九块分别画
        ModeNaiveStretch = 1,   // 朴素拉伸：整图一次拉到目标矩形
    };

    NinePatchTile()
        : m_hBitmap(NULL)
        , m_srcLeft(0)
        , m_srcTop(0)
        , m_srcRight(0)
        , m_srcBottom(0)
        , m_dstLeft(0)
        , m_dstTop(0)
        , m_dstRight(0)
        , m_dstBottom(0)
        , m_mode(ModeNinePatch)
    {
    }

    // 设置要画的位图。
    //   hBitmap：位图句柄。本控件只记句柄、不持有所有权，调用方须保证它比
    //            控件活得久；传 NULL 表示什么都不画。
    void SetBitmap(HBITMAP hBitmap)
    {
        if (m_hBitmap == hBitmap)
        {
            return;
        }
        m_hBitmap = hBitmap;
        Invalidate();
    }

    // 设置源内距与目标内距相同的经典九宫格内距。
    //   left / top / right / bottom：四边不参与拉伸的区域厚度（源图像素）。
    void SetInsets(int left, int top, int right, int bottom)
    {
        SetInsets(left, top, right, bottom, left, top, right, bottom);
    }

    // 设置源内距与目标内距各自独立的九宫格内距。
    //   srcLeft / srcTop / srcRight / srcBottom：源图里不参与拉伸的区域厚度（源图像素）。
    //   dstLeft / dstTop / dstRight / dstBottom：这些区域在目标里应当占多厚（目标像素）。
    void SetInsets(int srcLeft, int srcTop, int srcRight, int srcBottom,
                   int dstLeft, int dstTop, int dstRight, int dstBottom)
    {
        m_srcLeft = srcLeft;
        m_srcTop = srcTop;
        m_srcRight = srcRight;
        m_srcBottom = srcBottom;
        m_dstLeft = dstLeft;
        m_dstTop = dstTop;
        m_dstRight = dstRight;
        m_dstBottom = dstBottom;
        Invalidate();
    }

    // 设置画法。
    //   mode：ModeNinePatch 或 ModeNaiveStretch。
    void SetMode(Mode mode)
    {
        if (m_mode == mode)
        {
            return;
        }
        m_mode = mode;
        Invalidate();
    }

    // 绘制。
    //   hdc：目标设备上下文。
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible || m_hBitmap == NULL)
        {
            return;
        }

        if (m_mode == ModeNinePatch)
        {
            DuiNinePatch::Insets srcInsets;
            srcInsets.left = m_srcLeft;
            srcInsets.top = m_srcTop;
            srcInsets.right = m_srcRight;
            srcInsets.bottom = m_srcBottom;
            DuiNinePatch::Insets dstInsets;
            dstInsets.left = m_dstLeft;
            dstInsets.top = m_dstTop;
            dstInsets.right = m_dstRight;
            dstInsets.bottom = m_dstBottom;
            // 走双内距重载。两组内距相等时它退化为经典九宫格，与单内距重载等价。
            DuiNinePatch::Draw(hdc, m_hBitmap, m_rcItem, srcInsets, dstInsets);
            return;
        }

        // 反面对照：整张图一次拉伸到目标矩形。四角的圆角与顶部的装饰带都会
        // 按「目标尺寸 / 源尺寸」的比例被压扁或拉长。
        BITMAP bm;
        ::ZeroMemory(&bm, sizeof(bm));
        ::GetObject(m_hBitmap, sizeof(bm), &bm);
        HDC hMemDC = ::CreateCompatibleDC(hdc);
        HGDIOBJ hOld = ::SelectObject(hMemDC, m_hBitmap);
        ::SetStretchBltMode(hdc, COLORONCOLOR);
        ::StretchBlt(hdc,
                     m_rcItem.left, m_rcItem.top,
                     m_rcItem.right - m_rcItem.left,
                     m_rcItem.bottom - m_rcItem.top,
                     hMemDC, 0, 0, bm.bmWidth, bm.bmHeight,
                     SRCCOPY);
        ::SelectObject(hMemDC, hOld);
        ::DeleteDC(hMemDC);
    }

private:
    // 要画的位图。不持有所有权，可能为空。
    HBITMAP m_hBitmap;
    // 源内距四个分量（源图像素），生命周期与控件相同。
    int m_srcLeft;
    int m_srcTop;
    int m_srcRight;
    int m_srcBottom;
    // 目标内距四个分量（目标像素），生命周期与控件相同。
    int m_dstLeft;
    int m_dstTop;
    int m_dstRight;
    int m_dstBottom;
    // 当前画法。
    Mode m_mode;
};

// 九宫格分块示意图控件。
//
// 用途：把四角图画出来并叠上九块的边界线，每块中央标出它的缩放方式，让读者
// 直接看到哪一块原样复制、哪一块只沿一个方向拉伸、哪一块两个方向都拉伸。
//
// 两种视图：
//   · 源图视图：整张位图等比铺满控件矩形，边界线画在内距对应的位置上，
//     展示的是「源图是怎么被切开的」。
//   · 结果视图：调 DuiNinePatch::Draw 把位图画到控件矩形上，边界线画在
//     DuiNinePatch::ComputeCells 返回的目标矩形边界上，展示的是「切开之后
//     各块落在了哪里、各自被拉成了什么样」。
class NinePatchCellDiagram : public DuiControl
{
public:
    // 视图种类。
    enum ViewKind
    {
        ViewSource = 0,   // 源图视图：整张图铺满，边界线按源内距画
        ViewResult = 1,   // 结果视图：九宫格绘制结果，边界线按目标矩形画
    };

    NinePatchCellDiagram()
        : m_hBitmap(NULL)
        , m_insetLeft(0)
        , m_insetTop(0)
        , m_insetRight(0)
        , m_insetBottom(0)
        , m_viewKind(ViewSource)
    {
    }

    // 设置要演示的位图。
    //   hBitmap：位图句柄。本控件只记句柄、不持有所有权，调用方须保证它比
    //            控件活得久；传 NULL 表示什么都不画。
    void SetBitmap(HBITMAP hBitmap)
    {
        m_hBitmap = hBitmap;
        Invalidate();
    }

    // 设置九宫格内距（源图像素）。
    //   left / top / right / bottom：四边不参与拉伸的区域厚度。允许传负值或
    //   超过源图尺寸的值 —— DuiNinePatch::ClampInsets 会把它们收缩到合法范围，
    //   本控件的一个段落正是要演示这条规则。
    void SetInsets(int left, int top, int right, int bottom)
    {
        m_insetLeft = left;
        m_insetTop = top;
        m_insetRight = right;
        m_insetBottom = bottom;
        Invalidate();
    }

    // 设置视图种类。
    //   kind：ViewSource 或 ViewResult。
    void SetViewKind(ViewKind kind)
    {
        m_viewKind = kind;
        Invalidate();
    }

    // 绘制：先画底图，再叠边界线与缩放方式标记。
    //   hdc：目标设备上下文。
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible || m_hBitmap == NULL)
        {
            return;
        }

        BITMAP bm;
        ::ZeroMemory(&bm, sizeof(bm));
        if (::GetObject(m_hBitmap, sizeof(bm), &bm) == 0)
        {
            return;
        }
        if (bm.bmWidth <= 0 || bm.bmHeight <= 0)
        {
            return;
        }

        DuiNinePatch::Insets rawInsets;
        rawInsets.left = m_insetLeft;
        rawInsets.top = m_insetTop;
        rawInsets.right = m_insetRight;
        rawInsets.bottom = m_insetBottom;
        DuiNinePatch::Insets insets =
            DuiNinePatch::ClampInsets(bm.bmWidth, bm.bmHeight, rawInsets);

        // 四条竖直边界与四条水平边界的坐标，从左到右、从上到下。
        int xs[4];
        int ys[4];
        if (m_viewKind == ViewSource)
        {
            PaintSourceView(hdc, bm, insets, xs, ys);
        }
        else
        {
            PaintResultView(hdc, bm, insets, xs, ys);
        }
        DrawCellOverlay(hdc, xs, ys);
    }

private:
    // 画源图视图的底图，并算出九块的边界坐标。
    //   hdc：目标设备上下文。
    //   bm：位图信息，用来取源图尺寸。
    //   insets：已经收缩过的内距。
    //   xs / ys：输出四条竖直 / 水平边界的坐标（宿主客户区像素）。
    void PaintSourceView(HDC hdc, const BITMAP& bm,
                         const DuiNinePatch::Insets& insets,
                         int xs[4], int ys[4]) const
    {
        int nWidth = m_rcItem.right - m_rcItem.left;
        int nHeight = m_rcItem.bottom - m_rcItem.top;

        HDC hMemDC = ::CreateCompatibleDC(hdc);
        HGDIOBJ hOld = ::SelectObject(hMemDC, m_hBitmap);
        // 用 COLORONCOLOR 而不是 HALFTONE：放大时它取最近的那个源像素，
        // 色块之间保持硬边，四角与中央的分界看得清楚。
        ::SetStretchBltMode(hdc, COLORONCOLOR);
        ::StretchBlt(hdc, m_rcItem.left, m_rcItem.top, nWidth, nHeight,
                     hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        ::SelectObject(hMemDC, hOld);
        ::DeleteDC(hMemDC);

        // 整张图等比铺满，源图里的内距位置按同一个比例映射到控件矩形上。
        xs[0] = m_rcItem.left;
        xs[1] = m_rcItem.left + insets.left * nWidth / bm.bmWidth;
        xs[2] = m_rcItem.right - insets.right * nWidth / bm.bmWidth;
        xs[3] = m_rcItem.right;
        ys[0] = m_rcItem.top;
        ys[1] = m_rcItem.top + insets.top * nHeight / bm.bmHeight;
        ys[2] = m_rcItem.bottom - insets.bottom * nHeight / bm.bmHeight;
        ys[3] = m_rcItem.bottom;
    }

    // 画结果视图的底图，并从 ComputeCells 的输出里取九块的边界坐标。
    //   hdc：目标设备上下文。
    //   bm：位图信息，用来取源图尺寸。
    //   insets：已经收缩过的内距。
    //   xs / ys：输出四条竖直 / 水平边界的坐标（宿主客户区像素）。
    void PaintResultView(HDC hdc, const BITMAP& bm,
                         const DuiNinePatch::Insets& insets,
                         int xs[4], int ys[4]) const
    {
        DuiNinePatch::Options opts;
        // 四角图是自己合成的、alpha 全为不透明的位图，走不带混合的那条路径
        // 即可，与单元测试里的用法一致。
        opts.hasAlpha = false;
        DuiNinePatch::Draw(hdc, m_hBitmap, m_rcItem, insets, opts);

        DuiNinePatch::Cell cells[9];
        DuiNinePatch::ComputeCells(bm.bmWidth, bm.bmHeight, m_rcItem, insets, cells);
        // cells 的下标次序是 0=左上 1=上 2=右上 / 3=左 4=中 5=右 / 6=左下 7=下 8=右下，
        // 所以左上块的四条边正好给出内侧的两条边界，右下块给出外侧的两条。
        xs[0] = cells[0].dst.left;
        xs[1] = cells[0].dst.right;
        xs[2] = cells[8].dst.left;
        xs[3] = cells[8].dst.right;
        ys[0] = cells[0].dst.top;
        ys[1] = cells[0].dst.bottom;
        ys[2] = cells[8].dst.top;
        ys[3] = cells[8].dst.bottom;
    }

    // 在九块的边界上画线，并在每块中央标出它的缩放方式。
    //   hdc：目标设备上下文。
    //   xs / ys：四条竖直 / 水平边界的坐标。
    void DrawCellOverlay(HDC hdc, const int xs[4], const int ys[4]) const
    {
        // 边界线的颜色与粗细。
        const COLORREF kLineColor = RGB(20, 20, 20);
        const int kLineWidth = 1;
        // 缩放方式标记的底色与文字色。标记画在一小块不透明底色上，免得压在
        // 红蓝深色块上看不清。
        const COLORREF kMarkerBackColor = RGB(255, 255, 255);
        const COLORREF kMarkerTextColor = RGB(20, 20, 20);
        // 标记底色块的宽高（像素）。
        const int kMarkerWidth = 36;
        const int kMarkerHeight = 16;
        // 一块小于这个尺寸时不画标记，否则文字会盖满整块。
        const int kMarkerMinCellSize = 22;

        // 九块各自的缩放方式。1:1 表示原样复制，H 表示只沿水平方向拉伸，
        // V 表示只沿竖直方向拉伸，H+V 表示两个方向都拉伸。
        static const LPCTSTR kCellMarkers[9] = {
            _T("1:1"), _T("H"),   _T("1:1"),
            _T("V"),   _T("H+V"), _T("V"),
            _T("1:1"), _T("H"),   _T("1:1"),
        };

        HPEN hPen = ::CreatePen(PS_SOLID, kLineWidth, kLineColor);
        HGDIOBJ hOldPen = ::SelectObject(hdc, hPen);
        // 两条竖直内界线。
        for (int i = 1; i <= 2; ++i)
        {
            ::MoveToEx(hdc, xs[i], ys[0], NULL);
            ::LineTo(hdc, xs[i], ys[3]);
        }
        // 两条水平内界线。
        for (int i = 1; i <= 2; ++i)
        {
            ::MoveToEx(hdc, xs[0], ys[i], NULL);
            ::LineTo(hdc, xs[3], ys[i]);
        }
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);

        // 外框单独画一圈，让整张示意图的边界也清楚。
        RECT rcFrame;
        rcFrame.left = xs[0];
        rcFrame.top = ys[0];
        rcFrame.right = xs[3];
        rcFrame.bottom = ys[3];
        HBRUSH hFrameBrush = ::CreateSolidBrush(kLineColor);
        ::FrameRect(hdc, &rcFrame, hFrameBrush);
        ::DeleteObject(hFrameBrush);

        HBRUSH hMarkerBrush = ::CreateSolidBrush(kMarkerBackColor);
        int nOldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF clrOldText = ::SetTextColor(hdc, kMarkerTextColor);
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                int nCellWidth = xs[col + 1] - xs[col];
                int nCellHeight = ys[row + 1] - ys[row];
                if (nCellWidth < kMarkerMinCellSize || nCellHeight < kMarkerMinCellSize)
                {
                    continue;
                }
                int cx = (xs[col] + xs[col + 1]) / 2;
                int cy = (ys[row] + ys[row + 1]) / 2;
                RECT rcMarker;
                rcMarker.left = cx - kMarkerWidth / 2;
                rcMarker.top = cy - kMarkerHeight / 2;
                rcMarker.right = rcMarker.left + kMarkerWidth;
                rcMarker.bottom = rcMarker.top + kMarkerHeight;
                ::FillRect(hdc, &rcMarker, hMarkerBrush);
                ::DrawText(hdc, kCellMarkers[row * 3 + col], -1, &rcMarker,
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        ::SetTextColor(hdc, clrOldText);
        ::SetBkMode(hdc, nOldBkMode);
        ::DeleteObject(hMarkerBrush);
    }

    // 要演示的位图。不持有所有权，可能为空。
    HBITMAP m_hBitmap;
    // 九宫格内距四个分量（源图像素）。可能是负值或越界值，绘制时再收缩。
    int m_insetLeft;
    int m_insetTop;
    int m_insetRight;
    int m_insetBottom;
    // 当前视图种类。
    ViewKind m_viewKind;
};

// ---- 九宫格背景的真窗口演示 -----------------------------------------

// 演示九宫格背景的框架窗口。与上面那个框架窗口演示各用一个实例，互不干扰。
DuiFrameWindow s_bgFrame;
// 上一次完成配置时该窗口的句柄，用途同 s_hDemoFrameConfigured。
HWND s_hBgFrameConfigured = NULL;

// 传给 Create 的窗口名，用途同 kDemoFrameWindowName。
const LPCTSTR kBgFrameWindowName = _T("DuiGallery nine-patch bg demo");
// 该窗口弹出时的客户区尺寸（像素）。
const int kBgFrameInitWidth = 460;
const int kBgFrameInitHeight = 420;
// 该窗口允许缩到的最小尺寸（像素）。
const int kBgFrameMinWidth = 320;
const int kBgFrameMinHeight = 240;
// 该窗口客户区的内边距与行高（像素）。
const int kBgFrameContentPadding = 24;
const int kBgFrameContentGap = 10;
// 单行文字的高度。
const int kBgFrameTextHeight = 24;
// 需要换到两行的说明文字的高度。窗口可以被拖窄，所以这两条说明开了自动换行，
// 高度按两行留。
const int kBgFrameParagraphHeight = 46;

// 组装九宫格背景演示窗口的客户区内容。
// 返回：新建的控件树根节点，所有权交给调用方。
std::unique_ptr<DuiControl> BuildBgFrameContent()
{
    std::unique_ptr<DuiVBox> root(new DuiVBox());
    root->SetPadding(kBgFrameContentPadding);
    root->SetGap(kBgFrameContentGap);

    std::unique_ptr<DuiLabel> line1(new DuiLabel());
    line1->SetText(Txt(_T("整个客户区的背景由 DuiHost::LoadBgImageFromFile 画。"),
                       _T("The whole client area is painted by DuiHost::LoadBgImageFromFile.")));
    line1->SetTextColor(RGB(20, 20, 20));
    root->AddChild(std::move(line1), DuiLayout::Hint().Fixed(kBgFrameTextHeight));

    std::unique_ptr<DuiLabel> line2(new DuiLabel());
    line2->SetText(Txt(_T("拖动窗口的角把它放大缩小：四角的圆角不会变形，顶部那条渐变带也不会被压扁。"),
                       _T("Drag a corner to resize: the rounded corners keep their shape and the top ")
                       _T("gradient band is not squashed.")));
    line2->SetTextColor(RGB(40, 40, 40));
    line2->SetWordWrap(true);
    root->AddChild(std::move(line2), DuiLayout::Hint().Fixed(kBgFrameParagraphHeight));

    std::unique_ptr<DuiLabel> line3(new DuiLabel());
    line3->SetText(Txt(_T("标题栏设成了透明并且高度与目标内距一致，所以背景图顶部的渐变带直接充当标题栏。"),
                       _T("The title bar is transparent and exactly as tall as the destination inset, ")
                       _T("so the gradient band doubles as the title bar.")));
    line3->SetTextColor(RGB(40, 40, 40));
    line3->SetWordWrap(true);
    root->AddChild(std::move(line3), DuiLayout::Hint().Fixed(kBgFrameParagraphHeight));

    root->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
    return std::unique_ptr<DuiControl>(root.release());
}

// 给九宫格背景演示窗口装上标题、尺寸限制、背景图与客户区内容。
// 仅在窗口刚被创建出来时调用一次。
void ApplyBgFrameConfig()
{
    s_bgFrame.SetTitle(Txt(_T("九宫格背景窗口"), _T("Nine-patch background window")));
    s_bgFrame.SetMinSize(kBgFrameMinWidth, kBgFrameMinHeight);
    s_bgFrame.SetResizable(true);
    // 标题栏高度取目标内距的上边厚度，透明之后背景图顶部那条渐变带正好落在
    // 标题栏的位置上；标题文字与三个按钮改成白色以便在彩色渐变上看得清。
    s_bgFrame.SetTitleBarHeight(kDialogBgDstTitleBarHeight);
    s_bgFrame.SetTitleBarTransparent(true);
    s_bgFrame.SetTitleTextColor(RGB(255, 255, 255));
    s_bgFrame.SetCaptionGlyphColor(RGB(255, 255, 255));

    CString strPath = DuiXmlBuilder::ResolveAssetPath(kDialogBgFileName);
    if (!strPath.IsEmpty())
    {
        RECT rcSrcInsets;
        rcSrcInsets.left = kDialogBgInsetLeft;
        rcSrcInsets.top = kDialogBgSrcGradientHeight;
        rcSrcInsets.right = kDialogBgInsetRight;
        rcSrcInsets.bottom = kDialogBgInsetBottom;
        RECT rcDstInsets;
        rcDstInsets.left = kDialogBgInsetLeft;
        rcDstInsets.top = kDialogBgDstTitleBarHeight;
        rcDstInsets.right = kDialogBgInsetRight;
        rcDstInsets.bottom = kDialogBgInsetBottom;
        // 这条路径下位图由窗口自己加载并持有，窗口销毁时一并释放，调用方不用管。
        s_bgFrame.LoadBgImageFromFile(strPath, rcSrcInsets, rcDstInsets);
    }

    s_bgFrame.SetClientContent(BuildBgFrameContent());
}

// 弹出九宫格背景演示窗口。
//   hOwner：作为所有者的窗口句柄，允许传 NULL。
void ShowBgFrame(HWND hOwner)
{
    if (s_bgFrame.m_hWnd == NULL)
    {
        s_bgFrame.Create(hOwner, CWindow::rcDefault,
                         kBgFrameWindowName,
                         WS_OVERLAPPEDWINDOW,
                         0);
        if (s_bgFrame.m_hWnd == NULL)
        {
            return;
        }
        s_bgFrame.ResizeClient(kBgFrameInitWidth, kBgFrameInitHeight);
        s_bgFrame.CenterWindow();
    }
    if (s_hBgFrameConfigured != s_bgFrame.m_hWnd)
    {
        s_hBgFrameConfigured = s_bgFrame.m_hWnd;
        ApplyBgFrameConfig();
    }
    s_bgFrame.ShowWindow(SW_SHOW);
    ::SetForegroundWindow(s_bgFrame.m_hWnd);
}

// ---- 页面装配用的小工具 ---------------------------------------------

// 组装一个「上面一段小字说明、下面一块演示图」的竖直单元。
//   szCaption：小字说明。允许含换行符，标签开了自动换行，换行符会正常生效。
//   demo：演示控件，所有权转移给新建的容器。
//   nCaptionHeight：小字说明那一段占的高度（像素）。
// 返回：新建的竖直容器，所有权交给调用方。
std::unique_ptr<DuiVBox> MakeCaptionedDemo(LPCTSTR szCaption,
                                           std::unique_ptr<DuiControl> demo,
                                           int nCaptionHeight)
{
    // 小字说明与下方演示图之间的间距（像素）。
    const int kCaptionGap = 4;

    std::unique_ptr<DuiVBox> box(new DuiVBox());
    box->SetGap(kCaptionGap);
    std::unique_ptr<DuiLabel> caption = MakeHintLabel(szCaption, kHintTextColor);
    // 说明写成两行，必须开自动换行 —— 标签默认按单行绘制，那条路径会忽略
    // 文字里的换行符。
    caption->SetWordWrap(true);
    box->AddChild(std::move(caption), DuiLayout::Hint().Fixed(nCaptionHeight));
    box->AddChild(std::move(demo), DuiLayout::Hint().Weight(1));
    return box;
}

// 新建一块用给定内距画背景图的九宫格瓦片。
//   srcTop：源内距的上边厚度（源图像素）。
//   dstTop：目标内距的上边厚度（目标像素）。
//   mode：画法。
// 返回：新建的瓦片控件，所有权交给调用方。背景图缺失时返回的控件什么都不画。
std::unique_ptr<NinePatchTile> MakeDialogBgTile(int srcTop, int dstTop,
                                                NinePatchTile::Mode mode)
{
    std::unique_ptr<NinePatchTile> tile(new NinePatchTile());
    tile->SetBitmap(GetDialogBgBitmap());
    tile->SetInsets(kDialogBgInsetLeft, srcTop, kDialogBgInsetRight, kDialogBgInsetBottom,
                    kDialogBgInsetLeft, dstTop, kDialogBgInsetRight, kDialogBgInsetBottom);
    tile->SetMode(mode);
    return tile;
}

// 新建一条「素材文件缺失」的提示标签，用于背景图加载失败时的降级显示。
// 返回：新建的标签，所有权交给调用方。
std::unique_ptr<DuiLabel> MakeBgMissingLabel()
{
    CString strText;
    strText.Format(Txt(_T("找不到演示用的背景图 %s，这一段落无法显示。把它放到 DuiGallery.exe 所在目录即可。"),
                       _T("The demo background image %s is missing, so this section cannot be shown. ")
                       _T("Put it next to DuiGallery.exe to enable it.")),
                   kDialogBgFileName);
    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(strText);
    label->SetTextColor(kWarnTextColor);
    return label;
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_NinePatch()
{
    // 尺寸对照那一行里三块瓦片的宽度与整行的高度（像素）。三块用同一张源图、
    // 同一组内距，只有尺寸不同。
    const int kTileSmallWidth = 130;
    const int kTileMediumWidth = 200;
    const int kTileLargeWidth = 280;
    const int kTileRowHeight = 150;
    // 尺寸对照两行行首那一列说明文字的宽度（像素）。
    const int kTileRowCaptionWidth = 80;
    // 分块示意图那一行的高度与单张示意图的宽度（像素）。
    const int kDiagramRowHeight = 210;
    const int kDiagramWidth = 230;
    // 内距对照与收缩规则两行的高度，以及其中小字说明的行高（像素）。
    const int kInsetRowHeight = 190;
    const int kCaptionHeight = 34;
    // 收缩规则那一行里单块示意图的宽度（像素）。
    const int kClampDiagramWidth = 210;
    // 弹出按钮的宽度（像素）。
    const int kPopButtonWidth = 200;

    std::unique_ptr<GalleryPageBox> page = NewPage();

    // ---- 段落一：同一张图缩放到不同尺寸 ----
    AddSection(page.get(),
               Txt(_T("同一张图缩放到不同尺寸"), _T("One image, several sizes")),
               Txt(_T("九宫格把一张位图按四个内距值切成九块：四个角原样复制不缩放，")
                   _T("上下两条边只沿水平方向拉伸，左右两条边只沿竖直方向拉伸，中央一块两个方向都拉伸。")
                   _T("于是同一张图缩放到任意尺寸，圆角、描边、渐变装饰这些长在边缘上的细节都不会变形。")
                   _T("下面第一行是九宫格画法，第二行是同样三个尺寸下直接把整图拉过去的结果，")
                   _T("对比一下四角的圆角和顶部那条渐变带。"),
                   _T("A nine-patch cuts one bitmap into nine pieces using four inset values: the four ")
                   _T("corners are copied 1:1, the top and bottom edges stretch horizontally only, the ")
                   _T("left and right edges stretch vertically only, and the centre stretches both ways. ")
                   _T("That way the same image scales to any size without distorting rounded corners, ")
                   _T("borders or gradient bands. The first row below uses the nine-patch; the second ")
                   _T("stretches the whole image to the same three sizes. Compare the corners and the ")
                   _T("gradient band at the top.")));
    if (GetDialogBgBitmap() == NULL)
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(MakeBgMissingLabel(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    else
    {
        std::unique_ptr<DuiHBox> patchRow(new DuiHBox());
        patchRow->SetGap(kRowGap);
        patchRow->AddChild(MakeHintLabel(Txt(_T("九宫格"), _T("Nine-patch")), kHintTextColor),
                           DuiLayout::Hint().Fixed(kTileRowCaptionWidth));
        patchRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNinePatch),
                           DuiLayout::Hint().Fixed(kTileSmallWidth));
        patchRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNinePatch),
                           DuiLayout::Hint().Fixed(kTileMediumWidth));
        patchRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNinePatch),
                           DuiLayout::Hint().Fixed(kTileLargeWidth));
        patchRow->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(patchRow), kTileRowHeight);

        AddGap(page.get(), kInnerRowGap);

        std::unique_ptr<DuiHBox> naiveRow(new DuiHBox());
        naiveRow->SetGap(kRowGap);
        naiveRow->AddChild(MakeHintLabel(Txt(_T("整图拉伸"), _T("Plain stretch")), kWarnTextColor),
                           DuiLayout::Hint().Fixed(kTileRowCaptionWidth));
        naiveRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNaiveStretch),
                           DuiLayout::Hint().Fixed(kTileSmallWidth));
        naiveRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNaiveStretch),
                           DuiLayout::Hint().Fixed(kTileMediumWidth));
        naiveRow->AddChild(MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                            kDialogBgDstTitleBarHeight,
                                            NinePatchTile::ModeNaiveStretch),
                           DuiLayout::Hint().Fixed(kTileLargeWidth));
        naiveRow->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(naiveRow), kTileRowHeight);
    }

    // ---- 段落二：九块分别是怎么缩放的 ----
    AddSection(page.get(),
               Txt(_T("九块分别是怎么缩放的"), _T("How each of the nine cells scales")),
               Txt(_T("左边是源图，四个角故意涂成红 / 绿 / 蓝 / 黄四种颜色（配色照搬单元测试 ")
                   _T("DuiNinePatchTests 里那张 9×9 的图，这里按整数倍放大以便看清）；")
                   _T("右边是同一张图经 DuiNinePatch::Draw 画到一块更宽更矮的矩形上的结果。")
                   _T("两张图上的分割线都由 DuiNinePatch::ComputeCells 算出，")
                   _T("每块中央标着它的缩放方式：1:1 表示原样复制，H 表示只沿水平方向拉伸，")
                   _T("V 表示只沿竖直方向拉伸，H+V 表示两个方向都拉伸。")
                   _T("对照两张图能看出四个色块的形状始终没变，变形只发生在边和中央。"),
                   _T("On the left is the source image with its four corners painted red / green / blue / ")
                   _T("yellow (the colours come from the 9×9 image in the DuiNinePatchTests unit tests, ")
                   _T("scaled up here so the cells are readable). On the right is the same image drawn by ")
                   _T("DuiNinePatch::Draw into a wider, shorter rectangle. The dividing lines on both come ")
                   _T("from DuiNinePatch::ComputeCells, and each cell is marked with how it scales: 1:1 ")
                   _T("means copied as-is, H means stretched horizontally only, V vertically only, H+V ")
                   _T("both ways. The four coloured squares keep their shape; only the edges and the ")
                   _T("centre are distorted.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<NinePatchCellDiagram> sourceView(new NinePatchCellDiagram());
        sourceView->SetBitmap(GetCornerTestBitmap());
        sourceView->SetInsets(kCornerTestInset, kCornerTestInset,
                              kCornerTestInset, kCornerTestInset);
        sourceView->SetViewKind(NinePatchCellDiagram::ViewSource);
        row->AddChild(MakeCaptionedDemo(Txt(_T("源图与切分位置"), _T("Source and cut lines")),
                                        std::unique_ptr<DuiControl>(sourceView.release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Fixed(kDiagramWidth));

        std::unique_ptr<NinePatchCellDiagram> resultView(new NinePatchCellDiagram());
        resultView->SetBitmap(GetCornerTestBitmap());
        resultView->SetInsets(kCornerTestInset, kCornerTestInset,
                              kCornerTestInset, kCornerTestInset);
        resultView->SetViewKind(NinePatchCellDiagram::ViewResult);
        row->AddChild(MakeCaptionedDemo(Txt(_T("九宫格绘制结果"), _T("Nine-patch result")),
                                        std::unique_ptr<DuiControl>(resultView.release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kDiagramRowHeight);
    }

    // ---- 段落三：单内距与双内距 ----
    AddSection(page.get(),
               Txt(_T("单内距与双内距"), _T("Single insets vs. separate source / destination insets")),
               Txt(_T("演示用的这张背景图顶部有一条渐变装饰带，实测高度是 69 像素，")
                   _T("而希望它在窗口里呈现成 40 像素高的标题栏。")
                   _T("单内距只有一组值，源和目标必须相同，于是只能二选一：")
                   _T("上边取 69，标题栏就得做成 69 像素高，太高；上边取 40，")
                   _T("源图里第 40 到第 69 行那段渐变残余就落进了中间的拉伸区，")
                   _T("被竖向拉长后在标题栏下方留下一道明显的接缝。")
                   _T("双内距重载让两者独立：源内距取 69 覆盖整条装饰带，目标内距取 40，")
                   _T("整条渐变带被等比压缩到 40 像素，既完整又不越界。下面三块从左到右正是这三种取法。"),
                   _T("The demo image has a gradient band across the top; it is 69 px tall in the source, ")
                   _T("but we want it to appear as a 40 px title bar. With a single set of insets, source ")
                   _T("and destination must match, so it is one or the other: use 69 and the title bar has ")
                   _T("to be 69 px tall, which is too much; use 40 and the leftover rows 40..69 of the ")
                   _T("gradient fall into the stretched middle band, where they are pulled vertically and ")
                   _T("leave a dirty seam under the title bar. The two-inset overload separates them: a ")
                   _T("source inset of 69 covers the whole band, a destination inset of 40 compresses it ")
                   _T("proportionally into 40 px - complete and inside its area. The three tiles below ")
                   _T("show exactly these three choices.")));
    if (GetDialogBgBitmap() == NULL)
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(MakeBgMissingLabel(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    else
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        CString strCaptionTall;
        strCaptionTall.Format(Txt(_T("单内距，上边 %d\n装饰带完整，但标题栏太高"),
                                  _T("Single inset, top = %d\nband intact, title bar too tall")),
                              kDialogBgSrcGradientHeight);
        row->AddChild(MakeCaptionedDemo(strCaptionTall,
                                        std::unique_ptr<DuiControl>(
                                            MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                                             kDialogBgSrcGradientHeight,
                                                             NinePatchTile::ModeNinePatch).release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Weight(1));

        CString strCaptionSeam;
        strCaptionSeam.Format(Txt(_T("单内距，上边 %d\n渐变残余被拉进中间区"),
                                  _T("Single inset, top = %d\nleftover gradient in the middle")),
                              kDialogBgDstTitleBarHeight);
        row->AddChild(MakeCaptionedDemo(strCaptionSeam,
                                        std::unique_ptr<DuiControl>(
                                            MakeDialogBgTile(kDialogBgDstTitleBarHeight,
                                                             kDialogBgDstTitleBarHeight,
                                                             NinePatchTile::ModeNinePatch).release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Weight(1));

        CString strCaptionDual;
        strCaptionDual.Format(Txt(_T("双内距，源 %d 目标 %d\n整条装饰带压缩进标题栏"),
                                  _T("Two insets, src %d dst %d\nband compressed into the bar")),
                              kDialogBgSrcGradientHeight, kDialogBgDstTitleBarHeight);
        row->AddChild(MakeCaptionedDemo(strCaptionDual,
                                        std::unique_ptr<DuiControl>(
                                            MakeDialogBgTile(kDialogBgSrcGradientHeight,
                                                             kDialogBgDstTitleBarHeight,
                                                             NinePatchTile::ModeNinePatch).release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kInsetRowHeight);
    }

    // ---- 段落四：内距越界时的收缩规则 ----
    {
        // 单元测试 DuiNinePatchTests 里演示越界收缩用的内距，相对 9 像素的原图。
        // 两边合计 14 已经超过源图宽度，正好触发按比例收缩。
        const int kClampOverflowBaseInset = 7;

        // 这一组示意图用的三种内距（源图像素）。中间那一组故意超出源图尺寸，
        // 右边那一组故意取负值，用来演示 ClampInsets 的两条收缩规则。
        const int kClampNormalInset = kCornerTestInset;
        const int kClampOverflowInset = kClampOverflowBaseInset * kCornerTestScale;
        const int kClampNegativeInset = -kCornerTestInset;

        // 把这三组内距真的喂给 ClampInsets，把收缩之后的结果写进小字说明里，
        // 说明和实际行为就不会对不上。
        DuiNinePatch::Insets overflowRaw;
        overflowRaw.left = kClampOverflowInset;
        overflowRaw.top = kClampOverflowInset;
        overflowRaw.right = kClampOverflowInset;
        overflowRaw.bottom = kClampOverflowInset;
        DuiNinePatch::Insets overflowClamped =
            DuiNinePatch::ClampInsets(kCornerTestSize, kCornerTestSize, overflowRaw);

        // 段落说明里要带上源图边长，先拼好再传进去 —— AddSection 内部会把文字
        // 复制到标签上，临时的 CString 只需要活到调用返回。
        CString strClampDesc;
        strClampDesc.Format(
            Txt(_T("内距是调用方给的，难免出现不合法的值，DuiNinePatch::ClampInsets 负责把它们")
                _T("收进合法范围，不会让绘制出错：负值一律归零；同一根轴上两个值加起来超过源图")
                _T("对应的尺寸时，两个值按各自的比例一起缩小，四个角因此永远不会互相重叠。")
                _T("下面三块用的是同一张四角图（边长 %d 像素），只有内距不同。"),
                _T("Insets come from the caller, so invalid values are bound to happen. ")
                _T("DuiNinePatch::ClampInsets pulls them back into range instead of letting the ")
                _T("paint go wrong: negative values become zero, and when the two values on one ")
                _T("axis add up to more than the source dimension they are both shrunk ")
                _T("proportionally, so the corners can never overlap. The three tiles below use ")
                _T("the same corner image (%d px per side) with different insets.")),
            kCornerTestSize);

        AddSection(page.get(),
                   Txt(_T("内距越界时的收缩规则"), _T("What ClampInsets does with out-of-range values")),
                   strClampDesc);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        CString strNormalCaption;
        strNormalCaption.Format(Txt(_T("内距 %d，合法\n原样使用"), _T("Inset %d, valid\nused as-is")),
                                kClampNormalInset);
        std::unique_ptr<NinePatchCellDiagram> normalView(new NinePatchCellDiagram());
        normalView->SetBitmap(GetCornerTestBitmap());
        normalView->SetInsets(kClampNormalInset, kClampNormalInset,
                              kClampNormalInset, kClampNormalInset);
        normalView->SetViewKind(NinePatchCellDiagram::ViewResult);
        row->AddChild(MakeCaptionedDemo(strNormalCaption,
                                        std::unique_ptr<DuiControl>(normalView.release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Fixed(kClampDiagramWidth));

        CString strOverflowCaption;
        strOverflowCaption.Format(Txt(_T("内距 %d，两边合计超过 %d\n按比例缩成 %d 和 %d"),
                                      _T("Inset %d, pair exceeds %d\nshrunk to %d and %d")),
                                  kClampOverflowInset, kCornerTestSize,
                                  overflowClamped.left, overflowClamped.right);
        std::unique_ptr<NinePatchCellDiagram> overflowView(new NinePatchCellDiagram());
        overflowView->SetBitmap(GetCornerTestBitmap());
        overflowView->SetInsets(kClampOverflowInset, kClampOverflowInset,
                                kClampOverflowInset, kClampOverflowInset);
        overflowView->SetViewKind(NinePatchCellDiagram::ViewResult);
        row->AddChild(MakeCaptionedDemo(strOverflowCaption,
                                        std::unique_ptr<DuiControl>(overflowView.release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Fixed(kClampDiagramWidth));

        CString strNegativeCaption;
        strNegativeCaption.Format(Txt(_T("内距 %d，负值\n一律归零，退化成整图拉伸"),
                                      _T("Inset %d, negative\nclamped to zero, becomes a plain stretch")),
                                  kClampNegativeInset);
        std::unique_ptr<NinePatchCellDiagram> negativeView(new NinePatchCellDiagram());
        negativeView->SetBitmap(GetCornerTestBitmap());
        negativeView->SetInsets(kClampNegativeInset, kClampNegativeInset,
                                kClampNegativeInset, kClampNegativeInset);
        negativeView->SetViewKind(NinePatchCellDiagram::ViewResult);
        row->AddChild(MakeCaptionedDemo(strNegativeCaption,
                                        std::unique_ptr<DuiControl>(negativeView.release()),
                                        kCaptionHeight),
                      DuiLayout::Hint().Fixed(kClampDiagramWidth));

        row->AddChild(MakeSpacer(), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kInsetRowHeight);
    }

    // ---- 段落五：弹出一个真的用九宫格背景的窗口 ----
    AddSection(page.get(),
               Txt(_T("弹出一个真的用九宫格背景的窗口"),
                   _T("Pop a real window with a nine-patch background")),
               Txt(_T("上面几段用的都是自绘瓦片，因为一个窗口同一时刻只能是一种尺寸，")
                   _T("摆不下几种尺寸的对照。这一段走的是真正的接口：")
                   _T("DuiHost::LoadBgImageFromFile 加载图片并接管它的生命周期，")
                   _T("窗口的整个客户区由九宫格画出来。弹出之后拖动窗口的角把它放大缩小，")
                   _T("四角的圆角和顶部的渐变带都不会变形。"),
                   _T("The sections above use hand-drawn tiles, because one window can only be one size ")
                   _T("at a time and could not show several side by side. This one uses the real API: ")
                   _T("DuiHost::LoadBgImageFromFile loads the image and takes ownership of it, and the ")
                   _T("whole client area is painted by the nine-patch. Pop the window and drag a corner ")
                   _T("to resize it - the rounded corners and the gradient band keep their shape.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> popButton(new FnButton());
        popButton->SetText(Txt(_T("弹出九宫格背景窗口"), _T("Pop background window")));
        popButton->onClick = [](FnButton* b)
        {
            ShowBgFrame(HostHwndOf(b));
        };
        // 素材缺失时窗口只会画成默认底色，与本段落想说明的东西相反，
        // 所以直接把按钮置灰并给出提示。
        if (GetDialogBgBitmap() == NULL)
        {
            popButton->SetEnabled(false);
        }
        row->AddChild(std::move(popButton), DuiLayout::Hint().Fixed(kPopButtonWidth));

        if (GetDialogBgBitmap() == NULL)
        {
            row->AddChild(MakeBgMissingLabel(), DuiLayout::Hint().Weight(1));
        }
        else
        {
            row->AddChild(MakeHintLabel(
                              Txt(_T("（标题栏设成透明、高度与目标内距相同，背景图顶部的渐变带直接充当标题栏。）"),
                                  _T("(The title bar is transparent and as tall as the destination inset, ")
                                  _T("so the gradient band doubles as the title bar.)")),
                              kHintTextColor),
                          DuiLayout::Hint().Weight(1));
        }

        AddVariantRow(page.get(), std::move(row));
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 本分组的页面列表
// =====================================================================

const PageEntry* GetWindowPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("scroll-view"),  _T("DuiScrollView　滚动视图"),      _T("DuiScrollView"),   &Build_ScrollView,  true },
        { _T("frame-window"), _T("DuiFrameWindow　框架窗口"),     _T("DuiFrameWindow"),  &Build_FrameWindow, true },
        { _T("nine-patch"),   _T("九宫格背景　DuiNinePatch"),     _T("Nine-patch"),      &Build_NinePatch,   true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
