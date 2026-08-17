/**
 *  文档配图夹具页面（doc-captures）。
 *
 *  **本文件是文档配图夹具，不是演示页面。** 它的唯一用途是给命令行的
 *  --capture-all 模式提供截图对象：每一个段落恰好放一行演示控件，并用
 *  AddVariantRowCapture 给这一行登记一个截图标记，截图模式据此生成
 *  ctl-<名字>.png，供 flamingoclient/docs/guides.md 引用。
 *
 *  它的内容几乎全部是别的演示页面里已有段落的复制品，摆进导航树只会干扰
 *  阅读，因此本页在页面表里的 showInNav 填 false，只有截图模式会遍历到它。
 *
 *  三条与其它分组不同的约定：
 *
 *  一、**段落标题就是截图文件名，不得改动，也不做中英文切换。**
 *      标题写的是 layout-vbox、button-styles-overview 这一类标识，而不是
 *      面向读者的文案。改动它们会让文档里的图片引用失效，所以本文件里所有
 *      标题与说明都保持英文原样，不套 Txt()。
 *
 *  二、**段落末尾的 AddGap 保留。** 它们是从旧文件原样搬过来的，改动会
 *      改变各演示行在截图画布上的纵坐标。需要注意的是新版 PageKit 的
 *      AddGap 把空白插在当前卡片内部，因此这些调用现在的效果是每张卡片
 *      底部多留一段空白，段落之间的间距由页面容器的 gap 统一控制。
 *
 *  三、**演示行的宽度取决于卡片内容宽度。** 截图按演示行的矩形裁剪，
 *      加上卡片之后行的可用宽度比旧版窄了两倍卡片内边距，同时行的背景
 *      由窗口底色变成了卡片的白色。详见本文件末尾的说明。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "BalloonUiFeatures.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Layout/DuiSplitter.h"
#include "Controls/Layout/DuiDock.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiBadge.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/Basic/DuiGroupBox.h"
#include "Controls/Input/DuiSearchBox.h"
#include "Controls/Input/DuiSpinBox.h"
#include "Controls/Input/DuiSlider.h"
#include "Controls/Input/DuiComboBox.h"
#include "Controls/Input/DuiEditHost.h"
#include "Controls/Input/DuiRichEdit.h"
#include "Controls/Feedback/DuiProgressBar.h"
#include "Controls/Feedback/DuiEmojiPanel.h"
#include "Controls/List/DuiListBox.h"
#include "Controls/List/DuiTab.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/Window/DuiScrollBar.h"
#include "DuiPaintAA.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 演示用的配色
// =====================================================================

// 品牌主色（蓝）。徽标、链接文字、头像兜底底色共用这一个值。
const COLORREF kBrandColor = RGB(45, 108, 223);
// 品牌主色的加深版，用来表现链接的悬停状态。
const COLORREF kBrandDeepColor = RGB(30, 74, 153);
// 在线状态色（绿）。
const COLORREF kStatusOnlineColor = RGB(50, 160, 110);
// 离开状态色（黄）。
const COLORREF kStatusAwayColor = RGB(220, 170, 60);
// 忙碌状态色（红）。
const COLORREF kStatusBusyColor = RGB(220, 60, 60);
// 离线状态色（灰）。
const COLORREF kStatusOfflineColor = RGB(120, 120, 120);

// 演示文字用的深灰。DuiLabel 的默认文字色偏浅，色块与浅底容器上的示例
// 文字统一用这个值，截图里才看得清。
const COLORREF kDemoTextColor = RGB(50, 50, 50);
// 与上一个值同一用途，用在直接放在页面上的标签行，比色块里的略深。
const COLORREF kDemoTextDeepColor = RGB(40, 40, 40);

// =====================================================================
// 单色方块
// =====================================================================

// 纯色矩形加居中文字的最小自绘控件。
//
// 布局容器的演示（网格、分隔条、停靠）需要让每一个格子在截图里一眼分得开，
// 而 DuiLabel 没有设置背景色的接口，所以这里补一个只服务于本文件的小控件。
// 它不属于控件库的公开能力，别的地方需要类似效果时应当另行评估，而不是把
// 本类挪到库里。
class DocColorTile : public DuiControl
{
public:
    // 构造一个单色方块。
    //   text：方块中央显示的文字。允许传空指针，表示只画底色不画文字。
    //         本类在构造时复制一份，调用方不需要保证字符串的生命周期。
    //   bg：方块的填充色。
    //   fg：文字颜色。默认取接近黑色的深灰，浅色底上可读。
    DocColorTile(LPCTSTR text, COLORREF bg, COLORREF fg = RGB(40, 40, 40))
        : m_text(text != NULL ? text : _T(""))
        , m_bg(bg)
        , m_fg(fg)
    {
    }

    // 绘制方块本体与居中的文字。
    //   hdc：目标绘制上下文，由调用方负责创建与释放。
    //   第二个参数是本次需要重绘的矩形。本控件面积很小，整块重画的开销可以
    //   忽略，因此不使用它。
    void OnPaint(HDC hdc, const RECT&) override
    {
        HBRUSH br = ::CreateSolidBrush(m_bg);
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);

        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_fg);
        RECT r = m_rcItem;
        ::DrawText(hdc, m_text, -1, &r,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

private:
    // 方块中央显示的文字。构造时复制一份，生命周期与本控件相同。
    CString m_text;
    // 方块的填充色。
    COLORREF m_bg;
    // 文字颜色。
    COLORREF m_fg;
};

// =====================================================================
// 头像用的测试位图
// =====================================================================

// 头像源位图的边长（像素）。头像控件会把它缩放到实际显示尺寸。
const int kAvatarBitmapSize = 32;

// 生成一张带竖直双色渐变的 32 位测试位图，供头像演示使用。
//
// 头像如果填纯色，圆形裁剪与描边的效果在截图里几乎看不出来，所以这里合成
// 一张有明暗变化的位图代替真实照片。
//   r0 / g0 / b0：位图顶部一行的颜色分量，取值 0 ~ 255。
//   r1 / g1 / b1：位图底部一行的颜色分量，取值 0 ~ 255。中间各行在这两组
//                 分量之间线性插值。
// 返回：新建的位图句柄。所有权归调用方；本文件的调用点都把它存进函数内的
//       静态变量，进程退出时随之释放，不做显式删除。创建失败时返回空句柄。
HBITMAP MakeAvatarSourceBitmap(BYTE r0, BYTE g0, BYTE b0,
                               BYTE r1, BYTE g1, BYTE b1)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = kAvatarBitmapSize;
    // 高度取负值表示自上而下排列的位图，这样下面按行填充时行号与坐标一致。
    bi.bmiHeader.biHeight   = -kAvatarBitmapSize;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP h = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (h == NULL)
    {
        return NULL;
    }

    BYTE* p = (BYTE*)bits;
    for (int y = 0; y < kAvatarBitmapSize; ++y)
    {
        // 插值系数从顶行的 0 变到底行的 1。
        float t = y / (float)(kAvatarBitmapSize - 1);
        BYTE r = (BYTE)(r0 + (r1 - r0) * t);
        BYTE g = (BYTE)(g0 + (g1 - g0) * t);
        BYTE b = (BYTE)(b0 + (b1 - b0) * t);
        for (int x = 0; x < kAvatarBitmapSize; ++x)
        {
            // 32 位位图的分量顺序是蓝、绿、红、alpha。
            BYTE* px = p + (y * kAvatarBitmapSize + x) * 4;
            px[0] = b;
            px[1] = g;
            px[2] = r;
            px[3] = 255;
        }
    }
    return h;
}

// =====================================================================
// 抗锯齿对照方块
// =====================================================================

// 对照方块里两行图形的绘制坐标（像素）。上下两行共用同一组横向偏移，
// 逐个图形上下对齐，读者才能直接比较同一个图形的两种画法。
//
// 第一个图形距离控件左边缘的距离。
const int kAAGlyphLeftInset = 20;
// 图形距离它所在的那半行上下边缘的距离。
const int kAAGlyphInsetY = 12;
// 三角形顶点相对起始横坐标的偏移。
const int kAATriangleApexDx = 30;
// 三角形左下角顶点相对起始横坐标的偏移。
const int kAATriangleLeftDx = 8;
// 三角形右下角顶点相对起始横坐标的偏移。
const int kAATriangleRightDx = 52;
// 椭圆左边界相对起始横坐标的偏移。
const int kAAEllipseLeftDx = 80;
// 椭圆右边界相对起始横坐标的偏移。
const int kAAEllipseRightDx = 130;
// 斜线起点相对起始横坐标的偏移。
const int kAALineStartDx = 160;
// 斜线终点相对起始横坐标的偏移。
const int kAALineEndDx = 220;
// 右侧说明文字相对控件左边缘的距离。
const int kAALegendLeft = 240;
// 说明文字距离所在半行顶边的距离。
const int kAALegendTopInset = 4;
// 描边与斜线的线宽（像素）。
const int kAAStrokeWidth = 2;

// 图形的描边色。
const COLORREF kAAStrokeColor = RGB(40, 40, 40);
// 图形的填充色。
const COLORREF kAAFillColor = RGB(80, 130, 220);
// 上半行（普通画法）说明文字的颜色。
const COLORREF kAAPlainLegendColor = RGB(120, 30, 30);
// 下半行（抗锯齿画法）说明文字的颜色。
const COLORREF kAASmoothLegendColor = RGB(20, 100, 50);

// 抗锯齿画法与普通画法的对照方块。
//
// 上半行用系统的 Polygon / Ellipse / LineTo 画三角形、椭圆、斜线，下半行用
// DuiAA 的同名函数画同样的三个图形，文档配图据此展示两者边缘的差别。本类只
// 服务于本文件的 paintaa-comparison 段落。
class AAComparisonTile : public DuiControl
{
public:
    // 绘制上下两行对照图形。
    //   hdc：目标绘制上下文，由调用方负责创建与释放。
    //   第二个参数是本次需要重绘的矩形。本控件每次都整块重画，因此不使用它。
    void OnPaint(HDC hdc, const RECT&) override
    {
        // 底色取系统的窗口控件底色，与旧版本保持一致，换掉会改变已发布的配图。
        ::FillRect(hdc, &m_rcItem, ::GetSysColorBrush(COLOR_BTNFACE));

        int midY = (m_rcItem.top + m_rcItem.bottom) / 2;
        int rowH = (m_rcItem.bottom - m_rcItem.top) / 2;
        int x = m_rcItem.left + kAAGlyphLeftInset;

        // ---- 上半行：系统绘图接口，边缘有明显锯齿 ----
        HPEN penStroke = ::CreatePen(PS_SOLID, kAAStrokeWidth, kAAStrokeColor);
        HBRUSH brFill = ::CreateSolidBrush(kAAFillColor);
        HGDIOBJ oldPen = ::SelectObject(hdc, penStroke);
        HGDIOBJ oldBrush = ::SelectObject(hdc, brFill);

        POINT tri[3] = {
            { x + kAATriangleApexDx,  m_rcItem.top + kAAGlyphInsetY },
            { x + kAATriangleLeftDx,  m_rcItem.top + rowH - kAAGlyphInsetY },
            { x + kAATriangleRightDx, m_rcItem.top + rowH - kAAGlyphInsetY },
        };
        ::Polygon(hdc, tri, 3);

        ::Ellipse(hdc,
                  x + kAAEllipseLeftDx,  m_rcItem.top + kAAGlyphInsetY,
                  x + kAAEllipseRightDx, m_rcItem.top + rowH - kAAGlyphInsetY);

        ::MoveToEx(hdc, x + kAALineStartDx, m_rcItem.top + kAAGlyphInsetY, NULL);
        ::LineTo(hdc, x + kAALineEndDx, m_rcItem.top + rowH - kAAGlyphInsetY);

        ::SelectObject(hdc, oldPen);
        ::SelectObject(hdc, oldBrush);
        ::DeleteObject(penStroke);
        ::DeleteObject(brFill);

        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, kAAPlainLegendColor);
        RECT rTop = { m_rcItem.left + kAALegendLeft,
                      m_rcItem.top + kAALegendTopInset,
                      m_rcItem.right,
                      m_rcItem.top + rowH };
        ::DrawText(hdc, _T("plain GDI — jaggies"), -1, &rTop,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // ---- 下半行：DuiPaintAA 提供的抗锯齿绘图，边缘平滑 ----
        POINT triAA[3] = {
            { x + kAATriangleApexDx,  midY + kAAGlyphInsetY },
            { x + kAATriangleLeftDx,  m_rcItem.bottom - kAAGlyphInsetY },
            { x + kAATriangleRightDx, m_rcItem.bottom - kAAGlyphInsetY },
        };
        DuiAA::FillPolygon(hdc, triAA, 3, kAAFillColor, kAAStrokeColor);

        RECT rcEllipse = { x + kAAEllipseLeftDx,  midY + kAAGlyphInsetY,
                           x + kAAEllipseRightDx, m_rcItem.bottom - kAAGlyphInsetY };
        DuiAA::FillEllipse(hdc, rcEllipse, kAAFillColor, kAAStrokeColor);

        DuiAA::DrawLine(hdc,
                        x + kAALineStartDx, midY + kAAGlyphInsetY,
                        x + kAALineEndDx,   m_rcItem.bottom - kAAGlyphInsetY,
                        kAAStrokeColor, (float)kAAStrokeWidth);

        ::SetTextColor(hdc, kAASmoothLegendColor);
        RECT rBottom = { m_rcItem.left + kAALegendLeft,
                         midY + kAALegendTopInset,
                         m_rcItem.right,
                         m_rcItem.bottom };
        ::DrawText(hdc, _T("DuiPaintAA — smooth"), -1, &rBottom,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
};

// =====================================================================
// 各段落用到的成组数据
// =====================================================================

// layout-grid-equal 段落里 6 个格子的底色，按先行后列的顺序排列。
const COLORREF kGridEqualTints[] = {
    RGB(220, 235, 255), RGB(225, 245, 225), RGB(245, 225, 245),
    RGB(255, 235, 220), RGB(245, 245, 210), RGB(220, 245, 235),
};
const int kGridEqualTintCount =
    (int)(sizeof(kGridEqualTints) / sizeof(kGridEqualTints[0]));

// layout-grid-uneven 段落里 8 个格子的底色，按先行后列的顺序排列。
const COLORREF kGridUnevenTints[] = {
    RGB(220, 235, 255), RGB(255, 235, 220),
    RGB(225, 245, 225), RGB(245, 225, 245),
    RGB(245, 245, 210), RGB(225, 235, 245),
    RGB(235, 225, 210), RGB(220, 245, 235),
};
const int kGridUnevenTintCount =
    (int)(sizeof(kGridUnevenTints) / sizeof(kGridUnevenTints[0]));

// 主题色卡里的一格。
struct ThemeSwatch
{
    // 色块的填充色。
    COLORREF color;
    // 色块上显示的名字，同时也是这个颜色在主题里的用途。
    LPCTSTR name;
};

// theme-swatches 段落展示的全部色卡。
const ThemeSwatch kThemeSwatches[] = {
    { kBrandColor,         _T("brand")  },
    { kBrandDeepColor,     _T("deep")   },
    { kStatusOnlineColor,  _T("online") },
    { kStatusAwayColor,    _T("away")   },
    { kStatusBusyColor,    _T("busy")   },
    { kStatusOfflineColor, _T("off")    },
};
const int kThemeSwatchCount =
    (int)(sizeof(kThemeSwatches) / sizeof(kThemeSwatches[0]));

// slider-vertical 段落里三根竖直滑块各自的当前值，取值范围 0 ~ 100。
const int kVerticalSliderValues[] = { 20, 50, 80 };
const int kVerticalSliderCount =
    (int)(sizeof(kVerticalSliderValues) / sizeof(kVerticalSliderValues[0]));

// 滑块、进度条这类取值控件在本页统一使用的取值范围下限与上限。
const int kValueRangeMin = 0;
const int kValueRangeMax = 100;

// 滚动条演示统一使用的取值范围与页大小。
const int kScrollRangeMax = 1000;
const int kScrollPageSize = 200;
const int kScrollPos = 300;

} // 匿名命名空间

// =====================================================================
// 文档配图夹具页面
// =====================================================================
//
// 段落的排列顺序照 flamingoclient/docs/guides.md 的章节顺序，方便照着文档
// 逐个核对配图有没有漏掉。每个段落只放一行演示控件，行的截图名与段落标题
// 相同。
//
std::unique_ptr<DuiControl> Build_DocCaptures()
{
    // 这一页刻意用无卡片模式。命令行截图模式按每一行演示控件的矩形裁图，
    // 段落如果各自包一张卡片，裁出来的图会窄 32 像素、空白处的底色也会由灰
    // 变白，与已经发布在文档里的那批配图对不上。
    std::unique_ptr<GalleryPageBox> page = NewPlainPage();

    // ---- 一、布局容器 -------------------------------------------------

    AddSection(page.get(), _T("layout-vbox"),
               _T("DuiVBox with 3 children + gap=8 + padding=12."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiVBox> vb(new DuiVBox());
        vb->SetPadding(12);
        vb->SetGap(8);
        for (int i = 0; i < 3; ++i)
        {
            std::unique_ptr<DuiLabel> l(new DuiLabel());
            CString s;
            s.Format(_T("Row %d"), i + 1);
            l->SetText(s);
            l->SetTextColor(kDemoTextColor);
            vb->AddChild(std::move(l), DuiLayout::Hint().Fixed(20));
        }
        row->AddChild(std::move(vb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("layout-vbox"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-hbox"),
               _T("DuiHBox with 4 children + gap=8 + padding=12."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiHBox> hb(new DuiHBox());
        hb->SetPadding(12);
        hb->SetGap(8);
        for (int i = 0; i < 4; ++i)
        {
            std::unique_ptr<DuiLabel> l(new DuiLabel());
            CString s;
            s.Format(_T("#%d"), i + 1);
            l->SetText(s);
            l->SetTextColor(kDemoTextColor);
            hb->AddChild(std::move(l), DuiLayout::Hint().Fixed(60));
        }
        row->AddChild(std::move(hb), DuiLayout::Hint().Fixed(380));
        AddVariantRowCapture(page.get(), _T("layout-hbox"), std::move(row), 64);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-grid-equal"),
               _T("DuiGrid 3 columns equal width, 2 rows."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiGrid> g(new DuiGrid());
        g->SetGrid(2, 3);
        g->SetGap(6);
        g->SetPadding(8);
        for (int i = 0; i < kGridEqualTintCount; ++i)
        {
            CString s;
            s.Format(_T("Cell %d"), i + 1);
            g->AddChild(std::unique_ptr<DuiControl>(
                new DocColorTile(s, kGridEqualTints[i])));
        }
        row->AddChild(std::move(g), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("layout-grid-equal"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-grid-uneven"),
               _T("DuiGrid 4 columns × 2 rows of varied colored cells."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiGrid> g(new DuiGrid());
        g->SetGrid(2, 4);
        g->SetGap(4);
        g->SetPadding(6);
        for (int i = 0; i < kGridUnevenTintCount; ++i)
        {
            // 单元格的名字写成"第几行第几列"，配图里能直接读出网格的排列顺序。
            CString s;
            s.Format(_T("R%d C%d"), i / 4 + 1, i % 4 + 1);
            g->AddChild(std::unique_ptr<DuiControl>(
                new DocColorTile(s, kGridUnevenTints[i])));
        }
        row->AddChild(std::move(g), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("layout-grid-uneven"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("splitter-horizontal"),
               _T("Horizontal DuiSplitter — drag the central handle to resize panes."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiSplitter> sp(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Horizontal);
        sp->SetPane(0, std::unique_ptr<DuiControl>(
            new DocColorTile(_T(" Left pane "), RGB(220, 235, 255))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(
            new DocColorTile(_T(" Right pane "), RGB(245, 225, 245))));
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("splitter-horizontal"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("splitter-vertical"),
               _T("Vertical DuiSplitter."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiSplitter> sp(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Vertical);
        sp->SetPane(0, std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Top pane"), RGB(220, 235, 255))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Bottom pane"), RGB(245, 225, 245))));
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("splitter-vertical"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("dock-five-zones"),
               _T("DuiDock with 5 colored regions (top / bottom / left / right / fill)."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        // 停靠顺序决定四条边如何切分剩余空间：先上下、后左右，最后由填充区
        // 占掉中间剩下的部分。
        std::unique_ptr<DuiDock> dock(new DuiDock());
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Top"), RGB(220, 235, 255))),
            DuiDock::DockTop, 36);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Bottom"), RGB(245, 225, 245))),
            DuiDock::DockBottom, 36);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Left"), RGB(225, 245, 225))),
            DuiDock::DockLeft, 60);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Right"), RGB(255, 235, 220))),
            DuiDock::DockRight, 60);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Center / Fill"), RGB(245, 245, 210))),
            DuiDock::DockFill);
        row->AddChild(std::move(dock), DuiLayout::Hint().Fixed(380));
        AddVariantRowCapture(page.get(), _T("dock-five-zones"), std::move(row), 200);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 二、基础控件 -------------------------------------------------

    AddSection(page.get(), _T("label-single"),
               _T("Single-line label, default 9pt YaHei, ink-1 text."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiLabel> l(new DuiLabel());
        l->SetText(_T("Hello, balloonui — single-line label"));
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("label-single"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-multiline"),
               _T("Word-wrapped label spanning multiple lines."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiLabel> l(new DuiLabel());
        l->SetText(_T("Multi-line labels wrap on whitespace by default. ")
                   _T("Pass DT_WORDBREAK in the text-align flags to ")
                   _T("opt in. Width is bounded by the parent layout."));
        l->SetTextAlign(DT_LEFT | DT_TOP | DT_WORDBREAK);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("label-multiline"), std::move(row), 70);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-ellipsis"),
               _T("Single-line truncation with trailing ellipsis."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiLabel> l(new DuiLabel());
        l->SetText(_T("This text is a lot longer than the label is wide so it will be truncated with an ellipsis at the end."));
        l->SetTextAlign(DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("label-ellipsis"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-link-hover"),
               _T("Link-style label (blue underline). Hover state forced via DebugSetHover."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> l1(new DuiLabel());
        l1->SetText(_T("Forgot password?"));
        l1->SetTextColor(kBrandColor);

        // 悬停状态在截图里没有鼠标可用，只能用调试接口强制置上。
        std::unique_ptr<DuiLabel> l2(new DuiLabel());
        l2->SetText(_T("Forgot password? (hover)"));
        l2->SetTextColor(kBrandDeepColor);
        l2->DebugSetHover(true);

        row->AddChild(std::move(l1), DuiLayout::Hint().Fixed(180));
        row->AddChild(std::move(l2), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("label-link-hover"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("button-styles-overview"),
               _T("4 DuiButton styles: PushButton / Checkbox / Radio / Icon."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);

        std::unique_ptr<DuiButton> bPush(new DuiButton());
        bPush->SetText(_T("Save"));

        std::unique_ptr<DuiButton> bCheck(new DuiButton());
        bCheck->SetButtonType(DuiButton::StyleCheckbox);
        bCheck->SetText(_T("Checked"));
        // 第二个参数是是否发送通知。这里只是摆样子，不需要通知。
        bCheck->SetCheck(true, false);

        std::unique_ptr<DuiButton> bRadio(new DuiButton());
        bRadio->SetButtonType(DuiButton::StyleRadio);
        bRadio->SetText(_T("Radio"));
        bRadio->SetCheck(true, false);

        std::unique_ptr<DuiButton> bIcon(new DuiButton());
        bIcon->SetButtonType(DuiButton::StyleIcon);
        bIcon->SetText(_T("Open"));

        row->AddChild(std::move(bPush), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bCheck), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bRadio), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bIcon), DuiLayout::Hint().Fixed(110));
        AddVariantRowCapture(page.get(), _T("button-styles-overview"), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("badge-types"),
               _T("DuiBadge: red dot / count / 99+ / custom color."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        // 文字为一个空格时徽标退化成一个小圆点，用来表示"有新内容但不计数"。
        std::unique_ptr<DuiBadge> bDot(new DuiBadge());
        bDot->SetText(_T(" "));

        std::unique_ptr<DuiBadge> bFew(new DuiBadge());
        bFew->SetCount(3);

        // 计数超过两位时徽标显示为 99+。
        std::unique_ptr<DuiBadge> bMany(new DuiBadge());
        bMany->SetCount(150);

        std::unique_ptr<DuiBadge> bBrand(new DuiBadge());
        bBrand->SetCount(7);
        bBrand->SetBgColor(kBrandColor);

        row->AddChild(std::move(bDot), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(bFew), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(bMany), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(bBrand), DuiLayout::Hint().Fixed(40));
        AddVariantRowCapture(page.get(), _T("badge-types"), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("avatar-grid"),
               _T("Avatar variations: bitmap circle / bitmap rounded / initials / status dots."));
    {
        // 位图在函数内的静态变量里缓存一份。页面每次切换都会重建控件，位图
        // 必须比控件活得久，因此不随控件释放，进程退出时由系统回收。
        static HBITMAP s_blue = MakeAvatarSourceBitmap(80, 130, 220, 30, 60, 130);
        static HBITMAP s_purple = MakeAvatarSourceBitmap(170, 90, 200, 90, 30, 130);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(14);

        std::unique_ptr<DuiAvatar> aCircle(new DuiAvatar());
        aCircle->SetBitmap(s_blue);

        std::unique_ptr<DuiAvatar> aRounded(new DuiAvatar());
        aRounded->SetBitmap(s_purple);
        aRounded->SetShape(DuiAvatar::ShapeRoundRect);
        aRounded->SetCornerRadius(12);

        // 没有位图时头像退化成"取名字首字母 + 兜底底色"。
        std::unique_ptr<DuiAvatar> aInitialLatin(new DuiAvatar());
        aInitialLatin->SetName(_T("Alice Smith"));
        aInitialLatin->SetFallbackBgColor(kBrandColor);

        std::unique_ptr<DuiAvatar> aInitialCjk(new DuiAvatar());
        aInitialCjk->SetName(_T("陆星辰"));
        aInitialCjk->SetFallbackBgColor(kStatusOnlineColor);

        // 四个在线状态角标各来一个。
        std::unique_ptr<DuiAvatar> aOnline(new DuiAvatar());
        aOnline->SetBitmap(s_blue);
        aOnline->SetStatus(DuiAvatar::StatusOnline);

        std::unique_ptr<DuiAvatar> aAway(new DuiAvatar());
        aAway->SetBitmap(s_blue);
        aAway->SetStatus(DuiAvatar::StatusAway);

        std::unique_ptr<DuiAvatar> aBusy(new DuiAvatar());
        aBusy->SetBitmap(s_blue);
        aBusy->SetStatus(DuiAvatar::StatusBusy);

        std::unique_ptr<DuiAvatar> aOffline(new DuiAvatar());
        aOffline->SetBitmap(s_blue);
        aOffline->SetStatus(DuiAvatar::StatusOffline);

        row->AddChild(std::move(aCircle), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aRounded), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aInitialLatin), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aInitialCjk), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aOnline), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aAway), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aBusy), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(aOffline), DuiLayout::Hint().Fixed(56));
        AddVariantRowCapture(page.get(), _T("avatar-grid"), std::move(row), 56);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("separator-horizontal"),
               _T("Plain 1px horizontal separator."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiSeparator> s(new DuiSeparator());
        s->SetOrientation(DuiSeparator::Horizontal);
        row->AddChild(std::move(s), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("separator-horizontal"), std::move(row), 16);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("separator-labeled"),
               _T("Vertical separator alongside two labels."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiLabel> l1(new DuiLabel());
        l1->SetText(_T(" Online"));
        l1->SetTextColor(kDemoTextDeepColor);

        std::unique_ptr<DuiSeparator> sep(new DuiSeparator());
        sep->SetOrientation(DuiSeparator::Vertical);

        std::unique_ptr<DuiLabel> l2(new DuiLabel());
        l2->SetText(_T(" Offline"));
        l2->SetTextColor(kDemoTextDeepColor);

        row->AddChild(std::move(l1), DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(sep), DuiLayout::Hint().Fixed(2));
        row->AddChild(std::move(l2), DuiLayout::Hint().Fixed(80));
        AddVariantRowCapture(page.get(), _T("separator-labeled"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("groupbox-sample"),
               _T("DuiGroupBox with title strip and one nested child."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiGroupBox> gb(new DuiGroupBox());
        gb->SetTitle(_T(" Account "));

        std::unique_ptr<DuiLabel> lbl(new DuiLabel());
        lbl->SetText(_T("  Username:  alice@example.com"));
        gb->AddChild(std::move(lbl));

        row->AddChild(std::move(gb), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("groupbox-sample"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 三、输入控件 -------------------------------------------------

    AddSection(page.get(), _T("searchbox-states"),
               _T("Empty + populated DuiSearchBox side-by-side."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);

        std::unique_ptr<DuiSearchBox> sEmpty(new DuiSearchBox());
        sEmpty->SetPlaceholder(_T("Search contacts..."));

        std::unique_ptr<DuiSearchBox> sFilled(new DuiSearchBox());
        sFilled->SetText(_T("alice"));

        row->AddChild(std::move(sEmpty), DuiLayout::Hint().Fixed(220));
        row->AddChild(std::move(sFilled), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("searchbox-states"), std::move(row), 32);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("spinbox-default"),
               _T("DuiSpinBox showing a numeric value with up/down arrows."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiSpinBox> sp(new DuiSpinBox());
        sp->SetRange(0, 999);
        sp->SetValue(42, false);
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(120));
        AddVariantRowCapture(page.get(), _T("spinbox-default"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("progressbar-states"),
               _T("Progress bar at 0% / 35% / 100%."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiProgressBar> pEmpty(new DuiProgressBar());
        pEmpty->SetRange(kValueRangeMin, kValueRangeMax);
        pEmpty->SetPos(0);

        std::unique_ptr<DuiProgressBar> pPartial(new DuiProgressBar());
        pPartial->SetRange(kValueRangeMin, kValueRangeMax);
        pPartial->SetPos(35);

        std::unique_ptr<DuiProgressBar> pFull(new DuiProgressBar());
        pFull->SetRange(kValueRangeMin, kValueRangeMax);
        pFull->SetPos(kValueRangeMax);

        row->AddChild(std::move(pEmpty), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(pPartial), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(pFull), DuiLayout::Hint().Fixed(140));
        AddVariantRowCapture(page.get(), _T("progressbar-states"), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("slider-horizontal"),
               _T("Horizontal slider 0% / 50% / 100% / disabled."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        // SetPos 的第二个参数是是否发送通知，摆样子用的控件一律不发。
        std::unique_ptr<DuiSlider> sMin(new DuiSlider());
        sMin->SetRange(kValueRangeMin, kValueRangeMax);
        sMin->SetPos(kValueRangeMin, false);

        std::unique_ptr<DuiSlider> sHalf(new DuiSlider());
        sHalf->SetRange(kValueRangeMin, kValueRangeMax);
        sHalf->SetPos(50, false);

        std::unique_ptr<DuiSlider> sMax(new DuiSlider());
        sMax->SetRange(kValueRangeMin, kValueRangeMax);
        sMax->SetPos(kValueRangeMax, false);

        std::unique_ptr<DuiSlider> sDisabled(new DuiSlider());
        sDisabled->SetRange(kValueRangeMin, kValueRangeMax);
        sDisabled->SetPos(70, false);
        sDisabled->SetEnabled(false);

        row->AddChild(std::move(sMin), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(sHalf), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(sMax), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(sDisabled), DuiLayout::Hint().Fixed(120));
        AddVariantRowCapture(page.get(), _T("slider-horizontal"), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("slider-vertical"),
               _T("Vertical slider variants."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        for (int i = 0; i < kVerticalSliderCount; ++i)
        {
            std::unique_ptr<DuiSlider> s(new DuiSlider());
            s->SetVertical(true);
            s->SetRange(kValueRangeMin, kValueRangeMax);
            s->SetPos(kVerticalSliderValues[i], false);
            row->AddChild(std::move(s), DuiLayout::Hint().Fixed(28));
        }
        AddVariantRowCapture(page.get(), _T("slider-vertical"), std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("combobox-collapsed"),
               _T("Collapsed DuiComboBox showing the selected item + dropdown arrow."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiComboBox> cb(new DuiComboBox());
        cb->AddString(_T("English"));
        cb->AddString(_T("中文 简体"));
        cb->AddString(_T("日本語"));
        cb->SetCurSel(1, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Fixed(180));
        AddVariantRowCapture(page.get(), _T("combobox-collapsed"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 四、列表与导航 -----------------------------------------------

    AddSection(page.get(), _T("listbox-single"),
               _T("Single-select DuiListBox with hover and selected items."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiListBox> lb(new DuiListBox());
        lb->AddItem(_T("Inbox"));
        lb->AddItem(_T("Drafts"));
        lb->AddItem(_T("Sent"));
        lb->AddItem(_T("Trash"));
        lb->SetCurSel(1, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("listbox-single"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("listbox-multi"),
               _T("Multi-select list with several items checked."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiListBox> lb(new DuiListBox());
        lb->SetMultiSelect(true);
        lb->AddItem(_T("Apple"));
        lb->AddItem(_T("Banana"));
        lb->AddItem(_T("Cherry"));
        lb->AddItem(_T("Durian"));
        lb->SetItemSelected(0, true);
        lb->SetItemSelected(2, true);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("listbox-multi"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tab-horizontal"),
               _T("Horizontal DuiTab with selected / inactive / disabled tabs."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiTab> t(new DuiTab());
        t->AddTab(_T("Friends"));
        t->AddTab(_T("Groups"));
        t->AddTab(_T("Recent"));
        t->AddTab(_T("Archive"));
        t->SetCurSel(1, false);
        row->AddChild(std::move(t), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("tab-horizontal"), std::move(row), 36);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tab-vertical"),
               _T("Settings-style vertical nav (DuiListBox styled as a sidebar)."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiListBox> lb(new DuiListBox());
        lb->SetItemHeight(28);
        lb->AddItem(_T("General"));
        lb->AddItem(_T("Account"));
        lb->AddItem(_T("Privacy"));
        lb->AddItem(_T("Appearance"));
        lb->SetCurSel(0, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(160));
        AddVariantRowCapture(page.get(), _T("tab-vertical"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("treeview-states"),
               _T("DuiTreeView with expanded + collapsed parents."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        // 第一个根节点展开、第二个保持折叠，一张图里同时呈现两种状态。
        std::unique_ptr<DuiTreeView> tv(new DuiTreeView());
        int rootExpanded = tv->AddRoot(_T("Friends"));
        tv->AddChild(rootExpanded, _T("Alice"));
        tv->AddChild(rootExpanded, _T("Bob"));
        tv->AddChild(rootExpanded, _T("Cindy"));
        tv->Expand(rootExpanded);

        int rootCollapsed = tv->AddRoot(_T("Groups (collapsed)"));
        tv->AddChild(rootCollapsed, _T("Dev"));
        tv->AddChild(rootCollapsed, _T("Design"));

        row->AddChild(std::move(tv), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("treeview-states"), std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 五、主题配色 -------------------------------------------------

    AddSection(page.get(), _T("theme-swatches"),
               _T("Theme color swatches: brand / ink / status."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        for (int i = 0; i < kThemeSwatchCount; ++i)
        {
            // 色卡底色都比较深，文字统一用白色才读得出来。
            row->AddChild(std::unique_ptr<DuiControl>(
                new DocColorTile(kThemeSwatches[i].name,
                                 kThemeSwatches[i].color,
                                 RGB(255, 255, 255))),
                DuiLayout::Hint().Fixed(80));
        }
        AddVariantRowCapture(page.get(), _T("theme-swatches"), std::move(row), 36);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 六、抗锯齿绘图 -----------------------------------------------

    AddSection(page.get(), _T("paintaa-comparison"),
               _T("Anti-aliased vs plain GDI: triangle, ellipse, diagonal line."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<AAComparisonTile> tile(new AAComparisonTile());
        row->AddChild(std::move(tile), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("paintaa-comparison"),
                             std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 七、文本编辑与其它控件 ---------------------------------------

    AddSection(page.get(), _T("edit-states"),
               _T("DuiEditHost — empty placeholder / filled / focused / disabled."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiEditHost> eEmpty(new DuiEditHost());
        eEmpty->SetPlaceholder(_T("Type here..."));

        std::unique_ptr<DuiEditHost> eFilled(new DuiEditHost());
        eFilled->SetText(_T("alice@example.com"));

        // 截图时没有真正的输入焦点，用调试接口把焦点态强制置上。
        std::unique_ptr<DuiEditHost> eFocused(new DuiEditHost());
        eFocused->SetText(_T("focused"));
        eFocused->DebugSetFocused(true);

        std::unique_ptr<DuiEditHost> eDisabled(new DuiEditHost());
        eDisabled->SetText(_T("disabled"));
        eDisabled->SetEnabled(false);

        row->AddChild(std::move(eEmpty), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(eFilled), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(eFocused), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(eDisabled), DuiLayout::Hint().Fixed(150));
        AddVariantRowCapture(page.get(), _T("edit-states"), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

#if BUI_FEATURE_RICHTEXT
    AddSection(page.get(), _T("richtext-basic"),
               _T("DuiRichEdit (windowless) — plain text, border, default font."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiRichEdit> rt(new DuiRichEdit());
        rt->SetMultiLine(true);
        rt->SetWordWrap(true);
        rt->SetText(_T("Windowless rich text. 无窗口富文本控件。"));
        row->AddChild(std::move(rt), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("richtext-basic"), std::move(row), 50);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("richtext-scrollbar"),
               _T("DuiRichEdit (windowless) — overlay scrollbar, always visible."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiRichEdit> rt(new DuiRichEdit());
        rt->SetMultiLine(true);
        rt->SetWordWrap(true);
        // 强制滚动条常驻，否则截图时它会因为处于淡出状态而拍不到。
        rt->SetVScrollPolicy(DuiRichEdit::kScrollBarAlways);
        rt->SetText(_T("The quick brown fox jumps over the lazy dog. ")
                    _T("The quick brown fox jumps over the lazy dog. ")
                    _T("The quick brown fox jumps over the lazy dog. ")
                    _T("滚动条浮在文字之上，不占内容宽度。"));
        row->AddChild(std::move(rt), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("richtext-scrollbar"), std::move(row), 70);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("richtext-placeholder"),
               _T("DuiRichEdit (windowless) — empty document shows placeholder."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiRichEdit> rt(new DuiRichEdit());
        rt->SetPlaceholder(_T("type a message..."));
        row->AddChild(std::move(rt), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("richtext-placeholder"), std::move(row), 50);
    }
    AddGap(page.get(), kSectionGap);
#endif // BUI_FEATURE_RICHTEXT

    AddSection(page.get(), _T("scrollbar-states"),
               _T("DuiScrollBar — vertical / horizontal."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        // 竖直是滚动条的默认方向，不需要额外设置。
        std::unique_ptr<DuiScrollBar> sbVert(new DuiScrollBar());
        sbVert->SetRange(0, kScrollRangeMax);
        sbVert->SetPage(kScrollPageSize);
        sbVert->SetPos(kScrollPos);

        std::unique_ptr<DuiScrollBar> sbHorz(new DuiScrollBar());
        sbHorz->SetHorizontal(true);
        sbHorz->SetRange(0, kScrollRangeMax);
        sbHorz->SetPage(kScrollPageSize);
        sbHorz->SetPos(kScrollPos);

        row->AddChild(std::move(sbVert), DuiLayout::Hint().Fixed(14));
        row->AddChild(std::move(sbHorz), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("scrollbar-states"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("emojipanel-default"),
               _T("DuiEmojiPanel default 8×3 grid."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiEmojiPanel> ep(new DuiEmojiPanel());
        row->AddChild(std::move(ep), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("emojipanel-default"), std::move(row), 200);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tabpage-content"),
               _T("DuiTabPage hosting a small content area under a tab strip."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        // 标签条与内容区之间不留间距，两者才连成一体，看上去像一个标签页。
        std::unique_ptr<DuiVBox> vb(new DuiVBox());
        vb->SetGap(0);

        std::unique_ptr<DuiTab> t(new DuiTab());
        t->AddTab(_T("Profile"));
        t->AddTab(_T("Activity"));
        t->AddTab(_T("Notes"));
        t->SetCurSel(0, false);
        vb->AddChild(std::move(t), DuiLayout::Hint().Fixed(36));
        vb->AddChild(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Profile content panel"),
                             RGB(245, 245, 245))),
            DuiLayout::Hint().Weight(1));

        row->AddChild(std::move(vb), DuiLayout::Hint().Fixed(400));
        AddVariantRowCapture(page.get(), _T("tabpage-content"),
                             std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 八、窗口宿主 -------------------------------------------------

    AddSection(page.get(), _T("host-tree"),
               _T("DuiHost composing a small tree: root VBox holds a header label and an HBox of buttons."));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());

        std::unique_ptr<DuiVBox> root(new DuiVBox());
        root->SetPadding(10);
        root->SetGap(8);

        std::unique_ptr<DuiLabel> title(new DuiLabel());
        title->SetText(_T("Login"));
        title->SetTextColor(RGB(20, 20, 20));
        root->AddChild(std::move(title), DuiLayout::Hint().Fixed(24));

        std::unique_ptr<DuiHBox> buttons(new DuiHBox());
        buttons->SetGap(8);

        std::unique_ptr<DuiButton> bOk(new DuiButton());
        bOk->SetText(_T("OK"));

        std::unique_ptr<DuiButton> bCancel(new DuiButton());
        bCancel->SetText(_T("Cancel"));

        buttons->AddChild(std::move(bOk), DuiLayout::Hint().Fixed(80));
        buttons->AddChild(std::move(bCancel), DuiLayout::Hint().Fixed(80));
        root->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(28));

        row->AddChild(std::move(root), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("host-tree"),
                             std::move(row), 80);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 本分组的页面列表
// =====================================================================

const PageEntry* GetDocCapturePages(int& outCount)
{
    static const PageEntry s_pages[] = {
        // 最后一个字段是 false：本页只供命令行截图模式访问，不出现在导航树里。
        { _T("doc-captures"), _T("文档配图夹具"), _T("Doc Captures"),
          &Build_DocCaptures, false },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
