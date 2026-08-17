/**
 *  基础控件分组的演示页面：文本标签、按钮、徽标、头像、分隔线与分组框、
 *  浮动提示条。这几个页面对应左侧导航树「基础控件」分组下的各个节点，
 *  版式一律由 PageKit.h 提供的 AddSection / AddVariantRow 拼出来，本文件
 *  只负责准备演示控件、示例数据与说明文案。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiBadge.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/Basic/DuiGroupBox.h"
#include "Controls/Basic/DuiToast.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 共用的颜色与尺寸
// =====================================================================

// 品牌蓝。本文件多个页面的强调色都取这一个值，集中定义免得各处各抄一遍
// RGB 分量。
const COLORREF kBrandBlue = RGB(45, 108, 223);
// 演示控件下方那行小字说明的文字色。
const COLORREF kCaptionColor = RGB(80, 80, 80);
// 纯白。按钮皮肤上的文字、提示条的文字与图标都用它。
const COLORREF kWhiteColor = RGB(255, 255, 255);
// 演示控件下方那行小字说明的行高（像素）。
const int kCaptionH = 18;

// =====================================================================
// 位图合成
//
// 画廊不依赖任何外部图片文件，演示需要的位图都在这里按像素合成，方便
// 单独运行 DuiGallery.exe 就能看到完整效果。合成出来的 HBITMAP 一律保存
// 在页面构建函数的静态变量里，生命期到进程结束为止 —— 页面每次切换都会
// 重建控件，位图必须比控件活得久；进程退出时由操作系统统一回收，本文件
// 不单独释放。
// =====================================================================

// 32 位位图里每个像素占的字节数，四个字节依次是蓝、绿、红、alpha。
const int kBytesPerPixel = 4;
// 完全不透明像素的 alpha 值。
const BYTE kOpaqueAlpha = 255;

// 新建一张正方形、自上而下存放的 32 位 DIBSection 位图。
//   size：边长（像素），必须大于 0。
//   ppBits：出参，返回像素缓冲的首地址，不能为空。缓冲由位图自身持有，
//           调用方只借用，不要单独释放。
// 返回：位图句柄，所有权归调用方；创建失败时返回 NULL，此时 *ppBits 为 NULL。
HBITMAP CreateSquareDib(int size, void** ppBits)
{
    if (ppBits == NULL)
    {
        return NULL;
    }
    *ppBits = NULL;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    // 高度取负值表示像素自上而下存放，这样下面按行遍历时 y 与屏幕坐标同向。
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBitmap == NULL)
    {
        return NULL;
    }
    *ppBits = pBits;
    return hBitmap;
}

// 在一张正方形位图上点亮一个像素。
//   pBits：像素缓冲首地址，不能为空。
//   bitmapSize：位图边长（像素）。
//   x、y：像素坐标，落在位图之外时本函数直接返回，不做任何处理。
//   color：像素颜色。alpha 固定写成完全不透明。
void SetIconPixel(BYTE* pBits, int bitmapSize, int x, int y, COLORREF color)
{
    if (x < 0 || x >= bitmapSize || y < 0 || y >= bitmapSize)
    {
        return;
    }
    BYTE* pPixel = pBits + (y * bitmapSize + x) * kBytesPerPixel;
    pPixel[0] = GetBValue(color);
    pPixel[1] = GetGValue(color);
    pPixel[2] = GetRValue(color);
    pPixel[3] = kOpaqueAlpha;
}

// 在一张正方形位图上填充一个矩形区域。
//   pBits：像素缓冲首地址，不能为空。
//   bitmapSize：位图边长（像素）。
//   x0、y0：矩形左上角坐标，包含在内。
//   x1、y1：矩形右下角坐标，不包含在内。
//   color：填充颜色。
void FillIconBlock(BYTE* pBits, int bitmapSize,
                   int x0, int y0, int x1, int y1, COLORREF color)
{
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            SetIconPixel(pBits, bitmapSize, x, y, color);
        }
    }
}

// ---- 按钮的九宫格皮肤位图 ----

// 皮肤位图的边长（像素）。位图本身很小，靠九宫格拉伸铺满整个按钮。
const int kSkinBitmapSize = 12;
// 皮肤位图四周纯色边框的宽度（像素）。它同时用作 SetBgInsets 的四个边距，
// 于是四角像素按原大小绘制、只有中间被拉伸，按钮放到任何尺寸边框都不变形。
const int kSkinBorderWidth = 3;

// 合成一张按钮皮肤位图：四周一圈纯色边框，中间纯色填充。
//   borderColor：边框颜色。
//   centerColor：中心填充色。
// 返回：位图句柄，所有权归调用方；创建失败时返回 NULL。
HBITMAP MakeButtonSkinBitmap(COLORREF borderColor, COLORREF centerColor)
{
    void* pBits = NULL;
    HBITMAP hBitmap = CreateSquareDib(kSkinBitmapSize, &pBits);
    if (hBitmap == NULL)
    {
        return NULL;
    }
    BYTE* p = static_cast<BYTE*>(pBits);
    for (int y = 0; y < kSkinBitmapSize; ++y)
    {
        for (int x = 0; x < kSkinBitmapSize; ++x)
        {
            bool bOnBorder = (x < kSkinBorderWidth
                              || x >= kSkinBitmapSize - kSkinBorderWidth
                              || y < kSkinBorderWidth
                              || y >= kSkinBitmapSize - kSkinBorderWidth);
            SetIconPixel(p, kSkinBitmapSize, x, y, bOnBorder ? borderColor : centerColor);
        }
    }
    return hBitmap;
}

// ---- 按钮的前置图标位图 ----

// 图标位图的边长（像素）。DuiButton 默认按 16 像素绘制前置图标。
const int kLeadingIconSize = 16;
// 加号笔画的粗细（像素）。
const int kPlusStrokeWidth = 2;
// 加号笔画两端到位图边缘留出的空白（像素）。
const int kPlusMargin = 2;

// 合成一张 32 位预乘 alpha 的加号图标，给「前置图标」段落当演示素材。
// 笔画为白色，其余像素 alpha 为 0，因此叠在任何底色的按钮上都只看到加号。
// 返回：位图句柄，所有权归调用方；创建失败时返回 NULL。
HBITMAP MakePlusIconBitmap()
{
    void* pBits = NULL;
    HBITMAP hBitmap = CreateSquareDib(kLeadingIconSize, &pBits);
    if (hBitmap == NULL)
    {
        return NULL;
    }
    BYTE* p = static_cast<BYTE*>(pBits);
    // 先整张清零，未被笔画覆盖的像素 alpha 为 0，绘制时完全透明。
    ::ZeroMemory(p, kLeadingIconSize * kLeadingIconSize * kBytesPerPixel);

    // 笔画在两个方向上的起止位置：横杠占中间 kPlusStrokeWidth 行、左右各留
    // kPlusMargin 空白；竖杠与之对称。
    const int strokeBegin = (kLeadingIconSize - kPlusStrokeWidth) / 2;
    const int strokeEnd = strokeBegin + kPlusStrokeWidth;
    const int spanBegin = kPlusMargin;
    const int spanEnd = kLeadingIconSize - kPlusMargin;

    FillIconBlock(p, kLeadingIconSize, spanBegin, strokeBegin, spanEnd, strokeEnd, kWhiteColor);
    FillIconBlock(p, kLeadingIconSize, strokeBegin, spanBegin, strokeEnd, spanEnd, kWhiteColor);
    return hBitmap;
}

// ---- 头像的图片源位图 ----

// 头像演示用的位图边长（像素）。
const int kAvatarBitmapSize = 32;

// 合成一张竖直双色渐变位图给头像当图片源。用渐变而不是纯色，是为了让裁剪
// 出来的头像看得出是一张图片，而不是一个纯色圆块。
//   topColor：位图顶端的颜色。
//   bottomColor：位图底端的颜色。
// 返回：位图句柄，所有权归调用方；创建失败时返回 NULL。
HBITMAP MakeAvatarGradientBitmap(COLORREF topColor, COLORREF bottomColor)
{
    void* pBits = NULL;
    HBITMAP hBitmap = CreateSquareDib(kAvatarBitmapSize, &pBits);
    if (hBitmap == NULL)
    {
        return NULL;
    }
    BYTE* p = static_cast<BYTE*>(pBits);
    for (int y = 0; y < kAvatarBitmapSize; ++y)
    {
        // 插值系数自上而下从 0 走到 1。
        float t = static_cast<float>(y) / static_cast<float>(kAvatarBitmapSize - 1);
        int r = GetRValue(topColor) + static_cast<int>((GetRValue(bottomColor) - GetRValue(topColor)) * t);
        int g = GetGValue(topColor) + static_cast<int>((GetGValue(bottomColor) - GetGValue(topColor)) * t);
        int b = GetBValue(topColor) + static_cast<int>((GetBValue(bottomColor) - GetBValue(topColor)) * t);
        COLORREF rowColor = RGB(r, g, b);
        for (int x = 0; x < kAvatarBitmapSize; ++x)
        {
            SetIconPixel(p, kAvatarBitmapSize, x, y, rowColor);
        }
    }
    return hBitmap;
}

// ---- 提示条的图标位图 ----

// 提示条图标位图的边长（像素）。DuiToast 默认按 16 像素绘制图标。
const int kToastIconSize = 16;

// 提示条演示用的图标形状。
enum ToastIconKind
{
    // 信息：方框加中间一竖，对应蓝色提示。
    ToastIconInfo = 0,
    // 成功：一个对勾，对应绿色提示。
    ToastIconSuccess = 1,
    // 警告：三角形加中间一竖，对应橙色提示。
    ToastIconWarning = 2,
    // 错误：一个叉，对应红色提示。
    ToastIconError = 3,
};

// 合成一张提示条图标位图。
//   kind：要画的图标形状。
//   color：笔画颜色。提示条底色较深，四个图标都传白色。
// 返回：位图句柄，所有权归调用方；创建失败时返回 NULL。
//
// 下面各分支里的坐标都是在 16×16 的像素网格上手工点出来的位置，逐个提成
// 具名常量反而看不出画的是什么形状，因此保留字面量，并在每一段前写明这
// 一笔画的是图形的哪个部分。
HBITMAP MakeToastIconBitmap(ToastIconKind kind, COLORREF color)
{
    void* pBits = NULL;
    HBITMAP hBitmap = CreateSquareDib(kToastIconSize, &pBits);
    if (hBitmap == NULL)
    {
        return NULL;
    }
    BYTE* p = static_cast<BYTE*>(pBits);
    // 先整张清零，未被笔画覆盖的像素完全透明。
    ::ZeroMemory(p, kToastIconSize * kToastIconSize * kBytesPerPixel);

    switch (kind)
    {
    //信息图标：外面一圈方框，中间上方一个点、下方一条竖线。
    case ToastIconInfo:
        for (int i = 2; i <= 13; ++i)
        {
            SetIconPixel(p, kToastIconSize, i, 1, color);
            SetIconPixel(p, kToastIconSize, i, 14, color);
            SetIconPixel(p, kToastIconSize, 1, i, color);
            SetIconPixel(p, kToastIconSize, 14, i, color);
        }
        // 竖线上方那个独立的点。
        FillIconBlock(p, kToastIconSize, 7, 5, 9, 6, color);
        // 竖线本体。
        FillIconBlock(p, kToastIconSize, 7, 7, 9, 13, color);
        break;

    //成功图标：一条折线组成的对勾，先从左下走到中下，再从中下走到右上。
    case ToastIconSuccess:
        for (int k = 0; k < 4; ++k)
        {
            SetIconPixel(p, kToastIconSize, 3 + k, 8 + k, color);
            SetIconPixel(p, kToastIconSize, 4 + k, 8 + k, color);
        }
        for (int k = 0; k < 7; ++k)
        {
            SetIconPixel(p, kToastIconSize, 7 + k, 11 - k, color);
            SetIconPixel(p, kToastIconSize, 8 + k, 11 - k, color);
        }
        break;

    //警告图标：一个三角形轮廓，中间一条竖线，竖线下面一个点。
    case ToastIconWarning:
        // 三角形的两条斜边：从顶点 (8, 2) 分别走到 (1, 13) 与 (14, 13)。
        for (int k = 0; k <= 11; ++k)
        {
            int xLeft = 8 - (7 * k) / 11;
            int xRight = 8 + (7 * k) / 11;
            SetIconPixel(p, kToastIconSize, xLeft, 2 + k, color);
            SetIconPixel(p, kToastIconSize, xRight, 2 + k, color);
        }
        // 三角形的底边。
        for (int x = 1; x <= 14; ++x)
        {
            SetIconPixel(p, kToastIconSize, x, 13, color);
        }
        // 中间的竖线。
        FillIconBlock(p, kToastIconSize, 7, 6, 9, 10, color);
        // 竖线下方那个独立的点。
        FillIconBlock(p, kToastIconSize, 7, 11, 9, 12, color);
        break;

    //错误图标：两条对角线组成的叉。
    case ToastIconError:
        for (int k = 0; k < 12; ++k)
        {
            SetIconPixel(p, kToastIconSize, 2 + k, 2 + k, color);
            SetIconPixel(p, kToastIconSize, 3 + k, 2 + k, color);
            SetIconPixel(p, kToastIconSize, 2 + k, 13 - k, color);
            SetIconPixel(p, kToastIconSize, 3 + k, 13 - k, color);
        }
        break;

    //取值超出上面四种形状时不画任何笔画，返回一张全透明位图。
    default:
        break;
    }
    return hBitmap;
}

// =====================================================================
// 提示条页面用的控件编号
//
// 提示条页面靠通知里的 ctrlId 区分是哪个按钮被按下，所以每个按钮都要有
// 各自的编号。这些编号只在本页面内部使用，取一段不与其它页面重叠的值即可。
// =====================================================================

// 触发信息提示的按钮。
const int kToastBtnInfo = 9001;
// 触发成功提示的按钮。
const int kToastBtnSuccess = 9002;
// 触发警告提示的按钮。
const int kToastBtnWarning = 9003;
// 触发错误提示的按钮。
const int kToastBtnError = 9004;
// 触发超长文本提示（验证截断）的按钮。
const int kToastBtnLongText = 9005;
// 触发不带图标的提示的按钮。
const int kToastBtnNoIcon = 9006;
// 触发 8 秒长时长提示的按钮。
const int kToastBtnLongDur = 9007;
// 触发「显示后立即取消」的按钮。
const int kToastBtnCancel = 9008;
// 触发连续三次显示的按钮。
const int kToastBtnRapid = 9009;

// 提示条的默认显示时长（毫秒），与 DuiToast 自身的默认值一致。
const int kToastDefaultDurationMs = 3000;
// 长时长演示使用的显示时长（毫秒）。
const int kToastLongDurationMs = 8000;
// 「显示后立即取消」演示使用的显示时长（毫秒）。取一个足够长的值，这样
// 只要提示条真的显示出来就一定看得见，从而能验证取消确实生效。
const int kToastCancelDurationMs = 5000;
// 长文本演示的最大宽度（像素）。超过这个宽度的文字会被截断并追加省略号。
const int kToastEllipsisMaxWidth = 360;
// 提示条的圆角半径（像素）。
const int kToastCornerRadius = 16;

} // 匿名命名空间

// ===== 文本标签 ======================================================

std::unique_ptr<DuiControl> Build_Label()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 本页面每一行都只放单行文字，行高统一取 24 像素。
    const int kTextRowH = 24;

    AddSection(page.get(),
               Txt(_T("纯文本"), _T("Plain text")),
               Txt(_T("默认的 ModeText 模式。文字颜色用 SetTextColor 指定，对齐方式用 ")
                   _T("SetTextAlign 指定。"),
                   _T("ModeText. Different colors and alignments via SetTextColor / SetTextAlign.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> plain(new DuiLabel());
        plain->SetText(Txt(_T("默认黑色"), _T("Default black")));

        std::unique_ptr<DuiLabel> colored(new DuiLabel());
        colored->SetText(Txt(_T("品牌蓝"), _T("Brand blue")));
        colored->SetTextColor(kBrandBlue);

        std::unique_ptr<DuiLabel> aligned(new DuiLabel());
        aligned->SetText(Txt(_T("右对齐"), _T("Right aligned")));
        aligned->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        row->AddChild(std::move(plain), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(colored), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(aligned), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    AddSection(page.get(),
               Txt(_T("超链接（自动打开）"), _T("Hyperlink (auto-navigate)")),
               Txt(_T("把模式设成 ModeLink 并调用 SetAutoNavigate(true) 之后，点击标签会")
                   _T("通过 ShellExecute 打开 SetUrl 设置的地址。"),
                   _T("ModeLink + SetAutoNavigate(true). Click opens the URL via ShellExecute.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> link(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(Txt(_T("打开 example.com"), _T("Open example.com")));
        link->SetUrl(_T("https://example.com"));
        link->SetAutoNavigate(true);

        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    AddSection(page.get(),
               Txt(_T("超链接（不自动打开）"), _T("Hyperlink (no navigation)")),
               Txt(_T("同样是 ModeLink，但关闭了自动打开。点击只向上抛出 DUIN_CLICK 通知，")
                   _T("由父窗口决定接下来做什么。"),
                   _T("ModeLink without auto-navigate. Click bubbles DUIN_CLICK; the parent decides.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> link(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(Txt(_T("注册账号"), _T("Register account")));
        link->SetAutoNavigate(false);

        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    AddSection(page.get(),
               Txt(_T("已访问状态"), _T("Visited state")),
               Txt(_T("链接被点击之后会换成已访问颜色。也可以调用 SetVisited(bool) 直接")
                   _T("设置，不必真的点一次。"),
                   _T("After a click the link adopts the visited color. SetVisited(bool) toggles it programmatically.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> link(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(Txt(_T("这一条初始就是已访问状态"), _T("This one starts visited")));
        link->SetVisited(true);

        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    AddSection(page.get(),
               Txt(_T("可选中（拖动选择、Ctrl+C 复制）"),
                   _T("Selectable (drag-select + Ctrl+C copy)")),
               Txt(_T("调用 SetSelectable(true) 之后，可以按住鼠标拖选一段文字再按 Ctrl+C ")
                   _T("复制；没有选中任何内容时 Ctrl+C 复制整条文字，Ctrl+A 选中全部。")
                   _T("该能力只支持单行，调用 SetWordWrap 开启折行之后会自动关闭。"),
                   _T("SetSelectable(true) lets users drag-select a substring and copy it with ")
                   _T("Ctrl+C; with an empty selection Ctrl+C copies the whole text, and Ctrl+A ")
                   _T("selects all. Single-line only - it turns itself off under SetWordWrap.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        // 工号与邮箱是与语言无关的示例数据，照原样写；地址是自然语言，
        // 需要两份文案。
        std::unique_ptr<DuiLabel> employeeId(new DuiLabel());
        employeeId->SetText(_T("EMP100086"));
        employeeId->SetSelectable(true);

        std::unique_ptr<DuiLabel> mail(new DuiLabel());
        mail->SetText(_T("balloonwj@qq.com"));
        mail->SetSelectable(true);

        std::unique_ptr<DuiLabel> address(new DuiLabel());
        address->SetText(Txt(_T("北京市朝阳区望京街 10 号 1801"),
                             _T("Room 1801, 10 Wangjing Street, Beijing")));
        address->SetSelectable(true);

        row->AddChild(std::move(employeeId), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(mail), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(address), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    AddSection(page.get(),
               Txt(_T("自定义选中底色"), _T("Selection color override")),
               Txt(_T("SetSelectionColor 可以改选中区域的高亮色，默认是浅蓝 ")
                   _T("RGB(217, 232, 252)。"),
                   _T("SetSelectionColor changes the highlight color (default light blue ")
                   _T("RGB(217, 232, 252)).")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> defaultHighlight(new DuiLabel());
        defaultHighlight->SetText(Txt(_T("默认高亮色"), _T("Default highlight")));
        defaultHighlight->SetSelectable(true);

        std::unique_ptr<DuiLabel> yellowHighlight(new DuiLabel());
        yellowHighlight->SetText(Txt(_T("黄色高亮"), _T("Yellow highlight")));
        yellowHighlight->SetSelectable(true);
        yellowHighlight->SetSelectionColor(RGB(255, 230, 130));

        std::unique_ptr<DuiLabel> pinkHighlight(new DuiLabel());
        pinkHighlight->SetText(Txt(_T("粉色高亮"), _T("Pink highlight")));
        pinkHighlight->SetSelectable(true);
        pinkHighlight->SetSelectionColor(RGB(255, 200, 220));

        row->AddChild(std::move(defaultHighlight), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(yellowHighlight), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(pinkHighlight), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 按钮 ==========================================================

std::unique_ptr<DuiControl> Build_Button()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("主操作按钮"), _T("PushButton")),
               Txt(_T("品牌蓝圆角按钮，白色文字。鼠标悬停时底色加深，禁用之后整体置灰")
                   _T("并且不再响应点击。"),
                   _T("Brand-blue rounded primary action. White label, hover deepens, disabled grays out.")));
    {
        // 这一段两个按钮同宽。
        const int kBtnW = 120;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiButton> normal(new DuiButton());
        normal->SetText(Txt(_T("普通"), _T("Normal")));

        std::unique_ptr<DuiButton> disabled(new DuiButton());
        disabled->SetText(Txt(_T("禁用"), _T("Disabled")));
        disabled->SetEnabled(false);

        row->AddChild(std::move(normal), DuiLayout::Hint().Fixed(kBtnW));
        row->AddChild(std::move(disabled), DuiLayout::Hint().Fixed(kBtnW));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("主操作按钮 —— 四种状态并排"), _T("PushButton - 4-state strip")),
               Txt(_T("用 DebugSetHover / DebugSetPressed 强制指定视觉状态，把普通、悬停、")
                   _T("按下、禁用四种外观并排画出来，一眼看全整套色板。"),
                   _T("Normal / Hover / Pressed / Disabled rendered side-by-side via the Debug* APIs.")));
    {
        // 四个状态按钮同宽。
        const int kStateBtnW = 110;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiButton> normal(new DuiButton());
        normal->SetText(Txt(_T("普通"), _T("Normal")));

        std::unique_ptr<DuiButton> hover(new DuiButton());
        hover->SetText(Txt(_T("悬停"), _T("Hover")));
        hover->DebugSetHover(true);

        std::unique_ptr<DuiButton> pressed(new DuiButton());
        pressed->SetText(Txt(_T("按下"), _T("Pressed")));
        pressed->DebugSetHover(true);
        pressed->DebugSetPressed(true);

        std::unique_ptr<DuiButton> disabled(new DuiButton());
        disabled->SetText(Txt(_T("禁用"), _T("Disabled")));
        disabled->SetEnabled(false);

        row->AddChild(std::move(normal), DuiLayout::Hint().Fixed(kStateBtnW));
        row->AddChild(std::move(hover), DuiLayout::Hint().Fixed(kStateBtnW));
        row->AddChild(std::move(pressed), DuiLayout::Hint().Fixed(kStateBtnW));
        row->AddChild(std::move(disabled), DuiLayout::Hint().Fixed(kStateBtnW));
        // 这一行是文档配图的取景范围，截图文件名固定，不随语言变化。
        AddVariantRowCapture(page.get(), _T("button-pushbutton-states"), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("图标风格按钮"), _T("IconButton")),
               Txt(_T("浅色填充，左侧带一个图标字形，适合放在工具栏一类空间紧凑的位置。"),
                   _T("Light-fill button with a left-side icon glyph. Used for compact actions.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiButton> button(new DuiButton());
        button->SetButtonType(DuiButton::StyleIcon);
        button->SetText(Txt(_T("打开文件"), _T("Open File")));

        row->AddChild(std::move(button), DuiLayout::Hint().Fixed(160));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("复选框"), _T("Checkbox")),
               Txt(_T("点击切换选中状态；按下之后把鼠标拖出控件范围再抬起，这次点击不")
                   _T("生效。代码里调用 SetCheck 只改状态，不会发出界面事件。"),
                   _T("Toggles on click; dragging out before release cancels it. A programmatic ")
                   _T("SetCheck does not fire UI events.")));
    {
        // 三个复选框同宽。
        const int kCheckW = 140;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiButton> remember(new DuiButton());
        remember->SetButtonType(DuiButton::StyleCheckbox);
        remember->SetText(Txt(_T("记住我"), _T("Remember me")));

        std::unique_ptr<DuiButton> preChecked(new DuiButton());
        preChecked->SetButtonType(DuiButton::StyleCheckbox);
        preChecked->SetText(Txt(_T("默认已选中"), _T("Pre-checked")));
        preChecked->SetCheck(true, false);

        std::unique_ptr<DuiButton> disabled(new DuiButton());
        disabled->SetButtonType(DuiButton::StyleCheckbox);
        disabled->SetText(Txt(_T("禁用"), _T("Disabled")));
        disabled->SetEnabled(false);

        row->AddChild(std::move(remember), DuiLayout::Hint().Fixed(kCheckW));
        row->AddChild(std::move(preChecked), DuiLayout::Hint().Fixed(kCheckW));
        row->AddChild(std::move(disabled), DuiLayout::Hint().Fixed(kCheckW));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("单选按钮（组号 7）"), _T("Radio (group = 7)")),
               Txt(_T("同一个父容器内、radioGroup 相同的单选按钮之间互斥。点击已经选中的")
                   _T("那一个不做任何处理。"),
                   _T("Mutually exclusive within the same parent and matching radioGroup. ")
                   _T("Clicking the selected one is a no-op.")));
    {
        // 本段三个单选按钮所在的组号。
        const int kStatusRadioGroup = 7;
        // 三个单选按钮同宽。
        const int kRadioW = 110;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiButton> online(new DuiButton());
        online->SetButtonType(DuiButton::StyleRadio);
        online->SetRadioGroup(kStatusRadioGroup);
        online->SetText(Txt(_T("在线"), _T("Online")));
        online->SetCheck(true, false);

        std::unique_ptr<DuiButton> away(new DuiButton());
        away->SetButtonType(DuiButton::StyleRadio);
        away->SetRadioGroup(kStatusRadioGroup);
        away->SetText(Txt(_T("离开"), _T("Away")));

        std::unique_ptr<DuiButton> busy(new DuiButton());
        busy->SetButtonType(DuiButton::StyleRadio);
        busy->SetRadioGroup(kStatusRadioGroup);
        busy->SetText(Txt(_T("忙碌"), _T("Busy")));

        row->AddChild(std::move(online), DuiLayout::Hint().Fixed(kRadioW));
        row->AddChild(std::move(away), DuiLayout::Hint().Fixed(kRadioW));
        row->AddChild(std::move(busy), DuiLayout::Hint().Fixed(kRadioW));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("位图皮肤按钮（九宫格）"), _T("Skinned PushButton (9-grid bitmap)")),
               Txt(_T("SetBgBitmap 依次传入普通、悬停、按下、禁用四张位图，再用 ")
                   _T("SetBgInsets(3, 3, 3, 3) 指定九宫格边距。悬停时中心色加深，按下再")
                   _T("深一档；四角的像素按原大小绘制，只有中间被拉伸，所以按钮放到任何")
                   _T("尺寸边框都保持 3 像素宽、不变形。"),
                   _T("DuiButton::SetBgBitmap(normal / hover / pressed / disabled) plus ")
                   _T("SetBgInsets(3, 3, 3, 3). Hover deepens the center, press deepens it ")
                   _T("further; the corners stay crisp at any size.")));
    {
        // 一套按钮皮肤，四种状态各一张位图。
        struct ButtonSkinSet
        {
            // 普通状态的位图。
            HBITMAP normal;
            // 鼠标悬停时的位图。
            HBITMAP hover;
            // 按下时的位图。
            HBITMAP pressed;
            // 禁用时的位图。
            HBITMAP disabled;
        };

        // 位图必须比按钮活得久 —— 页面每次切换都会重建按钮，所以放静态变量里
        // 一次性合成，进程退出时由操作系统回收。
        static ButtonSkinSet s_blueSkin =
        {
            MakeButtonSkinBitmap(RGB( 30,  74, 153), RGB( 45, 108, 223)),   // 普通：深蓝边 + 品牌蓝底
            MakeButtonSkinBitmap(RGB( 30,  74, 153), RGB( 37,  89, 184)),   // 悬停：底色加深一档
            MakeButtonSkinBitmap(RGB( 20,  56, 120), RGB( 30,  74, 153)),   // 按下：再深一档
            MakeButtonSkinBitmap(RGB(150, 150, 150), RGB(200, 200, 200))    // 禁用：整体置灰
        };
        static ButtonSkinSet s_tealSkin =
        {
            MakeButtonSkinBitmap(RGB( 14,  90,  90), RGB( 30, 160, 160)),
            MakeButtonSkinBitmap(RGB( 14,  90,  90), RGB( 24, 140, 140)),
            MakeButtonSkinBitmap(RGB(  8,  60,  60), RGB( 18, 110, 110)),
            MakeButtonSkinBitmap(RGB(150, 150, 150), RGB(205, 215, 215))
        };
        static ButtonSkinSet s_redSkin =
        {
            MakeButtonSkinBitmap(RGB(140,  40,  40), RGB(210,  60,  60)),
            MakeButtonSkinBitmap(RGB(140,  40,  40), RGB(185,  50,  50)),
            MakeButtonSkinBitmap(RGB(110,  30,  30), RGB(150,  35,  35)),
            MakeButtonSkinBitmap(RGB(150, 150, 150), RGB(220, 200, 200))
        };

        // 四个皮肤按钮同宽。
        const int kSkinBtnW = 120;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiButton> blue(new DuiButton());
        blue->SetText(Txt(_T("蓝色"), _T("Blue")));
        blue->SetBgBitmap(s_blueSkin.normal, s_blueSkin.hover, s_blueSkin.pressed, s_blueSkin.disabled);
        blue->SetBgInsets(kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth);

        std::unique_ptr<DuiButton> teal(new DuiButton());
        teal->SetText(Txt(_T("青色"), _T("Teal")));
        teal->SetBgBitmap(s_tealSkin.normal, s_tealSkin.hover, s_tealSkin.pressed, s_tealSkin.disabled);
        teal->SetBgInsets(kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth);

        std::unique_ptr<DuiButton> red(new DuiButton());
        red->SetText(Txt(_T("红色"), _T("Red")));
        red->SetBgBitmap(s_redSkin.normal, s_redSkin.hover, s_redSkin.pressed, s_redSkin.disabled);
        red->SetBgInsets(kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth);

        std::unique_ptr<DuiButton> disabled(new DuiButton());
        disabled->SetText(Txt(_T("禁用"), _T("Disabled")));
        disabled->SetEnabled(false);
        disabled->SetBgBitmap(s_blueSkin.normal, s_blueSkin.hover, s_blueSkin.pressed, s_blueSkin.disabled);
        disabled->SetBgInsets(kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth, kSkinBorderWidth);

        row->AddChild(std::move(blue), DuiLayout::Hint().Fixed(kSkinBtnW));
        row->AddChild(std::move(teal), DuiLayout::Hint().Fixed(kSkinBtnW));
        row->AddChild(std::move(red), DuiLayout::Hint().Fixed(kSkinBtnW));
        row->AddChild(std::move(disabled), DuiLayout::Hint().Fixed(kSkinBtnW));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("视觉变体（只对主操作按钮生效）"), _T("Variants (StylePushButton only)")),
               Txt(_T("六种视觉变体乘四种状态。只有 StylePushButton 会读取 Variant，复选框、")
                   _T("单选按钮和图标按钮忽略它。位图皮肤（SetBgBitmap）的优先级高于 ")
                   _T("Variant，两者同时设置时按位图绘制。"),
                   _T("Six visual variants by four states (Normal / Hover / Pressed / Disabled). ")
                   _T("Only StylePushButton consults Variant; Checkbox / Radio / Icon ignore it. ")
                   _T("A bitmap skin (SetBgBitmap) takes precedence over Variant.")));
    {
        // 每一行的行首变体名称标签的宽度。
        const int kVariantNameW = 80;
        // 每行四个状态按钮的宽度。
        const int kVariantBtnW = 90;

        // 一行演示：一个变体，配四个状态按钮。
        struct VariantRowDesc
        {
            // 这一行演示的变体。
            DuiButton::Variant variant;
            // 行首显示的变体名称。它是枚举名，不需要翻译。
            LPCTSTR name;
        };
        const VariantRowDesc variantRows[] =
        {
            { DuiButton::Variant::Primary,  _T("Primary")  },
            { DuiButton::Variant::Default,  _T("Default")  },
            { DuiButton::Variant::Outlined, _T("Outlined") },
            { DuiButton::Variant::Ghost,    _T("Ghost")    },
            { DuiButton::Variant::Danger,   _T("Danger")   },
            { DuiButton::Variant::Text,     _T("Text")     },
        };
        const int kVariantRowCount = sizeof(variantRows) / sizeof(variantRows[0]);

        for (int i = 0; i < kVariantRowCount; ++i)
        {
            const VariantRowDesc& desc = variantRows[i];

            std::unique_ptr<DuiHBox> row(new DuiHBox());
            row->SetGap(8);

            std::unique_ptr<DuiLabel> name(new DuiLabel());
            name->SetText(desc.name);
            row->AddChild(std::move(name), DuiLayout::Hint().Fixed(kVariantNameW));

            std::unique_ptr<DuiButton> normal(new DuiButton());
            normal->SetVariant(desc.variant);
            normal->SetText(Txt(_T("普通"), _T("Normal")));

            std::unique_ptr<DuiButton> hover(new DuiButton());
            hover->SetVariant(desc.variant);
            hover->SetText(Txt(_T("悬停"), _T("Hover")));
            hover->DebugSetHover(true);

            std::unique_ptr<DuiButton> pressed(new DuiButton());
            pressed->SetVariant(desc.variant);
            pressed->SetText(Txt(_T("按下"), _T("Pressed")));
            pressed->DebugSetHover(true);
            pressed->DebugSetPressed(true);

            std::unique_ptr<DuiButton> disabled(new DuiButton());
            disabled->SetVariant(desc.variant);
            disabled->SetText(Txt(_T("禁用"), _T("Disabled")));
            disabled->SetEnabled(false);

            row->AddChild(std::move(normal), DuiLayout::Hint().Fixed(kVariantBtnW));
            row->AddChild(std::move(hover), DuiLayout::Hint().Fixed(kVariantBtnW));
            row->AddChild(std::move(pressed), DuiLayout::Hint().Fixed(kVariantBtnW));
            row->AddChild(std::move(disabled), DuiLayout::Hint().Fixed(kVariantBtnW));
            AddVariantRow(page.get(), std::move(row));
        }
    }

    AddSection(page.get(),
               Txt(_T("视觉变体 —— 实时交互"), _T("Variants - interactive (live hover / press)")),
               Txt(_T("同样是那六种变体，但没有用 Debug 接口强制状态。把鼠标移上去或者点")
                   _T("一下，可以直接看到每种变体的悬停与按下过渡效果。"),
                   _T("The same six variants without any Debug* state forcing - hover and click ")
                   _T("them to feel each variant's transition.")));
    {
        // 一个交互演示按钮：变体加上一句符合该变体语义的文案。
        struct VariantSample
        {
            // 按钮采用的变体。
            DuiButton::Variant variant;
            // 按钮上的文字。
            LPCTSTR text;
        };
        const VariantSample samples[] =
        {
            { DuiButton::Variant::Primary,  Txt(_T("保存"), _T("Save"))   },
            { DuiButton::Variant::Default,  Txt(_T("取消"), _T("Cancel")) },
            { DuiButton::Variant::Outlined, Txt(_T("应用"), _T("Apply"))  },
            { DuiButton::Variant::Ghost,    Txt(_T("更多"), _T("More"))   },
            { DuiButton::Variant::Danger,   Txt(_T("删除"), _T("Delete")) },
            { DuiButton::Variant::Text,     Txt(_T("跳过"), _T("Skip"))   },
        };
        const int kSampleCount = sizeof(samples) / sizeof(samples[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(10);
        for (int i = 0; i < kSampleCount; ++i)
        {
            std::unique_ptr<DuiButton> button(new DuiButton());
            button->SetVariant(samples[i].variant);
            button->SetText(samples[i].text);
            row->AddChild(std::move(button), DuiLayout::Hint().Fixed(96));
        }
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("抗锯齿开关（圆角对比）"), _T("Anti-aliasing on / off")),
               Txt(_T("上面一行调用 SetAntiAlias(false)，走 GDI 的 ::RoundRect，8 像素的")
                   _T("圆角上能看出明显的台阶；下面一行是默认的 SetAntiAlias(true)，走 ")
                   _T("GDI+ 的 DuiAA::FillRoundRect，边线连续没有锯齿。复选框的方框字形")
                   _T("同样受这个开关控制。"),
                   _T("The top row calls SetAntiAlias(false) and draws through GDI ::RoundRect - ")
                   _T("the 8px corners show stair-stepping. The bottom row is the default ")
                   _T("SetAntiAlias(true), drawing through GDI+ DuiAA::FillRoundRect with smooth ")
                   _T("edges. The checkbox glyph follows the same switch.")));
    {
        // 行首说明标签的宽度。
        const int kAaLabelW = 80;
        // 三个主操作按钮的宽度。
        const int kAaBtnW = 90;
        // 复选框较宽，单独取值。
        const int kAaCheckW = 130;

        // 上面一行：关闭抗锯齿。
        std::unique_ptr<DuiHBox> rowOff(new DuiHBox());
        rowOff->SetGap(12);

        std::unique_ptr<DuiLabel> labelOff(new DuiLabel());
        labelOff->SetText(Txt(_T("抗锯齿关"), _T("AA = off")));
        rowOff->AddChild(std::move(labelOff), DuiLayout::Hint().Fixed(kAaLabelW));

        std::unique_ptr<DuiButton> primaryOff(new DuiButton());
        primaryOff->SetText(_T("Primary"));
        primaryOff->SetAntiAlias(false);

        std::unique_ptr<DuiButton> defaultOff(new DuiButton());
        defaultOff->SetText(_T("Default"));
        defaultOff->SetVariant(DuiButton::Variant::Default);
        defaultOff->SetAntiAlias(false);

        std::unique_ptr<DuiButton> dangerOff(new DuiButton());
        dangerOff->SetText(_T("Danger"));
        dangerOff->SetVariant(DuiButton::Variant::Danger);
        dangerOff->SetAntiAlias(false);

        std::unique_ptr<DuiButton> checkOff(new DuiButton());
        checkOff->SetButtonType(DuiButton::StyleCheckbox);
        checkOff->SetText(Txt(_T("复选框"), _T("Checkbox")));
        checkOff->SetCheck(true, false);
        checkOff->SetAntiAlias(false);

        rowOff->AddChild(std::move(primaryOff), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOff->AddChild(std::move(defaultOff), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOff->AddChild(std::move(dangerOff), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOff->AddChild(std::move(checkOff), DuiLayout::Hint().Fixed(kAaCheckW));
        AddVariantRow(page.get(), std::move(rowOff));

        // 下面一行：默认开启抗锯齿。
        std::unique_ptr<DuiHBox> rowOn(new DuiHBox());
        rowOn->SetGap(12);

        std::unique_ptr<DuiLabel> labelOn(new DuiLabel());
        labelOn->SetText(Txt(_T("抗锯齿开（默认）"), _T("AA = on (default)")));
        rowOn->AddChild(std::move(labelOn), DuiLayout::Hint().Fixed(kAaLabelW));

        std::unique_ptr<DuiButton> primaryOn(new DuiButton());
        primaryOn->SetText(_T("Primary"));

        std::unique_ptr<DuiButton> defaultOn(new DuiButton());
        defaultOn->SetText(_T("Default"));
        defaultOn->SetVariant(DuiButton::Variant::Default);

        std::unique_ptr<DuiButton> dangerOn(new DuiButton());
        dangerOn->SetText(_T("Danger"));
        dangerOn->SetVariant(DuiButton::Variant::Danger);

        std::unique_ptr<DuiButton> checkOn(new DuiButton());
        checkOn->SetButtonType(DuiButton::StyleCheckbox);
        checkOn->SetText(Txt(_T("复选框"), _T("Checkbox")));
        checkOn->SetCheck(true, false);

        rowOn->AddChild(std::move(primaryOn), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOn->AddChild(std::move(defaultOn), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOn->AddChild(std::move(dangerOn), DuiLayout::Hint().Fixed(kAaBtnW));
        rowOn->AddChild(std::move(checkOn), DuiLayout::Hint().Fixed(kAaCheckW));
        AddVariantRow(page.get(), std::move(rowOn));
    }

    AddSection(page.get(),
               Txt(_T("复选框与单选按钮 —— 三种透明变体"),
                   _T("Checkbox / Radio - transparent variants")),
               Txt(_T("Ghost 与 Text 变体的外框完全透明，Outlined 变体的外框透明并带一圈")
                   _T("品牌色边线。无论哪一种透明变体，内部的小方框与圆环字形都始终可见")
                   _T("（取 kLight 系列色板），选中标记的颜色也保持不变。其余变体")
                   _T("（Primary / Default / Danger）用在复选框与单选按钮上时会回落到 ")
                   _T("kLight 系列，与历史视觉一致。"),
                   _T("Ghost and Text render a fully transparent frame; Outlined renders a ")
                   _T("transparent frame with a brand-colored border. Under every transparent ")
                   _T("variant the inner box / ring glyph stays visible (it uses the kLight ")
                   _T("palette) and the check mark keeps its color. The remaining variants ")
                   _T("(Primary / Default / Danger) fall back to the kLight palette on Checkbox ")
                   _T("and Radio, matching the historical look.")));
    {
        // 行首变体名称标签的宽度。
        const int kTransparentNameW = 80;
        // 每行四个演示控件的宽度。
        const int kTransparentCtrlW = 110;
        // 每一行的单选按钮各用一个独立的组号，避免跨行互斥。
        const int kTransparentRadioGroupBase = 100;

        // 一行演示：一个透明变体，配两个复选框与两个单选按钮。
        struct TransparentVariant
        {
            // 这一行演示的变体。
            DuiButton::Variant variant;
            // 行首显示的变体名称。它是枚举名，不需要翻译。
            LPCTSTR name;
        };
        const TransparentVariant variants[] =
        {
            { DuiButton::Variant::Ghost,    _T("Ghost")    },
            { DuiButton::Variant::Outlined, _T("Outlined") },
            { DuiButton::Variant::Text,     _T("Text")     },
        };
        const int kVariantCount = sizeof(variants) / sizeof(variants[0]);

        for (int i = 0; i < kVariantCount; ++i)
        {
            const TransparentVariant& item = variants[i];

            std::unique_ptr<DuiHBox> row(new DuiHBox());
            row->SetGap(20);

            std::unique_ptr<DuiLabel> name(new DuiLabel());
            name->SetText(item.name);
            row->AddChild(std::move(name), DuiLayout::Hint().Fixed(kTransparentNameW));

            // 复选框：未选中与已选中各一个。
            std::unique_ptr<DuiButton> unchecked(new DuiButton());
            unchecked->SetButtonType(DuiButton::StyleCheckbox);
            unchecked->SetVariant(item.variant);
            unchecked->SetText(Txt(_T("启用"), _T("Enable")));

            std::unique_ptr<DuiButton> checked(new DuiButton());
            checked->SetButtonType(DuiButton::StyleCheckbox);
            checked->SetVariant(item.variant);
            checked->SetText(Txt(_T("已选"), _T("Checked")));
            checked->SetCheck(true, false);

            // 单选按钮：同一行的两个属于同一组。
            std::unique_ptr<DuiButton> radioA(new DuiButton());
            radioA->SetButtonType(DuiButton::StyleRadio);
            radioA->SetVariant(item.variant);
            radioA->SetRadioGroup(kTransparentRadioGroupBase + i);
            radioA->SetText(Txt(_T("选项 A"), _T("Option A")));
            radioA->SetCheck(true, false);

            std::unique_ptr<DuiButton> radioB(new DuiButton());
            radioB->SetButtonType(DuiButton::StyleRadio);
            radioB->SetVariant(item.variant);
            radioB->SetRadioGroup(kTransparentRadioGroupBase + i);
            radioB->SetText(Txt(_T("选项 B"), _T("Option B")));

            row->AddChild(std::move(unchecked), DuiLayout::Hint().Fixed(kTransparentCtrlW));
            row->AddChild(std::move(checked), DuiLayout::Hint().Fixed(kTransparentCtrlW));
            row->AddChild(std::move(radioA), DuiLayout::Hint().Fixed(kTransparentCtrlW));
            row->AddChild(std::move(radioB), DuiLayout::Hint().Fixed(kTransparentCtrlW));
            AddVariantRow(page.get(), std::move(row));
        }
    }

    AddSection(page.get(),
               Txt(_T("自定义字号"), _T("SetTextPointSize / SetFont")),
               Txt(_T("SetFont(HFONT) 直接传入字体句柄；SetTextPointSize(pt, bold) 从 ")
                   _T("DuiResMgr 的缓存里取字体，业务代码不必自己管理句柄。下面三个按钮")
                   _T("文字相同，字号分别是 9 磅、11 磅和 14 磅加粗，差异肉眼可辨。"),
                   _T("SetFont(HFONT) takes a font handle directly; SetTextPointSize(pt, bold) ")
                   _T("pulls a cached font from DuiResMgr so callers never own a handle. The ")
                   _T("three buttons below share one caption at 9pt, 11pt and 14pt bold.")));
    {
        // 每个演示列的宽度。
        const int kFontColW = 120;
        // 演示按钮的高度，与整行高度配合留出下方小字的位置。
        const int kFontBtnH = 36;
        // 这一段每列是「按钮 + 小字」两行，所以行高比默认值大。
        const int kFontRowH = 70;

        // 一列演示：一个字号，加一行说明它是多少磅的小字。
        struct FontDemo
        {
            // 字号，单位为磅。
            int pointSize;
            // 是否加粗。
            bool bold;
            // 按钮下方的小字说明。
            LPCTSTR caption;
        };
        const FontDemo demos[] =
        {
            {  9, false, Txt(_T("9 磅"), _T("9pt"))           },
            { 11, false, Txt(_T("11 磅"), _T("11pt"))         },
            { 14, true,  Txt(_T("14 磅加粗"), _T("14pt bold")) },
        };
        const int kDemoCount = sizeof(demos) / sizeof(demos[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);
        for (int i = 0; i < kDemoCount; ++i)
        {
            std::unique_ptr<DuiVBox> column(new DuiVBox());
            column->SetGap(4);

            std::unique_ptr<DuiButton> button(new DuiButton());
            button->SetText(Txt(_T("保存"), _T("Save")));
            button->SetTextPointSize(demos[i].pointSize, demos[i].bold);

            std::unique_ptr<DuiLabel> caption(new DuiLabel());
            caption->SetText(demos[i].caption);
            caption->SetTextColor(kCaptionColor);

            column->AddChild(std::move(button), DuiLayout::Hint().Fixed(kFontBtnH));
            column->AddChild(std::move(caption), DuiLayout::Hint().Fixed(kCaptionH));
            row->AddChild(std::move(column), DuiLayout::Hint().Fixed(kFontColW));
        }
        AddVariantRow(page.get(), std::move(row), kFontRowH);
    }

    AddSection(page.get(),
               Txt(_T("前置图标（只对主操作按钮生效）"), _T("LeadingIcon (PushButton only)")),
               Txt(_T("SetLeadingIcon(HBITMAP) 配合 SetLeadingIconSize 与 SetLeadingIconGap ")
                   _T("使用。图标、间距、文字作为一个整体按 m_dtFlags 对齐，默认水平居中。")
                   _T("图标走 ::AlphaBlend 绘制，支持 32 位预乘 alpha 位图；HBITMAP 由调用方")
                   _T("持有，控件不负责释放。复选框、单选按钮和图标风格按钮忽略前置图标，")
                   _T("仍然画各自原有的字形。"),
                   _T("SetLeadingIcon(HBITMAP) together with SetLeadingIconSize / Gap. The whole ")
                   _T("group (icon + gap + text) is aligned by m_dtFlags, horizontally centered by ")
                   _T("default. The icon is drawn through ::AlphaBlend and accepts 32bpp ")
                   _T("premultiplied alpha; the HBITMAP stays caller-owned. Checkbox / Radio / ")
                   _T("Icon styles ignore the leading icon and keep their own glyph.")));
    {
        // 图标位图与皮肤位图一样只合成一次，生命期到进程结束为止。
        static HBITMAP s_plusIcon = MakePlusIconBitmap();

        // 带文字的按钮宽度。
        const int kIconBtnW = 110;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiButton> create(new DuiButton());
        create->SetText(Txt(_T("新建"), _T("New")));
        create->SetLeadingIcon(s_plusIcon);

        std::unique_ptr<DuiButton> leftAligned(new DuiButton());
        leftAligned->SetText(Txt(_T("DT_LEFT 对齐"), _T("DT_LEFT aligned")));
        leftAligned->SetLeadingIcon(s_plusIcon);
        leftAligned->SetTextAlign(DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        std::unique_ptr<DuiButton> danger(new DuiButton());
        danger->SetText(Txt(_T("删除"), _T("Delete")));
        danger->SetVariant(DuiButton::Variant::Danger);
        danger->SetLeadingIcon(s_plusIcon);

        // 文字为空时整组只剩图标，用来演示纯图标按钮。
        std::unique_ptr<DuiButton> iconOnly(new DuiButton());
        iconOnly->SetText(_T(""));
        iconOnly->SetLeadingIcon(s_plusIcon);

        row->AddChild(std::move(create), DuiLayout::Hint().Fixed(kIconBtnW));
        row->AddChild(std::move(leftAligned), DuiLayout::Hint().Fixed(160));
        row->AddChild(std::move(danger), DuiLayout::Hint().Fixed(kIconBtnW));
        row->AddChild(std::move(iconOnly), DuiLayout::Hint().Fixed(60));
        AddVariantRow(page.get(), std::move(row));
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 徽标 ==========================================================

std::unique_ptr<DuiControl> Build_Badge()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 徽标本身的高度（像素）。两个段落都按这个高度摆放。
    const int kBadgeH = 24;
    // 「徽标 + 下方小字」这种两行结构的行高（像素）。
    const int kBadgeColumnRowH = 60;

    AddSection(page.get(),
               Txt(_T("未读计数"), _T("Unread count pill / circle")),
               Txt(_T("计数为 0 时整个徽标隐藏；1 到 99 直接显示数字；100 及以上显示 ")
                   _T("99+。底色可以自定义。"),
                   _T("0 hides the badge; 1-99 shows the number; 100 and above shows \"99+\". ")
                   _T("The background color is configurable.")));
    {
        // 每列的宽度。
        const int kCountColW = 110;

        // 一列演示：一个计数值，配底色与下方的小字说明。
        struct CountDemo
        {
            // 徽标显示的计数值。
            int count;
            // 徽标下方的小字说明。
            LPCTSTR caption;
            // 徽标底色。
            COLORREF bgColor;
        };
        const CountDemo demos[] =
        {
            { 0,   Txt(_T("计数 0（隐藏）"), _T("count = 0 (hidden)")),  RGB(220, 60, 60) },
            { 1,   Txt(_T("计数 1"), _T("count = 1")),                   RGB(220, 60, 60) },
            { 9,   Txt(_T("计数 9"), _T("count = 9")),                   RGB(220, 60, 60) },
            { 42,  Txt(_T("计数 42"), _T("count = 42")),                 RGB(220, 60, 60) },
            { 240, Txt(_T("计数 240（显示 99+）"), _T("count = 240 (shows 99+)")), kBrandBlue },
        };
        const int kDemoCount = sizeof(demos) / sizeof(demos[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(28);
        for (int i = 0; i < kDemoCount; ++i)
        {
            std::unique_ptr<DuiVBox> column(new DuiVBox());
            column->SetGap(4);

            std::unique_ptr<DuiBadge> badge(new DuiBadge());
            badge->SetCount(demos[i].count);
            badge->SetBgColor(demos[i].bgColor);

            std::unique_ptr<DuiLabel> caption(new DuiLabel());
            caption->SetText(demos[i].caption);
            caption->SetTextColor(kCaptionColor);

            column->AddChild(std::move(badge), DuiLayout::Hint().Fixed(kBadgeH));
            column->AddChild(std::move(caption), DuiLayout::Hint().Fixed(kCaptionH));
            row->AddChild(std::move(column), DuiLayout::Hint().Fixed(kCountColW));
        }
        AddVariantRow(page.get(), std::move(row), kBadgeColumnRowH);
    }

    AddSection(page.get(),
               Txt(_T("圆角半径变体"), _T("Corner radius variants")),
               Txt(_T("SetCornerRadius(-1) 是默认的胶囊形，与历史视觉一致；传入 0 或正数")
                   _T("则是固定圆角，也就是常说的 chip 形态。同一个 DuiBadge 因此能同时")
                   _T("覆盖「短计数与小红点」和「长文字标签」两种用法，内部统一走 ")
                   _T("DuiAA::FillRoundRect 抗锯齿绘制。"),
                   _T("SetCornerRadius(-1) keeps the default pill shape; a value of 0 or above ")
                   _T("gives a fixed corner radius, i.e. the chip shape. One DuiBadge therefore ")
                   _T("covers both short counts / dots and long text labels, all drawn through ")
                   _T("DuiAA::FillRoundRect.")));
    {
        // 每列的宽度。
        const int kRadiusColW = 120;

        // 一列演示：一个圆角半径，配下方的小字说明。
        struct RadiusDemo
        {
            // 传给 SetCornerRadius 的半径（像素），-1 表示胶囊形。
            int radius;
            // 徽标下方的小字说明。
            LPCTSTR caption;
        };
        const RadiusDemo demos[] =
        {
            { -1, Txt(_T("r = -1（胶囊，默认）"), _T("r = -1 (pill, default)")) },
            {  0, Txt(_T("r = 0（直角）"), _T("r = 0 (square)"))               },
            {  2, _T("r = 2")                                                  },
            {  4, Txt(_T("r = 4（常用 chip）"), _T("r = 4 (typical chip)"))    },
            {  8, _T("r = 8")                                                  },
        };
        const int kDemoCount = sizeof(demos) / sizeof(demos[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);
        for (int i = 0; i < kDemoCount; ++i)
        {
            std::unique_ptr<DuiVBox> column(new DuiVBox());
            column->SetGap(4);

            std::unique_ptr<DuiBadge> badge(new DuiBadge());
            badge->SetText(Txt(_T("标签"), _T("Label")));
            // 传 0 表示不限制显示字数，也就是不截断。
            badge->SetMaxDisplayChars(0);
            badge->SetBgColor(kBrandBlue);
            badge->SetTextColor(kWhiteColor);
            badge->SetCornerRadius(demos[i].radius);

            std::unique_ptr<DuiLabel> caption(new DuiLabel());
            caption->SetText(demos[i].caption);
            caption->SetTextColor(kCaptionColor);

            column->AddChild(std::move(badge), DuiLayout::Hint().Fixed(kBadgeH));
            column->AddChild(std::move(caption), DuiLayout::Hint().Fixed(kCaptionH));
            row->AddChild(std::move(column), DuiLayout::Hint().Fixed(kRadiusColW));
        }
        AddVariantRow(page.get(), std::move(row), kBadgeColumnRowH);
    }

    AddSection(page.get(),
               Txt(_T("长文字 chip（前置圆点 + 不截断）"), _T("Long-text chip (leading dot)")),
               Txt(_T("状态 chip 的常见写法：SetCornerRadius(4) 定圆角，")
                   _T("SetMaxDisplayChars(0) 关闭截断，SetLeadingDot 在文字前面加一个语义色")
                   _T("圆点，再配上浅灰底与深色文字。"),
                   _T("The usual status-chip recipe: SetCornerRadius(4) for the corner, ")
                   _T("SetMaxDisplayChars(0) to disable truncation, SetLeadingDot for the ")
                   _T("semantic dot, over a light gray background with dark text.")));
    {
        // chip 的圆角半径（像素）。
        const int kChipRadius = 4;
        // 每个 chip 的宽度。
        const int kChipW = 96;
        // chip 的底色，浅灰。
        const COLORREF kChipBgColor = RGB(245, 246, 248);
        // chip 的文字色，深灰蓝。
        const COLORREF kChipTextColor = RGB(80, 88, 102);

        // 一个状态 chip：文字加上表示语义的圆点颜色。
        struct StatusDemo
        {
            // chip 上的文字。
            LPCTSTR text;
            // 文字前面那个小圆点的颜色。
            COLORREF dotColor;
        };
        const StatusDemo demos[] =
        {
            { Txt(_T("运行中"), _T("Running")),   RGB( 60, 200, 120) },   // 绿：正常运行
            { Txt(_T("审核中"), _T("In review")), RGB(220, 150,  40) },   // 橙：等待处理
            { Txt(_T("已停用"), _T("Disabled")),  RGB(160, 168, 180) },   // 灰：暂时不可用
            { Txt(_T("已删除"), _T("Deleted")),   RGB(220,  60,  60) },   // 红：不可恢复
        };
        const int kDemoCount = sizeof(demos) / sizeof(demos[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);
        for (int i = 0; i < kDemoCount; ++i)
        {
            std::unique_ptr<DuiBadge> badge(new DuiBadge());
            badge->SetText(demos[i].text);
            badge->SetMaxDisplayChars(0);
            badge->SetCornerRadius(kChipRadius);
            badge->SetLeadingDot(demos[i].dotColor);
            badge->SetBgColor(kChipBgColor);
            badge->SetTextColor(kChipTextColor);
            row->AddChild(std::move(badge), DuiLayout::Hint().Fixed(kChipW));
        }
        AddVariantRow(page.get(), std::move(row));
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 头像 ==========================================================

std::unique_ptr<DuiControl> Build_Avatar()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 本页面的头像共用这三张图片源。位图必须比头像控件活得久 —— 页面每次
    // 切换都会重建控件 —— 所以放静态变量里一次性合成，进程退出时由操作
    // 系统回收。
    static HBITMAP s_blueBitmap = MakeAvatarGradientBitmap(RGB( 80, 130, 220), RGB( 30,  60, 130));
    static HBITMAP s_purpleBitmap = MakeAvatarGradientBitmap(RGB(170,  90, 200), RGB( 90,  30, 130));
    static HBITMAP s_orangeBitmap = MakeAvatarGradientBitmap(RGB(255, 170,  60), RGB(200,  80,  20));

    AddSection(page.get(),
               Txt(_T("没有图片时显示姓名首字"), _T("Initials fallback")),
               Txt(_T("没有设置位图时，从姓名里取一到两个首字显示；一个字也取不出来时")
                   _T("只画一个纯色圆底。"),
                   _T("With no bitmap set, one or two uppercase initials are taken from the name; ")
                   _T("if none can be taken, only a colored disc is drawn.")));
    {
        // 头像的边长（像素）。
        const int kAvatarSize = 56;

        // 一个演示头像：姓名加上没有图片时的圆底颜色。
        struct InitialsDemo
        {
            // 头像对应的姓名。
            LPCTSTR name;
            // 没有图片时的圆底颜色。
            COLORREF bgColor;
        };
        // 这四条演示数据依次覆盖：能取出两个首字母的拉丁姓名、全小写的拉丁
        // 姓名、中文姓名、以及空姓名。它们演示的是姓名形态本身对首字提取的
        // 影响，不属于界面文案，因此不随语言切换。
        const InitialsDemo demos[] =
        {
            { _T("Alice Smith"), RGB( 45, 108, 223) },
            { _T("bob"),         RGB( 50, 160, 110) },
            { _T("零一"),        RGB(220,  60,  60) },
            { _T(""),            RGB(120, 120, 120) },
        };
        const int kDemoCount = sizeof(demos) / sizeof(demos[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kDemoCount; ++i)
        {
            std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
            avatar->SetName(demos[i].name);
            avatar->SetFallbackBgColor(demos[i].bgColor);
            row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(kAvatarSize));
        }
        AddVariantRow(page.get(), std::move(row), kAvatarSize);
    }

    AddSection(page.get(),
               Txt(_T("位图头像（圆形）"), _T("Bitmap source (circle)")),
               Txt(_T("位图由调用方提供，控件把它裁成圆形，并拉伸到当前尺寸。"),
                   _T("The HBITMAP is supplied by the caller, clipped to a circle and stretched to fit any size.")));
    {
        // 头像的边长（像素）。
        const int kAvatarSize = 64;

        HBITMAP bitmaps[] = { s_blueBitmap, s_purpleBitmap, s_orangeBitmap };
        const int kBitmapCount = sizeof(bitmaps) / sizeof(bitmaps[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kBitmapCount; ++i)
        {
            std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
            avatar->SetBitmap(bitmaps[i]);
            row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(kAvatarSize));
        }
        AddVariantRow(page.get(), std::move(row), kAvatarSize);
    }

    AddSection(page.get(),
               Txt(_T("位图头像（圆角矩形）"), _T("Bitmap source (rounded rect)")),
               Txt(_T("ShapeRoundRect 形态下可以指定不同的圆角半径。半径超过短边一半时会")
                   _T("自动收到一半，所以传一个很大的值等价于圆形。"),
                   _T("ShapeRoundRect with various corner radii. The radius is auto-clamped to ")
                   _T("half the shorter side, so a huge value ends up as a circle.")));
    {
        // 头像的边长（像素）。
        const int kAvatarSize = 64;
        // 最后一个半径故意取得很大，用来演示自动收窄。
        const int radii[] = { 4, 12, 24, 999 };
        const int kRadiusCount = sizeof(radii) / sizeof(radii[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kRadiusCount; ++i)
        {
            std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
            avatar->SetBitmap(s_purpleBitmap);
            avatar->SetShape(DuiAvatar::ShapeRoundRect);
            avatar->SetCornerRadius(radii[i]);
            row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(kAvatarSize));
        }
        AddVariantRow(page.get(), std::move(row), kAvatarSize);
    }

    AddSection(page.get(),
               Txt(_T("状态圆点"), _T("Status dot")),
               Txt(_T("右下角画一个状态圆点，外面套一圈白边，浅色照片上也分得清。五种")
                   _T("取值依次是在线、离开、忙碌、离线和不显示。"),
                   _T("A brand-colored dot at the bottom-right with a white outer ring so it stays ")
                   _T("visible on light photos. The five values are online, away, busy, offline and none.")));
    {
        // 头像的边长（像素）。
        const int kAvatarSize = 64;
        const DuiAvatar::Status statuses[] =
        {
            DuiAvatar::StatusOnline,
            DuiAvatar::StatusAway,
            DuiAvatar::StatusBusy,
            DuiAvatar::StatusOffline,
            DuiAvatar::StatusNone,
        };
        const int kStatusCount = sizeof(statuses) / sizeof(statuses[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kStatusCount; ++i)
        {
            std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
            avatar->SetBitmap(s_blueBitmap);
            avatar->SetStatus(statuses[i]);
            row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(kAvatarSize));
        }
        AddVariantRow(page.get(), std::move(row), kAvatarSize);
    }

    AddSection(page.get(),
               Txt(_T("不同尺寸"), _T("Sizes")),
               Txt(_T("同一张位图分别按 24、40、64、96 像素绘制。状态圆点的大小跟随头像")
                   _T("尺寸一起变化。"),
                   _T("The same source bitmap rendered at 24 / 40 / 64 / 96 px. The status dot ")
                   _T("scales with the avatar.")));
    {
        const int sizes[] = { 24, 40, 64, 96 };
        const int kSizeCount = sizeof(sizes) / sizeof(sizes[0]);
        // 整行的高度取最大的那个头像，小的按各自尺寸放。
        const int kMaxAvatarSize = 96;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kSizeCount; ++i)
        {
            std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
            avatar->SetBitmap(s_orangeBitmap);
            avatar->SetStatus(DuiAvatar::StatusOnline);
            row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(sizes[i]));
        }
        AddVariantRow(page.get(), std::move(row), kMaxAvatarSize);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 分隔线与分组框 ================================================

std::unique_ptr<DuiControl> Build_SeparatorGroupBox()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiSeparator —— 水平线与竖直线"),
                   _T("DuiSeparator - horizontal and vertical line")),
               Txt(_T("在控件矩形的正中画一条 1 像素的细线，颜色、粗细以及两端的内缩量")
                   _T("都可以单独设置。"),
                   _T("A centered 1px line with optional color, thickness and inset.")));
    {
        // 这一行里两组演示各占一半宽度，整行高度容纳三行文字。
        const int kSeparatorRowH = 90;
        // 分隔线自身占用的粗细方向尺寸（像素）。线画在这段空间的正中，
        // 留出的余量让它与上下（或左右）的文字不贴在一起。
        const int kSeparatorSlot = 8;
        // 演示文字的行高（像素）。
        const int kTextH = 20;

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        // 左边一组：默认样式的水平分隔线，夹在两行文字中间。
        std::unique_ptr<DuiVBox> horizontalGroup(new DuiVBox());
        horizontalGroup->SetGap(6);

        std::unique_ptr<DuiLabel> above(new DuiLabel());
        above->SetText(Txt(_T("上方"), _T("Above")));

        std::unique_ptr<DuiSeparator> horizontalLine(new DuiSeparator());

        std::unique_ptr<DuiLabel> below(new DuiLabel());
        below->SetText(Txt(_T("下方"), _T("Below")));

        horizontalGroup->AddChild(std::move(above), DuiLayout::Hint().Fixed(kTextH));
        horizontalGroup->AddChild(std::move(horizontalLine), DuiLayout::Hint().Fixed(kSeparatorSlot));
        horizontalGroup->AddChild(std::move(below), DuiLayout::Hint().Fixed(kTextH));
        row->AddChild(std::move(horizontalGroup), DuiLayout::Hint().Weight(1));

        // 右边一组：加粗并改成品牌色的竖直分隔线，两端各内缩 6 像素。
        std::unique_ptr<DuiHBox> verticalGroup(new DuiHBox());
        verticalGroup->SetGap(8);

        std::unique_ptr<DuiLabel> left(new DuiLabel());
        left->SetText(Txt(_T("左侧"), _T("Left")));

        std::unique_ptr<DuiSeparator> verticalLine(new DuiSeparator());
        verticalLine->SetOrientation(DuiSeparator::Vertical);
        verticalLine->SetThickness(2);
        verticalLine->SetColor(kBrandBlue);
        verticalLine->SetInset(6);

        std::unique_ptr<DuiLabel> right(new DuiLabel());
        right->SetText(Txt(_T("右侧"), _T("Right")));

        verticalGroup->AddChild(std::move(left), DuiLayout::Hint().Weight(1));
        verticalGroup->AddChild(std::move(verticalLine), DuiLayout::Hint().Fixed(kSeparatorSlot));
        verticalGroup->AddChild(std::move(right), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(verticalGroup), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kSeparatorRowH);
    }

    AddSection(page.get(),
               Txt(_T("DuiGroupBox —— 带标题的圆角边框"),
                   _T("DuiGroupBox - titled rounded-rect border")),
               Txt(_T("标题压在圆角边框的上边线上。调用方交进来的内容控件放在框内，四周")
                   _T("留出可配置的内边距。"),
                   _T("The title rests on the top edge of a rounded border. The caller's content ")
                   _T("control is placed inside with a configurable padding.")));
    {
        // 两个分组框并排，行高要容纳框内三行文字加上下内边距。
        const int kGroupBoxRowH = 140;
        // 分组框内每行文字的高度（像素）。
        const int kTextH = 20;

        std::unique_ptr<DuiHBox> outerRow(new DuiHBox());
        outerRow->SetGap(18);

        // 左边的分组框：内容是一列纯文字。
        {
            std::unique_ptr<DuiGroupBox> groupBox(new DuiGroupBox());
            groupBox->SetTitle(Txt(_T("账号"), _T("Account")));

            std::unique_ptr<DuiVBox> inner(new DuiVBox());
            inner->SetGap(6);

            std::unique_ptr<DuiLabel> account(new DuiLabel());
            account->SetText(Txt(_T("账号：12345"), _T("Account ID: 12345")));

            std::unique_ptr<DuiLabel> mail(new DuiLabel());
            mail->SetText(Txt(_T("邮箱：alice@example.com"), _T("Email: alice@example.com")));

            std::unique_ptr<DuiLabel> status(new DuiLabel());
            status->SetText(Txt(_T("状态：在线"), _T("Status: online")));

            inner->AddChild(std::move(account), DuiLayout::Hint().Fixed(kTextH));
            inner->AddChild(std::move(mail), DuiLayout::Hint().Fixed(kTextH));
            inner->AddChild(std::move(status), DuiLayout::Hint().Fixed(kTextH));
            groupBox->SetContent(std::move(inner));
            outerRow->AddChild(std::move(groupBox), DuiLayout::Hint().Weight(1));
        }

        // 右边的分组框：内容是一排单选按钮，标题与边框都换成品牌色。
        {
            // 这三个单选按钮所在的组号。
            const int kProxyRadioGroup = 31;

            std::unique_ptr<DuiGroupBox> groupBox(new DuiGroupBox());
            groupBox->SetTitle(Txt(_T("网络代理"), _T("Network proxy")));
            groupBox->SetTitleColor(kBrandBlue);
            groupBox->SetBorderColor(kBrandBlue);

            std::unique_ptr<DuiHBox> inner(new DuiHBox());
            inner->SetGap(8);

            LPCTSTR proxyNames[] =
            {
                Txt(_T("直连"), _T("Direct")),
                Txt(_T("跟随系统"), _T("System")),
                Txt(_T("自定义"), _T("Custom")),
            };
            const int kProxyCount = sizeof(proxyNames) / sizeof(proxyNames[0]);

            for (int i = 0; i < kProxyCount; ++i)
            {
                std::unique_ptr<DuiButton> option(new DuiButton());
                option->SetButtonType(DuiButton::StyleRadio);
                option->SetRadioGroup(kProxyRadioGroup);
                option->SetText(proxyNames[i]);
                // 默认选中第一项。
                if (i == 0)
                {
                    option->SetCheck(true, false);
                }
                inner->AddChild(std::move(option), DuiLayout::Hint().Weight(1));
            }
            groupBox->SetContent(std::move(inner));
            outerRow->AddChild(std::move(groupBox), DuiLayout::Hint().Weight(1));
        }

        AddVariantRow(page.get(), std::move(outerRow), kGroupBoxRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 浮动提示条 ====================================================

std::unique_ptr<DuiControl> Build_Toast()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("四种语义色与长文本截断"), _T("Four semantic colors + long-text ellipsis")),
               Txt(_T("浮在最上层的轻量提示条。它的 HitTest 返回空指针，不参与点击命中，")
                   _T("所以不会挡住下面的控件；显示过程由 DuiAnimMgr 驱动，渐入 200 毫秒、")
                   _T("停留 3 秒、渐出 200 毫秒。点下面的按钮触发。"),
                   _T("A lightweight toast floating above everything else. Its HitTest returns ")
                   _T("nullptr so it never swallows clicks; the sequence is driven by DuiAnimMgr - ")
                   _T("200ms fade in, 3s hold, 200ms fade out. Click a button below to trigger it.")));
    {
        // 这一段每个按钮的宽度。
        const int kToastBtnW = 120;

        // 一个触发按钮：控件编号加按钮文字。
        struct ToastButton
        {
            // 按钮的控件编号，通知回来时靠它区分是哪一个。
            int ctrlId;
            // 按钮上的文字。
            LPCTSTR text;
        };
        const ToastButton buttons[] =
        {
            { kToastBtnInfo,     Txt(_T("信息"), _T("Info"))                  },
            { kToastBtnSuccess,  Txt(_T("成功"), _T("Success"))               },
            { kToastBtnWarning,  Txt(_T("警告"), _T("Warning"))               },
            { kToastBtnError,    Txt(_T("错误"), _T("Error"))                 },
            { kToastBtnLongText, Txt(_T("长文本截断"), _T("Long text"))       },
        };
        const int kButtonCount = sizeof(buttons) / sizeof(buttons[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(10);
        for (int i = 0; i < kButtonCount; ++i)
        {
            std::unique_ptr<DuiButton> button(new DuiButton());
            button->SetCtrlId(buttons[i].ctrlId);
            button->SetText(buttons[i].text);
            button->SetVariant(DuiButton::Variant::Default);
            row->AddChild(std::move(button), DuiLayout::Hint().Fixed(kToastBtnW));
        }
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("配置项"), _T("Options")),
               Txt(_T("依次演示三种配置：不带图标、把停留时长改成 8 秒、以及调用 Show ")
                   _T("之后立刻调用 HideNow 取消 —— 最后这种情况下用户应当完全看不到提示。"),
                   _T("Three configurations in turn: no icon, an 8-second hold, and a Show ")
                   _T("cancelled by an immediate HideNow - in the last case nothing should appear at all.")));
    {
        // 这一段的文字较长，按钮比上一段宽。
        const int kToastBtnW = 140;

        // 一个触发按钮：控件编号加按钮文字。
        struct ToastButton
        {
            // 按钮的控件编号。
            int ctrlId;
            // 按钮上的文字。
            LPCTSTR text;
        };
        const ToastButton buttons[] =
        {
            { kToastBtnNoIcon,  Txt(_T("不带图标"), _T("No icon"))                       },
            { kToastBtnLongDur, Txt(_T("停留 8 秒"), _T("8s hold"))                      },
            { kToastBtnCancel,  Txt(_T("显示后立即取消"), _T("Show then cancel"))        },
        };
        const int kButtonCount = sizeof(buttons) / sizeof(buttons[0]);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(10);
        for (int i = 0; i < kButtonCount; ++i)
        {
            std::unique_ptr<DuiButton> button(new DuiButton());
            button->SetCtrlId(buttons[i].ctrlId);
            button->SetText(buttons[i].text);
            button->SetVariant(DuiButton::Variant::Default);
            row->AddChild(std::move(button), DuiLayout::Hint().Fixed(kToastBtnW));
        }
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("多次显示时的替换"), _T("Repeated Show replaces the previous toast")),
               Txt(_T("连续点三次：前一条提示会被立刻替换掉，不会堆成好几条。"),
                   _T("Click three times in a row: each toast replaces the previous one instead of stacking.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(10);

        std::unique_ptr<DuiButton> button(new DuiButton());
        button->SetCtrlId(kToastBtnRapid);
        button->SetText(Txt(_T("连续点三次"), _T("Click 3 times")));
        button->SetVariant(DuiButton::Variant::Default);

        row->AddChild(std::move(button), DuiLayout::Hint().Fixed(180));
        AddVariantRow(page.get(), std::move(row));
    }

    // 提示条实例加在页面的最后一个子控件位置：父容器按子控件顺序绘制，
    // 排在最后就画在最上层。它的 Layout 忽略父容器分配的矩形、自己定位到
    // 顶部居中，所以这里给 Fixed(0)，不让它占用竖直方向的空间。
    std::unique_ptr<DuiToast> toast(new DuiToast());
    DuiToast* pToast = toast.get();
    page->AddChild(std::move(toast), DuiLayout::Hint().Fixed(0));

    // 四个图标位图一次性合成，生命期到进程结束为止。
    static HBITMAP s_iconInfo = MakeToastIconBitmap(ToastIconInfo, kWhiteColor);
    static HBITMAP s_iconSuccess = MakeToastIconBitmap(ToastIconSuccess, kWhiteColor);
    static HBITMAP s_iconWarning = MakeToastIconBitmap(ToastIconWarning, kWhiteColor);
    static HBITMAP s_iconError = MakeToastIconBitmap(ToastIconError, kWhiteColor);

    // 注册本页面的通知钩子，按发出通知的控件编号触发不同配置的提示条。
    // 钩子里捕获的是提示条的裸指针，页面被销毁时这份钩子会被下一个页面覆盖。
    g_pageNotifyHook =
        [pToast](const DuiNotify* pNotify)
        {
            if (pNotify == NULL || pNotify->code != DUIN_CLICK)
            {
                return;
            }
            // 每次先把公共配置恢复成默认值，避免上一次点击留下的设置
            // （最大宽度、停留时长等）影响这一次。
            pToast->SetTextColor(kWhiteColor);
            pToast->SetDurationMs(kToastDefaultDurationMs);
            pToast->SetMaxWidth(0);
            pToast->SetCornerRadius(kToastCornerRadius);

            switch (pNotify->ctrlId)
            {
            //信息提示：品牌蓝底加信息图标。
            case kToastBtnInfo:
                pToast->SetBgColor(kBrandBlue);
                pToast->SetIcon(s_iconInfo);
                pToast->Show(Txt(_T("已切换到深色主题"), _T("Switched to the dark theme")));
                break;

            //成功提示：绿底加对勾图标。
            case kToastBtnSuccess:
                pToast->SetBgColor(RGB(40, 167, 69));
                pToast->SetIcon(s_iconSuccess);
                pToast->Show(Txt(_T("保存成功"), _T("Saved")));
                break;

            //警告提示：橙底加三角图标。
            case kToastBtnWarning:
                pToast->SetBgColor(RGB(245, 158, 11));
                pToast->SetIcon(s_iconWarning);
                pToast->Show(Txt(_T("请先在左侧选择一项"), _T("Select an item on the left first")));
                break;

            //错误提示：红底加叉号图标。
            case kToastBtnError:
                pToast->SetBgColor(RGB(220, 53, 69));
                pToast->SetIcon(s_iconError);
                pToast->Show(Txt(_T("网络连接失败"), _T("Network connection failed")));
                break;

            //长文本：限制最大宽度，超出部分截断并追加省略号。
            case kToastBtnLongText:
                pToast->SetBgColor(RGB(50, 50, 50));
                pToast->SetIcon(NULL);
                pToast->SetMaxWidth(kToastEllipsisMaxWidth);
                pToast->Show(Txt(_T("这是一段非常非常非常非常长的提示文字，用来验证超过最大宽度之后截断并追加省略号的行为"),
                                 _T("This is a deliberately very very very long toast message used to verify the MaxWidth ellipsis behaviour")));
                break;

            //不带图标：只显示文字。
            case kToastBtnNoIcon:
                pToast->SetBgColor(RGB(50, 50, 50));
                pToast->SetIcon(NULL);
                pToast->Show(Txt(_T("已复制到剪贴板"), _T("Copied to clipboard")));
                break;

            //长时长：把停留时间从默认的 3 秒改成 8 秒。
            case kToastBtnLongDur:
                pToast->SetBgColor(kBrandBlue);
                pToast->SetIcon(s_iconInfo);
                pToast->SetDurationMs(kToastLongDurationMs);
                pToast->Show(Txt(_T("上传中，请稍候（8 秒）"), _T("Uploading, please wait (8s)")));
                break;

            //显示后立即取消：Show 之后马上 HideNow，用户应当完全看不到提示，
            //以此验证 HideNow 能中断尚未播完的动画。
            case kToastBtnCancel:
                pToast->SetBgColor(RGB(50, 50, 50));
                pToast->SetIcon(s_iconInfo);
                pToast->SetDurationMs(kToastCancelDurationMs);
                pToast->Show(Txt(_T("这条提示应当被立即取消"), _T("This toast should be cancelled immediately")));
                pToast->HideNow();
                break;

            //连续三次显示：前两条立刻被替换，最终只看到第三条。
            case kToastBtnRapid:
                pToast->SetBgColor(RGB(50, 50, 50));
                pToast->SetIcon(NULL);
                pToast->Show(Txt(_T("第一条"), _T("first")));
                pToast->Show(Txt(_T("第二条"), _T("second")));
                pToast->Show(Txt(_T("第三条"), _T("third")));
                break;

            //本页面其它控件发来的点击通知与提示条无关，不做处理。
            default:
                break;
            }
        };

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ==============================================

const PageEntry* GetBasicPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("label"),      _T("DuiLabel　文本标签"),          _T("DuiLabel"),           &Build_Label,              true },
        { _T("button"),     _T("DuiButton　按钮"),             _T("DuiButton"),          &Build_Button,             true },
        { _T("badge"),      _T("DuiBadge　徽标"),              _T("DuiBadge"),           &Build_Badge,              true },
        { _T("avatar"),     _T("DuiAvatar　头像"),             _T("DuiAvatar"),          &Build_Avatar,             true },
        { _T("separator"),  _T("DuiSeparator / DuiGroupBox　分隔线与分组框"), _T("Separator & GroupBox"), &Build_SeparatorGroupBox, true },
        { _T("toast"),      _T("DuiToast　浮动提示条"),        _T("DuiToast"),           &Build_Toast,              true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
