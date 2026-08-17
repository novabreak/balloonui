/**
 *  画廊「布局容器」分组的演示页面：竖直 / 水平 / 网格排列（DuiVBox、
 *  DuiHBox、DuiGrid）、分隔条（DuiSplitter）、停靠布局（DuiDock）、
 *  标签页容器（DuiTabPage）。
 *
 *  这四个页面演示的都是子控件如何被摆放到指定位置，因此放在同一个文件里，
 *  共用同一个只负责绘制的演示面板类 ColorPane。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Layout/DuiSplitter.h"
#include "Controls/Layout/DuiDock.h"
#include "Controls/List/DuiTabPage.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiLabel.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 本文件私有的常量
// =====================================================================

// ---- 演示面板的配色 -------------------------------------------------
// 下面这组颜色只用于把相邻的演示面板区分开，不承担任何语义。

// 品牌蓝。分隔条左侧面板、标签页首页、停靠布局的顶部工具栏都用它。
const COLORREF kPaneBlue = RGB(45, 108, 223);
// 绿色。与品牌蓝对比明显，用作第二块面板。
const COLORREF kPaneGreen = RGB(60, 175, 80);
// 深蓝灰。用作侧边导航一类的深色区域。
const COLORREF kPaneNavy = RGB(40, 60, 90);
// 紫色。用作次要信息区。
const COLORREF kPanePurple = RGB(120, 90, 180);

// ---- 带前置图标的标签页演示专用配色 ---------------------------------
// 图标圆点与它对应的内容面板共用同一种颜色，便于看出图标与页面的对应
// 关系，因此这三个颜色不能与上面那组混用。

// 第一个标签的颜色。
const COLORREF kIconRed = RGB(220, 60, 60);
// 第二个标签的颜色。
const COLORREF kIconGreen = RGB(60, 170, 80);
// 第三个标签的颜色。
const COLORREF kIconBlue = RGB(60, 120, 220);

// ---- DuiVBox 卡片样式演示里的文字颜色 -------------------------------

// 卡片内主标题的文字颜色。
const COLORREF kCardTitleColor = RGB(20, 30, 50);
// 卡片内次要说明的文字颜色。
const COLORREF kCardHintColor = RGB(140, 140, 140);
// 未加任何装饰的那个 DuiVBox 里主标题的文字颜色，比卡片内的浅一档，
// 以示它不是一张真正的卡片。
const COLORREF kPlainTitleColor = RGB(80, 80, 80);

// ---- 标签页图标位图的尺寸 -------------------------------------------

// 图标位图的边长（像素）。取 16 是为了与 DuiTab 默认的图标尺寸一致，
// 位图不会被缩放。
const int kDotIconSize = 16;
// 圆点的半径（像素）。取 6 使圆点四周留有空白，不会顶到位图边缘。
const int kDotIconRadius = 6;
// 位图每个像素占用的字节数。32 位位图按 BGRA 四个分量存放。
const int kDotIconBytesPerPixel = 4;
// 不透明像素的 alpha 值。
const BYTE kDotIconOpaqueAlpha = 255;

// ---- 各演示行的高度（像素）-----------------------------------------
// 只在两处以上出现、或者不看上下文就说不清含义的高度提成常量；其余
// 一次性的尺寸直接写在调用处。

// 内嵌一个 DuiHBox / DuiVBox 的演示行高度。内边距演示与卡片样式演示
// 都用它，两者需要一致才便于横向比较。
const int kBoxDemoRowH = 80;
// 网格演示行的高度。两行按钮加上间距刚好占满。
const int kGridDemoRowH = 100;
// 左右并排的分隔条演示行高度。基本形态与最小尺寸限制两段都用它，
// 高度一致才能看出两者的差别只在于最小尺寸。
const int kSplitterRowH = 100;
// 标签宽度自适应那一组对照演示的行高度。关闭与开启两段必须取同一个值，
// 否则读者无法确定看到的差别是不是行高造成的。
const int kAutoFitRowH = 160;

// ---- 网格演示的行列数 -----------------------------------------------

// 演示网格的行数。
const int kGridRows = 2;
// 演示网格的列数。
const int kGridCols = 3;
// 演示网格的格子总数。
const int kGridCellCount = kGridRows * kGridCols;

// 等权重演示与内边距演示里各放几个子控件。三个足够看出均分效果，
// 再多会把每一个挤得太窄。
const int kDemoChildCount = 3;

// ---- DuiVBox 卡片样式演示的内部尺寸（像素）--------------------------
// 三个并排的 DuiVBox 必须用同一组尺寸，否则读者会把尺寸差异误当成卡片
// 设置函数造成的差别。

// 卡片内的四边内边距。
const int kCardDemoPadding = 12;
// 卡片内主标题行的高度。
const int kCardDemoTitleH = 20;
// 卡片内次要说明行的高度。
const int kCardDemoHintH = 18;

// =====================================================================
// 只负责绘制的演示面板
// =====================================================================

// 用纯色填满自己的矩形并在正中画一行文字的演示面板。
//
// 分隔条、停靠布局、标签页容器这三个页面要展示的是子控件最终落在了哪个
// 位置，面板里装什么内容并不重要，重要的是能一眼看清每一块区域的边界与
// 用途。本类不处理任何输入事件，也不接受键盘焦点。
class ColorPane : public DuiControl
{
public:
    // 构造一个演示面板。
    //   color：面板的填充色。
    //   label：画在面板正中的文字。允许传空指针，表示不画文字；本类内部
    //          复制一份，不引用调用方的缓冲区。
    ColorPane(COLORREF color, LPCTSTR label)
        : m_color(color)
        , m_label(label != NULL ? label : _T(""))
    {
        // 演示面板没有可操作的内容，Tab 键遍历时应当跳过它。基类默认值
        // 本来就是假，这里显式写出来是为了说明这是有意为之。
        SetTabStop(false);
    }

    // 绘制本面板：先用填充色铺满与脏矩形相交的部分，再在整个面板正中画
    // 一行白字。
    //   hdc：目标设备上下文，由调用方负责状态的整体保存与恢复；本函数
    //        自己改动的背景模式与文字颜色在返回前会还原。
    //   rcDirty：本次需要重绘的区域。与自身矩形不相交时直接返回。
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }

        RECT rcInter;
        if (!::IntersectRect(&rcInter, &m_rcItem, &rcDirty))
        {
            return;
        }

        HBRUSH hbr = ::CreateSolidBrush(m_color);
        ::FillRect(hdc, &rcInter, hbr);
        ::DeleteObject(hbr);

        if (!m_label.IsEmpty())
        {
            // 文字按整个面板矩形居中，而不是按脏矩形居中，否则局部重绘
            // 时文字位置会跟着脏矩形跑。
            int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldTextColor = ::SetTextColor(hdc, RGB(255, 255, 255));
            ::DrawText(hdc, m_label, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            ::SetTextColor(hdc, oldTextColor);
            ::SetBkMode(hdc, oldBkMode);
        }
    }

private:
    // 面板的填充色。构造时确定，之后不再改变。
    COLORREF m_color;
    // 画在面板正中的文字。构造时复制一份，生命期与本对象相同。
    CString m_label;
};

// 合成一个用作标签页前置图标的实心圆点位图。
//   color：圆点的颜色。
// 返回：kDotIconSize 见方的 32 位预乘 alpha 位图；创建失败时返回空句柄。
//       返回的位图由调用方保管。本文件把它存在函数内的静态变量里，生命期
//       与进程相同，因此没有对应的 DeleteObject 调用。
HBITMAP MakeTabDotIcon(COLORREF color)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = kDotIconSize;
    // 高度取负数表示自上而下的行序，扫描线的下标与屏幕坐标同向。
    bi.bmiHeader.biHeight      = -kDotIconSize;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbm = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS,
                                     &pBits, NULL, 0);
    if (hbm == NULL)
    {
        return NULL;
    }

    BYTE* pPixels = static_cast<BYTE*>(pBits);
    // 先整体清零。清零之后每个像素的 alpha 都是 0，也就是完全透明，
    // 圆点之外的部分不需要再单独处理。
    ::ZeroMemory(pPixels, kDotIconSize * kDotIconSize * kDotIconBytesPerPixel);

    // 圆心取位图中心，用距离的平方与半径的平方比较，避免开方运算。
    const int centerX = kDotIconSize / 2;
    const int centerY = kDotIconSize / 2;
    const int radiusSquared = kDotIconRadius * kDotIconRadius;

    for (int y = 0; y < kDotIconSize; ++y)
    {
        for (int x = 0; x < kDotIconSize; ++x)
        {
            int dx = x - centerX;
            int dy = y - centerY;
            if (dx * dx + dy * dy >= radiusSquared)
            {
                continue;
            }

            BYTE* pPixel = pPixels
                         + (y * kDotIconSize + x) * kDotIconBytesPerPixel;
            // 像素按 BGRA 顺序排列。alpha 取满值时预乘不改变颜色分量，
            // 所以三个颜色分量直接写入即可。
            pPixel[0] = GetBValue(color);
            pPixel[1] = GetGValue(color);
            pPixel[2] = GetRValue(color);
            pPixel[3] = kDotIconOpaqueAlpha;
        }
    }

    return hbm;
}

} // 匿名命名空间

// ===== 布局容器 =======================================================

std::unique_ptr<DuiControl> Build_Layout()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiHBox：等权重均分宽度"),
                   _T("HBox: equal weights")),
               Txt(_T("三个子控件的布局提示都是 Weight(1)，可用宽度按 1:1:1 平均分成三份。"
                      "权重描述的是空间的分配比例，与子控件自身内容有多宽无关。"),
                   _T("Three children with weight=1 evenly split width.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);
        for (int i = 0; i < kDemoChildCount; ++i)
        {
            std::unique_ptr<DuiButton> btn(new DuiButton());
            CString text;
            text.Format(_T("w=1 (%d)"), i + 1);
            btn->SetText(text);
            row->AddChild(std::move(btn), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("DuiHBox：固定宽度与权重混排"),
                   _T("HBox: fixed + weighted")),
               Txt(_T("左侧用 Fixed(80) 固定宽度，中间是 Weight(1)，右侧是 Weight(2)。"
                      "容器变宽时固定宽度的那一个始终是 80 像素，多出来的宽度按 1:2 "
                      "分给中间和右侧。"),
                   _T("Left fixed=80, middle weight=1, right weight=2. "
                      "Resize behavior: only middle/right grow.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiButton> btnFixed(new DuiButton());
        btnFixed->SetText(_T("fixed=80"));
        std::unique_ptr<DuiButton> btnWeight1(new DuiButton());
        btnWeight1->SetText(_T("w=1"));
        std::unique_ptr<DuiButton> btnWeight2(new DuiButton());
        btnWeight2->SetText(_T("w=2"));

        row->AddChild(std::move(btnFixed), DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(btnWeight1), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(btnWeight2), DuiLayout::Hint().Weight(2));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("DuiGrid：两行三列网格"),
                   _T("Grid 2x3")),
               Txt(_T("SetGrid(2, 3) 划出两行三列，SetGap(8) 设定格子之间的间距。"
                      "子控件按先行后列的顺序填入，因此第一行是 D、E、F，第二行是 "
                      "G、H、I。"),
                   _T("SetGrid(2, 3) + SetGap(8). "
                      "Children fill row-major (D, E, F / G, H, I).")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiGrid> grid(new DuiGrid());
        grid->SetGrid(kGridRows, kGridCols);
        grid->SetGap(8);

        LPCTSTR labels[kGridCellCount] = { _T("D"), _T("E"), _T("F"),
                                           _T("G"), _T("H"), _T("I") };
        for (int i = 0; i < kGridCellCount; ++i)
        {
            std::unique_ptr<DuiButton> btn(new DuiButton());
            btn->SetText(labels[i]);
            grid->AddChild(std::move(btn));
        }

        row->AddChild(std::move(grid), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kGridDemoRowH);
    }

    AddSection(page.get(),
               Txt(_T("内边距与子控件间距"),
                   _T("Padding + gap")),
               Txt(_T("SetPadding 设定容器边界到子控件之间的内边距，SetGap 设定相邻子"
                      "控件之间的间距。两者互不影响：内边距只在容器最外圈留一次，"
                      "间距在每两个相邻子控件之间各留一次。这里两者都取 20 像素。"),
                   _T("Padding is the inner margin between the box and its children; "
                      "gap is between siblings.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiHBox> inner(new DuiHBox());
        inner->SetPadding(20);
        inner->SetGap(20);
        for (int i = 0; i < kDemoChildCount; ++i)
        {
            std::unique_ptr<DuiButton> btn(new DuiButton());
            CString text;
            text.Format(_T("%s %d"), Txt(_T("子控件"), _T("inner")), i + 1);
            btn->SetText(text);
            inner->AddChild(std::move(btn), DuiLayout::Hint().Weight(1));
        }

        row->AddChild(std::move(inner), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kBoxDemoRowH);
    }

    AddSection(page.get(),
               Txt(_T("DuiVBox 卡片样式：SetBgColor / SetCornerRadius / "
                      "SetBorderColor / SetBorderWidth"),
                   _T("DuiVBox card styling "
                      "(BgColor / CornerRadius / BorderColor / BorderWidth)")),
               Txt(_T("DuiVBox 自身就能充当卡片容器。这四个设置函数默认全部关闭，"
                      "一旦设了值，OnPaint 会先绘制底色与描边，再调用基类绘制子控件。"
                      "底色与描边都经由 DuiAA::FillRoundRect 完成，自带抗锯齿。"
                      "自绘控件也可以直接调用静态函数 DuiVBox::PaintBackground，"
                      "与这里用的是同一段绘制代码。下面并排放了三个 DuiVBox："
                      "不加任何装饰的默认形态、白底圆角卡片、品牌色描边卡片。"),
                   _T("DuiVBox doubles as a card container. All four setters are off "
                      "by default; once a value is set, OnPaint draws the background "
                      "and border first and then calls the base class to draw the "
                      "children. Both go through DuiAA::FillRoundRect, so they are "
                      "anti-aliased. Owner-drawn controls can call the static "
                      "DuiVBox::PaintBackground to reuse the very same drawing code. "
                      "Three DuiVBox instances side by side: the undecorated default, "
                      "a classic white rounded card, and a brand-color outlined card.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);

        // 第一个：不调用任何卡片设置函数，作为对照，说明四个设置函数不设值
        // 时 DuiVBox 的形态与以往完全一致。
        std::unique_ptr<DuiVBox> boxPlain(new DuiVBox());
        boxPlain->SetPadding(kCardDemoPadding);

        std::unique_ptr<DuiLabel> plainTitle(new DuiLabel());
        plainTitle->SetText(Txt(_T("默认（无装饰）"), _T("Default (no decoration)")));
        plainTitle->SetTextColor(kPlainTitleColor);
        boxPlain->AddChild(std::move(plainTitle),
                           DuiLayout::Hint().Fixed(kCardDemoTitleH));

        std::unique_ptr<DuiLabel> plainHint(new DuiLabel());
        plainHint->SetText(Txt(_T("未调用任何卡片设置函数"), _T("No card setter called")));
        plainHint->SetTextColor(kCardHintColor);
        boxPlain->AddChild(std::move(plainHint),
                           DuiLayout::Hint().Fixed(kCardDemoHintH));

        row->AddChild(std::move(boxPlain), DuiLayout::Hint().Weight(1));

        // 第二个：经典卡片，白底加 8 像素圆角再加 1 像素浅灰描边。
        std::unique_ptr<DuiVBox> boxCard(new DuiVBox());
        boxCard->SetBgColor(RGB(255, 255, 255));
        boxCard->SetCornerRadius(8);
        boxCard->SetBorderColor(RGB(232, 236, 240));
        boxCard->SetBorderWidth(1.0f);
        boxCard->SetPadding(kCardDemoPadding);

        std::unique_ptr<DuiLabel> cardTitle(new DuiLabel());
        cardTitle->SetText(Txt(_T("经典卡片"), _T("Classic card")));
        cardTitle->SetTextColor(kCardTitleColor);
        boxCard->AddChild(std::move(cardTitle),
                          DuiLayout::Hint().Fixed(kCardDemoTitleH));

        std::unique_ptr<DuiLabel> cardHint(new DuiLabel());
        cardHint->SetText(Txt(_T("白底 + 8px 圆角 + 1px 浅灰描边"),
                              _T("White fill + 8px radius + 1px light gray border")));
        cardHint->SetTextColor(kCardHintColor);
        boxCard->AddChild(std::move(cardHint),
                          DuiLayout::Hint().Fixed(kCardDemoHintH));

        row->AddChild(std::move(boxCard), DuiLayout::Hint().Weight(1));

        // 第三个：品牌色描边卡片，浅蓝底加 12 像素圆角再加 2 像素品牌色描边，
        // 用来对照圆角与描边加粗之后的效果。
        std::unique_ptr<DuiVBox> boxBrand(new DuiVBox());
        boxBrand->SetBgColor(RGB(245, 248, 255));
        boxBrand->SetCornerRadius(12);
        boxBrand->SetBorderColor(kPaneBlue);
        boxBrand->SetBorderWidth(2.0f);
        boxBrand->SetPadding(kCardDemoPadding);

        std::unique_ptr<DuiLabel> brandTitle(new DuiLabel());
        brandTitle->SetText(Txt(_T("品牌色描边卡片"), _T("Brand-color outlined card")));
        brandTitle->SetTextColor(kCardTitleColor);
        boxBrand->AddChild(std::move(brandTitle),
                           DuiLayout::Hint().Fixed(kCardDemoTitleH));

        std::unique_ptr<DuiLabel> brandHint(new DuiLabel());
        brandHint->SetText(Txt(_T("12px 圆角 + 2px 品牌色描边"),
                               _T("12px radius + 2px brand-color border")));
        brandHint->SetTextColor(kCardHintColor);
        boxBrand->AddChild(std::move(brandHint),
                           DuiLayout::Hint().Fixed(kCardDemoHintH));

        row->AddChild(std::move(boxBrand), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kBoxDemoRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 分隔条 =========================================================

std::unique_ptr<DuiControl> Build_Splitter()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiSplitter::Vertical：左右并排，可拖动分隔条"),
                   _T("Vertical (side-by-side, drag the bar)")),
               Txt(_T("方向取 Vertical 时分隔条本身是竖直的，两块面板左右并排。"
                      "这里两块面板的最小宽度都是 60 像素，初始分割比例 0.4，"
                      "即左侧占四成。按住分隔条左右拖动即可改变比例。"),
                   _T("DuiSplitter::Vertical with two colored panes. "
                      "Min sizes 60/60. Initial fraction 0.4.")));
    {
        std::unique_ptr<DuiSplitter> splitter(new DuiSplitter());
        splitter->SetOrientation(DuiSplitter::Vertical);
        splitter->SetMinSizes(60, 60);
        splitter->SetSplitFraction(0.4);
        splitter->SetPane(0, std::unique_ptr<DuiControl>(
            new ColorPane(kPaneBlue, Txt(_T("面板 0"), _T("pane 0")))));
        splitter->SetPane(1, std::unique_ptr<DuiControl>(
            new ColorPane(kPaneGreen, Txt(_T("面板 1"), _T("pane 1")))));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(splitter), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kSplitterRowH);
    }

    AddSection(page.get(),
               Txt(_T("DuiSplitter::Horizontal：上下堆叠，可拖动分隔条"),
                   _T("Horizontal (stacked, drag the bar)")),
               Txt(_T("方向取 Horizontal 时分隔条本身是水平的，两块面板上下堆叠。"
                      "分隔条的厚度由 SetBarThickness 设为 6 像素。"),
                   _T("DuiSplitter::Horizontal. Bar runs horizontally; "
                      "panes stack top/bottom. Bar thickness 6.")));
    {
        std::unique_ptr<DuiSplitter> splitter(new DuiSplitter());
        splitter->SetOrientation(DuiSplitter::Horizontal);
        splitter->SetBarThickness(6);
        splitter->SetMinSizes(40, 40);
        splitter->SetSplitFraction(0.5);
        splitter->SetPane(0, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(220, 90, 70), Txt(_T("上"), _T("top")))));
        splitter->SetPane(1, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(255, 160, 50), Txt(_T("下"), _T("bottom")))));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(splitter), DuiLayout::Hint().Weight(1));
        // 上下堆叠时两块面板各占一半，行高要给足才看得出上下关系。
        AddVariantRow(page.get(), std::move(row), 160);
    }

    AddSection(page.get(),
               Txt(_T("嵌套分隔条：三栏式聊天界面"),
                   _T("Nested splitters (chat-like 3 panes)")),
               Txt(_T("外层是竖直分隔条，左边放联系人列表，右边放聊天区；右边这一块"
                      "本身又是一个水平分隔条，上半是消息记录、下半是输入框。"
                      "分隔条可以任意层数嵌套，每一层各自维护自己的分割比例。"),
                   _T("Outer vertical (contact list | chat area). Right side is a "
                      "horizontal splitter (history / input).")));
    {
        // 内层：上半显示消息记录，下半是输入框。
        std::unique_ptr<DuiSplitter> inner(new DuiSplitter());
        inner->SetOrientation(DuiSplitter::Horizontal);
        inner->SetMinSizes(80, 30);
        inner->SetSplitFraction(0.7);
        inner->SetPane(0, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(245, 245, 250), Txt(_T("消息记录"), _T("chat history")))));
        inner->SetPane(1, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(220, 230, 245), Txt(_T("输入框"), _T("input")))));

        // 外层：左边是联系人列表，右边挂上面拼好的内层分隔条。
        std::unique_ptr<DuiSplitter> outer(new DuiSplitter());
        outer->SetOrientation(DuiSplitter::Vertical);
        outer->SetMinSizes(80, 120);
        outer->SetSplitFraction(0.3);
        outer->SetPane(0, std::unique_ptr<DuiControl>(
            new ColorPane(kPaneNavy, Txt(_T("联系人列表"), _T("contact list")))));
        outer->SetPane(1, std::move(inner));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(outer), DuiLayout::Hint().Weight(1));
        // 嵌套两层之后竖直方向被切成两段，行高需要比单层的演示大一倍以上。
        AddVariantRow(page.get(), std::move(row), 220);
    }

    AddSection(page.get(),
               Txt(_T("最小尺寸限制：试着把分隔条往两端拖"),
                   _T("Min-size demo (try dragging far in either direction)")),
               Txt(_T("两块面板的最小宽度都是 100 像素，而整行总宽约 280 像素，"
                      "因此分隔条只能在中间一小段范围内移动，拖到两端时会被限制住。"),
                   _T("Min sizes 100/100, total ~280px. "
                      "Bar can travel only across the small middle band.")));
    {
        std::unique_ptr<DuiSplitter> splitter(new DuiSplitter());
        splitter->SetOrientation(DuiSplitter::Vertical);
        splitter->SetMinSizes(100, 100);
        splitter->SetBarThickness(8);
        splitter->SetSplitFraction(0.5);
        splitter->SetPane(0, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(120, 50, 180), Txt(_T("最小 100"), _T("min 100")))));
        splitter->SetPane(1, std::unique_ptr<DuiControl>(
            new ColorPane(RGB(180, 50, 120), Txt(_T("最小 100"), _T("min 100")))));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(splitter), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kSplitterRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 停靠布局 =======================================================

std::unique_ptr<DuiControl> Build_Dock()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiDock：工具栏、状态栏、导航栏与内容区放在同一个容器里"),
                   _T("DuiDock — toolbar / status / nav / fill in one container")),
               Txt(_T("每个子控件停靠到某一条边并给出像素尺寸，最后一个取 DockFill "
                      "的子控件占据剩下的全部空间。停靠的先后顺序决定切分顺序："
                      "先停靠的先从可用矩形里切走属于自己的那一条。用它描述即时通讯"
                      "主面板这类界面，比嵌套三层 DuiHBox / DuiVBox 直接得多。"),
                   _T("Children dock to a side with a pixel size; the last \"Fill\" "
                      "child takes the leftover. Cheaper than nesting HBox / VBox "
                      "three deep for the typical IM main panel layout.")));
    {
        std::unique_ptr<DuiDock> dock(new DuiDock());
        // 相邻停靠区之间留 2 像素缝隙，五块颜色相近时也能看清各自的边界。
        dock->SetGap(2);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(kPaneBlue, Txt(_T("工具栏（顶部，32）"), _T("toolbar (top, 32)")))),
            DuiDock::DockTop, 32);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(kPaneGreen, Txt(_T("状态栏（底部，24）"), _T("status (bottom, 24)")))),
            DuiDock::DockBottom, 24);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(kPaneNavy, Txt(_T("导航栏（左侧，140）"), _T("nav (left, 140)")))),
            DuiDock::DockLeft, 140);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(kPanePurple, Txt(_T("信息栏（右侧，120）"), _T("info (right, 120)")))),
            DuiDock::DockRight, 120);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(RGB(200, 90, 70), Txt(_T("内容区（填充）"), _T("content (fill)")))),
            DuiDock::DockFill);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(dock), DuiLayout::Hint().Weight(1));
        // 上下左右四条边都停了控件，行高要能同时容下顶部 32、底部 24
        // 以及中间足够辨认的填充区。
        AddVariantRow(page.get(), std::move(row), 280);
    }

    AddSection(page.get(),
               Txt(_T("精简形态：只有顶部条与内容区"),
                   _T("Compact (top + fill only)")),
               Txt(_T("只保留一个顶部标题条加一个内容区，是聊天窗口一类界面最常见的"
                      "形态。DockFill 的子控件不需要给尺寸，剩下多少就占多少。"),
                   _T("Just a header strip + content area — common pattern for "
                      "chat windows.")));
    {
        std::unique_ptr<DuiDock> dock(new DuiDock());
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(kPaneBlue, Txt(_T("标题条（顶部，30）"), _T("title strip (top, 30)")))),
            DuiDock::DockTop, 30);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new ColorPane(RGB(245, 245, 248), Txt(_T("正文区（填充）"), _T("body (fill)")))),
            DuiDock::DockFill);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(dock), DuiLayout::Hint().Weight(1));
        // 顶部标题条 30 像素，其余留给正文区。
        AddVariantRow(page.get(), std::move(row), 140);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 标签页容器 =====================================================

std::unique_ptr<DuiControl> Build_TabPage()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiTabPage：标签头加内容区"),
                   _T("DuiTabPage (header strip + content area)")),
               Txt(_T("点击标签头即可切换下方显示的页面，同一时刻只有当前页可见。"
                      "标签头的高度默认为 32 像素。"),
                   _T("Click a tab to swap the visible page. Header height = 32 px.")));
    {
        std::unique_ptr<DuiTabPage> tabPage(new DuiTabPage());
        tabPage->AddPage(Txt(_T("联系人"), _T("Contacts")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kPaneBlue, Txt(_T("联系人列表"), _T("contact list")))));
        tabPage->AddPage(Txt(_T("群组"), _T("Groups")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kPaneGreen, Txt(_T("群组列表"), _T("group list")))));
        tabPage->AddPage(Txt(_T("最近"), _T("Recent")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(220, 110, 60), Txt(_T("最近会话"), _T("recent chats")))));
        tabPage->AddPage(Txt(_T("设置"), _T("Settings")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kPanePurple, Txt(_T("设置项"), _T("settings")))));
        // 第二个参数取假表示只切换显示，不向外发出选中变化通知。页面刚建
        // 出来时还没有人监听，发通知没有意义。
        tabPage->SetCurSel(0, false);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tabPage), DuiLayout::Hint().Weight(1));
        // 32 像素标签头加上足够辨认的内容区。
        AddVariantRow(page.get(), std::move(row), 220);
    }

    AddSection(page.get(),
               Txt(_T("加高标签头"),
                   _T("Larger header")),
               Txt(_T("调用 SetHeaderHeight(48) 把标签头加高到 48 像素。内容区的构成"
                      "不变，只是标签头分到了更多竖直空间。"),
                   _T("SetHeaderHeight(48). Same content; "
                      "header just gets more vertical room.")));
    {
        std::unique_ptr<DuiTabPage> tabPage(new DuiTabPage());
        tabPage->SetHeaderHeight(48);
        tabPage->AddPage(Txt(_T("标签 A"), _T("Tab A")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(50, 160, 110), Txt(_T("页面 A 的内容"), _T("page A content")))));
        tabPage->AddPage(Txt(_T("标签 B"), _T("Tab B")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(180, 100, 200), Txt(_T("页面 B 的内容"), _T("page B content")))));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tabPage), DuiLayout::Hint().Weight(1));
        // 标签头加高到 48 像素，内容区仍需留出可辨认的高度。
        AddVariantRow(page.get(), std::move(row), 180);
    }

    AddSection(page.get(),
               Txt(_T("带前置图标的标签"),
                   _T("Tabs with leading icon")),
               Txt(_T("AddPage(title, page, icon) 或 SetPageIcon(idx, HBITMAP) 可以在标签"
                      "头文字的左侧加一个 16x16 的预乘 alpha 图标。这项设置会转交给内部的"
                      " DuiTab，行为与直接使用 DuiTab 时一致。SetIconSize 调整图标的显示"
                      "尺寸，SetIconGap 调整图标与文字之间的间距。"),
                   _T("AddPage(title, page, icon) or SetPageIcon(idx, HBITMAP) puts a "
                      "16x16 premultiplied-alpha icon to the left of the tab title. "
                      "The call is forwarded to the inner DuiTab, so the behaviour "
                      "matches DuiTab. SetIconSize and SetIconGap adjust the icon size "
                      "and the gap between icon and text.")));
    {
        // 三个图标位图只在第一次进入本页时合成一次，之后每次重建页面复用
        // 同一份句柄，生命期与进程相同。
        static HBITMAP s_iconRed = MakeTabDotIcon(kIconRed);
        static HBITMAP s_iconGreen = MakeTabDotIcon(kIconGreen);
        static HBITMAP s_iconBlue = MakeTabDotIcon(kIconBlue);

        std::unique_ptr<DuiTabPage> tabPage(new DuiTabPage());
        tabPage->AddPage(Txt(_T("收件箱"), _T("Inbox")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kIconRed, Txt(_T("收件箱页面"), _T("inbox page")))),
                         s_iconRed);
        tabPage->AddPage(Txt(_T("已发送"), _T("Sent")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kIconGreen, Txt(_T("已发送页面"), _T("sent page")))),
                         s_iconGreen);
        tabPage->AddPage(Txt(_T("归档"), _T("Archive")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             kIconBlue, Txt(_T("归档页面"), _T("archive page")))),
                         s_iconBlue);
        tabPage->SetCurSel(0, false);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tabPage), DuiLayout::Hint().Weight(1));
        // 三个标签，内容区高度与第一段大致相当，便于对照有无图标的差别。
        AddVariantRow(page.get(), std::move(row), 200);
    }

    AddSection(page.get(),
               Txt(_T("标签宽度自适应：关闭（默认）"),
                   _T("Auto-fit width: Off (default)")),
               Txt(_T("SetAutoFitTabWidth 默认取假，标签头的宽度被限制在 60 到 200 像素"
                      "之间：过短的标签也至少占 60 像素，过长的标签被截断到 200 像素并"
                      "显示省略号。请与下一段开启后的效果对照。"),
                   _T("SetAutoFitTabWidth(false) by default: the tab header width is "
                      "clamped to [60, 200]. A very short tab still takes 60 px; "
                      "a very long one is cut to 200 px with an ellipsis.")));
    {
        std::unique_ptr<DuiTabPage> tabPage(new DuiTabPage());
        tabPage->AddPage(_T("A"),
                         std::unique_ptr<DuiControl>(new ColorPane(RGB(120, 150, 200), _T("A"))));
        tabPage->AddPage(Txt(_T("你好"), _T("Hello")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(150, 200, 120), Txt(_T("你好"), _T("Hello")))));
        tabPage->AddPage(Txt(_T("设置"), _T("Settings")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(200, 150, 120), Txt(_T("设置"), _T("Settings")))));
        tabPage->AddPage(Txt(_T("非常非常非常非常非常非常长的标签标题"),
                             _T("A very very very very very long tab title")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(180, 120, 200), Txt(_T("长标题"), _T("long")))));
        tabPage->SetCurSel(0, false);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tabPage), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kAutoFitRowH);
    }

    AddSection(page.get(),
               Txt(_T("标签宽度自适应：开启"),
                   _T("Auto-fit width: On")),
               Txt(_T("调用 SetAutoFitTabWidth(true) 之后，标签头的宽度严格贴合文字内容，"
                      "不再做上下限限制：长标题完整显示，短标题只保留内边距。"
                      "这个开关与 DuiTab 上的同名开关是同一个。"),
                   _T("SetAutoFitTabWidth(true): the tab header width follows the "
                      "content exactly and the min / max clamp is skipped. Long titles "
                      "are shown in full, short ones keep only the padding. "
                      "Same switch as on DuiTab.")));
    {
        std::unique_ptr<DuiTabPage> tabPage(new DuiTabPage());
        tabPage->SetAutoFitTabWidth(true);
        tabPage->AddPage(_T("A"),
                         std::unique_ptr<DuiControl>(new ColorPane(RGB(120, 150, 200), _T("A"))));
        tabPage->AddPage(Txt(_T("你好"), _T("Hello")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(150, 200, 120), Txt(_T("你好"), _T("Hello")))));
        tabPage->AddPage(Txt(_T("设置"), _T("Settings")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(200, 150, 120), Txt(_T("设置"), _T("Settings")))));
        tabPage->AddPage(Txt(_T("非常非常非常非常非常非常长的标签标题"),
                             _T("A very very very very very long tab title")),
                         std::unique_ptr<DuiControl>(new ColorPane(
                             RGB(180, 120, 200), Txt(_T("长标题"), _T("long")))));
        tabPage->SetCurSel(0, false);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tabPage), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kAutoFitRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ===============================================

const PageEntry* GetLayoutPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("layout-boxes"), _T("DuiVBox / DuiHBox / DuiGrid　排列容器"), _T("Layout Containers"), &Build_Layout,   true },
        { _T("splitter"),     _T("DuiSplitter　分隔条"),                  _T("DuiSplitter"),       &Build_Splitter, true },
        { _T("dock"),         _T("DuiDock　停靠布局"),                    _T("DuiDock"),           &Build_Dock,     true },
        { _T("tab-page"),     _T("DuiTabPage　标签页容器"),               _T("DuiTabPage"),        &Build_TabPage,  true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
