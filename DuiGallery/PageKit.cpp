/**
 *  画廊页面公共构建工具的实现。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"

#include "DuiResMgr.h"
#include "DuiTheme.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// ---- 版式配色 ----
//
// 页面背景、卡片、标题与说明的颜色都从 DuiTheme 现取，而不是写死常量。
// 这样窗口右上角切换主题预设时，画廊自己的版式会立刻跟着变，而演示区里的
// balloonui 控件不会变 —— 因为库内的控件颜色是各自 .cpp 文件里的常量，
// 与主题无关。这个差异本身就是主题页要讲的内容，把它摆在眼前比藏起来好。
//
// 颜色在页面构建时取一次并写进控件。切换主题之后需要重建页面才会生效，
// 宿主窗口已经这样做了。

// 段落标题的字号（磅值）。比正文的 9 磅大一档，配合加粗与颜色形成层级。
const int kTitlePointSize = 11;
// 卡片内部各元素之间的竖直间距（像素）。
const int kCardGap = 6;
// 说明文字与其上方标题之间额外留出的间距（像素）。
const int kDescTopGap = 2;
// 无卡片模式下页面各元素之间的间距（像素），沿用重构之前的取值。
const int kPlainPageGap = 4;
// 测量说明文字时预留的滚动条宽度（像素），取 DuiScrollView 的默认值。
//
// 这里**总是**扣掉这个宽度，而不是只在滚动条真的出现时才扣：内容有多高
// 取决于文字折了几行，文字折几行又取决于宽度，如果宽度再反过来取决于
// 滚动条出不出现，就会出现"因为窄了所以更高、因为更高所以出滚动条、
// 因为出滚动条所以更窄"的来回摆动。恒定扣掉之后测量结果稳定，代价只是
// 没有滚动条时右侧多留十几像素空白。
const int kReservedScrollBarWidth = 12;

} // 匿名命名空间

// =====================================================================
// 截图标记
// =====================================================================

std::vector<CaptureMark>& GetCaptureMarks()
{
    static std::vector<CaptureMark> s_marks;
    return s_marks;
}

void RegisterCapture(LPCTSTR name, DuiControl* anchor)
{
    if (name == NULL || anchor == NULL)
    {
        return;
    }
    CaptureMark mark;
    mark.name = name;
    mark.anchor = anchor;
    GetCaptureMarks().push_back(mark);
}

// =====================================================================
// 当前页面的通知钩子
// =====================================================================

std::function<void(const DuiNotify*)> g_pageNotifyHook;

// =====================================================================
// FnButton
// =====================================================================

bool FnButton::OnLButtonDown(POINT pt, UINT mkFlags)
{
    m_localPressed = true;
    return DuiButton::OnLButtonDown(pt, mkFlags);
}

bool FnButton::OnLButtonUp(POINT pt, UINT mkFlags)
{
    bool wasPressed = m_localPressed;
    m_localPressed = false;
    bool handled = DuiButton::OnLButtonUp(pt, mkFlags);
    // 只有"在本按钮上按下、又在本按钮上抬起"才算一次点击，与基类的语义一致。
    if (wasPressed && IsEnabled() && ::PtInRect(&GetRect(), pt) && onClick)
    {
        onClick(this);
    }
    return handled;
}

// =====================================================================
// GalleryPageBox
// =====================================================================

GalleryPageBox::GalleryPageBox()
    : m_pCurrentCard(NULL)
    , m_lastMeasuredWidth(0)
    , m_plainMode(false)
{
    SetPadding(kPageMargin);
    SetGap(kSectionGap);
    SetBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceAltBg));
}

void GalleryPageBox::SetPlainMode(bool plain)
{
    m_plainMode = plain;
    if (!plain)
    {
        return;
    }
    // 恢复成重构之前的页面版式：段落之间只留 4 像素间距、页面不画背景色，
    // 这样按演示行矩形裁出来的配图与已经发布在文档里的那批保持一致。
    SetGap(kPlainPageGap);
    SetBgColor(CLR_INVALID);
}

DuiVBox* GalleryPageBox::BeginCard()
{
    // 无卡片模式下不再套一层容器，标题、说明与演示行直接排在页面上。
    if (m_plainMode)
    {
        m_pCurrentCard = this;
        return this;
    }

    std::unique_ptr<DuiVBox> card(new DuiVBox());
    card->SetPadding(kCardPadding);
    card->SetGap(kCardGap);
    card->SetBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceBg));
    card->SetCornerRadius(kCardRadius);
    card->SetBorderColor(DuiTheme::Inst().Get(DuiTheme::BorderLight));
    card->SetBorderWidth(1.0f);

    DuiVBox* raw = card.get();
    // 卡片有多高完全由它装了什么决定，所以用自动档：排列时向卡片询问期望
    // 尺寸。卡片是竖直布局，它的期望高度等于内部各元素的高度之和加上内边距
    // 与间距，前提是内部元素的高度都是确定的 —— 说明文字的高度由本页面容器
    // 在排列开始时先测好写回去，因此这个前提成立。
    AddChild(std::move(card), DuiLayout::Hint().Auto());
    m_pCurrentCard = raw;
    return raw;
}

DuiVBox* GalleryPageBox::CurrentCard()
{
    if (m_pCurrentCard == NULL)
    {
        return BeginCard();
    }
    return m_pCurrentCard;
}

void GalleryPageBox::RegisterWrapLabel(DuiVBox* pCard, DuiLabel* pLabel)
{
    if (pCard == NULL || pLabel == NULL)
    {
        return;
    }
    WrapLabelEntry entry;
    entry.card = pCard;
    entry.label = pLabel;
    m_wrapLabels.push_back(entry);
    // 新加进来的标签还没有被测量过，把记录的宽度清掉，强制下一次排列重测。
    m_lastMeasuredWidth = 0;
}

int GalleryPageBox::ResolveOwnWidth() const
{
    // 优先按父控件的矩形推算。页面容器是被 DuiScrollView::SetContent 装进去的，
    // 父控件就是那个滚动视图，而滚动视图在向内容询问期望尺寸之前已经更新过
    // 自己的矩形，所以这里拿到的是本次排列真正会用的视口宽度，测量一次到位，
    // 不会出现"滚动范围按上一次的宽度算"的情况。
    DuiControl* pParent = GetParent();
    if (pParent != NULL)
    {
        const RECT& rcParent = pParent->GetRect();
        int parentWidth = rcParent.right - rcParent.left;
        if (parentWidth > 0)
        {
            return parentWidth - kReservedScrollBarWidth;
        }
    }
    // 还没有挂上父控件时退回自己上一次拿到的矩形。
    const RECT& rcSelf = GetRect();
    return rcSelf.right - rcSelf.left;
}

void GalleryPageBox::MeasureWrapLabels(int cardContentWidth) const
{
    if (cardContentWidth < 1)
    {
        return;
    }
    // 宽度没有变化时上一次算出来的高度依然成立，跳过重复测量。每条说明都要
    // 走一次 DrawText(DT_CALCRECT)，而排列在窗口拖动期间会被频繁调用。
    if (cardContentWidth == m_lastMeasuredWidth)
    {
        return;
    }
    m_lastMeasuredWidth = cardContentWidth;

    for (size_t i = 0; i < m_wrapLabels.size(); ++i)
    {
        const WrapLabelEntry& entry = m_wrapLabels[i];
        if (entry.card == NULL || entry.label == NULL)
        {
            continue;
        }
        int height = entry.label->MeasureHeight(cardContentWidth);
        entry.card->SetHint(entry.label,
                            DuiLayout::Hint().Fixed(height).Margin(0, kDescTopGap, 0, 0));
    }
}

int GalleryPageBox::TextWidthFromPageWidth(int pageWidth) const
{
    // 说明文字能用的宽度 = 页面宽度 - 页面左右内边距 - 卡片左右内边距。
    // 无卡片模式下没有后面那一项。
    int width = pageWidth - kPageMargin * 2;
    if (!m_plainMode)
    {
        width -= kCardPadding * 2;
    }
    return width;
}

int GalleryPageBox::GetChildCountForTests() const
{
    return (int)m_children.size();
}

SIZE GalleryPageBox::GetDesiredSize() const
{
    // 外层滚动视图在排列自己之前会先问内容要多高。此时它自己的矩形已经更新，
    // 所以这里能推算出本页面这一次会有多宽，测量结果一次到位，不会出现
    // "滚动范围按上一次的宽度算"的情况。
    MeasureWrapLabels(TextWidthFromPageWidth(ResolveOwnWidth()));
    return DuiVBox::GetDesiredSize();
}

void GalleryPageBox::Layout(const RECT& rcAvail)
{
    // 本函数体内会调 SetHint 改子控件的布局提示，必须先立起这道护栏，否则
    // SetHint 会反过来再触发一次排列，形成无限递归。
    DuiLayout::LayoutGuard guard(*this);

    MeasureWrapLabels(TextWidthFromPageWidth(rcAvail.right - rcAvail.left));

    DuiVBox::Layout(rcAvail);
}

// =====================================================================
// 页面构建函数
// =====================================================================

std::unique_ptr<GalleryPageBox> NewPage()
{
    return std::unique_ptr<GalleryPageBox>(new GalleryPageBox());
}

std::unique_ptr<GalleryPageBox> NewPlainPage()
{
    std::unique_ptr<GalleryPageBox> page(new GalleryPageBox());
    page->SetPlainMode(true);
    return page;
}

void AddSection(GalleryPageBox* page, LPCTSTR title, LPCTSTR desc)
{
    if (page == NULL)
    {
        return;
    }
    DuiVBox* card = page->BeginCard();

    std::unique_ptr<DuiLabel> titleLabel(new DuiLabel());
    titleLabel->SetText(title != NULL ? title : _T(""));
    titleLabel->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextDefault));
    titleLabel->SetFont(DuiResMgr::Inst().GetFontByPointSize(kTitlePointSize, true));
    card->AddChild(std::move(titleLabel), DuiLayout::Hint().Fixed(kHeaderH));

    if (desc != NULL && desc[0] != _T('\0'))
    {
        std::unique_ptr<DuiLabel> descLabel(new DuiLabel());
        descLabel->SetText(desc);
        descLabel->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextSubtle));
        descLabel->SetWordWrap(true);
        DuiLabel* rawDesc = descLabel.get();
        // 先按一个占位高度加进去，真正的高度由页面容器在排列开始时按当前
        // 宽度测出来再写回。
        card->AddChild(std::move(descLabel), DuiLayout::Hint().Fixed(20));
        page->RegisterWrapLabel(card, rawDesc);
    }
}

DuiControl* AddVariantRow(GalleryPageBox* page,
                          std::unique_ptr<DuiHBox> row,
                          int rowH)
{
    if (page == NULL || !row)
    {
        return NULL;
    }
    DuiVBox* card = page->CurrentCard();
    DuiControl* anchor = row.get();
    card->AddChild(std::move(row), DuiLayout::Hint().Fixed(rowH));
    return anchor;
}

void AddVariantRowCapture(GalleryPageBox* page,
                          LPCTSTR captureName,
                          std::unique_ptr<DuiHBox> row,
                          int rowH)
{
    DuiControl* anchor = AddVariantRow(page, std::move(row), rowH);
    RegisterCapture(captureName, anchor);
}

void AddGap(GalleryPageBox* page, int h)
{
    if (page == NULL)
    {
        return;
    }
    DuiVBox* card = page->CurrentCard();
    card->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(h));
}

} // namespace Gallery
