/**
 *  画廊「工具」分组的演示页面：控件树查看器（DuiInspector）、XML 自定义
 *  标签（DuiXmlBuilder::CustomFactory）、键盘可达性（DuiMnemonic 与
 *  DuiFocus）、拖放接收（DuiDropTarget）、资源与皮肤（DuiResMgr、
 *  CSkinManager、CImageEx）。
 *
 *  这五个页面演示的都不是某一个控件长什么样，而是整套界面之外的横切能力，
 *  因此放在同一个文件里。它们各自需要的演示控件互不相同，所以每个页面
 *  各开一段匿名命名空间放自己的私有类与状态，不共用。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "DuiHost.h"
#include "DuiInspector.h"
#include "DuiXmlBuilder.h"
#include "DuiMnemonic.h"
#include "DuiFocusVisual.h"
#include "DuiDropTarget.h"
#include "DuiResMgr.h"
#include "DuiNinePatch.h"
#include "SkinManager.h"
#include "ImageEx.h"

#include <vector>
#include <cstdio>
#include <cstdlib>

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 本文件公用的常量与小工具
// =====================================================================

// ---- 文字颜色 -------------------------------------------------------

// 普通说明性文字的颜色。比段落说明深一点，因为它跟演示控件同处一行，
// 需要与控件本身分得开。
const COLORREF kNoteTextColor = RGB(90, 96, 108);
// 代码片段与 XML 源码的文字颜色。
const COLORREF kCodeTextColor = RGB(58, 70, 92);
// 运行期算出来的结果文字的颜色。用偏绿的深色与说明文字区分开。
const COLORREF kResultTextColor = RGB(24, 92, 62);
// 需要读者特别留意的提示文字颜色（例如某个接口有已知的坑）。
const COLORREF kWarnTextColor = RGB(176, 88, 20);

// ---- 演示区块的配色 -------------------------------------------------
// 这一组颜色只用于把相邻的演示区块区分开，不承担任何语义。

// 品牌蓝。
const COLORREF kDemoBlue = RGB(45, 108, 223);
// 绿色，表示「可以放」「通过」一类的肯定状态。
const COLORREF kDemoGreen = RGB(60, 165, 92);
// 红色，表示「不能放」「拒收」一类的否定状态。
const COLORREF kDemoRed = RGB(214, 68, 68);
// 浅灰。空闲状态的投放区、未获得焦点的方块都用它。
const COLORREF kDemoLightGray = RGB(232, 235, 240);
// 深灰。用作深色底的对照格子。
const COLORREF kDemoDarkGray = RGB(58, 64, 76);
// 纯白。用作浅色底的对照格子。
const COLORREF kDemoWhite = RGB(255, 255, 255);
// 深色底上的文字色。
const COLORREF kOnDarkTextColor = RGB(245, 246, 250);
// 浅色底上的文字色。
const COLORREF kOnLightTextColor = RGB(28, 32, 40);
// 演示区块四周框线的颜色。与卡片描边同色系，比底色深一档。
const COLORREF kDemoBorderColor = RGB(150, 158, 172);

// ---- 常用尺寸（像素）-----------------------------------------------

// 只放一行文字的行高。
const int kTextRowH = 22;
// 放两三行文字的行高。
const int kNoteRowH = 44;
// 放五六行文字的行高。
const int kReasonRowH = 96;
// 放一段代码片段的行高。
const int kCodeRowH = 120;
// 放一块运行期输出的行高。
const int kOutputRowH = 76;
// 放一块较长的运行期输出的行高（皮肤初始化那种一次报四五行的）。
const int kLongOutputRowH = 152;
// 同一段落内两组演示行之间的间距。
const int kInnerGap = 6;
// 演示行内相邻两个控件之间的默认间距。
const int kRowGap = 12;
// 表格式演示行内相邻两列之间的间距。列多时取得比 kRowGap 紧一些。
const int kColumnGap = 8;
// 演示区块的圆角半径。
const int kDemoCornerRadius = 6;

// 造一个单行说明标签。
//   text：文字内容，允许为空指针（当作空串）。
//   color：文字颜色。
// 返回：标签，所有权交给调用方。
// 一律带 DT_NOPREFIX：本文件里有大量标签显示的是运行期拼出来的字符串
// （文件路径、助记符演示的输入串、XML 源码），里面出现 '&' 是正常的，
// 不关掉 GDI 的助记符解析就会被吃掉一个字符并给后一个字加下划线。
std::unique_ptr<DuiLabel> MakeLine(LPCTSTR text, COLORREF color)
{
    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(text != NULL ? text : _T(""));
    label->SetTextColor(color);
    label->SetTextAlign(DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return label;
}

// 造一个可以换到多行的说明 / 输出标签。
//   text：文字内容，允许为空指针。
//   color：文字颜色。
// 返回：标签，所有权交给调用方。
// 与 MakeLine 同样带 DT_NOPREFIX，理由见上。
std::unique_ptr<DuiLabel> MakeBlock(LPCTSTR text, COLORREF color)
{
    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(text != NULL ? text : _T(""));
    label->SetTextColor(color);
    label->SetWordWrap(true);
    label->SetTextAlign(DT_LEFT | DT_TOP | DT_NOPREFIX);
    return label;
}

// 把一个已经建好的标签放进当前段落的卡片，占满整行。
//   page：页面容器。
//   label：标签，所有权转移给卡片。
//   rowH：这一行的高度（像素）。
// 返回：这一行的容器指针，所有权归卡片。
DuiControl* AddLabelRow(GalleryPageBox* page,
                        std::unique_ptr<DuiLabel> label,
                        int rowH)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());
    row->AddChild(std::move(label), DuiLayout::Hint().Weight(1));
    return AddVariantRow(page, std::move(row), rowH);
}

// 取矩形的中心点。
//   rc：宿主客户区坐标下的矩形。
// 返回：中心点坐标。
// 先算边长再折半，而不是把两端相加再折半，是为了避免坐标很大时相加溢出。
POINT RectCenter(const RECT& rc)
{
    POINT pt;
    pt.x = rc.left + (rc.right - rc.left) / 2;
    pt.y = rc.top + (rc.bottom - rc.top) / 2;
    return pt;
}

// 让整个宿主窗口重画一遍。
//   pCtrl：任意一个已经挂到宿主上的控件，用来反查宿主窗口；可为空。
// 演示里有几处改动的是「整棵控件树怎么画」而不是某一个控件的外观
// （典型是控件树查看器的红框），只重画单个控件的矩形是不够的。
void InvalidateWholeHost(DuiControl* pCtrl)
{
    if (pCtrl == NULL)
    {
        return;
    }
    DuiHost* pHost = pCtrl->GetHost();
    if (pHost == NULL || !pHost->IsWindow())
    {
        return;
    }
    ::InvalidateRect(pHost->m_hWnd, NULL, FALSE);
}

// 造一个复选框形态的按钮。
//   text：按钮上的文字。
//   checked：初始是否勾选。
// 返回：按钮，所有权交给调用方。调用方负责给它的 onClick 赋值。
std::unique_ptr<FnButton> MakeCheckButton(LPCTSTR text, bool checked)
{
    std::unique_ptr<FnButton> button(new FnButton());
    button->SetButtonType(DuiButton::StyleCheckbox);
    button->SetText(text != NULL ? text : _T(""));
    if (checked)
    {
        // 第二个参数是「要不要发通知」。这里只是把初始状态摆好，页面还没
        // 挂到宿主上，发通知没有意义。
        button->SetCheck(true, false);
    }
    return button;
}

} // 匿名命名空间

// =====================================================================
// 控件树查看器（DuiInspector）
// =====================================================================

namespace {

// ---- 版面尺寸（像素）-----------------------------------------------

// 覆盖层控件在版面里占的高度。它自己不画任何东西、只是借一次绘制时机，
// 所以给一个不影响排版的最小高度。
const int kOverlayRowH = 1;
// 示例控件树那一行的高度。
const int kSampleTreeRowH = 96;
// 示例控件树里每个按钮的宽度。
const int kSampleButtonW = 104;
// 示例控件树里嵌在竖直容器内那个按钮的高度。
const int kSampleInnerButtonH = 44;
// 开关按钮的宽度。
const int kToggleButtonW = 176;
// 两个开关按钮之间的间距。比一般演示行宽一点，两个开关才不至于粘在一起。
const int kToggleGap = 16;
// 「统计矩形个数」按钮的宽度。
const int kCollectButtonW = 168;

// 示例控件树各节点的控件编号起点。控件树查看器把编号显示在信息条上，
// 编号各不相同才能看出信息条跟着鼠标换了对象。
const UINT kIdInspectorSampleFirst = 9100;

// 借一次绘制时机把控件树查看器的覆盖层画出来的控件。
//
// DuiInspector 自己是纯绘制的：它需要有人在控件树画完之后、把画面交给
// 屏幕之前调一次 PaintOverlay。库里没有这个调用点（DuiInspector.h 的
// 注释说 DuiHost::OnPaint 会调，但实际的 DuiHost::OnPaint 里没有这一句），
// 所以由使用方自己补。
//
// 本类不画自己的任何内容，只在自己的 OnPaint 里转调 PaintOverlay。把它
// 摆成页面根容器的最后一个子控件，就能借到「前面的控件都画完了」这个
// 时机 —— 控件树是按声明顺序绘制的。
//
// 除了 OnPaint，还有两个覆写是必需的，缺一个都不成：
//
//   · Layout 要把自己的矩形撑到整个宿主客户区。容器绘制子控件之前会先判断
//     子控件的矩形与本次脏区有没有交集，没有交集就跳过；本控件在版面里只
//     占一像素高，页面比视口高时那一条更是落在视口之外，照原样根本轮不到
//     它绘制。
//   · HitTest 要一律返回空指针。矩形撑大之后它会盖住整个客户区，而命中
//     测试是按子控件<u>倒序</u>找的，最后一个子控件优先命中 —— 不退出命中
//     测试的话，页面上其它控件一个都点不到。
class InspectorOverlay : public DuiControl
{
public:
    InspectorOverlay()
    {
        // 覆盖层不是可操作的内容，Tab 键遍历时应当跳过。
        SetTabStop(false);
    }

    // 把自己的矩形撑到整个宿主客户区，理由见类注释。
    //   rcAvail：父容器分配给本控件的矩形。只有在还没挂到宿主上、拿不到
    //            客户区尺寸时才退回用它。
    void Layout(const RECT& rcAvail) override
    {
        DuiHost* pHost = GetHost();
        if (pHost != NULL && pHost->IsWindow())
        {
            RECT rcClient;
            ::GetClientRect(pHost->m_hWnd, &rcClient);
            m_rcItem = rcClient;
            return;
        }
        m_rcItem = rcAvail;
    }

    // 退出命中测试，理由见类注释。
    //   ptHostClient：命中点，本函数不使用。
    // 返回：始终是空指针，表示本控件不接收任何鼠标事件。
    DuiControl* HitTest(POINT /*ptHostClient*/) override
    {
        return NULL;
    }

    // 绘制：把控件树查看器的覆盖层叠在已经画好的画面上。
    //   hdc：目标设备上下文。PaintOverlay 内部自己保存与恢复画笔状态。
    //   rcDirty：本次需要重绘的区域。覆盖层画的是整棵树，不受本参数限制，
    //            因此这里不做相交判断。
    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        DuiInspector::Inst().PaintOverlay(GetHost(), hdc);
    }
};

// 控件树查看器页面在运行期需要回头改动的那几个控件。
//
// 页面构建函数把指针填进来，通知处理函数与按钮回调据此更新界面。页面被
// 销毁时这些指针全部失效，所以每次构建页面的第一件事就是把整块清空。
struct InspectorPageState
{
    // 显示查看器与「跟随鼠标刷新」两个开关当前状态的标签。
    DuiLabel* statusLabel;
    // 显示当前悬停控件信息串的标签。
    DuiLabel* hoverLabel;
    // 显示 CollectVisibleRects 统计结果的标签。
    DuiLabel* collectLabel;
    // 示例控件树的根容器。统计按钮以它为起点遍历。
    DuiControl* sampleRoot;
    // 「跟随鼠标刷新」是否打开。打开时每次悬停目标变化都让整个宿主窗口
    // 重画一遍，绿框才会跟着鼠标走。
    bool followMouse;
};

InspectorPageState g_inspector;

// 数一数以 pRoot 为根的子树里有多少个控件（含 pRoot 自己）。
//   pRoot：子树的根，可为空（返回 0）。
// 返回：控件个数。
int CountControls(const DuiControl* pRoot)
{
    if (pRoot == NULL)
    {
        return 0;
    }
    int total = 1;
    const std::vector<std::unique_ptr<DuiControl> >& children = pRoot->Children();
    for (size_t i = 0; i < children.size(); ++i)
    {
        total += CountControls(children[i].get());
    }
    return total;
}

// 按当前开关状态刷新状态标签。
void RefreshInspectorStatus()
{
    if (g_inspector.statusLabel == NULL)
    {
        return;
    }
    CString text;
    text.Format(_T("%s%s    %s%s"),
                Txt(_T("查看器："), _T("Inspector: ")),
                DuiInspector::Inst().IsEnabled()
                    ? Txt(_T("已开启"), _T("on"))
                    : Txt(_T("已关闭"), _T("off")),
                Txt(_T("跟随鼠标刷新："), _T("Follow mouse: ")),
                g_inspector.followMouse
                    ? Txt(_T("已开启"), _T("on"))
                    : Txt(_T("已关闭"), _T("off")));
    g_inspector.statusLabel->SetText(text);
}

// 「启用控件树查看器」开关的响应。
//   pButton：被点击的按钮。基类在调到这里之前已经把勾选状态切换好了，
//            所以直接读它的勾选状态即可。
void OnInspectorEnableClicked(FnButton* pButton)
{
    if (pButton == NULL)
    {
        return;
    }
    DuiInspector::Inst().Enable(pButton->IsChecked());
    RefreshInspectorStatus();
    InvalidateWholeHost(pButton);
}

// 「跟随鼠标刷新」开关的响应。
//   pButton：被点击的按钮。
void OnInspectorFollowClicked(FnButton* pButton)
{
    if (pButton == NULL)
    {
        return;
    }
    g_inspector.followMouse = pButton->IsChecked();
    RefreshInspectorStatus();
}

// 「统计矩形个数」按钮的响应。把示例控件树的实际控件个数与
// CollectVisibleRects 返回的矩形个数并排显示出来，让两者对不上的事实
// 直接呈现在页面上。
//   pButton：被点击的按钮，本函数不使用。
void OnInspectorCollectClicked(FnButton* /*pButton*/)
{
    if (g_inspector.collectLabel == NULL || g_inspector.sampleRoot == NULL)
    {
        return;
    }
    std::vector<RECT> rects;
    DuiInspector::CollectVisibleRects(g_inspector.sampleRoot, rects);

    CString text;
    text.Format(Txt(_T("示例控件树共 %d 个控件（含根容器），")
                    _T("CollectVisibleRects 返回 %d 个矩形。"),
                    _T("The sample tree holds %d controls (root included); ")
                    _T("CollectVisibleRects returned %d rect(s).")),
                CountControls(g_inspector.sampleRoot),
                (int)rects.size());
    g_inspector.collectLabel->SetText(text);
}

// 控件树查看器页面的通知处理。
//   pNotify：宿主转发过来的通知，可为空。
// 鼠标从一个控件移到另一个控件时，宿主会让旧控件收到「鼠标离开」、新控件
// 收到「鼠标进入」，这两条通知就是「悬停目标变了」的信号。它们由
// DuiControl 基类发出，页面里每个控件都会发，正是本处需要的范围，因此
// 这里只按通知码判断，不限定控件编号。
void OnInspectorPageNotify(const DuiNotify* pNotify)
{
    if (pNotify == NULL || g_inspector.hoverLabel == NULL)
    {
        return;
    }
    if (pNotify->code != (UINT)DUIN_MOUSEENTER
        && pNotify->code != (UINT)DUIN_MOUSELEAVE)
    {
        return;
    }

    DuiHost* pHost = g_inspector.hoverLabel->GetHost();
    if (pHost == NULL)
    {
        return;
    }

    DuiControl* pHover = pHost->GetDuiHover();
    CString text;
    if (pHover != NULL)
    {
        text = DuiInspector::FormatControlInfo(pHover);
    }
    else
    {
        text = Txt(_T("（鼠标不在任何控件上）"), _T("(cursor is not over any control)"));
    }
    g_inspector.hoverLabel->SetText(text);

    // 绿色的悬停框画在上一次的位置上，只重画信息标签是不够的，必须让整个
    // 宿主窗口重画一遍。这件事开销不小，所以做成开关，默认关着。
    if (g_inspector.followMouse && DuiInspector::Inst().IsEnabled())
    {
        ::InvalidateRect(pHost->m_hWnd, NULL, FALSE);
    }
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_Inspector()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 上一次进这个页面时留下的指针已经随旧页面一起失效，先整块清空。
    g_inspector.statusLabel = NULL;
    g_inspector.hoverLabel = NULL;
    g_inspector.collectLabel = NULL;
    g_inspector.sampleRoot = NULL;
    g_inspector.followMouse = false;
    // 离开页面时没有回调可以用来关掉查看器，所以每次进来都把它复位，
    // 免得上一次开着的状态影响别的页面。
    DuiInspector::Inst().Enable(false);
    g_pageNotifyHook = &OnInspectorPageNotify;

    // ---- 段落一：它是什么 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("DuiInspector 是什么"), _T("What DuiInspector is")),
               Txt(_T("一个调试用的覆盖层。开启之后，宿主控件树里每一个可见控件都会被画上 ")
                   _T("1 像素红框，鼠标当前悬停的那一个再叠一个 2 像素绿框，并在它左上角画一条 ")
                   _T("黑底信息条，写明控件编号与矩形。它是进程内的单例，用 Enable(true / false) ")
                   _T("开关，控件本身不需要任何配合。"),
                   _T("A debug overlay. Once enabled it outlines every visible control in the ")
                   _T("host tree with a 1px red frame, adds a 2px green frame around the control ")
                   _T("under the cursor, and draws a dark info pill at its top-left showing the ")
                   _T("control id and rect. It is a process-wide singleton toggled with ")
                   _T("Enable(true / false); controls need no cooperation at all.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kToggleGap);

        std::unique_ptr<FnButton> enableButton =
            MakeCheckButton(Txt(_T("启用控件树查看器"), _T("Enable inspector")), false);
        enableButton->onClick = &OnInspectorEnableClicked;

        std::unique_ptr<FnButton> followButton =
            MakeCheckButton(Txt(_T("跟随鼠标刷新"), _T("Repaint on hover")), false);
        followButton->onClick = &OnInspectorFollowClicked;

        row->AddChild(std::move(enableButton), DuiLayout::Hint().Fixed(kToggleButtonW));
        row->AddChild(std::move(followButton), DuiLayout::Hint().Fixed(kToggleButtonW));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> status =
            MakeLine(_T(""), kResultTextColor);
        g_inspector.statusLabel = status.get();
        AddLabelRow(page.get(), std::move(status), kTextRowH);
    }
    RefreshInspectorStatus();

    // ---- 段落二：库里没有调用点 --------------------------------------
    AddSection(page.get(),
               Txt(_T("必须自己补一次 PaintOverlay 调用"),
                   _T("You must call PaintOverlay yourself")),
               Txt(_T("DuiInspector.h 的注释写着「DuiHost::OnPaint 在控件树画完之后会调 ")
                   _T("PaintOverlay」，但实际的 DuiHost::OnPaint 里没有这一句 —— 整个仓库里 ")
                   _T("PaintOverlay 没有任何生产调用点。所以光调 Enable(true) 屏幕上不会有任何 ")
                   _T("变化。本页面的做法是在画廊这一侧写一个只转调 PaintOverlay 的覆盖层控件，")
                   _T("把它挂成页面根容器的最后一个子控件：控件树按声明顺序绘制，轮到最后一个 ")
                   _T("子控件时前面的都已经画好了，覆盖层自然就压在最上面。"),
                   _T("The comment in DuiInspector.h claims DuiHost::OnPaint calls PaintOverlay ")
                   _T("after painting the tree, but the real DuiHost::OnPaint contains no such ")
                   _T("line — PaintOverlay has no production call site anywhere in the repo. ")
                   _T("Enable(true) alone therefore changes nothing on screen. This page adds a ")
                   _T("gallery-side overlay control that does nothing but forward to ")
                   _T("PaintOverlay, installed as the last child of the page root: controls ")
                   _T("paint in declaration order, so by the time the last child paints, ")
                   _T("everything else is already on the canvas.")));
    {
        std::unique_ptr<DuiLabel> code = MakeBlock(
            _T("class InspectorOverlay : public DuiControl\n")
            _T("{\n")
            _T("public:\n")
            _T("    void OnPaint(HDC hdc, const RECT&) override\n")
            _T("    {\n")
            _T("        DuiInspector::Inst().PaintOverlay(GetHost(), hdc);\n")
            _T("    }\n")
            _T("    void Layout(const RECT&) override;        // 撑满整个客户区\n")
            _T("    DuiControl* HitTest(POINT) override;      // 一律返回空指针\n")
            _T("};\n")
            _T("page->AddChild(std::unique_ptr<DuiControl>(new InspectorOverlay()),\n")
            _T("               DuiLayout::Hint().Fixed(1));"),
            kCodeTextColor);
        AddLabelRow(page.get(), std::move(code), kCodeRowH);
    }
    {
        std::unique_ptr<DuiLabel> note = MakeBlock(
            Txt(_T("这个覆盖层控件除了 OnPaint 还必须覆写另外两个方法。一是 Layout：容器绘制 ")
                _T("子控件之前会先判断它的矩形与脏区有没有交集，覆盖层在版面里只占一像素高，")
                _T("页面比视口高时那一条还落在视口之外，不把矩形撑到整个客户区就根本轮不到它 ")
                _T("绘制。二是 HitTest：矩形撑大之后它会盖住整个客户区，而命中测试是按子控件 ")
                _T("倒序找的、最后一个优先命中，不退出命中测试的话页面上别的控件一个都点不到。\n")
                _T("还有一处限制：本页面在滚动视图内部，而 DuiScrollView 绘制内容时会把画笔 ")
                _T("裁剪到自己的可视区域，所以红框只出现在滚动视图这块区域里，左侧导航树与 ")
                _T("上方标题栏上不会有。真正接入业务时把覆盖层挂在宿主的根控件下就没有这个限制。"),
                _T("This overlay control has to override two more methods besides OnPaint. ")
                _T("Layout: a container checks whether a child's rect intersects the dirty ")
                _T("region before painting it, and the overlay only occupies one pixel of the ")
                _T("layout — on a page taller than the viewport that sliver sits off-screen ")
                _T("entirely, so without stretching the rect to the whole client area it never ")
                _T("paints at all. HitTest: once the rect is stretched it covers everything, and ")
                _T("hit testing walks children in reverse with the last one winning, so without ")
                _T("opting out no other control on the page could be clicked.\n")
                _T("One more limitation: this page lives inside a scroll view, and ")
                _T("DuiScrollView clips painting to its viewport, so the red frames only appear ")
                _T("inside that viewport — the navigation tree and the header stay unmarked. ")
                _T("Installing the overlay directly under the host root removes the ")
                _T("limitation.")),
            kWarnTextColor);
        AddLabelRow(page.get(), std::move(note), kCodeRowH);
    }

    // ---- 段落三：示例控件树 ------------------------------------------
    AddSection(page.get(),
               Txt(_T("一小块专门用来观察的控件树"),
                   _T("A small tree to watch")),
               Txt(_T("下面这块由「水平容器 + 竖直容器 + 按钮 + 标签」嵌套而成，每个节点都给了 ")
                   _T("互不相同的控件编号。开启查看器之后把鼠标依次移过去，绿框与信息条会跟着换 ")
                   _T("对象，信息条上的编号就是这里给的那些。"),
                   _T("The block below nests a horizontal box, a vertical box, buttons and ")
                   _T("labels, each carrying a distinct control id. Enable the inspector and move ")
                   _T("the cursor across them: the green frame and the info pill follow, and the ")
                   _T("id in the pill is the one assigned here.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);
        row->SetCtrlId(kIdInspectorSampleFirst);
        g_inspector.sampleRoot = row.get();

        std::unique_ptr<DuiVBox> column(new DuiVBox());
        column->SetGap(kInnerGap);
        column->SetCtrlId(kIdInspectorSampleFirst + 1);

        std::unique_ptr<DuiLabel> caption =
            MakeLine(Txt(_T("嵌套的竖直容器"), _T("nested vertical box")), kNoteTextColor);
        caption->SetCtrlId(kIdInspectorSampleFirst + 2);
        column->AddChild(std::move(caption), DuiLayout::Hint().Fixed(kTextRowH));

        std::unique_ptr<DuiButton> inner(new DuiButton());
        inner->SetText(Txt(_T("容器里的按钮"), _T("inner button")));
        inner->SetCtrlId(kIdInspectorSampleFirst + 3);
        column->AddChild(std::move(inner), DuiLayout::Hint().Fixed(kSampleInnerButtonH));

        std::unique_ptr<DuiButton> outerA(new DuiButton());
        outerA->SetText(Txt(_T("同层按钮 A"), _T("sibling A")));
        outerA->SetCtrlId(kIdInspectorSampleFirst + 4);

        std::unique_ptr<DuiButton> outerB(new DuiButton());
        outerB->SetButtonType(DuiButton::StyleCheckbox);
        outerB->SetText(Txt(_T("同层复选框 B"), _T("sibling B")));
        outerB->SetCtrlId(kIdInspectorSampleFirst + 5);

        row->AddChild(std::move(column), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(outerA), DuiLayout::Hint().Fixed(kSampleButtonW));
        row->AddChild(std::move(outerB), DuiLayout::Hint().Fixed(kSampleButtonW));
        AddVariantRow(page.get(), std::move(row), kSampleTreeRowH);
    }

    // ---- 段落四：信息串 ----------------------------------------------
    AddSection(page.get(),
               Txt(_T("当前悬停控件的信息串"), _T("Info string for the hovered control")),
               Txt(_T("信息条上那行字由 FormatControlInfo 拼出来，格式是「类名 id=编号 ")
                   _T("rect=(左,上,右,下)」。有一点要如实说明：这里的类名是写死的字符串 ")
                   _T("\"DuiControl\"，不是控件的真实类名 —— 库里没有运行期类型名可用，")
                   _T("函数直接把这四个字拼了进去。所以按钮、标签、容器显示出来的类名是一样的，")
                   _T("要区分只能靠编号。下面这行会随鼠标实时更新。"),
                   _T("The line in the pill comes from FormatControlInfo, formatted as ")
                   _T("\"class id=<id> rect=(l,t,r,b)\". One caveat worth stating plainly: the ")
                   _T("class name is the hard-coded string \"DuiControl\", not the real class ")
                   _T("name — the library has no runtime type name available, so the function ")
                   _T("simply writes those characters in. Buttons, labels and containers all ")
                   _T("report the same class name; only the id tells them apart. The line below ")
                   _T("updates as you move the cursor.")));
    {
        std::unique_ptr<DuiLabel> hover =
            MakeLine(Txt(_T("（把鼠标移到任意控件上）"), _T("(move the cursor over any control)")),
                     kResultTextColor);
        g_inspector.hoverLabel = hover.get();
        AddLabelRow(page.get(), std::move(hover), kTextRowH);
    }

    // ---- 段落五：CollectVisibleRects 的限制 ---------------------------
    AddSection(page.get(),
               Txt(_T("CollectVisibleRects 只返回根控件自己的矩形"),
                   _T("CollectVisibleRects only returns the root's own rect")),
               Txt(_T("这个静态函数的名字听起来会把整棵子树的矩形都收集出来，实际不会 —— ")
                   _T("它只把传进去的那一个控件的矩形放进结果，不往子控件递归。原因写在 ")
                   _T("DuiInspector.cpp 的注释里：子控件列表是受保护成员，非友元拿不到，")
                   _T("而覆盖层绘制那条路径另有一个友元遍历类可用，所以这个对外的纯函数就 ")
                   _T("只保留了根控件那一份，够单元测试用。点下面的按钮看实测数字。"),
                   _T("Despite the name, this static helper does not walk the subtree: it pushes ")
                   _T("only the rect of the control you pass in and never recurses. The reason is ")
                   _T("in the comment inside DuiInspector.cpp — the child list is a protected ")
                   _T("member that a non-friend cannot reach, while the overlay paint path has a ")
                   _T("dedicated friend walker, so this public pure helper keeps just the root ")
                   _T("rect, which is what the unit tests need. Press the button for the real ")
                   _T("numbers.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> collectButton(new FnButton());
        collectButton->SetText(Txt(_T("统计矩形个数"), _T("Count rects")));
        collectButton->onClick = &OnInspectorCollectClicked;

        row->AddChild(std::move(collectButton), DuiLayout::Hint().Fixed(kCollectButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> result =
            MakeBlock(Txt(_T("（还没有统计过）"), _T("(not measured yet)")), kResultTextColor);
        g_inspector.collectLabel = result.get();
        AddLabelRow(page.get(), std::move(result), kTextRowH);
    }

    // ---- 覆盖层：必须是页面根容器的最后一个子控件 ---------------------
    page->AddChild(std::unique_ptr<DuiControl>(new InspectorOverlay()),
                   DuiLayout::Hint().Fixed(kOverlayRowH));

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// XML 自定义标签（DuiXmlBuilder::CustomFactory）
// =====================================================================

namespace {

// ---- 版面尺寸（像素）-----------------------------------------------

// 现场解析出来的控件树那一格的高度。
const int kXmlResultRowH = 64;
// 自绘容器演示那一格的高度。里面竖着摞三个子节点，要留够。
const int kXmlStackRowH = 108;
// 混排演示那一格的高度。
const int kXmlMixedRowH = 96;
// 显示 XML 源码的标签高度。
const int kXmlSourceRowH = 80;
// 「重新解析」按钮的宽度。文案较长，取得比一般按钮宽。
const int kXmlButtonW = 340;

// ---- 演示控件的默认外观 ---------------------------------------------

// 自定义色块没写 bg-color 属性时用的底色。
const COLORREF kTileDefaultBg = kDemoBlue;
// 自定义色块没写 radius 属性时用的圆角半径（像素）。
const int kTileDefaultRadius = kDemoCornerRadius;
// 自绘容器排子节点时相邻两个之间的间距（像素）。
const int kStackGap = kInnerGap;

// 供 XML 自定义标签演示使用的色块控件，对应标签 <demo-tile/>。
//
// 它是一个纯粹的叶子节点：填一块圆角底色、在正中画一行白字，不处理任何
// 输入事件。底色、圆角、文字全部由 XML 属性给出，用来演示工厂函数怎么把
// 属性读出来落到控件上。
//
// 绘制底色复用 DuiVBox::PaintBackground 这个静态方法，它内部走抗锯齿的
// 圆角矩形填充，不必自己再写一份。
class XmlTile : public DuiControl
{
public:
    XmlTile()
        : m_bgColor(kTileDefaultBg)
        , m_cornerRadius(kTileDefaultRadius)
    {
        SetTabStop(false);
    }

    // 设置色块上显示的文字。
    //   text：文字内容，允许为空指针（当作空串）。本类内部复制一份。
    void SetTileText(LPCTSTR text)
    {
        m_text = (text != NULL) ? text : _T("");
    }

    // 设置色块的底色。
    //   color：底色。
    void SetTileBgColor(COLORREF color)
    {
        m_bgColor = color;
    }

    // 设置色块的圆角半径。
    //   radius：半径（像素），负数按 0 处理。
    void SetTileRadius(int radius)
    {
        m_cornerRadius = (radius > 0) ? radius : 0;
    }

    // 绘制色块。
    //   hdc：目标设备上下文。本函数改动的背景模式与文字色在返回前会还原。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }
        DuiVBox::PaintBackground(hdc, m_rcItem, m_bgColor, m_cornerRadius);
        if (m_text.IsEmpty())
        {
            return;
        }
        int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldTextColor = ::SetTextColor(hdc, kOnDarkTextColor);
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
        ::DrawText(hdc, m_text, -1, &m_rcItem,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont != NULL)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::SetTextColor(hdc, oldTextColor);
        ::SetBkMode(hdc, oldBkMode);
    }

private:
    // 画在色块正中的文字。构造后由工厂函数按 XML 属性填入。
    CString m_text;
    // 色块底色。
    COLORREF m_bgColor;
    // 圆角半径（像素）。
    int m_cornerRadius;
};

// 供 XML 自定义标签演示使用的自绘容器，对应标签 <demo-stack/>。
//
// 它演示的是「工厂返回非空控件之后，builder 会自动把每个 XML 子节点建出来
// 并挂到返回值上」这条契约：本类的工厂函数里一行子节点解析代码都没有，
// 子节点却照样进来了，本类只需要覆写 Layout 决定它们摆在哪里。
//
// 排法是把可用高度在可见子节点之间均分，并在自己四周画一圈框线标明范围。
class XmlStack : public DuiControl
{
public:
    XmlStack()
    {
        SetTabStop(false);
    }

    // 把子节点竖着均分排开。
    //   rcAvail：父容器分配给本控件的矩形（宿主客户区坐标）。
    // 基类的 Layout 不会写 m_rcItem（写 m_rcItem 是 SetRect 的事），但本函数
    // 也可能被直接调用，所以这里自己写一次，保证两种调用方式行为一致。
    void Layout(const RECT& rcAvail) override
    {
        m_rcItem = rcAvail;

        int visibleCount = 0;
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            if (m_children[i]->IsVisible())
            {
                ++visibleCount;
            }
        }
        if (visibleCount <= 0)
        {
            return;
        }

        int totalHeight = rcAvail.bottom - rcAvail.top;
        int gapTotal = kStackGap * (visibleCount - 1);
        int cellHeight = (totalHeight - gapTotal) / visibleCount;
        if (cellHeight < 0)
        {
            cellHeight = 0;
        }

        int y = rcAvail.top;
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            if (!m_children[i]->IsVisible())
            {
                continue;
            }
            RECT cell;
            cell.left = rcAvail.left;
            cell.top = y;
            cell.right = rcAvail.right;
            cell.bottom = y + cellHeight;
            m_children[i]->SetRect(cell);
            y = cell.bottom + kStackGap;
        }
    }

    // 先画一圈框线标出自己的范围，再让基类把子节点画出来。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        HBRUSH hBrush = ::CreateSolidBrush(kDemoBorderColor);
        ::FrameRect(hdc, &m_rcItem, hBrush);
        ::DeleteObject(hBrush);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// ---- XML 属性读取的小工具 -------------------------------------------
//
// 属性值在 Node 里一律是 UTF-8 字节串（哪怕 XML 源是宽字符，解析器内部也会
// 先转成 UTF-8）。要给 CString 就必须显式按 UTF-8 转码，直接赋值会把中文
// 当成本地代码页解读、变成乱码。

// 在节点的属性表里找一个属性。
//   node：XML 节点。
//   key：属性名。
// 返回：属性值的地址；没有这个属性时返回空指针。生命期与 node 相同。
const std::string* FindAttr(const DuiXmlBuilder::Node& node, const char* key)
{
    std::map<std::string, std::string>::const_iterator it = node.attrs.find(key);
    return (it != node.attrs.end()) ? &it->second : NULL;
}

// 读一个字符串属性。
//   node：XML 节点。
//   key：属性名。
//   defaultValue：属性不存在时返回的值。
// 返回：转成宽字符之后的属性值。
CString GetAttrString(const DuiXmlBuilder::Node& node, const char* key,
                      LPCTSTR defaultValue)
{
    const std::string* pValue = FindAttr(node, key);
    if (pValue == NULL)
    {
        return CString(defaultValue);
    }
    return CString(CA2W(pValue->c_str(), CP_UTF8));
}

// 读一个整数属性。
//   node：XML 节点。
//   key：属性名。
//   defaultValue：属性不存在时返回的值。
// 返回：属性值转成的整数；内容不是数字时按 atoi 的规则返回 0。
int GetAttrInt(const DuiXmlBuilder::Node& node, const char* key, int defaultValue)
{
    const std::string* pValue = FindAttr(node, key);
    if (pValue == NULL)
    {
        return defaultValue;
    }
    return atoi(pValue->c_str());
}

// 读一个「r,g,b」形式的颜色属性。
//   node：XML 节点。
//   key：属性名。
//   defaultValue：属性不存在或格式不对时返回的值。
// 返回：颜色值。
COLORREF GetAttrColor(const DuiXmlBuilder::Node& node, const char* key,
                      COLORREF defaultValue)
{
    const std::string* pValue = FindAttr(node, key);
    if (pValue == NULL)
    {
        return defaultValue;
    }
    int red = 0;
    int green = 0;
    int blue = 0;
    // 三个分量都读到才算数，读到一半的当作格式不对，退回默认值。
    const int kExpectedFieldCount = 3;
    if (sscanf(pValue->c_str(), "%d,%d,%d", &red, &green, &blue) != kExpectedFieldCount)
    {
        return defaultValue;
    }
    return RGB(red, green, blue);
}

// 本页面的自定义标签工厂。
//
// builder 遇到一个标签时先查内置标签表，不在表里才调这个函数。返回空指针
// 表示「我也不认识」，builder 会连同它的子节点一起跳过；返回非空则 builder
// 把它当成本节点的产物，并自动把每个子节点建出来挂上去。
//
// 通用属性（id / fixedWidth / fixedHeight / weight / margin）由 builder 在
// 父容器添加子控件时统一读取，本函数<u>不</u>解析它们，否则会与 builder 冲突。
//   node：待处理的 XML 节点。
// 返回：新建的控件，所有权交给 builder；不认识的标签返回空指针。
std::unique_ptr<DuiControl> DemoTagFactory(const DuiXmlBuilder::Node& node)
{
    if (node.tag == "demo-tile")
    {
        std::unique_ptr<XmlTile> tile(new XmlTile());
        tile->SetTileText(GetAttrString(node, "text", _T("")));
        tile->SetTileBgColor(GetAttrColor(node, "bg-color", kTileDefaultBg));
        tile->SetTileRadius(GetAttrInt(node, "radius", kTileDefaultRadius));
        return std::unique_ptr<DuiControl>(tile.release());
    }
    if (node.tag == "demo-stack")
    {
        // 这里没有任何解析子节点的代码 —— 子节点由 builder 自动建出来并
        // 挂到返回值上，本类只负责在 Layout 里决定它们摆在哪里。
        return std::unique_ptr<DuiControl>(new XmlStack());
    }
    return std::unique_ptr<DuiControl>();
}

// 一律返回空指针的工厂，用来演示「工厂不认识这个标签」时 builder 的行为。
//   node：待处理的 XML 节点，本函数不使用。
// 返回：始终是空指针。
std::unique_ptr<DuiControl> AlwaysNullFactory(const DuiXmlBuilder::Node& /*node*/)
{
    return std::unique_ptr<DuiControl>();
}

// XML 源码的一份中英文对照。
//
// 源码里的 text 属性是要显示给读者看的，所以两种语言各写一份；结构与其余
// 属性完全相同，切换语言时看到的差别只在文案上。
struct XmlVariant
{
    // 中文文案版本的 XML 源码，UTF-8 字节串。
    LPCSTR zh;
    // 英文文案版本的 XML 源码，UTF-8 字节串。
    LPCSTR en;
};

// 叶子标签演示用的三份 XML。点「重新解析」在它们之间轮换，属性一变，
// 现场建出来的控件外观立刻跟着变。
const XmlVariant kLeafVariants[] = {
    {
        "<hbox gap=\"12\">"
        "  <demo-tile text=\"品牌蓝 圆角 6\" bg-color=\"45,108,223\" radius=\"6\" weight=\"1\"/>"
        "  <demo-tile text=\"绿色 圆角 6\" bg-color=\"60,165,92\" radius=\"6\" weight=\"1\"/>"
        "  <demo-tile text=\"固定 140 宽\" bg-color=\"176,88,20\" radius=\"6\" fixedWidth=\"140\"/>"
        "</hbox>",

        "<hbox gap=\"12\">"
        "  <demo-tile text=\"brand blue r=6\" bg-color=\"45,108,223\" radius=\"6\" weight=\"1\"/>"
        "  <demo-tile text=\"green r=6\" bg-color=\"60,165,92\" radius=\"6\" weight=\"1\"/>"
        "  <demo-tile text=\"fixed 140px\" bg-color=\"176,88,20\" radius=\"6\" fixedWidth=\"140\"/>"
        "</hbox>"
    },
    {
        "<hbox gap=\"12\">"
        "  <demo-tile text=\"直角\" bg-color=\"58,64,76\" radius=\"0\" weight=\"1\"/>"
        "  <demo-tile text=\"大圆角 18\" bg-color=\"120,90,180\" radius=\"18\" weight=\"1\"/>"
        "  <demo-tile text=\"权重 2\" bg-color=\"214,68,68\" radius=\"18\" weight=\"2\"/>"
        "</hbox>",

        "<hbox gap=\"12\">"
        "  <demo-tile text=\"square\" bg-color=\"58,64,76\" radius=\"0\" weight=\"1\"/>"
        "  <demo-tile text=\"radius 18\" bg-color=\"120,90,180\" radius=\"18\" weight=\"1\"/>"
        "  <demo-tile text=\"weight 2\" bg-color=\"214,68,68\" radius=\"18\" weight=\"2\"/>"
        "</hbox>"
    },
    {
        "<hbox gap=\"12\">"
        "  <demo-tile text=\"没写 bg-color，用默认值\" radius=\"10\" weight=\"1\"/>"
        "  <demo-tile text=\"颜色写坏了\" bg-color=\"not-a-color\" radius=\"10\" weight=\"1\"/>"
        "</hbox>",

        "<hbox gap=\"12\">"
        "  <demo-tile text=\"bg-color omitted, default used\" radius=\"10\" weight=\"1\"/>"
        "  <demo-tile text=\"bg-color is malformed\" bg-color=\"not-a-color\" radius=\"10\" weight=\"1\"/>"
        "</hbox>"
    },
};

// 叶子标签演示用的 XML 份数。
const int kLeafVariantCount = (int)(sizeof(kLeafVariants) / sizeof(kLeafVariants[0]));

// XML 页面在运行期需要回头改动的那几个控件。页面被销毁后这些指针全部失效，
// 所以每次构建页面的第一件事就是把整块清空。
struct XmlPageState
{
    // 现场解析出来的控件树挂在这个格子里。
    DuiVBox* leafHost;
    // 显示当前这份 XML 源码的标签。
    DuiLabel* leafSource;
    // 当前用的是第几份 XML，取值 0 到 kLeafVariantCount - 1。
    int variantIndex;
};

XmlPageState g_xml;

// 按当前语言取一份 XML 源码。
//   variant：一份中英文对照的 XML。
// 返回：当前语言对应的那一份，UTF-8 字节串。
// Txt() 只处理宽字符串，而 DuiXmlBuilder 的入口要求 UTF-8 窄字符串，所以
// 这里另写一个同语义的窄字符串版本。
LPCSTR PickXml(const XmlVariant& variant)
{
    return (CurrentLanguage() == LangEnglish) ? variant.en : variant.zh;
}

// 把一个格子里原有的内容全部换成新的一棵控件树。
//   pHost：承载控件树的格子，可为空（直接返回）。
//   content：新的控件树，所有权转移给格子；可为空表示只清空。
// 换完之后必须显式重排一次：格子的矩形并没有变化，而 SetRect 在矩形没变时
// 直接返回，新子树就永远不会被定位。
void ReplaceHostedTree(DuiVBox* pHost, std::unique_ptr<DuiControl> content)
{
    if (pHost == NULL)
    {
        return;
    }
    while (!pHost->Children().empty())
    {
        pHost->RemoveChild(pHost->Children()[0].get());
    }
    if (content)
    {
        pHost->AddChild(std::move(content), DuiLayout::Hint().Weight(1));
    }
    pHost->ForceLayout(pHost->GetRect());
    pHost->Invalidate();
}

// 按 g_xml.variantIndex 重新解析一份 XML，并把结果装进格子里。
void RebuildLeafDemo()
{
    if (g_xml.leafHost == NULL || g_xml.leafSource == NULL)
    {
        return;
    }
    LPCSTR xml = PickXml(kLeafVariants[g_xml.variantIndex]);

    // 源码本身是 UTF-8 字节串，要显示给读者看就必须按 UTF-8 转成宽字符。
    CString source;
    source.Format(_T("%s%d / %d\n%s"),
                  Txt(_T("当前 XML（第 "), _T("Current XML (")),
                  g_xml.variantIndex + 1,
                  kLeafVariantCount,
                  (LPCTSTR)CString(CA2W(xml, CP_UTF8)));
    g_xml.leafSource->SetText(source);

    DuiXmlBuilder::CustomFactory factory = &DemoTagFactory;
    ReplaceHostedTree(g_xml.leafHost, DuiXmlBuilder::FromString(xml, factory));
}

// 「重新解析」按钮的响应：换下一份 XML 再解析一次。
//   pButton：被点击的按钮，本函数不使用。
void OnXmlReparseClicked(FnButton* /*pButton*/)
{
    g_xml.variantIndex = (g_xml.variantIndex + 1) % kLeafVariantCount;
    RebuildLeafDemo();
}

// 造一个承载控件树的格子。
//   返回：格子，所有权交给调用方。
// 给它一个浅色底与圆角，好把「XML 建出来的那部分」与页面卡片区分开。
std::unique_ptr<DuiVBox> MakeTreeHost()
{
    std::unique_ptr<DuiVBox> host(new DuiVBox());
    host->SetBgColor(kDemoLightGray);
    host->SetCornerRadius(kTileDefaultRadius);
    host->SetPadding(kStackGap);
    return host;
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_XmlCustomTag()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    g_xml.leafHost = NULL;
    g_xml.leafSource = NULL;
    g_xml.variantIndex = 0;

    // ---- 段落一：四条契约 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("自定义标签是怎么接进来的"), _T("How a custom tag gets in")),
               Txt(_T("DuiXmlBuilder 认识一批内置标签（vbox / hbox / grid / label / button / ")
                   _T("edit 等）。遇到不认识的标签时，它把决定权交给调用方通过 CustomFactory ")
                   _T("注册的那个函数 —— 这是把自绘控件接进 XML 的唯一入口。四条契约必须记住："),
                   _T("DuiXmlBuilder knows a fixed set of built-in tags (vbox / hbox / grid / ")
                   _T("label / button / edit and friends). For any other tag it hands the ")
                   _T("decision to the CustomFactory the caller registered — the only way to get ")
                   _T("a self-drawn control into XML. Four contract points to remember:")));
    {
        std::unique_ptr<DuiLabel> rules = MakeBlock(
            Txt(_T("一、工厂返回空指针时，builder 把这个节点连同它的子节点一起跳过，")
                _T("周围认识的标签照常建出来。\n")
                _T("二、工厂返回非空时，builder 会自动把每个子节点建出来并挂到返回值上，")
                _T("工厂里一行子节点解析代码都不用写。\n")
                _T("三、id / fixedWidth / fixedHeight / weight / margin 这五个通用属性由 ")
                _T("builder 在父容器添加子控件时统一处理，工厂不要自己解析，否则与 builder 冲突。\n")
                _T("四、属性值永远是 UTF-8 字节串，即便 XML 源本身是宽字符也一样。转成 ")
                _T("CString 必须写 CA2W(s.c_str(), CP_UTF8)，直接赋值会把中文按本地代码页 ")
                _T("解读、变成乱码。"),
                _T("1. When the factory returns null the builder skips that node together with ")
                _T("its children; recognized siblings are still built.\n")
                _T("2. When the factory returns a control the builder builds every child node ")
                _T("and AddChild()s it onto that control — the factory needs no child-parsing ")
                _T("code at all.\n")
                _T("3. The five common attributes id / fixedWidth / fixedHeight / weight / ")
                _T("margin are handled by the builder when the parent adds the child. Do not ")
                _T("parse them in the factory or the two will fight.\n")
                _T("4. Attribute values are always UTF-8 bytes, even when the XML source was ")
                _T("wide. Converting to CString requires CA2W(s.c_str(), CP_UTF8); a plain ")
                _T("assignment decodes them with the local code page and produces garbage.")),
            kNoteTextColor);
        AddLabelRow(page.get(), std::move(rules), kCodeRowH);
    }
    {
        std::unique_ptr<DuiLabel> pointer = MakeBlock(
            Txt(_T("只用内置标签建整棵界面树的例子在「完整示例」分组的登录界面那一页，")
                _T("本页不再重复，专讲自定义标签这一侧。"),
                _T("Building a whole UI tree out of built-in tags is covered by the login page ")
                _T("in the \"Complete samples\" group; this page does not repeat it and focuses ")
                _T("on the custom-tag side.")),
            kNoteTextColor);
        AddLabelRow(page.get(), std::move(pointer), kNoteRowH);
    }

    // ---- 段落二：自定义叶子标签 ---------------------------------------
    AddSection(page.get(),
               Txt(_T("自定义叶子标签 <demo-tile/>"), _T("A custom leaf tag: <demo-tile/>")),
               Txt(_T("左边是这一刻真正喂给 DuiXmlBuilder::FromString 的那串 XML，右边是它现场 ")
                   _T("解析出来的控件。工厂只做三件事：读 text 属性、读 bg-color 属性、读 ")
                   _T("radius 属性，其余全交给 builder。点「重新解析」换一份属性再建一次，")
                   _T("外观立刻跟着变。第三份 XML 故意把 bg-color 写坏，用来看默认值的兜底。"),
                   _T("On the left is the exact XML string handed to ")
                   _T("DuiXmlBuilder::FromString right now; on the right is the control tree it ")
                   _T("just produced. The factory does three things — read text, read bg-color, ")
                   _T("read radius — and leaves everything else to the builder. Press \"Reparse\" ")
                   _T("to cycle to another set of attributes and rebuild. The third variant ")
                   _T("deliberately malforms bg-color so the fallback is visible.")));
    {
        std::unique_ptr<DuiLabel> source = MakeBlock(_T(""), kCodeTextColor);
        g_xml.leafSource = source.get();
        AddLabelRow(page.get(), std::move(source), kXmlSourceRowH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiVBox> host = MakeTreeHost();
        g_xml.leafHost = host.get();
        row->AddChild(std::move(host), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kXmlResultRowH);
    }
    AddGap(page.get(), kStackGap);
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<FnButton> reparse(new FnButton());
        reparse->SetText(Txt(_T("重新解析（换一份属性）"), _T("Reparse (next attribute set)")));
        reparse->onClick = &OnXmlReparseClicked;
        row->AddChild(std::move(reparse), DuiLayout::Hint().Fixed(kXmlButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    RebuildLeafDemo();

    // ---- 段落三：自绘容器标签 -----------------------------------------
    AddSection(page.get(),
               Txt(_T("自绘容器标签 <demo-stack/>：子节点自动递归"),
                   _T("A self-drawn container tag: <demo-stack/>")),
               Txt(_T("下面这一块的工厂函数只有一行「返回一个新的 XmlStack」，里面没有任何 ")
                   _T("解析子节点的代码。三个子节点（两个内置的 label、一个自定义的 ")
                   _T("demo-tile）是 builder 自己建出来并挂上去的，XmlStack 只覆写了 Layout ")
                   _T("决定它们竖着均分。灰色框线是 XmlStack 自己画的，用来标出它的范围。"),
                   _T("The factory for the block below is a single line returning a new ")
                   _T("XmlStack; it contains no child-parsing code whatsoever. The three ")
                   _T("children (two built-in labels and one custom demo-tile) were built and ")
                   _T("attached by the builder, and XmlStack only overrides Layout to split the ")
                   _T("height evenly. The gray frame is drawn by XmlStack itself to mark its ")
                   _T("bounds.")));
    {
        LPCSTR xml = (CurrentLanguage() == LangEnglish)
            ? "<demo-stack>"
              "  <label text=\"built-in label, first child\"/>"
              "  <demo-tile text=\"custom tile, second child\" bg-color=\"60,165,92\" radius=\"4\"/>"
              "  <label text=\"built-in label, third child\"/>"
              "</demo-stack>"
            : "<demo-stack>"
              "  <label text=\"内置 label，第一个子节点\"/>"
              "  <demo-tile text=\"自定义 demo-tile，第二个子节点\" bg-color=\"60,165,92\" radius=\"4\"/>"
              "  <label text=\"内置 label，第三个子节点\"/>"
              "</demo-stack>";

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiVBox> host = MakeTreeHost();
        DuiXmlBuilder::CustomFactory factory = &DemoTagFactory;
        ReplaceHostedTree(host.get(), DuiXmlBuilder::FromString(xml, factory));
        row->AddChild(std::move(host), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kXmlStackRowH);
    }

    // ---- 段落四：混排 --------------------------------------------------
    AddSection(page.get(),
               Txt(_T("内置标签与自定义标签互相嵌套"),
                   _T("Built-in and custom tags nest both ways")),
               Txt(_T("自定义控件可以放进任何内置容器，内置控件也可以放进自绘容器。下面这份 ")
                   _T("XML 里，最外层是内置的 vbox，里面一层是内置的 hbox 装两个自定义色块，")
                   _T("再下面是一个自定义容器装一个内置按钮 —— 三层交替，工厂与 builder 各管 ")
                   _T("各的，不需要任何额外协调。"),
                   _T("Custom controls go into any built-in container, and built-in controls go ")
                   _T("into self-drawn ones. In the XML below the outermost element is a ")
                   _T("built-in vbox, inside it a built-in hbox holding two custom tiles, and ")
                   _T("below that a custom container holding a built-in button — three ")
                   _T("alternating levels, with the factory and the builder each minding their ")
                   _T("own business.")));
    {
        LPCSTR xml = (CurrentLanguage() == LangEnglish)
            ? "<vbox gap=\"6\">"
              "  <hbox gap=\"8\" fixedHeight=\"40\">"
              "    <demo-tile text=\"custom\" bg-color=\"45,108,223\" weight=\"1\"/>"
              "    <demo-tile text=\"custom\" bg-color=\"120,90,180\" weight=\"1\"/>"
              "  </hbox>"
              "  <demo-stack>"
              "    <button text=\"built-in button inside a custom container\"/>"
              "  </demo-stack>"
              "</vbox>"
            : "<vbox gap=\"6\">"
              "  <hbox gap=\"8\" fixedHeight=\"40\">"
              "    <demo-tile text=\"自定义\" bg-color=\"45,108,223\" weight=\"1\"/>"
              "    <demo-tile text=\"自定义\" bg-color=\"120,90,180\" weight=\"1\"/>"
              "  </hbox>"
              "  <demo-stack>"
              "    <button text=\"自绘容器里的内置按钮\"/>"
              "  </demo-stack>"
              "</vbox>";

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiVBox> host = MakeTreeHost();
        DuiXmlBuilder::CustomFactory factory = &DemoTagFactory;
        ReplaceHostedTree(host.get(), DuiXmlBuilder::FromString(xml, factory));
        row->AddChild(std::move(host), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kXmlMixedRowH);
    }

    // ---- 段落五：工厂返回空指针与语法错误 -----------------------------
    AddSection(page.get(),
               Txt(_T("工厂返回空指针与 XML 语法错误"),
                   _T("A null factory result, and malformed XML")),
               Txt(_T("这两种情况都不会抛异常、也不会崩溃，但表现完全不同，要分清。")
                   _T("左边这份 XML 里夹了一个 <weird/> 标签，工厂对它返回空指针：builder ")
                   _T("跳过这个节点，前后两个内置 label 照常建出来。右边这份 XML 的结束标签 ")
                   _T("缺了，解析阶段就失败，FromString 返回空指针，一个控件都没有 —— ")
                   _T("调用方必须自己判空，否则后面对返回值解引用就要出事。"),
                   _T("Neither case throws or crashes, but they behave very differently. The ")
                   _T("XML on the left contains a <weird/> tag for which the factory returns ")
                   _T("null: the builder skips that node and the two built-in labels around it ")
                   _T("are still built. The XML on the right is missing its closing tag, so ")
                   _T("parsing fails and FromString returns null — no control at all. Callers ")
                   _T("must check for null before dereferencing the result.")));
    {
        LPCSTR xml = (CurrentLanguage() == LangEnglish)
            ? "<vbox gap=\"4\">"
              "  <label text=\"label before the unknown tag\" fixedHeight=\"20\"/>"
              "  <weird/>"
              "  <label text=\"label after the unknown tag\" fixedHeight=\"20\"/>"
              "</vbox>"
            : "<vbox gap=\"4\">"
              "  <label text=\"未知标签之前的 label\" fixedHeight=\"20\"/>"
              "  <weird/>"
              "  <label text=\"未知标签之后的 label\" fixedHeight=\"20\"/>"
              "</vbox>";

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiVBox> host = MakeTreeHost();
        DuiXmlBuilder::CustomFactory factory = &AlwaysNullFactory;
        ReplaceHostedTree(host.get(), DuiXmlBuilder::FromString(xml, factory));
        row->AddChild(std::move(host), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kXmlResultRowH);
    }
    AddGap(page.get(), kStackGap);
    {
        // 结束标签缺失的 XML。解析结果如实显示出来，不预先断言它一定失败。
        const char* kBrokenXml = "<vbox><label text=\"no closing tag\" fixedHeight=\"20\"/>";
        DuiXmlBuilder::CustomFactory factory = &DemoTagFactory;
        std::unique_ptr<DuiControl> broken = DuiXmlBuilder::FromString(kBrokenXml, factory);

        CString text;
        if (broken)
        {
            text = Txt(_T("这份缺结束标签的 XML，FromString 仍然建出了一棵控件树。"),
                       _T("For this XML with no closing tag, FromString still returned a tree."));
        }
        else
        {
            text = Txt(_T("这份缺结束标签的 XML，FromString 返回了空指针。"),
                       _T("For this XML with no closing tag, FromString returned null."));
        }
        std::unique_ptr<DuiLabel> result = MakeBlock(text, kResultTextColor);
        AddLabelRow(page.get(), std::move(result), kTextRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 键盘可达性（DuiMnemonic 与 DuiFocus）
// =====================================================================

namespace {

// ---- 版面尺寸（像素）-----------------------------------------------

// 焦点方块那一行的高度。
const int kSquareRowH = 60;
// 每个焦点方块的宽度。
const int kSquareW = 96;
// 焦点框对照格子那一行的高度。
const int kRingRowH = 52;
// 每个焦点框对照格子的宽度。
const int kRingCellW = 132;
// 焦点框对照每一行左侧那个画法名称标签的宽度。
const int kRingNameW = 216;
// 助记符表格里「输入字符串」这一列的宽度。
const int kMnemonicInputW = 172;
// 助记符表格里两个结果列的宽度。
const int kMnemonicResultW = 132;
// 助记符演示里真按钮与真标签的宽度。
const int kMnemonicCtrlW = 168;

// 焦点框画在离格子边缘多少像素的位置上。四种画法共用这一个内缩量，
// 差别才只落在画法本身。
const int kRingInset = 6;
// 手工指定颜色的那两种画法用的颜色。选一个在白、浅灰、品牌蓝、深灰四种
// 底色上都能看清的橙色，好让「实线环处处清楚」这件事成立。
const COLORREF kRingSampleColor = RGB(255, 140, 0);

// 焦点方块的控件编号起点。通知处理函数按编号认出是哪一个方块获得了焦点。
const UINT kIdFocusSquareFirst = 9200;

// 键盘遍历演示用的方块。
//
// 它只做三件事：把自己涂成一块颜色、获得焦点时用 balloonui 的焦点环把自己
// 圈起来、并在获得焦点时让宿主窗口去要 Win32 键盘焦点。第三件事是必需的 ——
// 纯 DUI 控件没有自己的窗口，宿主窗口不持有 Win32 键盘焦点时，按键消息会
// 全部投递给宿主的父窗口，DuiHost::OnKeyDown 根本收不到 Tab 键。
class FocusSquare : public DuiControl
{
public:
    // 构造一个方块。
    //   caption：画在方块正中的文字。允许传空指针，表示不画文字；本类内部
    //            复制一份，不引用调用方的缓冲区。
    explicit FocusSquare(LPCTSTR caption)
        : m_caption(caption != NULL ? caption : _T(""))
    {
    }

    // 绘制方块：底色 + 文字，获得焦点时再叠一圈跟随主题的焦点环。
    //   hdc：目标设备上下文。本函数改动的背景模式与文字色在返回前会还原。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }

        // 不能 Tab 停靠的方块画得更浅，好一眼看出遍历为什么跳过它。
        COLORREF bgColor = IsTabStop() ? kDemoLightGray : kDemoWhite;
        DuiVBox::PaintBackground(hdc, m_rcItem, bgColor, kTileDefaultRadius,
                                 kDemoBorderColor, 1.0f);

        if (!m_caption.IsEmpty())
        {
            int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldTextColor = ::SetTextColor(
                hdc, IsTabStop() ? kOnLightTextColor : kNoteTextColor);
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
            ::DrawText(hdc, m_caption, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (oldFont != NULL)
            {
                ::SelectObject(hdc, oldFont);
            }
            ::SetTextColor(hdc, oldTextColor);
            ::SetBkMode(hdc, oldBkMode);
        }

        if (IsFocused())
        {
            DuiFocus::DrawRingThemed(hdc, m_rcItem);
        }
    }

    // 本控件获得 DUI 焦点时需要宿主窗口持有 Win32 键盘焦点，理由见类注释。
    bool NeedsWin32Focus() const override
    {
        return true;
    }

private:
    // 画在方块正中的文字。构造时复制一份，生命期与本对象相同。
    CString m_caption;
};

// 焦点框画法对照用的格子。四种画法各画一行，每行四个不同底色的格子。
class FocusRingCell : public DuiControl
{
public:
    // 焦点框的四种画法。
    enum RingStyle
    {
        // Win32 自带的 ::DrawFocusRect：异或方式画 1 像素虚线，不能指定颜色。
        RingWin32Dotted = 0,
        // DuiFocus::DrawRing，1 像素实线，不加内嵌白线。
        RingSolidThin = 1,
        // DuiFocus::DrawRing，2 像素实线 + 1 像素内嵌白线。
        RingSolidInset = 2,
        // DuiFocus::DrawRingThemed，颜色取自当前主题的品牌色。
        RingThemed = 3,
    };

    // 构造一个对照格子。
    //   style：这一格用哪种画法。
    //   bgColor：格子底色。
    //   textColor：在该底色上能看清的文字色。
    //   caption：底色的名字，画在格子正中。允许为空指针。
    FocusRingCell(RingStyle style, COLORREF bgColor, COLORREF textColor, LPCTSTR caption)
        : m_style(style)
        , m_bgColor(bgColor)
        , m_textColor(textColor)
        , m_caption(caption != NULL ? caption : _T(""))
    {
        // 本格子只是给人看的样张，不参与 Tab 遍历。
        SetTabStop(false);
    }

    // 绘制格子：底色 + 底色名 + 按 m_style 指定的画法画一圈焦点框。
    //   hdc：目标设备上下文。本函数改动的背景模式与文字色在返回前会还原。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }

        HBRUSH hBrush = ::CreateSolidBrush(m_bgColor);
        ::FillRect(hdc, &m_rcItem, hBrush);
        ::DeleteObject(hBrush);

        if (!m_caption.IsEmpty())
        {
            int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldTextColor = ::SetTextColor(hdc, m_textColor);
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
            ::DrawText(hdc, m_caption, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (oldFont != NULL)
            {
                ::SelectObject(hdc, oldFont);
            }
            ::SetTextColor(hdc, oldTextColor);
            ::SetBkMode(hdc, oldBkMode);
        }

        RECT rcRing = m_rcItem;
        ::InflateRect(&rcRing, -kRingInset, -kRingInset);
        switch (m_style)
        {
        //Win32 自带画法：异或 1 像素虚线，与底色混合，花色或深色底上会看不清。
        case RingWin32Dotted:
            ::DrawFocusRect(hdc, &rcRing);
            break;

        //1 像素实线，不加内嵌白线。深色底上边缘对比度靠颜色本身撑着。
        case RingSolidThin:
            DuiFocus::DrawRing(hdc, rcRing, kRingSampleColor, 1, false);
            break;

        //2 像素实线 + 1 像素内嵌白线。内嵌白线让深色底上也有一条亮边。
        case RingSolidInset:
            DuiFocus::DrawRing(hdc, rcRing, kRingSampleColor,
                               DuiFocus::kDefaultThickness, true);
            break;

        //颜色取自当前主题的品牌色，与「主题」页切换预设联动。
        case RingThemed:
            DuiFocus::DrawRingThemed(hdc, rcRing);
            break;

        //枚举已经穷举完，这里不会走到；留一个空分支避免编译器报缺失分支。
        default:
            break;
        }
    }

private:
    // 本格子用哪种画法。构造时确定，之后不再改变。
    RingStyle m_style;
    // 格子底色。
    COLORREF m_bgColor;
    // 底色上的文字色。
    COLORREF m_textColor;
    // 画在格子正中的底色名。
    CString m_caption;
};

// 焦点框对照演示用的一种底色，以及配在上面能看清的文字色。
struct RingBackdrop
{
    // 格子底色。
    COLORREF bg;
    // 该底色上能看清的文字色。
    COLORREF text;
    // 底色的中文名。
    LPCTSTR nameZh;
    // 底色的英文名。
    LPCTSTR nameEn;
};

// 四种底色。白与浅灰代表常规浅色界面，品牌蓝代表选中行一类的花色底，
// 深灰代表深色主题。异或虚线在后两种底色上基本看不见，这正是要看到的事。
const RingBackdrop kRingBackdrops[] = {
    { kDemoWhite,     kOnLightTextColor, _T("白"),     _T("white")      },
    { kDemoLightGray, kOnLightTextColor, _T("浅灰"),   _T("light gray") },
    { kDemoBlue,      kOnDarkTextColor,  _T("品牌蓝"), _T("brand blue") },
    { kDemoDarkGray,  kOnDarkTextColor,  _T("深灰"),   _T("dark gray")  },
};

// 底色的种类数。
const int kRingBackdropCount = (int)(sizeof(kRingBackdrops) / sizeof(kRingBackdrops[0]));

// 助记符解析的一条演示用例。
struct MnemonicCase
{
    // 输入字符串，覆盖各种边界情况。
    LPCTSTR input;
    // 这一条要说明什么，中文。
    LPCTSTR noteZh;
    // 这一条要说明什么，英文。
    LPCTSTR noteEn;
};

// 助记符解析的全部演示用例，含转义、末尾孤立的 '&'、完全没有 '&' 等边界情况。
const MnemonicCase kMnemonicCases[] = {
    { _T("&Save"),        _T("普通助记符，取 '&' 后面那个字符"),
                          _T("plain mnemonic: the char right after '&'") },
    { _T("Save &As"),     _T("助记符在字符串中间"),
                          _T("mnemonic in the middle of the string") },
    { _T("Save && Quit"), _T("只有转义，没有真正的助记符"),
                          _T("escape only, no real mnemonic") },
    { _T("&&&Foo"),       _T("前两个 '&' 是转义，第三个才是标记"),
                          _T("first two '&' escape, the third one marks") },
    { _T("trailing&"),    _T("末尾孤立的 '&'，后面没有字符可取"),
                          _T("dangling '&' with nothing after it") },
    { _T("plain"),        _T("完全没有 '&'"),
                          _T("no '&' at all") },
    { _T("保存(&S)"),      _T("中文文案的常见写法，标记放在括号里"),
                          _T("typical CJK caption: the marker sits in parentheses") },
};

// 助记符演示用例的条数。
const int kMnemonicCaseCount = (int)(sizeof(kMnemonicCases) / sizeof(kMnemonicCases[0]));

// 键盘可达性页面在运行期需要回头改动的东西。页面被销毁后这些指针全部失效，
// 所以每次构建页面的第一件事就是把整块清空。
struct KeyboardPageState
{
    // 显示焦点当前落在第几个方块上的标签。
    DuiLabel* focusLabel;
    // 可以 Tab 停靠的方块的控件编号，按声明顺序排列。遍历顺序就是这个顺序，
    // 所以用它把控件编号换算成「第几个」。
    std::vector<UINT> tabStopIds;
};

KeyboardPageState g_keyboard;

// 造一个键盘演示用的方块，并按需把它登记进 Tab 停靠点列表。
//   caption：方块上的文字。
//   tabStop：是否允许 Tab 停靠。
//   ctrlId：控件编号，必须互不相同。
// 返回：方块，所有权交给调用方。
std::unique_ptr<FocusSquare> MakeFocusSquare(LPCTSTR caption, bool tabStop, UINT ctrlId)
{
    std::unique_ptr<FocusSquare> square(new FocusSquare(caption));
    square->SetCtrlId(ctrlId);
    // 控件默认不是 Tab 停靠点，要参与遍历必须显式打开。
    square->SetTabStop(tabStop);
    if (tabStop)
    {
        g_keyboard.tabStopIds.push_back(ctrlId);
    }
    return square;
}

// 键盘可达性页面的通知处理。
//   pNotify：宿主转发过来的通知，可为空。
// DUIN_SETFOCUS 是 DuiControl 基类发出的通用通知码，页面里任何控件获得焦点
// 都会发一条，所以必须连控件编号一起判断，只认本演示登记过的那几个方块；
// 编号对不上时直接返回，不影响别的处理。
void OnKeyboardPageNotify(const DuiNotify* pNotify)
{
    if (pNotify == NULL || g_keyboard.focusLabel == NULL)
    {
        return;
    }
    if (pNotify->code != (UINT)DUIN_SETFOCUS)
    {
        return;
    }

    int index = -1;
    for (size_t i = 0; i < g_keyboard.tabStopIds.size(); ++i)
    {
        if (g_keyboard.tabStopIds[i] == pNotify->ctrlId)
        {
            index = (int)i;
            break;
        }
    }
    if (index < 0)
    {
        return;
    }

    CString text;
    text.Format(Txt(_T("焦点在第 %d 个方块（本页共 %d 个可停靠方块），控件编号 %u。"),
                    _T("Focus is on square %d of %d tab stops on this page; control id %u.")),
                index + 1,
                (int)g_keyboard.tabStopIds.size(),
                (unsigned)pNotify->ctrlId);
    g_keyboard.focusLabel->SetText(text);
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_Keyboard()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    g_keyboard.focusLabel = NULL;
    g_keyboard.tabStopIds.clear();
    g_pageNotifyHook = &OnKeyboardPageNotify;

    // 控件编号按建出来的先后顺序依次发放，保证互不相同。
    UINT nextSquareId = kIdFocusSquareFirst;

    // ---- 段落一：Tab 遍历怎么走 --------------------------------------
    AddSection(page.get(),
               Txt(_T("按 Tab 走一圈"), _T("Walking the tab order")),
               Txt(_T("宿主在 DuiHost::OnKeyDown 里判断 Tab 键，按住 Shift 时反向，调用 ")
                   _T("FocusNext；FocusNext 用 CollectTabStops 按声明顺序深度优先收集所有 ")
                   _T("IsTabStop() 为真的控件，然后在这个列表里循环推进，走到头绕回开头。")
                   _T("控件默认不是 Tab 停靠点，演示控件必须显式调 SetTabStop(true)。\n")
                   _T("先用鼠标点一下任意一个方块，再按 Tab —— 第一步不能省：纯 DUI 控件 ")
                   _T("没有自己的窗口，宿主窗口不持有 Win32 键盘焦点时按键消息根本到不了 ")
                   _T("DuiHost。这几个方块覆写了 NeedsWin32Focus() 返回真，点一下就会让宿主 ")
                   _T("把键盘焦点要过来。"),
                   _T("The host checks for the Tab key in DuiHost::OnKeyDown, reverses when ")
                   _T("Shift is held, and calls FocusNext. FocusNext uses CollectTabStops to ")
                   _T("gather every control whose IsTabStop() is true, depth-first in ")
                   _T("declaration order, then cycles through that list and wraps around. ")
                   _T("Controls are not tab stops by default; a demo control must call ")
                   _T("SetTabStop(true) explicitly.\n")
                   _T("Click any square with the mouse first, then press Tab — the first step ")
                   _T("cannot be skipped. A pure DUI control has no window of its own, so while ")
                   _T("the host window does not hold the Win32 keyboard focus, key messages ")
                   _T("never reach DuiHost at all. These squares override NeedsWin32Focus() to ")
                   _T("return true, so one click makes the host claim the keyboard focus.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);
        const int kFirstGroupCount = 5;    // 第一组放几个方块
        for (int i = 0; i < kFirstGroupCount; ++i)
        {
            CString caption;
            caption.Format(_T("%d"), i + 1);
            row->AddChild(MakeFocusSquare(caption, true, nextSquareId),
                          DuiLayout::Hint().Fixed(kSquareW));
            ++nextSquareId;
        }
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kSquareRowH);
    }
    {
        std::unique_ptr<DuiLabel> focusInfo =
            MakeBlock(Txt(_T("（还没有方块获得焦点）"), _T("(no square has focus yet)")),
                      kResultTextColor);
        g_keyboard.focusLabel = focusInfo.get();
        AddLabelRow(page.get(), std::move(focusInfo), kTextRowH);
    }

    // ---- 段落二：跳过不可 Tab 的控件 ----------------------------------
    AddSection(page.get(),
               Txt(_T("遍历会跳过不可停靠的控件"), _T("Non-stops are skipped")),
               Txt(_T("下面这一排里，第二个和第四个方块没有调 SetTabStop(true)，画得更浅。")
                   _T("按 Tab 走过来时焦点环会直接跳过它们。IsTabStop() 除了看这个标志，")
                   _T("还要求控件本身可见且可用 —— 隐藏或禁用的控件即便设过标志也不会被收集。")
                   _T("上面那个计数标签统计的是本页全部可停靠方块，这一排里能停的两个也算在内。"),
                   _T("In the row below the second and fourth squares never called ")
                   _T("SetTabStop(true) and are drawn paler. Tab walks straight past them. ")
                   _T("Besides that flag, IsTabStop() also requires the control to be visible and ")
                   _T("enabled — a hidden or disabled control is never collected even if the ")
                   _T("flag was set. The counter above covers every tab stop on the page, ")
                   _T("including the stoppable ones in this row.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);
        const int kSecondGroupCount = 5;   // 第二组放几个方块
        for (int i = 0; i < kSecondGroupCount; ++i)
        {
            // 第二个（下标 1）与第四个（下标 3）不是 Tab 停靠点。
            bool tabStop = (i != 1 && i != 3);
            CString caption;
            if (tabStop)
            {
                caption = Txt(_T("可停靠"), _T("stop"));
            }
            else
            {
                caption = Txt(_T("跳过"), _T("skipped"));
            }
            row->AddChild(MakeFocusSquare(caption, tabStop, nextSquareId),
                          DuiLayout::Hint().Fixed(kSquareW));
            ++nextSquareId;
        }
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kSquareRowH);
    }

    // ---- 段落三：四种焦点框画法并排对照 -------------------------------
    AddSection(page.get(),
               Txt(_T("四种焦点框画法在四种底色上的表现"),
                   _T("Four ways of drawing a focus ring on four backdrops")),
               Txt(_T("DuiFocus 存在的全部理由就在这张对照表里。Win32 自带的 ")
                   _T("::DrawFocusRect 用异或方式画 1 像素虚线：异或的结果取决于底下是什么 ")
                   _T("颜色，在品牌蓝和深灰上几乎看不见，而且它不接受颜色参数，想画成品牌色 ")
                   _T("也做不到。DuiFocus::DrawRing 画的是实线，颜色由调用方指定，四种底色上 ")
                   _T("都清清楚楚；带内嵌白线的那一档在深色底上还多一条亮边。最后一行的 ")
                   _T("DrawRingThemed 从主题里取品牌色 —— 它在品牌蓝底上同样看不清，这不是缺陷，")
                   _T("而是「焦点色与底色撞了」的正常结果，实际界面里不会把焦点框画在同色底上。\n")
                   _T("还有一件必须如实说明的事：DuiFocusVisual.h 的注释写着 DuiButton / ")
                   _T("DuiSlider / DuiListBox / DuiEditHost 都在用 DuiFocus，实际并没有 —— ")
                   _T("库里这几个控件画焦点框走的仍然是 Win32 的 ::DrawFocusRect，")
                   _T("DuiFocus 目前没有任何控件在用。所以这张表左边一列就是你现在按 Tab ")
                   _T("在真按钮上看到的样子。"),
                   _T("This table is the entire reason DuiFocus exists. The Win32 ")
                   _T("::DrawFocusRect draws a 1px dotted rectangle by XOR: the result depends ")
                   _T("on whatever is underneath, so it nearly vanishes on brand blue and dark ")
                   _T("gray — and it takes no color argument, so drawing it in the brand color ")
                   _T("is simply not possible. DuiFocus::DrawRing draws a solid ring in a color ")
                   _T("you choose and stays legible on all four backdrops; the variant with the ")
                   _T("white inset adds a bright edge for dark surfaces. The last row, ")
                   _T("DrawRingThemed, takes the brand color from the theme — it is equally hard ")
                   _T("to see on the brand-blue backdrop, which is not a defect but the expected ")
                   _T("result of a focus color colliding with the surface color.\n")
                   _T("One more thing to state plainly: DuiFocusVisual.h claims DuiButton, ")
                   _T("DuiSlider, DuiListBox and DuiEditHost all use DuiFocus. They do not — ")
                   _T("those controls still call the Win32 ::DrawFocusRect, and DuiFocus has no ")
                   _T("user in the library at all. So the leftmost column is exactly what you ")
                   _T("see today when you Tab onto a real button.")));
    {
        // 对照表里的一行：一种焦点框画法，加上它显示在行首的名字。
        // 行内的四个格子由 kRingBackdrops 提供四种底色。
        struct RingRow
        {
            // 这一行用的画法。
            FocusRingCell::RingStyle style;
            // 画法名字，中文。
            LPCTSTR nameZh;
            // 画法名字，英文。
            LPCTSTR nameEn;
        };
        const RingRow kRingRows[] = {
            { FocusRingCell::RingWin32Dotted,
              _T("::DrawFocusRect（异或虚线）"), _T("::DrawFocusRect (XOR dots)") },
            { FocusRingCell::RingSolidThin,
              _T("DrawRing 1 像素、无内嵌线"),   _T("DrawRing 1px, no inset") },
            { FocusRingCell::RingSolidInset,
              _T("DrawRing 2 像素、带内嵌线"),   _T("DrawRing 2px, white inset") },
            { FocusRingCell::RingThemed,
              _T("DrawRingThemed（跟随主题）"),  _T("DrawRingThemed (from theme)") },
        };
        const int kRingRowCount = (int)(sizeof(kRingRows) / sizeof(kRingRows[0]));

        for (int r = 0; r < kRingRowCount; ++r)
        {
            std::unique_ptr<DuiHBox> row(new DuiHBox());
            row->SetGap(kColumnGap);
            row->AddChild(MakeLine(Txt(kRingRows[r].nameZh, kRingRows[r].nameEn),
                                   kNoteTextColor),
                          DuiLayout::Hint().Fixed(kRingNameW));
            for (int c = 0; c < kRingBackdropCount; ++c)
            {
                std::unique_ptr<FocusRingCell> cell(new FocusRingCell(
                    kRingRows[r].style,
                    kRingBackdrops[c].bg,
                    kRingBackdrops[c].text,
                    Txt(kRingBackdrops[c].nameZh, kRingBackdrops[c].nameEn)));
                row->AddChild(std::move(cell), DuiLayout::Hint().Fixed(kRingCellW));
            }
            row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                          DuiLayout::Hint().Weight(1));
            AddVariantRow(page.get(), std::move(row), kRingRowH);
        }
    }

    // ---- 段落四：助记符解析结果表 -------------------------------------
    AddSection(page.get(),
               Txt(_T("助记符解析结果表"), _T("Mnemonic parsing results")),
               Txt(_T("DuiMnemonic 是两个纯函数。FindChar 返回第一个单个 '&' 后面那个字符的 ")
                   _T("小写形式，没有就返回 0；StripPrefix 把所有单个 '&' 去掉、把 \"&&\" 折成 ")
                   _T("一个字面的 '&'，返回可以直接显示给用户的字符串。下表每一行都是现场调 ")
                   _T("这两个函数算出来的，不是写死的期望值。\n")
                   _T("注意表格这几列的标签都关掉了 GDI 的助记符解析（DT_NOPREFIX），")
                   _T("否则输入字符串里的 '&' 会被 DrawText 自己吃掉，读者看到的就不是真正的 ")
                   _T("输入了。"),
                   _T("DuiMnemonic is two pure functions. FindChar returns the lowercase form of ")
                   _T("the character after the first single '&', or 0 when there is none. ")
                   _T("StripPrefix removes every single '&' and folds \"&&\" into one literal ")
                   _T("'&', producing a string ready to show to the user. Every row below is ")
                   _T("computed by calling these two functions right now, not a hard-coded ")
                   _T("expectation.\n")
                   _T("Note that the labels in these columns turn off GDI's own mnemonic parsing ")
                   _T("(DT_NOPREFIX); otherwise DrawText would swallow the '&' in the input ")
                   _T("string and the reader would not see the real input.")));
    {
        std::unique_ptr<DuiHBox> header(new DuiHBox());
        header->SetGap(kColumnGap);
        header->AddChild(MakeLine(Txt(_T("输入字符串"), _T("input")), kOnLightTextColor),
                         DuiLayout::Hint().Fixed(kMnemonicInputW));
        header->AddChild(MakeLine(_T("FindChar"), kOnLightTextColor),
                         DuiLayout::Hint().Fixed(kMnemonicResultW));
        header->AddChild(MakeLine(_T("StripPrefix"), kOnLightTextColor),
                         DuiLayout::Hint().Fixed(kMnemonicResultW));
        header->AddChild(MakeLine(Txt(_T("说明"), _T("note")), kOnLightTextColor),
                         DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(header), kTextRowH);
    }
    for (int i = 0; i < kMnemonicCaseCount; ++i)
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kColumnGap);

        // 输入字符串两侧加一对引号，行尾有没有空格、末尾那个 '&' 在不在，
        // 一眼就看得出来。
        CString shown;
        shown.Format(_T("\"%s\""), kMnemonicCases[i].input);

        TCHAR found = DuiMnemonic::FindChar(kMnemonicCases[i].input);
        CString foundText;
        if (found == 0)
        {
            foundText = Txt(_T("0（没有）"), _T("0 (none)"));
        }
        else
        {
            foundText.Format(_T("'%c'"), found);
        }

        CString stripped;
        stripped.Format(_T("\"%s\""),
                        (LPCTSTR)DuiMnemonic::StripPrefix(kMnemonicCases[i].input));

        row->AddChild(MakeLine(shown, kCodeTextColor),
                      DuiLayout::Hint().Fixed(kMnemonicInputW));
        row->AddChild(MakeLine(foundText, kResultTextColor),
                      DuiLayout::Hint().Fixed(kMnemonicResultW));
        row->AddChild(MakeLine(stripped, kResultTextColor),
                      DuiLayout::Hint().Fixed(kMnemonicResultW));
        row->AddChild(MakeLine(Txt(kMnemonicCases[i].noteZh, kMnemonicCases[i].noteEn),
                               kNoteTextColor),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    // ---- 段落五：按住 Alt 看下划线 ------------------------------------
    {
        // 系统设置决定「助记符下划线是一直显示，还是按下 Alt 才显示」。
        // 把这台机器上的当前设置读出来，读者才知道自己该看到什么。
        BOOL keyboardCues = TRUE;
        ::SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &keyboardCues, 0);

        AddSection(page.get(),
                   Txt(_T("按住 Alt 看下划线"), _T("Hold Alt to reveal the underline")),
                   keyboardCues
                       ? Txt(_T("DuiButton 与 DuiLabel 绘制文字时都没有传 DT_NOPREFIX，所以 ")
                             _T("GDI 会把 \"&X\" 里的 X 画上下划线。这台机器的系统设置是")
                             _T("「一直显示下划线」，所以下面这两个控件的下划线现在就看得见。"),
                             _T("Neither DuiButton nor DuiLabel passes DT_NOPREFIX when drawing ")
                             _T("text, so GDI underlines the X in \"&X\". This machine is ")
                             _T("configured to always show the underline, so it is already ")
                             _T("visible on the two controls below."))
                       : Txt(_T("DuiButton 与 DuiLabel 绘制文字时都没有传 DT_NOPREFIX，所以 ")
                             _T("GDI 会把 \"&X\" 里的 X 画上下划线。这台机器的系统设置是")
                             _T("「按下 Alt 才显示下划线」，所以现在看不到，按住 Alt 键就出来了。"),
                             _T("Neither DuiButton nor DuiLabel passes DT_NOPREFIX when drawing ")
                             _T("text, so GDI underlines the X in \"&X\". This machine is ")
                             _T("configured to reveal the underline only while Alt is held, so ")
                             _T("press and hold Alt to see it.")));
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiButton> button(new DuiButton());
        button->SetText(Txt(_T("保存(&S)"), _T("&Save")));

        std::unique_ptr<DuiLabel> label(new DuiLabel());
        label->SetText(Txt(_T("文件名(&F)："), _T("&Filename:")));
        label->SetTextColor(kOnLightTextColor);

        row->AddChild(std::move(button), DuiLayout::Hint().Fixed(kMnemonicCtrlW));
        row->AddChild(std::move(label), DuiLayout::Hint().Fixed(kMnemonicCtrlW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> note = MakeBlock(
            Txt(_T("库里只提供解析，不提供分发 —— 没有现成的「按 Alt+X 自动找到对应控件并 ")
                _T("点它」的机制，这一步一直由业务侧自己写。宿主会把 WM_SYSCHAR 转发给当前 ")
                _T("焦点控件，业务通常在自己的窗口过程里这样接："),
                _T("The library parses but does not dispatch — there is no built-in \"press ")
                _T("Alt+X, find the matching control and click it\" mechanism; business code has ")
                _T("always written that step itself. The host forwards WM_SYSCHAR to the focused ")
                _T("control, and business code usually hooks it in its own window procedure ")
                _T("like this:")),
            kNoteTextColor);
        AddLabelRow(page.get(), std::move(note), kNoteRowH);
    }
    {
        std::unique_ptr<DuiLabel> code = MakeBlock(
            _T("if (uMsg == WM_SYSCHAR)\n")
            _T("{\n")
            _T("    TCHAR pressed = (TCHAR)_totlower((TCHAR)wParam);\n")
            _T("    if (pressed == DuiMnemonic::FindChar(button.GetText()))\n")
            _T("    {\n")
            _T("        //  ......  \n")
            _T("    }\n")
            _T("}"),
            kCodeTextColor);
        AddLabelRow(page.get(), std::move(code), kCodeRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 拖放接收（DuiDropTarget）
// =====================================================================

namespace {

// ---- 版面尺寸（像素）-----------------------------------------------

// 投放区那一行的高度。
const int kDropZoneRowH = 120;
// 回调时序日志那一块的高度。最多显示 kMaxLogLines 行，留够。
const int kDropLogRowH = 108;
// 落手结果（文件全路径）那一块的高度。
const int kDropResultRowH = 60;
// 位图预览控件的高度。
const int kPreviewRowH = 96;
// 位图预览控件的宽度。
const int kPreviewW = 140;
// 拖放页面上按钮的宽度。两个按钮的文案都不短，取宽一点免得被截断。
const int kDropButtonW = 320;

// 回调时序日志最多保留几行。够看清「进入 → 若干次经过 → 落手 → 离开」
// 这一整轮就行，再多会把旧的挤得看不清重点。
const int kMaxLogLines = 6;

// 投放区四周框线的粗细（像素）。
const int kDropBorderWidth = 2;
// 左右分半模式那条分界竖线上下各留多少空白（像素），不要顶到框线上。
const int kDropSplitInset = 8;

// 悬停且当前位置可以放时投放区的底色。浅绿，与绿色框线同色系。
const COLORREF kDropAcceptBg = RGB(224, 244, 231);
// 悬停但当前位置不能放时投放区的底色。浅红，与红色框线同色系。
const COLORREF kDropRejectBg = RGB(250, 228, 228);

// 合成拖放事件用的两个假文件路径。它们不需要真实存在 —— 拖放的数据里
// 装的本来就只是路径字符串。
LPCTSTR kFakeFilePath0 = _T("C:\\demo\\quarterly-report.pdf");
LPCTSTR kFakeFilePath1 = _T("C:\\demo\\office-photo.png");

// 只提供文件列表（CF_HDROP）的最小数据对象。
//
// 用来在没有鼠标参与的情况下把一次完整拖放走一遍：截图与录屏环境里没法真的
// 从资源管理器拖一个文件进来，但把 OLE 会调的那四个方法自己调一遍，效果与
// 真拖放完全一样。写法照抄 balloonui 的单元测试 DuiDropTargetTests。
class FakeFileDataObject : public IDataObject
{
public:
    FakeFileDataObject()
        : m_refCount(1)
    {
    }
    virtual ~FakeFileDataObject()
    {
    }

    // ---- IUnknown ----

    // 查询接口。本对象只实现 IUnknown 与 IDataObject。
    //   riid：请求的接口标识。
    //   ppv：出参，成功时写入接口指针并已 AddRef；失败时写入空指针。
    // 返回：S_OK 或 E_NOINTERFACE / E_POINTER。
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == NULL)
        {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject)
        {
            *ppv = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    // 引用计数加一。返回：加一之后的引用计数。
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&m_refCount);
    }

    // 引用计数减一，减到 0 时删除自身。返回：减一之后的引用计数。
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG left = InterlockedDecrement(&m_refCount);
        if (left == 0)
        {
            delete this;
        }
        return (ULONG)left;
    }

    // ---- IDataObject：只实现取数据用得上的两个方法 ----

    // 询问是否提供某种数据格式。
    //   pFmt：要询问的格式。
    // 返回：提供该格式时 S_OK，否则 DV_E_FORMATETC。
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pFmt) override
    {
        if (pFmt == NULL)
        {
            return E_POINTER;
        }
        if (pFmt->cfFormat == CF_HDROP && (pFmt->tymed & TYMED_HGLOBAL) != 0)
        {
            return S_OK;
        }
        return DV_E_FORMATETC;
    }

    // 取出数据。
    //   pFmt：要取的格式。
    //   pMed：出参，成功时写入一块 CF_HDROP 内存，由调用方 ReleaseStgMedium 释放。
    // 返回：S_OK / DV_E_FORMATETC / E_OUTOFMEMORY / E_POINTER。
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pFmt, STGMEDIUM* pMed) override
    {
        if (pFmt == NULL || pMed == NULL)
        {
            return E_POINTER;
        }
        if (pFmt->cfFormat != CF_HDROP || (pFmt->tymed & TYMED_HGLOBAL) == 0)
        {
            return DV_E_FORMATETC;
        }
        HGLOBAL hGlobal = BuildHDrop();
        if (hGlobal == NULL)
        {
            return E_OUTOFMEMORY;
        }
        ZeroMemory(pMed, sizeof(*pMed));
        pMed->tymed = TYMED_HGLOBAL;
        pMed->hGlobal = hGlobal;
        pMed->pUnkForRelease = NULL;
        return S_OK;
    }

    // ---- 其余方法一律不支持 ----

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    // 现搓一块 CF_HDROP 内存：DROPFILES 头 + 两条以 '\0' 结尾的路径 +
    // 一个结尾哨兵。布局与资源管理器真拖过来的完全一致。
    // 返回：内存句柄；分配失败时返回空句柄。
    static HGLOBAL BuildHDrop()
    {
        SIZE_T length0 = _tcslen(kFakeFilePath0);
        SIZE_T length1 = _tcslen(kFakeFilePath1);
        // 两条路径各自带一个结尾的 '\0'，末尾再加一个哨兵 '\0'。
        SIZE_T payloadChars = length0 + 1 + length1 + 1 + 1;
        SIZE_T totalBytes = sizeof(DROPFILES) + payloadChars * sizeof(TCHAR);

        HGLOBAL hGlobal = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalBytes);
        if (hGlobal == NULL)
        {
            return NULL;
        }
        DROPFILES* pHeader = (DROPFILES*)::GlobalLock(hGlobal);
        pHeader->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
        pHeader->fWide = TRUE;
#else
        pHeader->fWide = FALSE;
#endif
        TCHAR* pPaths = (TCHAR*)((BYTE*)pHeader + sizeof(DROPFILES));
        _tcscpy_s(pPaths, length0 + 1, kFakeFilePath0);
        _tcscpy_s(pPaths + length0 + 1, length1 + 1, kFakeFilePath1);
        pPaths[length0 + 1 + length1 + 1] = 0;
        ::GlobalUnlock(hGlobal);
        return hGlobal;
    }

    // COM 引用计数。本对象只在界面线程上使用，但仍按 COM 约定用原子操作。
    LONG m_refCount;
};

// 拖入位图的预览控件。
//
// 位图由拖放回调交进来。必须<u>当场</u>复制一份自己保管：DuiDropTarget 在
// 回调返回之后就会 ReleaseStgMedium，那一步会把系统交来的位图删掉，直接
// 存下句柄的话拿到的是一个已经失效的句柄。
class BitmapPreview : public DuiControl
{
public:
    BitmapPreview()
        : m_hBitmap(NULL)
    {
        SetTabStop(false);
    }
    ~BitmapPreview()
    {
        if (m_hBitmap != NULL)
        {
            ::DeleteObject(m_hBitmap);
            m_hBitmap = NULL;
        }
    }

    // 换一张要预览的位图。
    //   hBitmap：新位图，<u>所有权转移给本控件</u>，本控件析构时释放；
    //            允许传空句柄，表示清空预览。
    void SetPreviewBitmap(HBITMAP hBitmap)
    {
        if (m_hBitmap != NULL)
        {
            ::DeleteObject(m_hBitmap);
        }
        m_hBitmap = hBitmap;
        Invalidate();
    }

    // 绘制预览：没有位图时画一行占位说明，有位图时等比缩放着画在正中。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }
        DuiVBox::PaintBackground(hdc, m_rcItem, kDemoWhite, kTileDefaultRadius,
                                 kDemoBorderColor, 1.0f);
        if (m_hBitmap == NULL)
        {
            int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldTextColor = ::SetTextColor(hdc, kNoteTextColor);
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
            ::DrawText(hdc, Txt(_T("还没有拖入位图"), _T("no bitmap yet")), -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (oldFont != NULL)
            {
                ::SelectObject(hdc, oldFont);
            }
            ::SetTextColor(hdc, oldTextColor);
            ::SetBkMode(hdc, oldBkMode);
            return;
        }

        BITMAP info;
        ZeroMemory(&info, sizeof(info));
        if (::GetObject(m_hBitmap, sizeof(info), &info) == 0
            || info.bmWidth <= 0 || info.bmHeight <= 0)
        {
            return;
        }

        // 等比缩放到能塞进控件内部（四边各留一点空白）为止。
        const int kPreviewPadding = 6;
        int availW = (m_rcItem.right - m_rcItem.left) - kPreviewPadding * 2;
        int availH = (m_rcItem.bottom - m_rcItem.top) - kPreviewPadding * 2;
        if (availW <= 0 || availH <= 0)
        {
            return;
        }
        int drawW = info.bmWidth;
        int drawH = info.bmHeight;
        if (drawW > availW)
        {
            drawH = ::MulDiv(drawH, availW, drawW);
            drawW = availW;
        }
        if (drawH > availH)
        {
            drawW = ::MulDiv(drawW, availH, drawH);
            drawH = availH;
        }

        POINT center = RectCenter(m_rcItem);
        int left = center.x - drawW / 2;
        int top = center.y - drawH / 2;

        HDC hMemDC = ::CreateCompatibleDC(hdc);
        if (hMemDC == NULL)
        {
            return;
        }
        HGDIOBJ oldBitmap = ::SelectObject(hMemDC, m_hBitmap);
        int oldMode = ::SetStretchBltMode(hdc, HALFTONE);
        ::StretchBlt(hdc, left, top, drawW, drawH,
                     hMemDC, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);
        ::SetStretchBltMode(hdc, oldMode);
        ::SelectObject(hMemDC, oldBitmap);
        ::DeleteDC(hMemDC);
    }

private:
    // 当前预览的位图。本控件持有，换新图与析构时都会释放。空句柄表示还没有图。
    HBITMAP m_hBitmap;
};

// 拖放接收演示区。
//
// 本类把 balloonui 的 DuiDropTargetHelper 包成一个可以直接摆进布局里的方块。
// 它<u>不</u>自己调 helper 的 Register：拖放目标是按窗口注册的，一个窗口只能
// 注册一个，页面里并排放几个投放区就没法各注册各的。走的是宿主提供的那条路 ——
// DuiHost::EnableDropDispatch(true) 让宿主注册唯一一个分发器，分发器按光标
// 位置找到命中的控件，再转发给该控件 GetDropTarget() 返回的对象。
//
// 这条路还有一个好处：注册归宿主所有，宿主在窗口销毁时会自己注销，页面来回
// 切换时不会出现「上一次没注销，这一次注册失败」。
class DropZone : public DuiControl
{
public:
    // 投放区的判定模式。
    enum Mode
    {
        // 整块都能放。
        ModeAcceptAll = 0,
        // 只有左半边能放，右半边不能。用来演示悬停回调逐帧改判。
        ModeLeftHalfOnly = 1,
        // 整块都不能放。用来演示拖入回调返回假之后的表现。
        ModeRejectAll = 2,
    };

    // 构造一个投放区。
    //   mode：初始判定模式。
    //   caption：画在投放区正中的说明文字。允许传空指针；本类内部复制一份。
    DropZone(Mode mode, LPCTSTR caption)
        : m_mode(mode)
        , m_caption(caption != NULL ? caption : _T(""))
        , m_pLogLabel(NULL)
        , m_pResultLabel(NULL)
        , m_pPreview(NULL)
        , m_hovering(false)
        , m_allowHere(true)
        , m_dispatchTried(false)
        , m_dispatchOk(false)
    {
        SetTabStop(false);
        m_lastPoint.x = 0;
        m_lastPoint.y = 0;

        // 装回调。这里用 std::bind 而不是 lambda：仓库约定尽量不写 lambda，
        // 而 std::function 又不能直接接一个成员函数指针，std::bind 是 C++11
        // 里最直接的桥接办法。
        m_helper.SetDragCallbacks(
            std::bind(&DropZone::HandleDragEnter, this,
                      std::placeholders::_1, std::placeholders::_2),
            std::bind(&DropZone::HandleDragOver, this, std::placeholders::_1),
            std::bind(&DropZone::HandleDragLeave, this));
        m_helper.SetFilesAtPointCallback(
            std::bind(&DropZone::HandleFiles, this,
                      std::placeholders::_1, std::placeholders::_2));
        // 文件回调传空：装了带落点坐标的那个之后，落手时只回调它，不再回调
        // 这个不带坐标的旧版本，两者只会触发其一。位图回调仍然要装。
        m_helper.SetCallbacks(DuiDropTargetHelper::FilesCallback(),
                              std::bind(&DropZone::HandleBitmap, this,
                                        std::placeholders::_1));
    }

    ~DropZone()
    {
        // 与 EnsureDispatch 里打开分发器成对：离开页面时关掉它，免得别的
        // 页面还带着一个没人用的拖放目标。宿主在窗口销毁时也会关一次，
        // 所以这里可能已经是关着的，重复调用无害。
        DuiHost* pHost = GetHost();
        if (pHost != NULL)
        {
            pHost->EnableDropDispatch(false);
        }
    }

    // 换一种判定模式。
    //   mode：新的模式。
    void SetMode(Mode mode)
    {
        m_mode = mode;
        Invalidate();
    }

    // 装一个用来显示回调时序的标签。
    //   pLabel：标签，<u>所有权归它所在的卡片</u>，本类只记裸指针；
    //           页面销毁时一并失效。允许传空指针表示不显示时序。
    void SetLogLabel(DuiLabel* pLabel)
    {
        m_pLogLabel = pLabel;
    }

    // 装一个用来显示落手结果（文件全路径）的标签。所有权约定同 SetLogLabel。
    void SetResultLabel(DuiLabel* pLabel)
    {
        m_pResultLabel = pLabel;
    }

    // 装一个用来预览拖入位图的控件。所有权约定同 SetLogLabel。
    void SetPreview(BitmapPreview* pPreview)
    {
        m_pPreview = pPreview;
    }

    // 不用鼠标，用一份合成的假数据对象把一次完整拖放走一遍。
    //
    // 依次调用 DragEnter、DragOver、Drop 三个方法，与真实拖放时 OLE 调的
    // 完全一样，因此四个回调的时序、投放区的变色、落手结果全都能看到。
    // 截图与录屏环境下没法真的拖一个文件进来，这个入口就是为那种场合准备的。
    void RunSyntheticDrop()
    {
        DuiHost* pHost = GetHost();
        if (pHost == NULL || !pHost->IsWindow())
        {
            return;
        }
        IDropTarget* pTarget = m_helper.GetDropTarget();
        if (pTarget == NULL)
        {
            return;
        }

        AppendLog(Txt(_T("---- 合成事件开始 ----"), _T("---- synthetic drop begins ----")));

        // 落点取本投放区正中，并换算成屏幕坐标：真实拖放里 OLE 传进来的就是
        // 屏幕坐标，本类的回调也按屏幕坐标往回换算，两边必须一致。
        POINT center = RectCenter(m_rcItem);
        ::ClientToScreen(pHost->m_hWnd, &center);
        POINTL ptScreen;
        ptScreen.x = center.x;
        ptScreen.y = center.y;

        FakeFileDataObject* pData = new FakeFileDataObject();
        DWORD effect = DROPEFFECT_NONE;
        pTarget->DragEnter(pData, MK_LBUTTON, ptScreen, &effect);
        pTarget->DragOver(MK_LBUTTON, ptScreen, &effect);
        pTarget->Drop(pData, MK_LBUTTON, ptScreen, &effect);
        pData->Release();

        AppendLog(Txt(_T("---- 合成事件结束 ----"), _T("---- synthetic drop ends ----")));
    }

    // 交出本控件的拖放接收对象，供宿主的分发器转发事件。
    // 返回：内部 helper 的接口，<u>所有权归本控件</u>，调用方只借用。
    ::IDropTarget* GetDropTarget() override
    {
        return m_helper.GetDropTarget();
    }

    // 绘制投放区。顺带在第一次绘制时把宿主的拖放分发器打开 —— 页面构建的
    // 那一刻控件还没有挂到宿主上，拿不到窗口，只能等到第一次绘制。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        EnsureDispatch();

        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }

        COLORREF bgColor = kDemoLightGray;
        COLORREF borderColor = kDemoBorderColor;
        if (m_hovering)
        {
            bgColor = m_allowHere ? kDropAcceptBg : kDropRejectBg;
            borderColor = m_allowHere ? kDemoGreen : kDemoRed;
        }
        DuiVBox::PaintBackground(hdc, m_rcItem, bgColor, kTileDefaultRadius,
                                 borderColor, (float)kDropBorderWidth);

        // 左右分半的模式画一条竖线标出分界，读者才知道该往哪半边放。
        if (m_mode == ModeLeftHalfOnly)
        {
            POINT center = RectCenter(m_rcItem);
            RECT rcSplit;
            rcSplit.left = center.x;
            rcSplit.top = m_rcItem.top + kDropSplitInset;
            rcSplit.right = center.x + 1;
            rcSplit.bottom = m_rcItem.bottom - kDropSplitInset;
            HBRUSH hBrush = ::CreateSolidBrush(kDemoBorderColor);
            ::FillRect(hdc, &rcSplit, hBrush);
            ::DeleteObject(hBrush);
        }

        CString text = m_caption;
        if (m_hovering)
        {
            CString extra;
            extra.Format(Txt(_T("\n光标 (%d, %d) —— %s"), _T("\ncursor (%d, %d) — %s")),
                         (int)m_lastPoint.x,
                         (int)m_lastPoint.y,
                         m_allowHere ? Txt(_T("可以放"), _T("droppable"))
                                     : Txt(_T("不能放"), _T("not droppable")));
            text += extra;
        }

        int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldTextColor = ::SetTextColor(hdc, kOnLightTextColor);
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
        ::DrawText(hdc, text, -1, &m_rcItem,
                   DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        if (oldFont != NULL)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::SetTextColor(hdc, oldTextColor);
        ::SetBkMode(hdc, oldBkMode);
    }

private:
    // 第一次绘制时打开宿主的拖放分发器。只尝试一次，失败也不再重试。
    void EnsureDispatch()
    {
        if (m_dispatchTried)
        {
            return;
        }
        DuiHost* pHost = GetHost();
        if (pHost == NULL || !pHost->IsWindow())
        {
            return;
        }
        m_dispatchTried = true;
        m_dispatchOk = pHost->EnableDropDispatch(true);
        if (!m_dispatchOk)
        {
            AppendLog(Txt(_T("EnableDropDispatch 返回假：这个窗口已经被别的代码注册过")
                          _T("拖放目标了，一个窗口只能有一个。"),
                          _T("EnableDropDispatch returned false: this window already has a ")
                          _T("drop target registered by someone else; a window can only have ")
                          _T("one.")));
        }
    }

    // 把回调给的坐标换算成宿主客户区坐标。
    //
    // DuiDropTargetHelper 是靠自己的 Register(hwnd) 记住宿主窗口的。本类走的
    // 是宿主分发器那条路、从来没有调过 Register，helper 因此无从知道宿主窗口
    // 是谁，回调里给出的仍然是 OLE 原始的屏幕坐标。这里自己换算一次。
    //   ptFromCallback：回调收到的坐标。
    // 返回：宿主客户区坐标。
    POINT ToClientPoint(const POINT& ptFromCallback) const
    {
        POINT pt = ptFromCallback;
        DuiHost* pHost = GetHost();
        if (pHost != NULL && pHost->IsWindow())
        {
            ::ScreenToClient(pHost->m_hWnd, &pt);
        }
        return pt;
    }

    // 拖入回调。
    //   ptClient：落点坐标（实际是屏幕坐标，见 ToClientPoint 的说明）。
    //   hasFiles：本次拖的数据里是否含文件列表。
    // 返回：真表示本区收，假表示本次拒收 —— 光标显示禁止符，随后的悬停回调
    //       不再来，落手时也什么都不做。
    bool HandleDragEnter(const POINT& ptClient, bool hasFiles)
    {
        m_lastPoint = ToClientPoint(ptClient);
        m_hovering = true;
        m_allowHere = (m_mode != ModeRejectAll);

        CString line;
        line.Format(Txt(_T("DragEnter：(%d, %d)，拖的是%s，本区%s"),
                        _T("DragEnter: (%d, %d), payload is %s, this zone %s")),
                    (int)m_lastPoint.x,
                    (int)m_lastPoint.y,
                    hasFiles ? Txt(_T("文件"), _T("files")) : Txt(_T("位图"), _T("a bitmap")),
                    m_allowHere ? Txt(_T("收"), _T("accepts"))
                                : Txt(_T("拒收"), _T("rejects")));
        AppendLog(line);
        Invalidate();
        return m_allowHere;
    }

    // 悬停回调。光标在本窗口内移动时持续调用。
    //   ptClient：当前坐标（实际是屏幕坐标，见 ToClientPoint 的说明）。
    // 返回：真表示<u>当前这个位置</u>能放，假表示不能放。
    bool HandleDragOver(const POINT& ptClient)
    {
        m_lastPoint = ToClientPoint(ptClient);

        bool allow = true;
        if (m_mode == ModeLeftHalfOnly)
        {
            // 光标在同一个窗口里移动是不会再触发拖入回调的，能不能放只能在
            // 这里逐帧重新判断。
            allow = (m_lastPoint.x < RectCenter(m_rcItem).x);
        }

        // 只在判定发生变化时记一行，否则鼠标一动就刷屏，看不出重点。
        if (allow != m_allowHere)
        {
            CString line;
            line.Format(Txt(_T("DragOver：(%d, %d) 改判为%s"),
                            _T("DragOver: (%d, %d) now reports %s")),
                        (int)m_lastPoint.x,
                        (int)m_lastPoint.y,
                        allow ? Txt(_T("可以放"), _T("droppable"))
                              : Txt(_T("不能放"), _T("not droppable")));
            AppendLog(line);
        }
        m_allowHere = allow;
        Invalidate();
        return allow;
    }

    // 拖离回调。一次落手完成之后本回调也会被调一次，调用方收起悬停提示
    // 只需要写在这一个地方。
    void HandleDragLeave()
    {
        m_hovering = false;
        AppendLog(Txt(_T("DragLeave：悬停结束（落手之后也会来这一次）"),
                      _T("DragLeave: hover ended (this also fires after a drop)")));
        Invalidate();
    }

    // 带落点坐标的文件回调。
    //   files：文件全路径列表。
    //   ptClient：落点坐标（实际是屏幕坐标，见 ToClientPoint 的说明）。
    void HandleFiles(const std::vector<CString>& files, const POINT& ptClient)
    {
        POINT pt = ToClientPoint(ptClient);

        CString line;
        line.Format(Txt(_T("Drop：(%d, %d)，收到 %d 个文件"),
                        _T("Drop: (%d, %d), %d file(s) received")),
                    (int)pt.x, (int)pt.y, (int)files.size());
        AppendLog(line);

        if (m_pResultLabel == NULL)
        {
            return;
        }
        CString text = Txt(_T("最近一次落手收到的文件："), _T("Files from the last drop:"));
        for (size_t i = 0; i < files.size(); ++i)
        {
            text += _T("\n");
            text += files[i];
        }
        m_pResultLabel->SetText(text);
    }

    // 位图回调。
    //   hBitmap：系统交来的位图。<u>所有权不归本函数</u> —— 回调返回之后
    //            DuiDropTarget 会把它释放掉，因此必须当场复制一份自己保管。
    void HandleBitmap(HBITMAP hBitmap)
    {
        AppendLog(Txt(_T("Drop：收到一张位图"), _T("Drop: a bitmap was received")));
        if (m_pPreview == NULL || hBitmap == NULL)
        {
            return;
        }
        // LR_CREATEDIBSECTION 强制复制出一份新的位图，不会退化成「返回原句柄」。
        HBITMAP copy = (HBITMAP)::CopyImage(hBitmap, IMAGE_BITMAP, 0, 0,
                                            LR_CREATEDIBSECTION);
        if (copy != NULL)
        {
            m_pPreview->SetPreviewBitmap(copy);
        }
    }

    // 往回调时序里追加一行。超出 kMaxLogLines 行时丢掉最旧的一行。
    //   line：要追加的一行文字。
    void AppendLog(LPCTSTR line)
    {
        if (line == NULL)
        {
            return;
        }
        m_logLines.push_back(CString(line));
        while ((int)m_logLines.size() > kMaxLogLines)
        {
            m_logLines.erase(m_logLines.begin());
        }
        if (m_pLogLabel == NULL)
        {
            return;
        }
        CString text;
        for (size_t i = 0; i < m_logLines.size(); ++i)
        {
            if (i > 0)
            {
                text += _T("\n");
            }
            text += m_logLines[i];
        }
        m_pLogLabel->SetText(text);
    }

private:
    // 拖放接收器。本控件持有，析构时一并销毁。
    DuiDropTargetHelper m_helper;
    // 当前的判定模式。
    Mode m_mode;
    // 画在投放区正中的说明文字。
    CString m_caption;
    // 显示回调时序的标签。所有权归它所在的卡片，本控件只记裸指针。可为空。
    DuiLabel* m_pLogLabel;
    // 显示落手结果的标签。所有权同上。可为空。
    DuiLabel* m_pResultLabel;
    // 位图预览控件。所有权同上。可为空。
    BitmapPreview* m_pPreview;
    // 最近若干行回调时序，最旧的在前。
    std::vector<CString> m_logLines;
    // 当前是否有拖动停在本区上。决定投放区画成什么颜色。
    bool m_hovering;
    // 当前这个位置能不能放。左右分半模式下随光标位置逐帧变化。
    bool m_allowHere;
    // 光标最近一次落在哪里，宿主客户区坐标。
    POINT m_lastPoint;
    // 是否已经尝试过打开宿主的拖放分发器。只尝试一次，避免每次绘制都试。
    bool m_dispatchTried;
    // 打开分发器成功了没有。失败时拖入功能不可用，但不影响其它行为。
    bool m_dispatchOk;
};

// 拖放页面在运行期需要回头改动的那几个控件。页面被销毁后这些指针全部失效，
// 所以每次构建页面的第一件事就是把整块清空。
struct DropPageState
{
    // 主投放区。合成事件按钮与整体拒收开关都作用在它身上。
    DropZone* mainZone;
};

DropPageState g_drop;

// 「整体拒收」开关的响应。
//   pButton：被点击的按钮。基类在调到这里之前已经把勾选状态切换好了。
void OnDropRejectClicked(FnButton* pButton)
{
    if (pButton == NULL || g_drop.mainZone == NULL)
    {
        return;
    }
    g_drop.mainZone->SetMode(pButton->IsChecked()
                                 ? DropZone::ModeRejectAll
                                 : DropZone::ModeAcceptAll);
}

// 「合成一次拖放」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnDropSyntheticClicked(FnButton* /*pButton*/)
{
    if (g_drop.mainZone == NULL)
    {
        return;
    }
    g_drop.mainZone->RunSyntheticDrop();
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_DropTarget()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    g_drop.mainZone = NULL;

    // ---- 段落一：怎么接上 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("拖放目标是按窗口注册的，所以要有一个分发器"),
                   _T("Drop targets are per-window, hence a dispatcher")),
               Txt(_T("操作系统的拖放目标按窗口注册，而且一个窗口只能注册一个。纯 DUI 控件 ")
                   _T("没有自己的窗口、共用宿主窗口，所以不能各注册各的。库里给的解法是：")
                   _T("宿主调 EnableDropDispatch(true) 注册唯一一个分发器，分发器收到事件后 ")
                   _T("按光标位置找到命中的控件，沿父链往上找到第一个 GetDropTarget() 非空的，")
                   _T("再把调用原样转发过去。本页面的投放区就是这么接的：控件覆写 ")
                   _T("GetDropTarget 把内部 DuiDropTargetHelper 的接口交出去，第一次绘制时 ")
                   _T("顺手把分发器打开（页面构建的那一刻控件还没挂到宿主上，拿不到窗口）。\n")
                   _T("几个必须知道的行为：Unregister 会连同内部实现对象一起丢弃已装的回调，")
                   _T("重新 Register 之前要重新装一遍；装了带落点坐标的文件回调之后，落手时 ")
                   _T("只回调它、不再回调不带坐标的旧版本；一次落手完成之后拖离回调也会被调 ")
                   _T("一次，所以收起悬停提示只需要写在拖离回调这一个地方；Register 会因为 ")
                   _T("窗口已经被别的代码注册过而失败，此时拖入用不了，但拖出和其它行为都不受影响。"),
                   _T("The OS registers drop targets per window, and a window can hold only one. ")
                   _T("Pure DUI controls have no window of their own and share the host's, so ")
                   _T("they cannot each register. The library's answer: the host calls ")
                   _T("EnableDropDispatch(true) to register a single dispatcher, which on each ")
                   _T("event hit-tests the cursor position, walks up the parent chain to the ")
                   _T("first control whose GetDropTarget() is non-null, and forwards the call ")
                   _T("verbatim. The zones on this page work exactly that way: the control ")
                   _T("overrides GetDropTarget to hand out its internal ")
                   _T("DuiDropTargetHelper's interface, and turns the dispatcher on during its ")
                   _T("first paint (at page-build time the control is not attached to a host ")
                   _T("yet, so there is no window to register on).\n")
                   _T("Behaviours worth knowing: Unregister throws away the internal ")
                   _T("implementation object along with the callbacks you installed, so they ")
                   _T("must be reinstalled before registering again; once the ")
                   _T("point-carrying files callback is installed, a drop invokes only that one ")
                   _T("and never the older point-less variant; after a completed drop the ")
                   _T("drag-leave callback fires once as well, so hiding the hover hint only ")
                   _T("has to be written in that one place; and Register fails when the window ")
                   _T("already has a drop target, in which case dropping into the control does ")
                   _T("not work while everything else is unaffected.")));

    // ---- 段落二：拖真文件进来 ----------------------------------------
    AddSection(page.get(),
               Txt(_T("拖真文件进来，看四个回调的时序"),
                   _T("Drag real files in and watch the four callbacks")),
               Txt(_T("从资源管理器里拖一个或几个文件到下面这块区域：拖进来时它变绿并显示 ")
                   _T("坐标，光标移动时坐标跟着变，拖出去时恢复原样，松手时下面列出全部文件的 ")
                   _T("完整路径。时序框里记着最近几条回调，注意落手之后还会补一条 DragLeave。"),
                   _T("Drag one or more files from the file manager onto the area below: it ")
                   _T("turns green and shows the coordinates on entry, the coordinates follow ")
                   _T("the cursor, it reverts when the drag leaves, and on release the full ")
                   _T("paths are listed underneath. The trace box keeps the last few callbacks; ")
                   _T("note the extra DragLeave that arrives after a drop.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DropZone> zone(new DropZone(
            DropZone::ModeAcceptAll,
            Txt(_T("把文件或图片拖到这里"), _T("Drop files or an image here"))));
        g_drop.mainZone = zone.get();

        std::unique_ptr<BitmapPreview> preview(new BitmapPreview());
        g_drop.mainZone->SetPreview(preview.get());

        row->AddChild(std::move(zone), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(preview), DuiLayout::Hint().Fixed(kPreviewW));
        AddVariantRow(page.get(), std::move(row), kDropZoneRowH);
    }
    {
        std::unique_ptr<DuiLabel> log =
            MakeBlock(Txt(_T("（回调时序会显示在这里）"), _T("(the callback trace shows up here)")),
                      kCodeTextColor);
        g_drop.mainZone->SetLogLabel(log.get());
        AddLabelRow(page.get(), std::move(log), kDropLogRowH);
    }
    {
        std::unique_ptr<DuiLabel> result =
            MakeBlock(Txt(_T("（还没有落手）"), _T("(nothing dropped yet)")), kResultTextColor);
        g_drop.mainZone->SetResultLabel(result.get());
        AddLabelRow(page.get(), std::move(result), kDropResultRowH);
    }

    // ---- 段落三：合成事件 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("不用鼠标也能走一次完整拖放"),
                   _T("A full drop without touching the mouse")),
               Txt(_T("拖放没法用键盘触发，截图与录屏环境里也没有人真的去拖一个文件。")
                   _T("好在拖放接口本身就是几个普通的方法调用，自己造一份只提供文件列表的数据 ")
                   _T("对象，把 DragEnter、DragOver、Drop 依次调一遍，效果与真拖放完全一样 —— ")
                   _T("上面的投放区会变色、时序框会记满、文件路径会列出来。balloonui 的单元 ")
                   _T("测试就是这么测这个模块的。点下面的按钮试试。"),
                   _T("A drop cannot be triggered from the keyboard, and nobody drags a real ")
                   _T("file inside a screenshot or screen-recording run. Luckily the drop ")
                   _T("interface is nothing but ordinary method calls: build a data object that ")
                   _T("offers a file list, call DragEnter, DragOver and Drop in turn, and the ")
                   _T("effect is indistinguishable from a real drag — the zone above changes ")
                   _T("color, the trace box fills up, the paths get listed. This is exactly how ")
                   _T("balloonui's own unit tests exercise the module. Press the button below.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> synthetic(new FnButton());
        synthetic->SetText(Txt(_T("合成一次拖放（两个假文件）"),
                               _T("Synthesize a drop (two fake files)")));
        synthetic->onClick = &OnDropSyntheticClicked;

        std::unique_ptr<FnButton> reject = MakeCheckButton(
            Txt(_T("整体拒收（拖入回调返回假）"), _T("Reject everything (enter returns false)")),
            false);
        reject->onClick = &OnDropRejectClicked;

        row->AddChild(std::move(synthetic), DuiLayout::Hint().Fixed(kDropButtonW));
        row->AddChild(std::move(reject), DuiLayout::Hint().Fixed(kDropButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> note = MakeBlock(
            Txt(_T("勾上「整体拒收」之后再拖一次：光标显示禁止符，悬停回调一次都不会来 ")
                _T("（拖入已经判定为拒收，库里就不再为一次收不下的拖动去打扰调用方），")
                _T("松手也什么都不会发生 —— 但拖离回调仍然会来一次。"),
                _T("Tick \"Reject everything\" and drag again: the cursor shows the no-drop ")
                _T("sign, the drag-over callback never fires at all (the drag was already ")
                _T("judged as rejected, and the library will not bother the caller for a drag ")
                _T("it cannot accept), and releasing does nothing — yet the drag-leave callback ")
                _T("still arrives once.")),
            kWarnTextColor);
        AddLabelRow(page.get(), std::move(note), kNoteRowH);
    }

    // ---- 段落四：左右分半 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("同一块区域里逐帧改判：只有左半边能放"),
                   _T("Re-judging frame by frame: only the left half accepts")),
               Txt(_T("光标在同一个窗口里移动是不会再触发拖入回调的，所以「窗口里只有 ")
                   _T("一块地方收拖放」这种需求只能靠悬停回调逐帧改判。下面这块区域的悬停回调 ")
                   _T("按光标在竖线左边还是右边返回真假：拖着文件从左往右划过去，边框会在中线 ")
                   _T("处由绿变红，光标也跟着从可放变成禁止符。"),
                   _T("Moving the cursor inside one window does not fire the drag-enter callback ")
                   _T("again, so \"only one spot in this window accepts drops\" has to be ")
                   _T("re-judged frame by frame in the drag-over callback. The zone below ")
                   _T("returns true or false depending on which side of the divider the cursor ")
                   _T("is on: drag a file across from left to right and the border flips from ")
                   _T("green to red at the middle, with the cursor changing accordingly.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DropZone> zone(new DropZone(
            DropZone::ModeLeftHalfOnly,
            Txt(_T("左半边可以放，右半边不能放"),
                _T("Left half accepts, right half does not"))));
        row->AddChild(std::move(zone), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kDropZoneRowH);
    }

    // ---- 段落五：位图拖入 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("拖入位图的预览"), _T("Previewing a dropped bitmap")),
               Txt(_T("从看图软件或画图程序里把一张图片拖进上面那块主投放区，右边的方框会把它 ")
                   _T("等比缩小显示出来。这里有一个必须注意的所有权问题：位图回调收到的句柄 ")
                   _T("不归调用方所有 —— 回调返回之后库里会把系统交来的这块数据释放掉，")
                   _T("直接把句柄存下来，之后拿到的就是一个已经失效的句柄。本页面的做法是在 ")
                   _T("回调里当场用 CopyImage 复制一份（带 LR_CREATEDIBSECTION，")
                   _T("强制真的复制而不是把原句柄退回来），预览控件持有这一份副本并在析构时释放。"),
                   _T("Drag an image from a viewer or a paint program onto the main zone above ")
                   _T("and the box on the right scales it down for preview. There is an ")
                   _T("ownership point to watch: the handle the bitmap callback receives does ")
                   _T("not belong to the caller — after the callback returns, the library ")
                   _T("releases the medium the system handed over, so storing the handle leaves ")
                   _T("you with a dead one. This page copies it on the spot with CopyImage plus ")
                   _T("LR_CREATEDIBSECTION (which forces a real copy instead of returning the ")
                   _T("original handle), and the preview control owns and frees that copy.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 资源与皮肤（DuiResMgr、CSkinManager、CImageEx）
// =====================================================================

namespace {

// ---- 素材路径 -------------------------------------------------------
//
// 全部写成相对路径，相对画廊可执行文件的运行目录。这套素材来自 PC 客户端的
// 皮肤包，画廊运行目录里目前<u>没有</u>，缺失时页面会显示说明文字而不是崩溃。

// 皮肤包的根目录。CSkinManager::SetSkinPath 收的就是这个，它会在后面接上
// SkinConfig.xml 去读皮肤清单，所以末尾的反斜杠不能省。
LPCTSTR kSkinRootPath = _T("Skins\\");
// 引用计数演示用的图片文件名。这个名字要交给皮肤管理器，由它拼上当前皮肤的
// 目录变成完整路径，所以这里只写文件名。
LPCTSTR kSkinImageName = _T("DefaultFace.png");
// 灰度演示用的图片，直接用 CImageEx 加载，所以要写完整的相对路径。
LPCTSTR kFaceImagePath = _T("Skins\\Skin0\\DefaultFace.png");
// 九宫格演示用的按钮皮肤图，同样直接加载。原图 94 x 30。
LPCTSTR kButtonImagePath = _T("Skins\\Skin0\\Button\\btn_normal.png");

// 九宫格演示里四边不参与拉伸的边宽（像素）。按钮皮肤图的圆角与描边大致占这么宽，
// 取这个值四角才不会被拉花。
const int kNinePartInset = 8;

// ---- 版面尺寸（像素）-----------------------------------------------

// 图片演示格子那一行的高度。
const int kImageRowH = 92;
// 原图与灰度图那两个格子的宽度。原图 64 x 64，留出四边空白。
const int kImageCellW = 96;
// 拉伸演示那两个格子的宽度。故意比原图宽很多，四角变形才看得出来。
const int kStretchCellW = 240;
// 图片演示行里那几个说明标签的宽度。
const int kImageLabelW = 152;
// 字体演示格子那一行的高度。里面竖着摞五行示例文字。
const int kFontRowH = 148;
// 资源页面上按钮的宽度。
const int kResButtonW = 232;
// 资源页面上文案较长的按钮的宽度。
const int kResWideButtonW = 360;

// ---- 模拟 DPI 切换用的两个取值 ---------------------------------------

// 基准 DPI。Windows 上 100% 缩放就是这个值。
const int kBaseDpi = 96;
// 切换到的高 DPI。150% 缩放对应的值，字号差别一眼能看出来。
const int kHighDpi = 144;

// 字体演示里列出的几个磅值。9 磅是库里默认正文字号，其余三档用来看缓存。
const int kSampleFontSizes[] = { 9, 11, 14, 18 };
// 字体演示里列出几个磅值。
const int kSampleFontSizeCount =
    (int)(sizeof(kSampleFontSizes) / sizeof(kSampleFontSizes[0]));

// 直接用 CImageEx 绘制一张图片的演示格子。
//
// 这里刻意<u>不</u>走 DuiResMgr / CSkinManager，各格子各自加载一份自己的
// CImageEx：SetNinePart 是记在图片对象上的状态，GrayScale 更是直接改像素，
// 两者都会影响到共用同一份缓存的所有使用者。演示要并排展示几种效果，只能
// 各用各的副本。
class ImageDemoBox : public DuiControl
{
public:
    // 这一格用哪种画法。
    enum DrawMode
    {
        // 按原始尺寸画在格子正中。
        DrawOriginal = 0,
        // 直接拉伸铺满整格，四角跟着一起变形。
        DrawStretched = 1,
        // 设过九宫格内距之后再铺满整格，四角保持原样、只有中间被拉伸。
        DrawNinePart = 2,
        // 灰度化之后的副本，按原始尺寸画在格子正中。
        DrawGray = 3,
    };

    // 构造一个图片演示格子。
    //   mode：这一格用哪种画法。
    //   path：图片文件路径，相对可执行文件的运行目录。
    //   ninePartInset：九宫格四边不参与拉伸的边宽（像素），只有 DrawNinePart
    //                  这一档用得上。
    ImageDemoBox(DrawMode mode, LPCTSTR path, int ninePartInset)
        : m_mode(mode)
        , m_path(path != NULL ? path : _T(""))
        , m_loaded(false)
    {
        SetTabStop(false);
        m_loaded = (m_image.LoadFromFile(m_path) != FALSE);
        if (!m_loaded)
        {
            return;
        }
        if (m_mode == DrawNinePart)
        {
            RECT insets;
            insets.left = ninePartInset;
            insets.top = ninePartInset;
            insets.right = ninePartInset;
            insets.bottom = ninePartInset;
            m_image.SetNinePart(&insets);
        }
        if (m_mode == DrawGray)
        {
            // 灰度化直接改的是这份副本的像素，一次性的、不可撤销。
            m_image.GrayScale();
        }
    }

    // 绘制格子。素材缺失时画一行说明文字，不崩溃、也不留空白让人以为坏了。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }
        DuiVBox::PaintBackground(hdc, m_rcItem, kDemoWhite, kDemoCornerRadius,
                                 kDemoBorderColor, 1.0f);

        if (!m_loaded)
        {
            CString text;
            text.Format(Txt(_T("素材缺失：%s"), _T("asset missing: %s")), (LPCTSTR)m_path);
            int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldTextColor = ::SetTextColor(hdc, kWarnTextColor);
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = (useFont != NULL) ? (HFONT)::SelectObject(hdc, useFont) : NULL;
            RECT rcText = m_rcItem;
            ::DrawText(hdc, text, -1, &rcText,
                       DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
            if (oldFont != NULL)
            {
                ::SelectObject(hdc, oldFont);
            }
            ::SetTextColor(hdc, oldTextColor);
            ::SetBkMode(hdc, oldBkMode);
            return;
        }

        // 四边各留一点空白，图片不至于贴着格子的描边。
        const int kImagePadding = 6;
        RECT rcTarget = m_rcItem;
        ::InflateRect(&rcTarget, -kImagePadding, -kImagePadding);
        if (::IsRectEmpty(&rcTarget))
        {
            return;
        }

        switch (m_mode)
        {
        //原图与灰度图都按原始尺寸画在正中，两者并排才能看出灰度化改了什么。
        case DrawOriginal:
        case DrawGray:
            {
                POINT center = RectCenter(rcTarget);
                RECT rcNatural;
                rcNatural.left = center.x - m_image.GetWidth() / 2;
                rcNatural.top = center.y - m_image.GetHeight() / 2;
                rcNatural.right = rcNatural.left + m_image.GetWidth();
                rcNatural.bottom = rcNatural.top + m_image.GetHeight();
                m_image.Draw(hdc, rcNatural);
            }
            break;

        //直接拉伸：整张图连同四角一起被拉到目标尺寸，圆角与描边跟着变形。
        case DrawStretched:
            m_image.Draw(hdc, rcTarget);
            break;

        //九宫格：Draw2 发现目标尺寸与原图不同，就按四边内距把图切成九块，
        //四角原样贴、四边单向拉、中间双向拉，圆角与描边保持不变形。
        case DrawNinePart:
            m_image.Draw2(hdc, rcTarget);
            break;

        //枚举已经穷举完，这里不会走到；留一个空分支避免编译器报缺失分支。
        default:
            break;
        }
    }

private:
    // 本格子自己的图片副本。九宫格内距与灰度化都只影响这一份。
    CImageEx m_image;
    // 这一格用哪种画法。
    DrawMode m_mode;
    // 图片路径。加载失败时要显示出来，所以留一份。
    CString m_path;
    // 图片有没有加载成功。为假时只画说明文字。
    bool m_loaded;
};

// 共享字体缓存的演示格子：用默认字体与几个指定磅值的字体各画一行示例文字。
//
// 每次绘制都重新向 DuiResMgr 要一次字体句柄，所以模拟 DPI 切换之后立刻
// 就能看到整组字号一起变大或变小。
class FontDemoBox : public DuiControl
{
public:
    FontDemoBox()
    {
        SetTabStop(false);
    }

    // 绘制：第一行用默认字体，随后每行用一个指定磅值的字体。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重绘的区域，与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }
        DuiVBox::PaintBackground(hdc, m_rcItem, kDemoWhite, kDemoCornerRadius,
                                 kDemoBorderColor, 1.0f);

        DuiResMgr& resMgr = DuiResMgr::Inst();
        int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldTextColor = ::SetTextColor(hdc, kOnLightTextColor);

        // 每行占多高。行数是「默认字体一行 + 各磅值各一行」。
        const int kLinePadding = 6;
        int lineCount = kSampleFontSizeCount + 1;
        int lineHeight = ((m_rcItem.bottom - m_rcItem.top) - kLinePadding * 2) / lineCount;
        int y = m_rcItem.top + kLinePadding;

        DrawOneLine(hdc, y, lineHeight,
                    resMgr.GetDefaultFont(),
                    Txt(_T("GetDefaultFont（微软雅黑 9 磅）"),
                        _T("GetDefaultFont (Microsoft YaHei, 9pt)")));
        y += lineHeight;

        for (int i = 0; i < kSampleFontSizeCount; ++i)
        {
            // 最后一档顺便演示加粗那一路，缓存的键是「磅值 + 粗细」两项。
            bool bold = (i == kSampleFontSizeCount - 1);
            CString caption;
            caption.Format(bold
                               ? Txt(_T("GetFontByPointSize(%d, 加粗)"),
                                     _T("GetFontByPointSize(%d, bold)"))
                               : Txt(_T("GetFontByPointSize(%d)"),
                                     _T("GetFontByPointSize(%d)")),
                           kSampleFontSizes[i]);
            DrawOneLine(hdc, y, lineHeight,
                        resMgr.GetFontByPointSize(kSampleFontSizes[i], bold),
                        caption);
            y += lineHeight;
        }

        ::SetTextColor(hdc, oldTextColor);
        ::SetBkMode(hdc, oldBkMode);
    }

private:
    // 用指定字体画一行文字。
    //   hdc：目标设备上下文，调用方已经设好背景模式与文字色。
    //   top：这一行的顶边（宿主客户区坐标）。
    //   height：这一行的高度（像素）。
    //   hFont：这一行用的字体。所有权归 DuiResMgr，本函数只借用、不释放。
    //   text：要画的文字。
    void DrawOneLine(HDC hdc, int top, int height, HFONT hFont, LPCTSTR text)
    {
        // 左边留一点空白，文字不至于贴着格子的描边。
        const int kTextIndent = 10;
        RECT rcLine;
        rcLine.left = m_rcItem.left + kTextIndent;
        rcLine.top = top;
        rcLine.right = m_rcItem.right - kTextIndent;
        rcLine.bottom = top + height;

        HFONT oldFont = (hFont != NULL) ? (HFONT)::SelectObject(hdc, hFont) : NULL;
        ::DrawText(hdc, text, -1, &rcLine,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont != NULL)
        {
            ::SelectObject(hdc, oldFont);
        }
    }
};

// 资源页面在运行期需要回头改动的那几个控件。页面被销毁后这些指针全部失效，
// 所以每次构建页面的第一件事就是把整块清空。
struct ResourcePageState
{
    // 显示皮肤初始化四步结果的标签。
    DuiLabel* initLabel;
    // 显示皮肤清单与当前皮肤的标签。
    DuiLabel* skinListLabel;
    // 显示引用计数实验结果的标签。
    DuiLabel* refCountLabel;
    // 显示字体句柄与当前 DPI 的标签。
    DuiLabel* fontLabel;
    // 字体演示格子。模拟 DPI 切换之后要让它重画。
    FontDemoBox* fontBox;
};

ResourcePageState g_resource;

// 把一个指针格式化成便于对照的字符串。
//   pointer：任意指针，可为空。
// 返回：形如 "0x00A1B2C3" 的字符串；空指针返回一段说明文字。
// 引用计数那一段全靠对照地址是否相同来说明问题，所以要显示出来。
CString FormatPointer(const void* pointer)
{
    if (pointer == NULL)
    {
        return CString(Txt(_T("空指针"), _T("null")));
    }
    CString text;
    text.Format(_T("%p"), pointer);
    return text;
}

// 「探测：不初始化直接取图」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceProbeClicked(FnButton* /*pButton*/)
{
    if (g_resource.initLabel == NULL)
    {
        return;
    }
    CImageEx* pImage = DuiResMgr::Inst().LoadImage(kSkinImageName);
    CString text;
    text.Format(Txt(_T("此刻 CSkinManager::GetInstance() = %s，")
                    _T("DuiResMgr::LoadImage(\"%s\") = %s。"),
                    _T("Right now CSkinManager::GetInstance() = %s and ")
                    _T("DuiResMgr::LoadImage(\"%s\") = %s.")),
                (LPCTSTR)FormatPointer(CSkinManager::GetInstance()),
                kSkinImageName,
                (LPCTSTR)FormatPointer(pImage));
    if (pImage != NULL)
    {
        // 探测本身也会让引用计数加一，取到了就立刻还回去，别影响后面那一段。
        DuiResMgr::Inst().ReleaseImage(pImage);
    }
    g_resource.initLabel->SetText(text);
}

// 「执行皮肤初始化四步」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceInitClicked(FnButton* /*pButton*/)
{
    if (g_resource.initLabel == NULL)
    {
        return;
    }
    CString report;

    // 第一步：建皮肤管理器单例。CSkinManager::Init 每次调用都会 new 一个新的
    // 实例并覆盖静态指针，上一份连同它缓存的图片就再也没人释放了，所以这里
    // 先判断有没有建过。
    if (CSkinManager::GetInstance() == NULL)
    {
        BOOL created = CSkinManager::Init();
        report.AppendFormat(_T("CSkinManager::Init() = %d\n"), (int)created);
    }
    else
    {
        report += Txt(_T("CSkinManager::Init()：之前已经建过，跳过\n"),
                      _T("CSkinManager::Init(): already created, skipped\n"));
    }

    CSkinManager* pManager = CSkinManager::GetInstance();
    if (pManager == NULL)
    {
        report += Txt(_T("单例仍然为空，后面三步没法继续。"),
                      _T("The singleton is still null; the remaining steps cannot run."));
        g_resource.initLabel->SetText(report);
        return;
    }

    // 第二步：告诉它皮肤包在哪儿。
    pManager->SetSkinPath(kSkinRootPath);
    report.AppendFormat(_T("SetSkinPath(\"%s\")\n"), kSkinRootPath);

    // 第三步：读皮肤清单。读的是 <皮肤包根目录>SkinConfig.xml。
    BOOL configLoaded = pManager->LoadConfigXml();
    report.AppendFormat(_T("LoadConfigXml() = %d%s\n"),
                        (int)configLoaded,
                        configLoaded
                            ? _T("")
                            : Txt(_T("（多半是素材没拷到运行目录）"),
                                  _T(" (the assets are probably missing)")));

    // 第四步：切到某一套皮肤。
    const int kDefaultSkinId = 0;
    BOOL skinSelected = pManager->SetCurSkin(kDefaultSkinId);
    report.AppendFormat(_T("SetCurSkin(%d) = %d\n"), kDefaultSkinId, (int)skinSelected);

    // 四步走完再探测一次，与初始化之前的结果对照。
    CImageEx* pImage = DuiResMgr::Inst().LoadImage(kSkinImageName);
    report.AppendFormat(Txt(_T("现在 DuiResMgr::LoadImage(\"%s\") = %s"),
                            _T("Now DuiResMgr::LoadImage(\"%s\") = %s")),
                        kSkinImageName,
                        (LPCTSTR)FormatPointer(pImage));
    if (pImage != NULL)
    {
        DuiResMgr::Inst().ReleaseImage(pImage);
    }

    g_resource.initLabel->SetText(report);
}

// 「列出皮肤清单」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceListSkinsClicked(FnButton* /*pButton*/)
{
    if (g_resource.skinListLabel == NULL)
    {
        return;
    }
    CSkinManager* pManager = CSkinManager::GetInstance();
    if (pManager == NULL)
    {
        g_resource.skinListLabel->SetText(
            Txt(_T("皮肤管理器还没有初始化，先点上一段那个初始化按钮。"),
                _T("The skin manager is not initialized yet; press the init button above.")));
        return;
    }

    std::vector<SKIN_INFO*>& skins = pManager->GetSkinList();
    if (skins.empty())
    {
        g_resource.skinListLabel->SetText(
            Txt(_T("皮肤清单是空的：SkinConfig.xml 没读到，或者里面一个 Skin 节点都没有。"),
                _T("The skin list is empty: SkinConfig.xml was not read, or it holds no ")
                _T("Skin nodes.")));
        return;
    }

    CString report;
    report.AppendFormat(Txt(_T("共 %d 套皮肤：\n"), _T("%d skin(s):\n")), (int)skins.size());
    for (size_t i = 0; i < skins.size(); ++i)
    {
        if (skins[i] == NULL)
        {
            continue;
        }
        report.AppendFormat(_T("  SkinID=%d  SkinName=%s  SkinPath=%s\n"),
                            skins[i]->nSkinID,
                            (LPCTSTR)skins[i]->strSkinName,
                            (LPCTSTR)skins[i]->strSkinPath);
    }
    g_resource.skinListLabel->SetText(report);
}

// 「切到第二套皮肤」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceSwitchSkinClicked(FnButton* /*pButton*/)
{
    if (g_resource.skinListLabel == NULL)
    {
        return;
    }
    CSkinManager* pManager = CSkinManager::GetInstance();
    if (pManager == NULL)
    {
        g_resource.skinListLabel->SetText(
            Txt(_T("皮肤管理器还没有初始化，先点上一段那个初始化按钮。"),
                _T("The skin manager is not initialized yet; press the init button above.")));
        return;
    }

    const int kSecondSkinId = 1;
    // 一个不存在的皮肤编号，用来说明 SetCurSkin 的返回值靠不住。
    const int kMissingSkinId = 999;
    BOOL switched = pManager->SetCurSkin(kSecondSkinId);
    BOOL switchedToMissing = pManager->SetCurSkin(kMissingSkinId);
    // 试完那个不存在的编号之后要切回来，否则后面按文件名取图会拼不出路径。
    pManager->SetCurSkin(kSecondSkinId);

    CString report;
    report.AppendFormat(Txt(_T("SetCurSkin(%d) = %d，SetCurSkin(%d)（这个编号不存在）= %d。\n")
                            _T("两个返回值一样，是因为这个函数校验的是<切换前>的当前编号")
                            _T("而不是传进来的那个 —— 只要当前编号有效，传什么都返回真，")
                            _T("并且照样把当前编号改成传进来的值。"),
                            _T("SetCurSkin(%d) = %d, SetCurSkin(%d) (a nonexistent id) = %d.\n")
                            _T("Both return the same value because the function validates the ")
                            _T("id that was current *before* the call, not the one passed in — ")
                            _T("as long as the current id is valid it returns true for any ")
                            _T("argument, and still overwrites the current id with it.")),
                        kSecondSkinId, (int)switched,
                        kMissingSkinId, (int)switchedToMissing);
    g_resource.skinListLabel->SetText(report);
}

// 「跑一遍引用计数」按钮的响应。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceRefCountClicked(FnButton* /*pButton*/)
{
    if (g_resource.refCountLabel == NULL)
    {
        return;
    }
    DuiResMgr& resMgr = DuiResMgr::Inst();

    CImageEx* pFirst = resMgr.AcquireImage(kSkinImageName);
    if (pFirst == NULL)
    {
        g_resource.refCountLabel->SetText(
            Txt(_T("取不到图片：皮肤管理器没初始化，或者素材不在运行目录里。"),
                _T("Could not get the image: the skin manager is not initialized, or the ")
                _T("asset is not in the run directory.")));
        return;
    }
    CImageEx* pSecond = resMgr.AcquireImage(kSkinImageName);
    CImageEx* pThird = resMgr.AcquireImage(kSkinImageName);

    // 释放接口会把调用方的指针一并置空，所以三个地址都要先留一份下来对照。
    const void* address1 = pFirst;
    const void* address2 = pSecond;
    const void* address3 = pThird;
    resMgr.ReleaseImage(pFirst);
    resMgr.ReleaseImage(pSecond);
    resMgr.ReleaseImage(pThird);

    // 三次释放之后底层对象应当已经销毁。再取一次拿到的如果是新建的另一个
    // 对象，地址就会与之前不同 —— 这是能从外部观察到的销毁证据。
    CImageEx* pAfter = resMgr.AcquireImage(kSkinImageName);
    const void* addressAfter = pAfter;
    bool sameAsBefore = (addressAfter == address1);

    CString report;
    report.AppendFormat(Txt(_T("连取三次拿到的地址：%s / %s / %s。\n")
                            _T("释放三次之后再取一次：%s。\n")
                            _T("最后这个地址与最初%s，说明第三次释放时底层对象%s。"),
                            _T("Three acquires returned: %s / %s / %s.\n")
                            _T("After three releases, acquiring again gives: %s.\n")
                            _T("That last address %s the first one, which means the third ")
                            _T("release %s.")),
                        (LPCTSTR)FormatPointer(address1),
                        (LPCTSTR)FormatPointer(address2),
                        (LPCTSTR)FormatPointer(address3),
                        (LPCTSTR)FormatPointer(addressAfter),
                        sameAsBefore ? Txt(_T("相同"), _T("matches"))
                                     : Txt(_T("不同"), _T("differs from")),
                        sameAsBefore
                            ? Txt(_T("要么没有销毁，要么销毁后这块内存又被分配回来了"),
                                  _T("either did not destroy it, or the allocator handed the ")
                                  _T("same block back"))
                            : Txt(_T("确实把对象销毁了"), _T("really destroyed the object")));

    // 收尾：刚才为了对照又取了一次，还回去，保持引用计数平衡。
    if (pAfter != NULL)
    {
        resMgr.ReleaseImage(pAfter);
    }
    g_resource.refCountLabel->SetText(report);
}

// 按当前状态刷新字体那一段的说明文字。
void RefreshFontReport()
{
    if (g_resource.fontLabel == NULL)
    {
        return;
    }
    DuiResMgr& resMgr = DuiResMgr::Inst();
    // 先要一次默认字体：DuiResMgr 的 DPI 在第一次建字体时才会被填上，
    // 不先要一次就会读到还没初始化的 0。
    HFONT hDefault = resMgr.GetDefaultFont();

    CString report;
    report.AppendFormat(Txt(_T("当前 DuiResMgr 的 DPI = %d。\n")
                            _T("默认字体句柄 = %s，11 磅 = %s，18 磅加粗 = %s。\n")
                            _T("同一个磅值连要两次拿到的是同一个句柄：%s。"),
                            _T("DuiResMgr's current DPI = %d.\n")
                            _T("Default font handle = %s, 11pt = %s, 18pt bold = %s.\n")
                            _T("Asking twice for the same point size returns one handle: %s.")),
                        resMgr.GetDpi(),
                        (LPCTSTR)FormatPointer(hDefault),
                        (LPCTSTR)FormatPointer(resMgr.GetFontByPointSize(11, false)),
                        (LPCTSTR)FormatPointer(resMgr.GetFontByPointSize(18, true)),
                        (resMgr.GetFontByPointSize(11, false)
                             == resMgr.GetFontByPointSize(11, false))
                            ? Txt(_T("是"), _T("yes"))
                            : Txt(_T("否"), _T("no")));
    g_resource.fontLabel->SetText(report);
}

// 「模拟 DPI 切换」按钮的响应：在基准 DPI 与高 DPI 之间来回切。
//   pButton：被点击的按钮，本函数不使用。
void OnResourceDpiClicked(FnButton* /*pButton*/)
{
    DuiResMgr& resMgr = DuiResMgr::Inst();
    // 先要一次默认字体，把 DPI 字段填上，否则第一次点的时候读到的是 0。
    resMgr.GetDefaultFont();
    int next = (resMgr.GetDpi() == kHighDpi) ? kBaseDpi : kHighDpi;
    resMgr.SetDpi(next);

    RefreshFontReport();
    if (g_resource.fontBox != NULL)
    {
        g_resource.fontBox->Invalidate();
    }
    // 字号一变，整个窗口里所有用共享字体的控件都得重画。
    InvalidateWholeHost(g_resource.fontBox);
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_Resource()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    g_resource.initLabel = NULL;
    g_resource.skinListLabel = NULL;
    g_resource.refCountLabel = NULL;
    g_resource.fontLabel = NULL;
    g_resource.fontBox = NULL;

    // ---- 段落一：不初始化就全返空 ------------------------------------
    AddSection(page.get(),
               Txt(_T("图片三件套在皮肤管理器初始化之前一律返回空指针"),
                   _T("The image trio returns null until the skin manager is initialized")),
               Txt(_T("DuiResMgr 管两样东西：皮肤图与共享字体。皮肤图这三个方法 ")
                   _T("（LoadImage / GetImage / ReleaseImage）全部透传给老的 CSkinManager，")
                   _T("而 LoadImage 的第一行就是取皮肤管理器单例，取不到直接返回空指针。")
                   _T("画廊从来没有调过 CSkinManager::Init()，所以现在点左边那个按钮，")
                   _T("拿到的必然是空指针。点右边那个按钮把四步初始化跑一遍，再点左边，")
                   _T("结果就不一样了。\n")
                   _T("字体那一半并不依赖皮肤管理器 —— GetDefaultFont 与 ")
                   _T("GetFontByPointSize 自己 CreateFontIndirect，什么都不用先初始化。"),
                   _T("DuiResMgr covers two things: skin images and shared fonts. The three ")
                   _T("image methods (LoadImage / GetImage / ReleaseImage) all forward to the ")
                   _T("older CSkinManager, and the very first line of LoadImage fetches the skin ")
                   _T("manager singleton, returning null when there is none. The gallery has ")
                   _T("never called CSkinManager::Init(), so pressing the left button right now ")
                   _T("is guaranteed to yield null. Press the right button to run the four ")
                   _T("initialization steps, then press the left one again for a different ")
                   _T("answer.\n")
                   _T("The font half does NOT depend on the skin manager — GetDefaultFont and ")
                   _T("GetFontByPointSize call CreateFontIndirect themselves and need no setup ")
                   _T("at all.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> probe(new FnButton());
        probe->SetText(Txt(_T("探测：现在取一张图看看"), _T("Probe: fetch an image now")));
        probe->onClick = &OnResourceProbeClicked;

        std::unique_ptr<FnButton> init(new FnButton());
        init->SetText(Txt(_T("执行皮肤初始化四步"), _T("Run the four init steps")));
        init->onClick = &OnResourceInitClicked;

        row->AddChild(std::move(probe), DuiLayout::Hint().Fixed(kResButtonW));
        row->AddChild(std::move(init), DuiLayout::Hint().Fixed(kResButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> result =
            MakeBlock(Txt(_T("（还没有探测过）"), _T("(nothing probed yet)")), kResultTextColor);
        g_resource.initLabel = result.get();
        AddLabelRow(page.get(), std::move(result), kLongOutputRowH);
    }

    // ---- 段落二：皮肤清单与切换 --------------------------------------
    AddSection(page.get(),
               Txt(_T("皮肤清单与切换"), _T("The skin list, and switching skins")),
               Txt(_T("LoadConfigXml 读的是「皮肤包根目录 + SkinConfig.xml」，把里面每个 Skin ")
                   _T("节点的编号、名称、目录读进一张清单，同时把 CurSkinID 记下来作为当前皮肤。")
                   _T("之后按文件名取图时，GetAbsolutePath 会拿当前皮肤的目录去拼完整路径。\n")
                   _T("SetCurSkin 有一处需要留意：它校验的是<切换前>的当前编号有没有对应的皮肤，")
                   _T("而不是传进来的那个编号。因此只要当前编号有效，传一个根本不存在的编号 ")
                   _T("也会返回真，并且照样把当前编号改成那个不存在的值 —— 之后所有按文件名 ")
                   _T("取图都会拼出空路径。点第二个按钮可以看到这个结果。"),
                   _T("LoadConfigXml reads \"<skin root> + SkinConfig.xml\", collecting each ")
                   _T("Skin node's id, name and directory into a list, and remembers CurSkinID ")
                   _T("as the current skin. Later, when an image is requested by file name, ")
                   _T("GetAbsolutePath prepends the current skin's directory.\n")
                   _T("SetCurSkin has a wrinkle worth noting: it validates the id that was ")
                   _T("current *before* the call, not the one passed in. So as long as the ")
                   _T("current id is valid, passing an id that does not exist still returns ")
                   _T("true — and still overwrites the current id with it, after which every ")
                   _T("image lookup by file name resolves to an empty path. The second button ")
                   _T("shows this.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> list(new FnButton());
        list->SetText(Txt(_T("列出皮肤清单"), _T("List the skins")));
        list->onClick = &OnResourceListSkinsClicked;

        std::unique_ptr<FnButton> switchSkin(new FnButton());
        switchSkin->SetText(Txt(_T("切换皮肤（含不存在的编号）"),
                                _T("Switch skin (incl. a bogus id)")));
        switchSkin->onClick = &OnResourceSwitchSkinClicked;

        row->AddChild(std::move(list), DuiLayout::Hint().Fixed(kResButtonW));
        row->AddChild(std::move(switchSkin), DuiLayout::Hint().Fixed(kResButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> result =
            MakeBlock(Txt(_T("（还没有列过）"), _T("(not listed yet)")), kResultTextColor);
        g_resource.skinListLabel = result.get();
        AddLabelRow(page.get(), std::move(result), kOutputRowH);
    }

    // ---- 段落三：引用计数 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("图片的引用计数"), _T("Reference counting on images")),
               Txt(_T("皮肤管理器按文件名缓存图片，每命中一次缓存计数加一，每释放一次减一，")
                   _T("减到零就把底层对象销毁并从缓存里摘掉。释放接口收的是指针的引用，")
                   _T("释放完会把调用方的指针一并置空，免得留着一个悬空指针。\n")
                   _T("下面这个按钮连取三次、连放三次，再取一次做对照：如果第三次释放确实把 ")
                   _T("对象销毁了，那么最后这一次取到的必然是新建出来的另一个对象，地址与 ")
                   _T("之前不同。"),
                   _T("The skin manager caches images by file name, bumping a counter on every ")
                   _T("cache hit and dropping it on every release; at zero the underlying object ")
                   _T("is destroyed and removed from the cache. The release call takes the ")
                   _T("pointer by reference and nulls the caller's copy so no dangling pointer ")
                   _T("is left behind.\n")
                   _T("The button below acquires three times, releases three times, then ")
                   _T("acquires once more for comparison: if the third release really destroyed ")
                   _T("the object, this last acquire must produce a freshly built one at a ")
                   _T("different address.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> refCount(new FnButton());
        refCount->SetText(Txt(_T("跑一遍引用计数"), _T("Run the refcount experiment")));
        refCount->onClick = &OnResourceRefCountClicked;

        row->AddChild(std::move(refCount), DuiLayout::Hint().Fixed(kResButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> result =
            MakeBlock(Txt(_T("（还没有跑过）"), _T("(not run yet)")), kResultTextColor);
        g_resource.refCountLabel = result.get();
        AddLabelRow(page.get(), std::move(result), kOutputRowH);
    }

    // ---- 段落四：九宫格与灰度 ----------------------------------------
    AddSection(page.get(),
               Txt(_T("CImageEx 自带的九宫格与灰度处理"),
                   _T("Nine-part drawing and grayscale built into CImageEx")),
               Txt(_T("CImageEx 在 ATL 的 CImage 上加了三样东西：PNG 加载时做 alpha 预乘、")
                   _T("九宫格绘制（SetNinePart 设四边内距，Draw2 在目标尺寸与原图不同时按九块 ")
                   _T("画）、以及原地灰度化。下面第一行左边是原图，右边是灰度化之后的副本；")
                   _T("第二行左边是直接拉伸（四角跟着变形），右边是设过九宫格内距之后再拉伸 ")
                   _T("（四角保持原样）。\n")
                   _T("注意灰度化改的是图片对象自己的像素，一次性、不可撤销 —— 如果这份图片 ")
                   _T("是从皮肤管理器的缓存里取出来的，那所有共用这份缓存的地方都会跟着变灰。")
                   _T("所以这四个格子各自直接用 CImageEx 加载了一份自己的副本，没有走缓存。\n")
                   _T("库里的新代码画九宫格已经改用 DuiNinePatch 了，理由写在它的头文件里："),
                   _T("CImageEx adds three things on top of ATL's CImage: alpha premultiplication ")
                   _T("when loading PNGs, nine-part drawing (SetNinePart sets the four insets, ")
                   _T("Draw2 splits into nine pieces whenever the destination size differs from ")
                   _T("the source), and in-place grayscaling. The first row below shows the ")
                   _T("original on the left and a grayscaled copy on the right; the second row ")
                   _T("shows a plain stretch on the left (corners distort) and a nine-part ")
                   _T("stretch on the right (corners stay intact).\n")
                   _T("Note that grayscaling rewrites the image object's own pixels — one way, ")
                   _T("no undo. If that image came out of the skin manager's cache, every other ")
                   _T("user of the same cache entry turns gray with it. That is why these four ")
                   _T("cells each load their own copy directly through CImageEx instead of ")
                   _T("going through the cache.\n")
                   _T("New code in the library draws nine-patch bitmaps with DuiNinePatch ")
                   _T("instead; the reasons are stated in its header:")));
    {
        std::unique_ptr<DuiLabel> reason = MakeBlock(
            Txt(_T("一、CImageEx 绑死了磁盘图片缓存，单元测试里没法用；DuiNinePatch 直接吃 ")
                _T("HBITMAP，测试可以现搓一张位图画完再逐像素检查。\n")
                _T("二、老版只支持拉伸；DuiNinePatch 保留了绘制模式这个维度，将来加平铺 ")
                _T("不必改接口。此外它还支持源内距与目标内距分开指定，")
                _T("可以把源图上很高的装饰带压成较矮的标题栏。"),
                _T("1. CImageEx is welded to the on-disk image cache and cannot be used from a ")
                _T("unit test; DuiNinePatch takes an HBITMAP directly, so a test can synthesize ")
                _T("a bitmap, draw, and check pixels.\n")
                _T("2. The old code only stretches; DuiNinePatch keeps a draw-mode dimension so ")
                _T("a tiling mode can be added later without breaking the API. It also accepts ")
                _T("separate source and destination insets, which lets a tall decorative band in ")
                _T("the source render as a shorter title bar.")),
            kNoteTextColor);
        AddLabelRow(page.get(), std::move(reason), kReasonRowH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);
        row->AddChild(MakeLine(Txt(_T("原图"), _T("original")), kNoteTextColor),
                      DuiLayout::Hint().Fixed(kImageCellW));
        row->AddChild(std::unique_ptr<DuiControl>(new ImageDemoBox(
                          ImageDemoBox::DrawOriginal, kFaceImagePath, kNinePartInset)),
                      DuiLayout::Hint().Fixed(kImageCellW));
        row->AddChild(MakeLine(Txt(_T("灰度化之后"), _T("after GrayScale()")), kNoteTextColor),
                      DuiLayout::Hint().Fixed(kImageLabelW));
        row->AddChild(std::unique_ptr<DuiControl>(new ImageDemoBox(
                          ImageDemoBox::DrawGray, kFaceImagePath, kNinePartInset)),
                      DuiLayout::Hint().Fixed(kImageCellW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kImageRowH);
    }
    AddGap(page.get(), kInnerGap);
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);
        row->AddChild(MakeLine(Txt(_T("直接拉伸"), _T("plain stretch")), kNoteTextColor),
                      DuiLayout::Hint().Fixed(kImageCellW));
        row->AddChild(std::unique_ptr<DuiControl>(new ImageDemoBox(
                          ImageDemoBox::DrawStretched, kButtonImagePath, kNinePartInset)),
                      DuiLayout::Hint().Fixed(kStretchCellW));
        row->AddChild(MakeLine(Txt(_T("九宫格拉伸"), _T("nine-part stretch")), kNoteTextColor),
                      DuiLayout::Hint().Fixed(kImageLabelW));
        row->AddChild(std::unique_ptr<DuiControl>(new ImageDemoBox(
                          ImageDemoBox::DrawNinePart, kButtonImagePath, kNinePartInset)),
                      DuiLayout::Hint().Fixed(kStretchCellW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kImageRowH);
    }

    // ---- 段落五：共享字体缓存 ----------------------------------------
    AddSection(page.get(),
               Txt(_T("共享字体缓存与 DPI 变化"), _T("The shared font cache and DPI changes")),
               Txt(_T("DuiResMgr 把字体按「磅值 + 粗细」缓存起来，同一组参数无论要多少次拿到的 ")
                   _T("都是同一个句柄。这解决的是句柄泄漏：控件想要一个非默认字号的字体，")
                   _T("如果各自 CreateFontIndirect，用一次漏一个。拿到的句柄一律不要 ")
                   _T("DeleteObject，所有权在管理器手里。\n")
                   _T("宿主收到系统的 DPI 变化消息时会调 SetDpi，管理器把默认字体与整张缓存 ")
                   _T("一起清空、下次再要时按新 DPI 重建。下面的按钮就是模拟这一步。\n")
                   _T("这里有一个真实的坑要提醒：SetDpi 会把缓存里的字体句柄 DeleteObject 掉，")
                   _T("而 DuiLabel::SetFont 存的是句柄本身。谁把 GetFontByPointSize 的返回值 ")
                   _T("存下来了，SetDpi 之后那份句柄就失效了。画廊的段落标题恰好是这么存的，")
                   _T("所以点完下面这个按钮，本页面的段落标题字体会退回系统默认 —— 切到别的 ")
                   _T("页面再回来就恢复了，因为标题是在建页面时重新要的字体。"),
                   _T("DuiResMgr caches fonts by point size plus weight, so the same parameters ")
                   _T("always return the same handle. The problem it solves is handle leakage: ")
                   _T("if every control that wants a non-default size called ")
                   _T("CreateFontIndirect itself, each use would leak one. Do NOT DeleteObject ")
                   _T("the handle you get — the manager owns it.\n")
                   _T("When the host receives a DPI-change message it calls SetDpi, and the ")
                   _T("manager drops the default font and the whole cache, rebuilding lazily at ")
                   _T("the new DPI. The button below simulates that step.\n")
                   _T("One real hazard to flag: SetDpi calls DeleteObject on the cached handles, ")
                   _T("while DuiLabel::SetFont stores the handle itself. Anyone who kept a ")
                   _T("GetFontByPointSize result is left holding a dead handle. The gallery's ")
                   _T("own section titles do exactly that, so after pressing this button the ")
                   _T("section titles on this page fall back to the system default font — ")
                   _T("switching to another page and back restores them, because the titles ask ")
                   _T("for their font again when the page is built.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<FontDemoBox> fontBox(new FontDemoBox());
        g_resource.fontBox = fontBox.get();
        row->AddChild(std::move(fontBox), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kFontRowH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> dpiButton(new FnButton());
        dpiButton->SetText(Txt(_T("模拟 DPI 切换（96 与 144 来回切）"),
                               _T("Simulate a DPI change (96 / 144)")));
        dpiButton->onClick = &OnResourceDpiClicked;

        row->AddChild(std::move(dpiButton), DuiLayout::Hint().Fixed(kResWideButtonW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    {
        std::unique_ptr<DuiLabel> result = MakeBlock(_T(""), kResultTextColor);
        g_resource.fontLabel = result.get();
        AddLabelRow(page.get(), std::move(result), kOutputRowH);
    }
    RefreshFontReport();

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 本分组的页面列表
// =====================================================================

const PageEntry* GetToolPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("inspector"),   _T("DuiInspector　控件树查看器"),    _T("DuiInspector"),      &Build_Inspector,    true },
        { _T("xml-custom"),  _T("XML 自定义标签　CustomFactory"), _T("Custom XML Tags"),   &Build_XmlCustomTag, true },
        { _T("keyboard"),    _T("键盘可达性　助记符与焦点框"),    _T("Keyboard Access"),   &Build_Keyboard,     true },
        { _T("drop-target"), _T("DuiDropTarget　拖放接收"),       _T("DuiDropTarget"),     &Build_DropTarget,   true },
        { _T("resource"),    _T("资源与皮肤　DuiResMgr"),         _T("Resources & Skins"), &Build_Resource,     true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
