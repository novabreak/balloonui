/**
 *  画廊「引擎」分组的演示页面：主题（DuiTheme）、高 DPI（DuiDpi）、
 *  动画与缓动（DuiAnimation）、运行期跟踪（DuiTrace）。
 *
 *  这四个模块都不是控件，而是进程级的设施，所以本文件里的演示大量使用
 *  自绘控件把它们的内部状态读出来显示。凡是「库的实际行为与文档描述不
 *  一致」的地方，页面上都按实际行为如实说明，不做美化。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "DuiHost.h"
#include "DuiResMgr.h"
#include "DuiTheme.h"
#include "DuiDpi.h"
#include "DuiAnimation.h"
#include "DuiTrace.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Input/DuiSlider.h"

#include <vector>

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 本文件公用的配色与尺寸
// =====================================================================

// 自绘演示控件的描边色。与 PageKit 里卡片的描边色取同一个值，同一张卡片
// 内不会出现深浅不一的两种灰。
const COLORREF kDemoBorderColor = RGB(226, 229, 234);
// 自绘演示控件里正文的文字色。
const COLORREF kDemoTextColor   = RGB( 23,  26,  33);
// 自绘演示控件里次要文字（表头、单位、说明）的文字色。
const COLORREF kDemoSubtleColor = RGB(107, 114, 128);
// 需要强调的数值（实测值、当前值）的文字色，取品牌蓝。
const COLORREF kDemoAccentColor = RGB( 45, 108, 223);
// 需要提示「与预期不符」的文字色。
const COLORREF kDemoWarnColor   = RGB(200,  60,  60);
// 自绘演示控件的浅色底，用于把表格 / 日志区与白色卡片区分开。
const COLORREF kDemoPanelColor  = RGB(248, 249, 251);
// 轨道、分隔线一类装饰性线条的颜色。
const COLORREF kDemoTrackColor  = RGB(224, 227, 232);

// 自绘控件里一行文字占的高度（像素）。
const int kTextLineH = 20;
// 自绘控件内容与自身边框之间的内边距（像素）。
const int kDemoPadding = 8;

// 亮度加权系数（BT.601 的整数近似，三项之和即为分母）。用来决定色块上
// 该压深色文字还是白色文字。
const int kLumaWeightR   = 299;
const int kLumaWeightG   = 587;
const int kLumaWeightB   = 114;
const int kLumaWeightSum = 1000;
// 亮度高于该阈值时用深色文字，否则用白色文字。阈值取自原画廊主题页，
// 在库里全部预设上都能读清。
const int kLumaDarkTextThreshold = 140;

// =====================================================================
// 绘制辅助函数
// =====================================================================

// 用纯色填满一个矩形。
//   hdc：目标设备上下文。
//   rc：要填充的矩形。
//   color：填充色。
void FillDemoRect(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH hBrush = ::CreateSolidBrush(color);
    if (hBrush == NULL)
    {
        return;
    }
    RECT rcFill = rc;
    ::FillRect(hdc, &rcFill, hBrush);
    ::DeleteObject(hBrush);
}

// 给一个矩形描 1 像素的边。
//   hdc：目标设备上下文。
//   rc：要描边的矩形。
//   color：描边色。
void FrameDemoRect(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH hBrush = ::CreateSolidBrush(color);
    if (hBrush == NULL)
    {
        return;
    }
    RECT rcFrame = rc;
    ::FrameRect(hdc, &rcFrame, hBrush);
    ::DeleteObject(hBrush);
}

// 在给定矩形里用默认界面字体画一行（或多行）文字。
//   hdc：目标设备上下文。
//   rc：文字的排版矩形。
//   text：要画的文字，为空指针时本函数直接返回。
//   color：文字色。
//   flags：DrawText 的排版标志组合。
void DrawDemoText(HDC hdc, const RECT& rc, LPCTSTR text, COLORREF color, DWORD flags)
{
    if (text == NULL)
    {
        return;
    }
    HFONT hFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT hOldFont = (HFONT)::SelectObject(hdc, hFont);
    int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = ::SetTextColor(hdc, color);
    RECT rcDraw = rc;
    ::DrawText(hdc, text, -1, &rcDraw, flags);
    ::SetTextColor(hdc, oldColor);
    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, hOldFont);
}

// 把颜色格式化成 "#RRGGBB" 形式。
//   color：要格式化的颜色。
//   返回：七个字符的十六进制字符串。
CString FormatColorHex(COLORREF color)
{
    CString text;
    text.Format(_T("#%02X%02X%02X"),
                (int)GetRValue(color), (int)GetGValue(color), (int)GetBValue(color));
    return text;
}

// 按底色亮度挑一个能读清的文字色。
//   background：底色。
//   返回：深色或白色。
COLORREF PickReadableTextColor(COLORREF background)
{
    int luma = ((int)GetRValue(background) * kLumaWeightR
              + (int)GetGValue(background) * kLumaWeightG
              + (int)GetBValue(background) * kLumaWeightB) / kLumaWeightSum;
    if (luma > kLumaDarkTextThreshold)
    {
        return RGB(20, 20, 20);
    }
    return RGB(255, 255, 255);
}

// =====================================================================
// 公用的自绘控件
// =====================================================================

// 把绘制限制在自身矩形内的多行文本块。
//
// DuiLabel 本身不裁剪（DuiControl::OnPaint 的注释里写明了宿主不负责裁剪），
// 文字一旦超出控件高度就会画到相邻控件上。跟踪日志这类「内容长度事先不
// 知道」的文本必须裁剪，否则多读几行就会盖到下一张卡片上。
class ClippedTextBlock : public DuiLabel
{
public:
    ClippedTextBlock();

    // 绘制自身。先铺一层浅色底，再把裁剪限制到自身矩形内交给基类画文字。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

ClippedTextBlock::ClippedTextBlock()
{
    // DT_NOPREFIX 不能省：日志正文里可能出现 & 字符，缺了它 DrawText 会
    // 把它当成助记符前缀吞掉并给下一个字符加下划线。
    SetTextAlign(DT_LEFT | DT_TOP | DT_NOPREFIX);
    SetWordWrap(true);
    SetTextColor(kDemoTextColor);
}

void ClippedTextBlock::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    int nSavedDC = ::SaveDC(hdc);
    if (nSavedDC != 0)
    {
        ::IntersectClipRect(hdc, m_rcItem.left, m_rcItem.top,
                            m_rcItem.right, m_rcItem.bottom);
    }
    DuiLabel::OnPaint(hdc, rcDirty);
    if (nSavedDC != 0)
    {
        ::RestoreDC(hdc, nSavedDC);
    }
}

// =====================================================================
// 主题页专用的控件与处理器
// =====================================================================

// 主题还原哨兵。
//
// DuiTheme 是进程级单例，本页面把预设切成深色之后不会自动恢复，会一直
// 影响后面打开的每一个页面。本控件在构造时记下当前的全部色槽与字体设置，
// 在析构时原样写回。它被添加为页面容器的第一个子控件，页面被销毁（切换
// 到别的页面、或者关闭窗口）时随之析构，还原就发生在那一刻。
//
// 本控件不参与显示：构造时即置为不可见，而竖直布局会完整跳过不可见的
// 子控件（连它前后的间距也一并跳过），因此它不占任何版面。
class ThemePaletteGuard : public DuiControl
{
public:
    ThemePaletteGuard();
    ~ThemePaletteGuard() override;

private:
    // 进入本页面时全部色槽的取值。析构时逐个写回。
    COLORREF m_savedColors[DuiTheme::SlotCount];
    // 进入本页面时的默认字号（磅）。
    int      m_savedFontPt;
    // 进入本页面时的默认字体名。
    CString  m_savedFontFace;
};

ThemePaletteGuard::ThemePaletteGuard()
    : m_savedFontPt(0)
{
    DuiTheme& theme = DuiTheme::Inst();
    for (int i = 0; i < (int)DuiTheme::SlotCount; ++i)
    {
        m_savedColors[i] = theme.Get((DuiTheme::Slot)i);
    }
    m_savedFontPt = theme.GetDefaultFontPt();
    m_savedFontFace = theme.GetDefaultFontFace();
    SetVisible(false);
}

ThemePaletteGuard::~ThemePaletteGuard()
{
    DuiTheme& theme = DuiTheme::Inst();
    for (int i = 0; i < (int)DuiTheme::SlotCount; ++i)
    {
        theme.Set((DuiTheme::Slot)i, m_savedColors[i]);
    }
    theme.SetDefaultFontPt(m_savedFontPt);
    theme.SetDefaultFontFace(m_savedFontFace);
}

// 一个色槽的色块。绘制时现取 DuiTheme 里该槽位的颜色，所以切换预设之后
// 只要重画一次就能看到新颜色，不需要为每个色块单独订阅主题变化。
class ThemeSwatch : public DuiControl
{
public:
    // 构造。
    //   slot：本色块显示哪个色槽。
    //   name：槽位名，直接作为界面文字，不翻译（它是 API 标识符）。
    //   showHex：true 时在槽位名下面再画一行十六进制颜色值。
    ThemeSwatch(DuiTheme::Slot slot, LPCTSTR name, bool showHex);

    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 本色块对应的槽位。
    DuiTheme::Slot m_slot;
    // 槽位名。
    CString        m_name;
    // 是否额外显示十六进制颜色值。
    bool           m_showHex;
};

ThemeSwatch::ThemeSwatch(DuiTheme::Slot slot, LPCTSTR name, bool showHex)
    : m_slot(slot)
    , m_name(name != NULL ? name : _T(""))
    , m_showHex(showHex)
{
}

void ThemeSwatch::OnPaint(HDC hdc, const RECT& rcDirty)
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

    COLORREF fill = DuiTheme::Inst().Get(m_slot);
    FillDemoRect(hdc, m_rcItem, fill);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    COLORREF textColor = PickReadableTextColor(fill);
    if (!m_showHex)
    {
        DrawDemoText(hdc, m_rcItem, m_name, textColor,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }

    // 显示十六进制值时分成上下两行：上行槽位名，下行颜色值。
    int height = m_rcItem.bottom - m_rcItem.top;
    RECT rcName = m_rcItem;
    rcName.bottom = rcName.top + height / 2;
    RECT rcHex = m_rcItem;
    rcHex.top = rcName.bottom;
    DrawDemoText(hdc, rcName, m_name, textColor,
                 DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    DrawDemoText(hdc, rcHex, FormatColorHex(fill), textColor,
                 DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
}

// 主题当前状态的只读显示控件。绘制时现读 DuiTheme 的版本号与字体设置。
class ThemeStatusView : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

void ThemeStatusView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    DuiTheme& theme = DuiTheme::Inst();
    RECT rcLine = m_rcItem;
    rcLine.left += kDemoPadding;
    rcLine.right -= kDemoPadding;
    rcLine.top += kDemoPadding / 2;
    rcLine.bottom = rcLine.top + kTextLineH;

    CString line;
    line.Format(_T("GetVersion() = %u        GetSubscriberCount() = %d"),
                theme.GetVersion(), theme.GetSubscriberCount());
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("GetDefaultFontPt() = %d        GetDefaultFontFace() = %s"),
                theme.GetDefaultFontPt(), (LPCTSTR)theme.GetDefaultFontFace());
    DrawDemoText(hdc, rcLine, line, kDemoTextColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("SurfaceBg = %s        TextDefault = %s        BrandPrimary = %s"),
                (LPCTSTR)FormatColorHex(theme.Get(DuiTheme::SurfaceBg)),
                (LPCTSTR)FormatColorHex(theme.Get(DuiTheme::TextDefault)),
                (LPCTSTR)FormatColorHex(theme.Get(DuiTheme::BrandPrimary)));
    DrawDemoText(hdc, rcLine, line, kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 主题变化订阅的演示控件。
//
// 它既是订阅者也是显示者：Subscribe 之后每次主题发生变化都会被回调一次，
// 回调里把计数加一并请求重绘，于是界面上能直接看到「订阅者个数」「回调
// 被触发了几次」「版本号涨到了多少」三个数字的联动。
class ThemeSubscriberView : public DuiControl
{
public:
    ThemeSubscriberView();
    ~ThemeSubscriberView() override;

    // 订阅主题变化。已经订阅时本函数不做任何事。
    void Subscribe();

    // 退订。尚未订阅时本函数不做任何事。
    void Unsubscribe();

    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

    // 主题变化回调。DuiTheme 要求的是一个普通函数指针，所以这里用静态
    // 成员函数，通过 userdata 找回对象本身。
    //   pUserData：SubscribeChange 时传进去的本对象指针。
    static void OnThemeChanged(void* pUserData);

private:
    // 订阅令牌。0 表示当前没有订阅。
    int m_token;
    // 订阅期间回调被触发的次数。
    int m_hitCount;
};

ThemeSubscriberView::ThemeSubscriberView()
    : m_token(0)
    , m_hitCount(0)
{
}

ThemeSubscriberView::~ThemeSubscriberView()
{
    // 订阅表保存的是裸指针，本对象销毁后再被回调就是访问已释放的内存，
    // 所以析构时必须退订。
    Unsubscribe();
}

void ThemeSubscriberView::Subscribe()
{
    if (m_token != 0)
    {
        return;
    }
    m_token = DuiTheme::Inst().SubscribeChange(&ThemeSubscriberView::OnThemeChanged, this);
    Invalidate();
}

void ThemeSubscriberView::Unsubscribe()
{
    if (m_token == 0)
    {
        return;
    }
    DuiTheme::Inst().Unsubscribe(m_token);
    m_token = 0;
    Invalidate();
}

void ThemeSubscriberView::OnThemeChanged(void* pUserData)
{
    ThemeSubscriberView* pSelf = (ThemeSubscriberView*)pUserData;
    if (pSelf == NULL)
    {
        return;
    }
    ++pSelf->m_hitCount;
    pSelf->Invalidate();
}

void ThemeSubscriberView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    RECT rcLine = m_rcItem;
    rcLine.left += kDemoPadding;
    rcLine.right -= kDemoPadding;
    rcLine.top += kDemoPadding / 2;
    rcLine.bottom = rcLine.top + kTextLineH;

    CString line;
    if (m_token != 0)
    {
        line.Format(Txt(_T("本页面的订阅令牌 = %d（已订阅）"),
                        _T("This page's token = %d (subscribed)")), m_token);
    }
    else
    {
        line = Txt(_T("本页面尚未订阅"), _T("This page is not subscribed"));
    }
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("DuiTheme::GetSubscriberCount() = %d        DuiTheme::GetVersion() = %u"),
                DuiTheme::Inst().GetSubscriberCount(), DuiTheme::Inst().GetVersion());
    DrawDemoText(hdc, rcLine, line, kDemoTextColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(Txt(_T("回调被触发的次数 = %d"), _T("Callback fired %d time(s)")),
                m_hitCount);
    DrawDemoText(hdc, rcLine, line, kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 字号跟随 DuiTheme::GetDefaultFontPt() 的预览标签。
//
// 它是本页面里唯一会随字体钩子变化的控件 —— 因为它自己在绘制前去读了
// 那个值。库内的控件都不读它，见本页面「默认字体钩子」一段的说明。
class ThemeFontPreviewLabel : public DuiLabel
{
public:
    ThemeFontPreviewLabel();

    // 绘制自身。绘制前把字体同步成主题当前的字号。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 上一次已经应用到本标签上的字号（磅）。与主题当前值不同时才重新取
    // 字体 —— SetFont 会无条件请求一次重绘，每次绘制都调会造成反复重绘。
    int m_appliedPt;
};

ThemeFontPreviewLabel::ThemeFontPreviewLabel()
    : m_appliedPt(0)
{
    SetTextAlign(DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(kDemoTextColor);
}

void ThemeFontPreviewLabel::OnPaint(HDC hdc, const RECT& rcDirty)
{
    int pt = DuiTheme::Inst().GetDefaultFontPt();
    if (pt != m_appliedPt)
    {
        m_appliedPt = pt;
        SetFont(DuiResMgr::Inst().GetFontByPointSize(pt));
    }

    int nSavedDC = ::SaveDC(hdc);
    if (nSavedDC != 0)
    {
        ::IntersectClipRect(hdc, m_rcItem.left, m_rcItem.top,
                            m_rcItem.right, m_rcItem.bottom);
    }
    DuiLabel::OnPaint(hdc, rcDirty);
    if (nSavedDC != 0)
    {
        ::RestoreDC(hdc, nSavedDC);
    }
}

// 点击后切换主题预设的按钮处理器。
struct ApplyPresetHandler
{
    // 要切换到的预设。
    DuiTheme::Preset m_preset;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，用它找到宿主窗口发起整窗重绘。
    void operator()(FnButton* pButton) const;
};

void ApplyPresetHandler::operator()(FnButton* pButton) const
{
    DuiTheme::Inst().ApplyPreset(m_preset);
    // 读取主题的自绘控件散落在整个页面上，逐个 Invalidate 不如整窗重绘
    // 来得直接，演示页面也不在意这点开销。
    if (pButton != NULL && pButton->GetHost() != NULL)
    {
        pButton->GetHost()->Invalidate(FALSE);
    }
}

// 订阅 / 退订按钮的处理器。
struct ThemeSubscribeHandler
{
    // 订阅演示控件。不持有所有权。
    ThemeSubscriberView* m_pView;
    // true 表示这个按钮负责订阅，false 表示负责退订。
    bool                 m_bSubscribe;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，本处理器不使用它。
    void operator()(FnButton* pButton) const;
};

void ThemeSubscribeHandler::operator()(FnButton* pButton) const
{
    (void)pButton;
    if (m_pView == NULL)
    {
        return;
    }
    if (m_bSubscribe)
    {
        m_pView->Subscribe();
    }
    else
    {
        m_pView->Unsubscribe();
    }
}

// 「改一个色槽」按钮用的两个颜色。反复点击时在两者之间来回切换，保证每次
// 都真的发生变化 —— DuiTheme::Set 在新旧颜色相同时会直接返回，既不涨版本
// 号也不发通知。
const COLORREF kMutateColorA = RGB(200,  60,  60);
const COLORREF kMutateColorB = RGB( 60, 160,  90);

// 改动一个色槽以触发一次主题变化通知的按钮处理器。
struct ThemeMutateHandler
{
    // 主题状态显示控件。不持有所有权，可以为空。
    DuiControl* m_pStatusView;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，用它找到宿主窗口发起整窗重绘。
    void operator()(FnButton* pButton) const;
};

void ThemeMutateHandler::operator()(FnButton* pButton) const
{
    DuiTheme& theme = DuiTheme::Inst();
    COLORREF current = theme.Get(DuiTheme::TextLink);
    if (current == kMutateColorA)
    {
        theme.Set(DuiTheme::TextLink, kMutateColorB);
    }
    else
    {
        theme.Set(DuiTheme::TextLink, kMutateColorA);
    }
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
    if (pButton != NULL && pButton->GetHost() != NULL)
    {
        pButton->GetHost()->Invalidate(FALSE);
    }
}

// 主题页里字号滑块的控件编号。取值只需在本页面内唯一。
const UINT kIdThemeFontSlider = 9101;

// 主题页的通知钩子：把字号滑块的新值写进 DuiTheme，并刷新受影响的显示。
struct ThemeFontSliderHook
{
    // 预览标签。不持有所有权。
    DuiControl* m_pPreview;
    // 显示当前字号数值的标签。不持有所有权。
    DuiLabel*   m_pValueLabel;
    // 主题状态显示控件。改字号会让版本号加一，需要一并刷新。不持有所有权。
    DuiControl* m_pStatusView;

    // 收到控件通知时被调用。
    //   pNotify：通知内容，可能为空。
    void operator()(const DuiNotify* pNotify) const;
};

void ThemeFontSliderHook::operator()(const DuiNotify* pNotify) const
{
    if (pNotify == NULL)
    {
        return;
    }
    // 控件编号必须与通知码写在同一个条件里。DuiSlider 发的是通用的
    // DUIN_VALUECHANGED，本页面里别的控件也可能发同一个码。
    if (pNotify->code != (UINT)DUIN_VALUECHANGED
        || pNotify->ctrlId != kIdThemeFontSlider)
    {
        return;
    }

    int pt = (int)pNotify->extra;
    DuiTheme::Inst().SetDefaultFontPt(pt);
    if (m_pValueLabel != NULL)
    {
        CString text;
        text.Format(_T("%d pt"), DuiTheme::Inst().GetDefaultFontPt());
        m_pValueLabel->SetText(text);
    }
    if (m_pPreview != NULL)
    {
        m_pPreview->Invalidate();
        // 改字号会让主题的版本号加一，而版本号在本页面上有两处显示，分别
        // 位于第一段和第四段。逐个刷新不如整窗重绘来得可靠。
        if (m_pPreview->GetHost() != NULL)
        {
            m_pPreview->GetHost()->Invalidate(FALSE);
        }
    }
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
}

// 主题色槽的完整清单。槽位名照 DuiTheme::Slot 的枚举名写，不翻译。
struct ThemeSlotEntry
{
    // 槽位枚举值。
    DuiTheme::Slot slot;
    // 槽位名。
    LPCTSTR        name;
};

const ThemeSlotEntry kAllThemeSlots[(int)DuiTheme::SlotCount] = {
    { DuiTheme::BrandPrimary,   _T("BrandPrimary")   },
    { DuiTheme::BrandHover,     _T("BrandHover")     },
    { DuiTheme::BrandPressed,   _T("BrandPressed")   },
    { DuiTheme::BrandBorder,    _T("BrandBorder")    },
    { DuiTheme::TextOnPrimary,  _T("TextOnPrimary")  },
    { DuiTheme::TextDefault,    _T("TextDefault")    },
    { DuiTheme::TextSubtle,     _T("TextSubtle")     },
    { DuiTheme::TextDisabled,   _T("TextDisabled")   },
    { DuiTheme::TextLink,       _T("TextLink")       },
    { DuiTheme::SurfaceBg,      _T("SurfaceBg")      },
    { DuiTheme::SurfaceAltBg,   _T("SurfaceAltBg")   },
    { DuiTheme::BorderLight,    _T("BorderLight")    },
    { DuiTheme::BorderHeavy,    _T("BorderHeavy")    },
    { DuiTheme::RowHover,       _T("RowHover")       },
    { DuiTheme::RowSel,         _T("RowSel")         },
    { DuiTheme::TextOnRowSel,   _T("TextOnRowSel")   },
    { DuiTheme::StatusOnline,   _T("StatusOnline")   },
    { DuiTheme::StatusAway,     _T("StatusAway")     },
    { DuiTheme::StatusBusy,     _T("StatusBusy")     },
    { DuiTheme::StatusOffline,  _T("StatusOffline")  },
};

// 色板每行摆几个色块。20 个槽位刚好排成 4 行。
const int kSwatchesPerRow = 5;
// 带十六进制值的色块行高（像素）。要装下槽位名与颜色值两行文字。
const int kSwatchRowH = 44;

// =====================================================================
// 高 DPI 页专用的控件
// =====================================================================

// 换算表里横向列出的 DPI 值。
const int kDpiColumnCount = 4;
const int kDpiColumns[kDpiColumnCount]  = {  96, 120, 144, 192 };
// 与上面各 DPI 对应的系统缩放比例（百分数），只用于表头显示。
const int kDpiPercents[kDpiColumnCount] = { 100, 125, 150, 200 };

// 换算表里纵向列出的逻辑值。挑的是控件几何里最常见的几档：1 像素分隔线、
// 4 / 8 像素内边距、12 / 16 像素图标间距、24 / 32 / 48 像素图标边长。
const int kLogicalRowCount = 8;
const int kLogicalValues[kLogicalRowCount] = { 1, 4, 8, 12, 16, 24, 32, 48 };

// 换算表每一行的高度（像素）。
const int kTableRowH = 22;
// 换算表第一列（逻辑值那一列）的宽度（像素）。
const int kTableFirstColW = 90;

// Scale / Unscale 换算表。表格内容全部在绘制时现算，纯函数不依赖任何
// 运行期状态，所以不需要改系统设置就能看到四种 DPI 下的结果。
class DpiScaleTableView : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

void DpiScaleTableView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    int width = m_rcItem.right - m_rcItem.left;
    int dataWidth = width - kTableFirstColW - kDemoPadding * 2;
    if (dataWidth <= 0)
    {
        return;
    }
    int colWidth = dataWidth / kDpiColumnCount;

    // ---- 表头 ----
    RECT rcCell;
    rcCell.left = m_rcItem.left + kDemoPadding;
    rcCell.right = rcCell.left + kTableFirstColW;
    rcCell.top = m_rcItem.top;
    rcCell.bottom = rcCell.top + kTableRowH;
    DrawDemoText(hdc, rcCell, Txt(_T("逻辑值"), _T("Logical")), kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    CString text;
    for (int col = 0; col < kDpiColumnCount; ++col)
    {
        rcCell.left = m_rcItem.left + kDemoPadding + kTableFirstColW + col * colWidth;
        rcCell.right = rcCell.left + colWidth;
        text.Format(_T("%d dpi / %d%%"), kDpiColumns[col], kDpiPercents[col]);
        DrawDemoText(hdc, rcCell, text, kDemoSubtleColor,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // 表头与数据之间的分隔线。
    RECT rcSep;
    rcSep.left = m_rcItem.left + kDemoPadding;
    rcSep.right = m_rcItem.right - kDemoPadding;
    rcSep.top = m_rcItem.top + kTableRowH;
    rcSep.bottom = rcSep.top + 1;
    FillDemoRect(hdc, rcSep, kDemoTrackColor);

    // ---- 数据行 ----
    for (int row = 0; row < kLogicalRowCount; ++row)
    {
        int logical = kLogicalValues[row];
        int rowTop = m_rcItem.top + (row + 1) * kTableRowH;

        rcCell.left = m_rcItem.left + kDemoPadding;
        rcCell.right = rcCell.left + kTableFirstColW;
        rcCell.top = rowTop;
        rcCell.bottom = rowTop + kTableRowH;
        text.Format(_T("%d px"), logical);
        DrawDemoText(hdc, rcCell, text, kDemoTextColor,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        for (int col = 0; col < kDpiColumnCount; ++col)
        {
            int dpi = kDpiColumns[col];
            int scaled = DuiDpi::Scale(logical, dpi);
            int roundTrip = DuiDpi::Unscale(scaled, dpi);

            rcCell.left = m_rcItem.left + kDemoPadding + kTableFirstColW + col * colWidth;
            rcCell.right = rcCell.left + colWidth;
            text.Format(_T("%d  (%s %d)"), scaled,
                        Txt(_T("往返"), _T("back")), roundTrip);

            COLORREF cellColor = kDemoTextColor;
            if (roundTrip != logical)
            {
                cellColor = kDemoWarnColor;
            }
            DrawDemoText(hdc, rcCell, text, cellColor,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
}

// 本机 DPI 的实测显示控件。
//
// 其中「宿主窗口的 DPI」必须等控件挂进控件树、拿到宿主之后才能查，所以
// 三个数值统一放到绘制时现取，而不是在页面构建时算好写死。
class DpiRuntimeView : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

void DpiRuntimeView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    HWND hHostWnd = NULL;
    if (GetHost() != NULL)
    {
        hHostWnd = GetHost()->m_hWnd;
    }

    RECT rcLine = m_rcItem;
    rcLine.left += kDemoPadding;
    rcLine.right -= kDemoPadding;
    rcLine.top += kDemoPadding / 2;
    rcLine.bottom = rcLine.top + kTextLineH;

    CString line;
    line.Format(_T("DuiDpi::GetSystemDpi() = %d"), DuiDpi::GetSystemDpi());
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("DuiDpi::GetWindowDpi(%s) = %d"),
                Txt(_T("宿主窗口"), _T("host window")),
                DuiDpi::GetWindowDpi(hHostWnd));
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("DuiResMgr::Inst().GetDpi() = %d"), DuiResMgr::Inst().GetDpi());
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(Txt(_T("按 DuiResMgr 当前的 DPI 算，缩放比例是 %d%%"),
                    _T("Scaling implied by DuiResMgr's current DPI: %d%%")),
                DuiResMgr::Inst().GetDpi() * 100 / DuiDpi::kDefaultDpi);
    DrawDemoText(hdc, rcLine, line, kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 同一个逻辑边长在某个 DPI 下的物理方块。方块下面标出该 DPI 与换算结果。
class PhysicalSquareTile : public DuiControl
{
public:
    // 构造。
    //   logicalSide：逻辑边长（96 dpi 基线下的像素数）。
    //   dpi：假定的显示器 DPI。
    PhysicalSquareTile(int logicalSide, int dpi);

    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 逻辑边长（像素）。
    int m_logicalSide;
    // 本方块假定的显示器 DPI。
    int m_dpi;
};

PhysicalSquareTile::PhysicalSquareTile(int logicalSide, int dpi)
    : m_logicalSide(logicalSide)
    , m_dpi(dpi)
{
}

void PhysicalSquareTile::OnPaint(HDC hdc, const RECT& rcDirty)
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

    int side = DuiDpi::Scale(m_logicalSide, m_dpi);
    int width = m_rcItem.right - m_rcItem.left;

    RECT rcSquare;
    rcSquare.left = m_rcItem.left + (width - side) / 2;
    rcSquare.right = rcSquare.left + side;
    rcSquare.top = m_rcItem.top;
    rcSquare.bottom = rcSquare.top + side;
    FillDemoRect(hdc, rcSquare, kDemoAccentColor);

    RECT rcCaption = m_rcItem;
    rcCaption.top = m_rcItem.bottom - kTextLineH;
    CString caption;
    caption.Format(_T("%d dpi  ->  %d px"), m_dpi, side);
    DrawDemoText(hdc, rcCaption, caption, kDemoSubtleColor,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 物理方块演示用的逻辑边长（像素）。取 24 是因为它在四种 DPI 下的结果
// （24 / 30 / 36 / 48）都还能并排放进一张卡片。
const int kPhysicalTileLogicalSide = 24;
// 物理方块那一行的高度（像素）：最大的方块是 48，下面再留一行说明文字。
const int kPhysicalTileRowH = 48 + kTextLineH;

// =====================================================================
// 动画页专用的控件与处理器
// =====================================================================

// 一条缓动曲线的展示信息。
struct EaseCurveInfo
{
    // 曲线在 DuiEase 里的函数名。它是 API 标识符，界面上直接显示，不翻译。
    LPCTSTR   name;
    // 曲线函数本身。
    double  (*fn)(double);
    // 这条曲线在轨道演示与折线图里使用的颜色。
    COLORREF  color;
};

// DuiEase 里的全部七条曲线。顺序与 DuiAnimation.h 里的声明顺序一致。
const int kEaseCurveCount = 7;
const EaseCurveInfo kEaseCurves[kEaseCurveCount] = {
    { _T("Linear"),         &DuiEase::Linear,         RGB(120, 128, 140) },
    { _T("EaseInQuad"),     &DuiEase::EaseInQuad,     RGB( 45, 108, 223) },
    { _T("EaseOutQuad"),    &DuiEase::EaseOutQuad,    RGB( 30, 160, 120) },
    { _T("EaseInOutQuad"),  &DuiEase::EaseInOutQuad,  RGB(210, 140,  20) },
    { _T("EaseInCubic"),    &DuiEase::EaseInCubic,    RGB(190,  70, 180) },
    { _T("EaseOutCubic"),   &DuiEase::EaseOutCubic,   RGB(200,  60,  60) },
    { _T("EaseInOutCubic"), &DuiEase::EaseInOutCubic, RGB( 90,  90, 200) },
};

// 轨道演示里方块的边长（像素）。
const int kEaseBlockSide = 18;
// 轨道那条细线的粗细（像素）。
const int kEaseTrackThickness = 2;
// 轨道演示每一行的高度（像素）。
const int kEaseTrackRowH = 24;
// 轨道演示里曲线名那一列的宽度（像素）。最长的名字是 EaseInOutCubic。
const int kEaseNameColW = 120;
// 七条轨道动画的时长（毫秒）。取 1.5 秒是为了让曲线之间的差异看得清楚 ——
// 太快分辨不出先后，太慢又要等。
const int kEaseDemoDurationMs = 1500;

// 沿水平轨道移动的方块。
//
// 它不改变自己的矩形，而是在自身矩形内按进度把方块画到不同的位置。这样
// 动画期间完全不触发布局，只有重绘。
class EaseTrackBlock : public DuiControl
{
public:
    // 构造。
    //   color：方块的填充色。
    explicit EaseTrackBlock(COLORREF color);

    // 设置当前进度。
    //   t：进度，取值范围 [0, 1]，超出范围时被截断到边界。0 表示停在轨道
    //      最左端，1 表示停在最右端。取值与上一次相同时不触发重绘。
    void SetProgress(double t);

    // 读取当前进度。
    //   返回：[0, 1] 之间的进度值。
    double GetProgress() const { return m_progress; }

    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 当前进度，取值 [0, 1]。
    double   m_progress;
    // 方块的填充色。
    COLORREF m_color;
};

EaseTrackBlock::EaseTrackBlock(COLORREF color)
    : m_progress(0.0)
    , m_color(color)
{
}

void EaseTrackBlock::SetProgress(double t)
{
    if (t < 0.0)
    {
        t = 0.0;
    }
    if (t > 1.0)
    {
        t = 1.0;
    }
    if (t == m_progress)
    {
        return;
    }
    m_progress = t;
    Invalidate();
}

void EaseTrackBlock::OnPaint(HDC hdc, const RECT& rcDirty)
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

    int width = m_rcItem.right - m_rcItem.left;
    int height = m_rcItem.bottom - m_rcItem.top;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    RECT rcTrack;
    rcTrack.left = m_rcItem.left;
    rcTrack.right = m_rcItem.right;
    rcTrack.top = m_rcItem.top + height / 2 - kEaseTrackThickness / 2;
    rcTrack.bottom = rcTrack.top + kEaseTrackThickness;
    FillDemoRect(hdc, rcTrack, kDemoTrackColor);

    int travel = width - kEaseBlockSide;
    if (travel < 0)
    {
        travel = 0;
    }
    int offset = (int)(m_progress * travel + 0.5);

    RECT rcBlock;
    rcBlock.left = m_rcItem.left + offset;
    rcBlock.right = rcBlock.left + kEaseBlockSide;
    rcBlock.top = m_rcItem.top + height / 2 - kEaseBlockSide / 2;
    rcBlock.bottom = rcBlock.top + kEaseBlockSide;
    FillDemoRect(hdc, rcBlock, m_color);
}

// 动画管理器状态的只读显示控件。绘制时现读管理器的三个诊断接口。
class AnimStatusView : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

void AnimStatusView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    DuiAnimMgr& mgr = DuiAnimMgr::Inst();

    RECT rcLine = m_rcItem;
    rcLine.left += kDemoPadding;
    rcLine.right -= kDemoPadding;
    rcLine.top += kDemoPadding / 2;
    rcLine.bottom = rcLine.top + kTextLineH;

    CString line;
    line.Format(_T("GetActiveCount() = %d"), mgr.GetActiveCount());
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("IsSelfDriving() = %s"),
                mgr.IsSelfDriving() ? _T("true") : _T("false"));
    DrawDemoText(hdc, rcLine, line, kDemoTextColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("GetPulseTimerId() = %llu"),
                (unsigned long long)mgr.GetPulseTimerId());
    DrawDemoText(hdc, rcLine, line, kDemoTextColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// 动画页面的清理哨兵。
//
// 动画对象被交给管理器之后，页面这边只剩下裸指针形式的引用（各个方块）。
// 用户在动画还没跑完时切走页面，控件树会被销毁而管理器仍然持有这些动画，
// 下一次脉冲就会写向已释放的内存。本控件在析构时清空管理器，把这条路堵死。
//
// 与主题页的还原哨兵一样，它被加为页面容器的第一个子控件且不可见：竖直
// 布局会跳过不可见的子控件，所以它不占版面；子控件按加入顺序销毁，所以
// 它先于各个方块析构。
class AnimPageGuard : public DuiControl
{
public:
    AnimPageGuard();
    ~AnimPageGuard() override;
};

AnimPageGuard::AnimPageGuard()
{
    SetVisible(false);
}

AnimPageGuard::~AnimPageGuard()
{
    DuiAnimMgr::Inst().Clear();
}

// 把动画输出的数值写进一个方块的进度。
//
// DuiDoubleAnim 需要一个可调用对象作为设置器。这里用具名函数对象而不是
// lambda，符合仓库对 lambda 的限制。
struct BlockProgressSetter
{
    // 被驱动的方块。不持有所有权。
    EaseTrackBlock* m_pBlock;
    // 顺带刷新的状态显示控件。可以为空；不持有所有权。
    DuiControl*     m_pStatusView;

    // 动画每一帧被调用。
    //   value：本帧的进度值，取值 [0, 1]。
    void operator()(double value) const;
};

void BlockProgressSetter::operator()(double value) const
{
    if (m_pBlock != NULL)
    {
        m_pBlock->SetProgress(value);
    }
    // 状态显示里的活跃动画个数随动画开始 / 结束而变，跟着一起刷新才能
    // 看到数字回落到 0。
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
}

// 只刷新状态显示、不驱动任何图形的空设置器。
//
// 用于「再加几个动画」那一段：目的只是让活跃动画的个数涨上去，好观察
// 共享脉冲定时器的编号始终不变。
struct StatusOnlySetter
{
    // 状态显示控件。不持有所有权。
    DuiControl* m_pStatusView;

    // 动画每一帧被调用。
    //   value：本帧的进度值，本设置器不使用它。
    void operator()(double value) const;
};

void StatusOnlySetter::operator()(double value) const
{
    (void)value;
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
}

// 折线图的采样点数。65 个点在两百像素宽的图上已经看不出折线的棱角。
const int kChartSamples = 65;
// 折线图左侧留给纵轴刻度文字的宽度（像素）。
const int kChartAxisW = 34;
// 折线图右侧图例区的宽度（像素）。
const int kChartLegendW = 132;
// 折线图底部留给横轴刻度文字的高度（像素）。
const int kChartAxisH = 18;
// 折线的线宽（像素）。
const int kChartLineWidth = 2;
// 图例里色块的边长（像素）。
const int kChartLegendChipSide = 10;
// 折线图那一行的高度（像素）。要装下七条图例，每条一行。
const int kChartRowH = 200;

// 把七条缓动曲线画成折线图的控件。
//
// 曲线是纯函数，这里直接采样，与上面那段真跑动画的演示互为印证：轨道上
// 某一时刻各个方块的相对位置，应当与图上同一横坐标处各条曲线的高低一致。
class EaseCurveChart : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

void EaseCurveChart::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    int plotLeft = m_rcItem.left + kDemoPadding + kChartAxisW;
    int plotRight = m_rcItem.right - kDemoPadding - kChartLegendW;
    int plotTop = m_rcItem.top + kDemoPadding;
    int plotBottom = m_rcItem.bottom - kDemoPadding - kChartAxisH;
    int plotW = plotRight - plotLeft;
    int plotH = plotBottom - plotTop;
    if (plotW <= 1 || plotH <= 1)
    {
        return;
    }

    RECT rcPlot;
    rcPlot.left = plotLeft;
    rcPlot.top = plotTop;
    rcPlot.right = plotRight;
    rcPlot.bottom = plotBottom;
    FillDemoRect(hdc, rcPlot, RGB(255, 255, 255));
    FrameDemoRect(hdc, rcPlot, kDemoTrackColor);

    // 纵轴与横轴的端点刻度。曲线的定义域和值域都是 [0, 1]，标出两端即可。
    RECT rcTick;
    rcTick.left = m_rcItem.left + kDemoPadding;
    rcTick.right = plotLeft - 4;
    rcTick.top = plotTop;
    rcTick.bottom = plotTop + kTextLineH;
    DrawDemoText(hdc, rcTick, _T("1.0"), kDemoSubtleColor,
                 DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    rcTick.top = plotBottom - kTextLineH;
    rcTick.bottom = plotBottom;
    DrawDemoText(hdc, rcTick, _T("0.0"), kDemoSubtleColor,
                 DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

    RECT rcAxis;
    rcAxis.left = plotLeft;
    rcAxis.right = plotRight;
    rcAxis.top = plotBottom;
    rcAxis.bottom = plotBottom + kChartAxisH;
    // 横轴两端的刻度是数学式子，两种语言下写法相同，不需要走 Txt。
    DrawDemoText(hdc, rcAxis, _T("t = 0"), kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawDemoText(hdc, rcAxis, _T("t = 1"), kDemoSubtleColor,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // ---- 七条曲线 ----
    std::vector<POINT> points;
    points.resize(kChartSamples);
    for (int c = 0; c < kEaseCurveCount; ++c)
    {
        if (kEaseCurves[c].fn == NULL)
        {
            continue;
        }
        for (int i = 0; i < kChartSamples; ++i)
        {
            double t = (double)i / (double)(kChartSamples - 1);
            double v = kEaseCurves[c].fn(t);
            points[i].x = plotLeft + (LONG)(t * plotW + 0.5);
            points[i].y = plotBottom - (LONG)(v * plotH + 0.5);
        }
        HPEN hPen = ::CreatePen(PS_SOLID, kChartLineWidth, kEaseCurves[c].color);
        if (hPen == NULL)
        {
            continue;
        }
        HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
        ::Polyline(hdc, &points[0], kChartSamples);
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);
    }

    // ---- 图例 ----
    for (int c = 0; c < kEaseCurveCount; ++c)
    {
        int lineTop = plotTop + c * kTextLineH;

        RECT rcChip;
        rcChip.left = plotRight + kDemoPadding;
        rcChip.right = rcChip.left + kChartLegendChipSide;
        rcChip.top = lineTop + (kTextLineH - kChartLegendChipSide) / 2;
        rcChip.bottom = rcChip.top + kChartLegendChipSide;
        FillDemoRect(hdc, rcChip, kEaseCurves[c].color);

        RECT rcName;
        rcName.left = rcChip.right + 6;
        rcName.right = m_rcItem.right - kDemoPadding;
        rcName.top = lineTop;
        rcName.bottom = lineTop + kTextLineH;
        DrawDemoText(hdc, rcName, kEaseCurves[c].name, kDemoTextColor,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

// 启动七条轨道动画的按钮处理器。
struct StartEaseTracksHandler
{
    // 七个方块，下标与 kEaseCurves 一一对应。不持有所有权。
    EaseTrackBlock* m_blocks[kEaseCurveCount];
    // 管理器状态显示控件。不持有所有权，可以为空。
    DuiControl*     m_pStatusView;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，本处理器不使用它。
    void operator()(FnButton* pButton) const;
};

void StartEaseTracksHandler::operator()(FnButton* pButton) const
{
    (void)pButton;
    for (int i = 0; i < kEaseCurveCount; ++i)
    {
        if (m_blocks[i] == NULL)
        {
            continue;
        }
        m_blocks[i]->SetProgress(0.0);

        BlockProgressSetter setter;
        setter.m_pBlock = m_blocks[i];
        setter.m_pStatusView = m_pStatusView;

        std::unique_ptr<DuiDoubleAnim> anim(
            new DuiDoubleAnim(kEaseDemoDurationMs, 0.0, 1.0, setter));
        // SetEasing 必须在 Add 之前调 —— Add 之后所有权已经交给管理器，
        // 这边的 unique_ptr 已经为空。
        anim->SetEasing(kEaseCurves[i].fn);
        DuiAnimMgr::Inst().Add(std::move(anim));
    }
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
}

// 「再加几个动画」按钮一次加多少条。个数只要大于一就能说明问题，取 5 是
// 为了让活跃个数的变化足够显眼。
const int kExtraAnimCount = 5;
// 这批空动画的时长（毫秒）。比轨道动画长一些，好让人有时间看清定时器编号。
const int kExtraAnimDurationMs = 3000;

// 追加若干条空动画的按钮处理器。
struct AddExtraAnimsHandler
{
    // 管理器状态显示控件。不持有所有权。
    DuiControl* m_pStatusView;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，本处理器不使用它。
    void operator()(FnButton* pButton) const;
};

void AddExtraAnimsHandler::operator()(FnButton* pButton) const
{
    (void)pButton;
    for (int i = 0; i < kExtraAnimCount; ++i)
    {
        StatusOnlySetter setter;
        setter.m_pStatusView = m_pStatusView;
        DuiAnimMgr::Inst().Add(std::unique_ptr<DuiDoubleAnim>(
            new DuiDoubleAnim(kExtraAnimDurationMs, 0.0, 1.0, setter)));
    }
    if (m_pStatusView != NULL)
    {
        m_pStatusView->Invalidate();
    }
}

class FinishClearView;

// 动画完成回调。Finish() 会触发它，Clear() 不会 —— 这正是这一段要展示的
// 区别，所以计数就记在这个回调里。
struct AnimCompleteNotifier
{
    // 记录完成次数的控件。不持有所有权。
    FinishClearView* m_pView;
    // true 表示这是上面那条动画，false 表示下面那条。
    bool             m_bTop;

    // 动画完成时被调用。
    void operator()() const;
};

// Finish 与 Clear 行为差别的演示控件。
//
// 它同时负责三件事：持有两条动画的裸指针、记录完成回调被触发的次数、
// 把这些状态画出来。裸指针的安全性由两条路径共同保证 —— 动画自然完成或
// 被 Finish 时回调里置空，被 Clear 时由发起 Clear 的那个函数置空。
class FinishClearView : public DuiControl
{
public:
    // 构造。
    //   pTopBlock：上面那条轨道的方块。不持有所有权。
    //   pBottomBlock：下面那条轨道的方块。不持有所有权。
    FinishClearView(EaseTrackBlock* pTopBlock, EaseTrackBlock* pBottomBlock);

    // 同时启动两条长时间的动画。已经在跑时先取消再重来。
    void StartBoth();

    // 让上面那条动画立刻跳到终点。它会触发完成回调。
    void FinishTop();

    // 清空管理器里的全部动画。被清掉的动画不会触发完成回调，方块停在
    // 半路。注意 Clear 是进程级的，本页面别处正在跑的动画也会一并取消。
    void ClearAll();

    // 动画完成时由 AnimCompleteNotifier 调用。
    //   bTop：true 表示完成的是上面那条动画。
    void OnAnimCompleted(bool bTop);

    // 绘制自身。
    //   hdc：目标设备上下文。
    //   rcDirty：本次需要重画的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 上面那条动画。所有权在管理器，本控件只借用指针；完成或被清空后置空。
    DuiAnim*        m_pTopAnim;
    // 下面那条动画。所有权同上。
    DuiAnim*        m_pBottomAnim;
    // 上面那条轨道的方块。不持有所有权。
    EaseTrackBlock* m_pTopBlock;
    // 下面那条轨道的方块。不持有所有权。
    EaseTrackBlock* m_pBottomBlock;
    // 完成回调累计被触发的次数。
    int             m_completeHits;
};

void AnimCompleteNotifier::operator()() const
{
    if (m_pView != NULL)
    {
        m_pView->OnAnimCompleted(m_bTop);
    }
}

// Finish / Clear 演示里两条动画的时长（毫秒）。要留够时间让人在动画跑
// 到一半时按下按钮。
const int kFinishClearDurationMs = 6000;

FinishClearView::FinishClearView(EaseTrackBlock* pTopBlock, EaseTrackBlock* pBottomBlock)
    : m_pTopAnim(NULL)
    , m_pBottomAnim(NULL)
    , m_pTopBlock(pTopBlock)
    , m_pBottomBlock(pBottomBlock)
    , m_completeHits(0)
{
}

void FinishClearView::StartBoth()
{
    // 重新开始之前先把上一轮取消掉，否则同一个方块会被两条动画同时驱动。
    ClearAll();

    if (m_pTopBlock != NULL)
    {
        m_pTopBlock->SetProgress(0.0);
        BlockProgressSetter setter;
        setter.m_pBlock = m_pTopBlock;
        setter.m_pStatusView = this;
        std::unique_ptr<DuiDoubleAnim> anim(
            new DuiDoubleAnim(kFinishClearDurationMs, 0.0, 1.0, setter));
        AnimCompleteNotifier notifier;
        notifier.m_pView = this;
        notifier.m_bTop = true;
        // SetOnComplete 同样必须在 Add 之前调。
        anim->SetOnComplete(notifier);
        m_pTopAnim = anim.get();
        DuiAnimMgr::Inst().Add(std::move(anim));
    }

    if (m_pBottomBlock != NULL)
    {
        m_pBottomBlock->SetProgress(0.0);
        BlockProgressSetter setter;
        setter.m_pBlock = m_pBottomBlock;
        setter.m_pStatusView = this;
        std::unique_ptr<DuiDoubleAnim> anim(
            new DuiDoubleAnim(kFinishClearDurationMs, 0.0, 1.0, setter));
        AnimCompleteNotifier notifier;
        notifier.m_pView = this;
        notifier.m_bTop = false;
        anim->SetOnComplete(notifier);
        m_pBottomAnim = anim.get();
        DuiAnimMgr::Inst().Add(std::move(anim));
    }

    Invalidate();
}

void FinishClearView::FinishTop()
{
    if (m_pTopAnim == NULL)
    {
        return;
    }
    // Finish 内部会先按 t = 1 走一帧（方块跳到终点），再触发完成回调
    // （回调里把 m_pTopAnim 置空）。
    m_pTopAnim->Finish();
    Invalidate();
}

void FinishClearView::ClearAll()
{
    DuiAnimMgr::Inst().Clear();
    // Clear 不触发完成回调，所以这两个指针只能在这里手工置空，否则下一次
    // Finish 就会访问已经被管理器释放的动画对象。
    m_pTopAnim = NULL;
    m_pBottomAnim = NULL;
    Invalidate();
}

void FinishClearView::OnAnimCompleted(bool bTop)
{
    ++m_completeHits;
    if (bTop)
    {
        m_pTopAnim = NULL;
    }
    else
    {
        m_pBottomAnim = NULL;
    }
    Invalidate();
}

void FinishClearView::OnPaint(HDC hdc, const RECT& rcDirty)
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

    FillDemoRect(hdc, m_rcItem, kDemoPanelColor);
    FrameDemoRect(hdc, m_rcItem, kDemoBorderColor);

    RECT rcLine = m_rcItem;
    rcLine.left += kDemoPadding;
    rcLine.right -= kDemoPadding;
    rcLine.top += kDemoPadding / 2;
    rcLine.bottom = rcLine.top + kTextLineH;

    CString line;
    line.Format(Txt(_T("完成回调累计触发 %d 次（Finish 会触发，Clear 不会）"),
                    _T("OnComplete fired %d time(s) (Finish fires it, Clear does not)")),
                m_completeHits);
    DrawDemoText(hdc, rcLine, line, kDemoAccentColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(Txt(_T("上面这条动画：%s        下面这条动画：%s"),
                    _T("Top anim: %s        Bottom anim: %s")),
                m_pTopAnim != NULL
                    ? Txt(_T("进行中"), _T("running"))
                    : Txt(_T("已结束或已取消"), _T("finished or cancelled")),
                m_pBottomAnim != NULL
                    ? Txt(_T("进行中"), _T("running"))
                    : Txt(_T("已结束或已取消"), _T("finished or cancelled")));
    DrawDemoText(hdc, rcLine, line, kDemoTextColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::OffsetRect(&rcLine, 0, kTextLineH);
    line.Format(_T("DuiAnimMgr::GetActiveCount() = %d"),
                DuiAnimMgr::Inst().GetActiveCount());
    DrawDemoText(hdc, rcLine, line, kDemoSubtleColor,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// Finish / Clear 演示里三个按钮共用的处理器。
struct FinishClearHandler
{
    // 演示控件。不持有所有权。
    FinishClearView* m_pView;
    // 本按钮执行哪个动作：0 = 启动两条动画，1 = Finish 上面那条，
    // 2 = Clear 全部。
    int              m_action;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，本处理器不使用它。
    void operator()(FnButton* pButton) const;
};

void FinishClearHandler::operator()(FnButton* pButton) const
{
    (void)pButton;
    if (m_pView == NULL)
    {
        return;
    }
    switch (m_action)
    {
    //启动两条长时间动画，供后面 Finish / Clear 使用
    case 0:
        m_pView->StartBoth();
        break;
    //让上面那条动画立刻跳到终点并触发完成回调
    case 1:
        m_pView->FinishTop();
        break;
    //取消全部动画，方块停在半路且不触发完成回调
    case 2:
        m_pView->ClearAll();
        break;
    //取值超出约定范围，说明构建页面时填错了，什么都不做
    default:
        break;
    }
}

// =====================================================================
// 运行期跟踪页专用的辅助函数与处理器
// =====================================================================

// 跟踪日志的文件名。与 DuiTrace.cpp 里的定义保持一致。
const TCHAR* const kTraceLogFileName = _T("DuiTrace.log");
// 回读日志时最多向前读多少字节。跟踪日志一行不过百余字符，八千字节足够
// 覆盖几十行。
const int kTraceTailBytes = 8192;
// 界面上显示日志末尾的多少行。
const int kTraceTailLines = 10;
// 日志显示区里一行文字占的高度（像素）。9 磅微软雅黑的行高约 16 像素，
// 这里按 17 像素估算，避免行距过紧导致最后一行被切掉半截。
const int kTraceLineH = 17;
// 日志显示区的高度（像素）。比实际行数多留两行余量：个别过长的记录会折行，
// 有余量才不至于把最新的几条挤出可见范围（裁掉的是下边，而下边正是最新的）。
const int kTraceOutputH = (kTraceTailLines + 2) * kTraceLineH + kDemoPadding * 2;

// 取跟踪日志文件的完整路径。
//   返回：完整路径；取临时目录失败时返回空串。
CString GetTraceLogPath()
{
    TCHAR szTemp[MAX_PATH] = { 0 };
    DWORD n = ::GetTempPath(MAX_PATH, szTemp);
    if (n == 0 || n >= MAX_PATH)
    {
        return CString();
    }
    CString path = szTemp;
    if (!path.IsEmpty() && path[path.GetLength() - 1] != _T('\\'))
    {
        path += _T("\\");
    }
    path += kTraceLogFileName;
    return path;
}

// 读取跟踪日志末尾的若干行。
//
// 打开方式必须允许共享读写：DuiTrace 全程持有这个文件的句柄，独占方式
// 会直接打不开。
//   maxLines：最多返回多少行，小于一时按一行处理。
//   返回：以换行符分隔的若干行；文件不存在或内容为空时返回空串。
CString ReadTraceLogTail(int maxLines)
{
    if (maxLines < 1)
    {
        maxLines = 1;
    }
    CString path = GetTraceLogPath();
    if (path.IsEmpty())
    {
        return CString();
    }

    HANDLE hFile = ::CreateFile(path, GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return CString();
    }

    LARGE_INTEGER size;
    size.QuadPart = 0;
    if (!::GetFileSizeEx(hFile, &size) || size.QuadPart <= 0)
    {
        ::CloseHandle(hFile);
        return CString();
    }

    LONGLONG offset = 0;
    if (size.QuadPart > (LONGLONG)kTraceTailBytes)
    {
        offset = size.QuadPart - (LONGLONG)kTraceTailBytes;
    }
    LARGE_INTEGER seek;
    seek.QuadPart = offset;
    if (!::SetFilePointerEx(hFile, seek, NULL, FILE_BEGIN))
    {
        ::CloseHandle(hFile);
        return CString();
    }

    std::vector<char> buffer;
    buffer.resize(kTraceTailBytes + 1, 0);
    DWORD read = 0;
    BOOL ok = ::ReadFile(hFile, &buffer[0], (DWORD)kTraceTailBytes, &read, NULL);
    ::CloseHandle(hFile);
    if (!ok || read == 0)
    {
        return CString();
    }
    buffer[read] = '\0';

    // 日志是用窄字符写的纯 ASCII，按 ANSI 解读即可；上面已经补过结尾的空
    // 字符，这里可以直接当作 C 字符串交给 CString 转换。
    CString text(&buffer[0]);
    // 日志文件是以文本方式打开的，换行会被写成回车加换行两个字符。回车留着
    // 会在 DrawText 里画成一个方框，先去掉。
    text.Replace(_T("\r"), _T(""));

    // 从后往前数行。跳过最后一个换行符之后的空片段。
    std::vector<CString> lines;
    int start = 0;
    while (start <= text.GetLength())
    {
        int pos = text.Find(_T('\n'), start);
        if (pos < 0)
        {
            lines.push_back(text.Mid(start));
            break;
        }
        lines.push_back(text.Mid(start, pos - start));
        start = pos + 1;
    }
    // 从中间截断的第一行是残缺的，丢掉它。
    if (offset > 0 && !lines.empty())
    {
        lines.erase(lines.begin());
    }

    int first = (int)lines.size() - maxLines;
    if (first < 0)
    {
        first = 0;
    }
    CString result;
    for (int i = first; i < (int)lines.size(); ++i)
    {
        if (lines[i].IsEmpty())
        {
            continue;
        }
        if (!result.IsEmpty())
        {
            result += _T("\n");
        }
        result += lines[i];
    }
    return result;
}

// 把日志末尾读回来写进显示控件；跟踪没开启时写一句提示。
//   pOutput：日志显示控件，为空时本函数直接返回。
void RefreshTraceOutput(DuiLabel* pOutput)
{
    if (pOutput == NULL)
    {
        return;
    }
    if (!DuiTrace::IsEnabled())
    {
        pOutput->SetText(Txt(
            _T("跟踪未开启，日志文件不存在。请先设置环境变量 BUI_DUI_TRACE=1，")
            _T("再重新启动本程序。"),
            _T("Tracing is off, so there is no log file. Set BUI_DUI_TRACE=1 ")
            _T("and restart the program first.")));
        return;
    }
    CString tail = ReadTraceLogTail(kTraceTailLines);
    if (tail.IsEmpty())
    {
        pOutput->SetText(Txt(_T("日志文件为空。"), _T("The log file is empty.")));
        return;
    }
    pOutput->SetText(tail);
}

// 写入几条示例跟踪记录，再把日志末尾读回来显示的按钮处理器。
struct WriteTraceSampleHandler
{
    // 日志显示控件。不持有所有权。
    DuiLabel* m_pOutput;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，用它找到宿主窗口发起重绘。
    void operator()(FnButton* pButton) const;
};

void WriteTraceSampleHandler::operator()(FnButton* pButton) const
{
    // 跟踪日志一律用英文，与仓库约定一致。
    BUI_TRACE("GALLERY-DEMO-BEGIN sample sequence");
    BUI_TRACE("GALLERY-DEMO-STEP index=1 note=first");
    BUI_TRACE("GALLERY-DEMO-STEP index=2 note=second");
    BUI_TRACE("GALLERY-DEMO-END");

    RefreshTraceOutput(m_pOutput);
    if (pButton != NULL && pButton->GetHost() != NULL)
    {
        pButton->GetHost()->Invalidate(FALSE);
    }
}

// 强制整窗重绘、再把日志末尾读回来的按钮处理器。
//
// 这一段看的是库里现成的埋点：DuiHost::OnPaint 里有 HOST-PAINT-BEGIN、
// HOST-PAINT-BG-DONE、HOST-PAINT-TREE-DONE 三条，读回来就能看到这一次
// 绘制各阶段的实测间隔。
struct CaptureRepaintTraceHandler
{
    // 日志显示控件。不持有所有权。
    DuiLabel* m_pOutput;

    // 按钮点击时被调用。
    //   pButton：被点击的按钮，用它找到宿主窗口。
    void operator()(FnButton* pButton) const;
};

void CaptureRepaintTraceHandler::operator()(FnButton* pButton) const
{
    if (pButton == NULL || pButton->GetHost() == NULL)
    {
        return;
    }
    DuiHost* pHost = pButton->GetHost();

    BUI_TRACE("GALLERY-DEMO-FORCE-REPAINT");
    pHost->Invalidate(FALSE);
    // UpdateWindow 让这次绘制同步完成，回读日志时那三条记录已经落盘。
    pHost->UpdateWindow();

    RefreshTraceOutput(m_pOutput);
    pHost->Invalidate(FALSE);
}

} // 匿名命名空间

// ===== DuiTheme 主题 =================================================

std::unique_ptr<DuiControl> Build_Theme()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 还原哨兵必须是第一个子控件：子控件按加入顺序销毁，它先于订阅演示
    // 控件析构，写回色板时订阅者仍然有效。
    page->AddChild(std::unique_ptr<DuiControl>(new ThemePaletteGuard()),
                   DuiLayout::Hint().Fixed(0));

    // ---- 预设切换 ----

    AddSection(page.get(),
               Txt(_T("三套预设"), _T("Three presets")),
               Txt(_T("DuiTheme 是进程级单例，Get(slot) 取色、Set(slot, color) 改色、")
                   _T("ApplyPreset 一次换掉整套。库里内置 Light（默认）、Dark、")
                   _T("HighContrast 三套预设，HighContrast 是黑底加纯黄强调色，")
                   _T("不依赖操作系统的高对比度设置。切换之后本页面的色板会立刻跟着变；")
                   _T("离开本页面时会自动还原成进入前的样子，不会影响其它页面。"),
                   _T("DuiTheme is a process-wide singleton: Get(slot) reads a color, ")
                   _T("Set(slot, color) writes one, and ApplyPreset swaps the whole ")
                   _T("palette at once. Three presets ship with the library: Light ")
                   _T("(the default), Dark, and HighContrast — black background with ")
                   _T("pure-yellow accents, independent of the OS high-contrast ")
                   _T("setting. The swatches below follow the switch immediately, and ")
                   _T("the palette is restored when you leave this page.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<FnButton> btnLight(new FnButton());
        btnLight->SetText(_T("ApplyPreset(Light)"));
        ApplyPresetHandler handlerLight;
        handlerLight.m_preset = DuiTheme::Light;
        btnLight->onClick = handlerLight;
        row->AddChild(std::move(btnLight), DuiLayout::Hint().Fixed(170));

        std::unique_ptr<FnButton> btnDark(new FnButton());
        btnDark->SetText(_T("ApplyPreset(Dark)"));
        ApplyPresetHandler handlerDark;
        handlerDark.m_preset = DuiTheme::Dark;
        btnDark->onClick = handlerDark;
        row->AddChild(std::move(btnDark), DuiLayout::Hint().Fixed(170));

        std::unique_ptr<FnButton> btnHigh(new FnButton());
        btnHigh->SetText(_T("ApplyPreset(HighContrast)"));
        ApplyPresetHandler handlerHigh;
        handlerHigh.m_preset = DuiTheme::HighContrast;
        btnHigh->onClick = handlerHigh;
        row->AddChild(std::move(btnHigh), DuiLayout::Hint().Fixed(220));

        AddVariantRow(page.get(), std::move(row), 32);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::unique_ptr<DuiControl>(new ThemeStatusView()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextLineH * 3 + kDemoPadding);
    }

    // ---- 主题目前没有接入任何控件 ----

    AddSection(page.get(),
               Txt(_T("主题目前没有接入任何 balloonui 控件"),
                   _T("No balloonui control reads DuiTheme yet")),
               Txt(_T("这是一件必须说清楚的事：整个 balloonui/Controls/ 目录里没有一处")
                   _T("引用 DuiTheme，控件的颜色都是各自 .cpp 文件里的 const COLORREF ")
                   _T("常量（例如 DuiButton.cpp 第 26 至 58 行的那一组），与主题的色板")
                   _T("毫无关联；SubscribeChange 在库内和客户端里也没有任何使用者。")
                   _T("所以切换预设时，只有像下面左边这样、在自己的 OnPaint 里现取 ")
                   _T("DuiTheme::Inst().Get(...) 的自绘控件会变色，右边这三个真控件")
                   _T("纹丝不动。请点上面的按钮亲自看一遍。"),
                   _T("This needs to be stated plainly: nothing under ")
                   _T("balloonui/Controls/ references DuiTheme. Control colors are ")
                   _T("const COLORREF constants inside each .cpp (see DuiButton.cpp ")
                   _T("lines 26-58), unrelated to the theme palette, and ")
                   _T("SubscribeChange has no callers in the library or the client. ")
                   _T("So switching presets only recolors self-drawn controls that ")
                   _T("read DuiTheme::Inst().Get(...) in their own OnPaint, like the ")
                   _T("ones on the left. The three real controls on the right do not ")
                   _T("move. Press the buttons above and see for yourself.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiLabel> leftCaption(new DuiLabel());
        leftCaption->SetText(Txt(_T("读取主题的自绘方块 —— 会变"),
                                 _T("Self-drawn, reads DuiTheme — changes")));
        leftCaption->SetTextColor(kDemoSubtleColor);
        row->AddChild(std::move(leftCaption), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiLabel> rightCaption(new DuiLabel());
        rightCaption->SetText(Txt(_T("balloonui 真控件 —— 不会变"),
                                  _T("Real balloonui controls — unchanged")));
        rightCaption->SetTextColor(kDemoSubtleColor);
        row->AddChild(std::move(rightCaption), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kTextLineH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(20);

        std::unique_ptr<DuiHBox> leftBox(new DuiHBox());
        leftBox->SetGap(6);
        leftBox->AddChild(std::unique_ptr<DuiControl>(
            new ThemeSwatch(DuiTheme::BrandPrimary, _T("BrandPrimary"), false)),
            DuiLayout::Hint().Weight(1));
        leftBox->AddChild(std::unique_ptr<DuiControl>(
            new ThemeSwatch(DuiTheme::SurfaceBg, _T("SurfaceBg"), false)),
            DuiLayout::Hint().Weight(1));
        leftBox->AddChild(std::unique_ptr<DuiControl>(
            new ThemeSwatch(DuiTheme::RowSel, _T("RowSel"), false)),
            DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(leftBox), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiHBox> rightBox(new DuiHBox());
        rightBox->SetGap(10);

        std::unique_ptr<DuiButton> btnPrimary(new DuiButton());
        btnPrimary->SetText(Txt(_T("主操作"), _T("Primary")));
        btnPrimary->SetVariant(DuiButton::Variant::Primary);
        rightBox->AddChild(std::move(btnPrimary), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiButton> btnDefault(new DuiButton());
        btnDefault->SetText(Txt(_T("次操作"), _T("Default")));
        btnDefault->SetVariant(DuiButton::Variant::Default);
        rightBox->AddChild(std::move(btnDefault), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiSlider> slider(new DuiSlider());
        slider->SetRange(0, 100);
        slider->SetPos(60, false);
        rightBox->AddChild(std::move(slider), DuiLayout::Hint().Weight(1));

        row->AddChild(std::move(rightBox), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 34);
    }

    // ---- 完整色板 ----

    AddSection(page.get(),
               Txt(_T("全部 20 个色槽"), _T("All 20 palette slots")),
               Txt(_T("DuiTheme::Slot 一共 20 个槽位，分成品牌色、文字色、表面色、")
                   _T("边框色、行状态色、在线状态色六组。每格上面是槽位名、下面是")
                   _T("当前的十六进制值，都是绘制时现取的。新增槽位必须加在枚举末尾，")
                   _T("否则已有取值会整体错位。"),
                   _T("DuiTheme::Slot has 20 entries grouped into brand, text, ")
                   _T("surface, border, row-state and presence colors. Each cell shows ")
                   _T("the slot name above its current hex value, both read at paint ")
                   _T("time. New slots must be appended to the end of the enum, ")
                   _T("otherwise every existing value shifts.")));
    {
        int slotIndex = 0;
        while (slotIndex < (int)DuiTheme::SlotCount)
        {
            std::unique_ptr<DuiHBox> row(new DuiHBox());
            row->SetGap(6);
            for (int col = 0; col < kSwatchesPerRow; ++col)
            {
                if (slotIndex >= (int)DuiTheme::SlotCount)
                {
                    break;
                }
                row->AddChild(std::unique_ptr<DuiControl>(
                    new ThemeSwatch(kAllThemeSlots[slotIndex].slot,
                                    kAllThemeSlots[slotIndex].name, true)),
                    DuiLayout::Hint().Weight(1));
                ++slotIndex;
            }
            AddVariantRow(page.get(), std::move(row), kSwatchRowH);
        }
    }

    // ---- 变化订阅 ----

    ThemeSubscriberView* pSubscriberView = NULL;
    AddSection(page.get(),
               Txt(_T("变化订阅"), _T("Change subscription")),
               Txt(_T("SubscribeChange(回调, 用户数据) 返回一个大于零的令牌，")
                   _T("Unsubscribe(令牌) 取消订阅。Set 与 ApplyPreset 都会让版本号")
                   _T("加一并逐个通知订阅者；把同一个颜色再写一遍则既不涨版本号也不")
                   _T("发通知。注意回调表里存的是裸指针，控件销毁前必须退订 —— ")
                   _T("下面这个演示控件在自己的析构函数里做了这件事。"),
                   _T("SubscribeChange(callback, userdata) returns a token greater ")
                   _T("than zero; Unsubscribe(token) cancels it. Both Set and ")
                   _T("ApplyPreset bump the version and notify every subscriber, ")
                   _T("while writing the same color again bumps nothing and notifies ")
                   _T("nobody. The registry holds raw pointers, so a control must ")
                   _T("unsubscribe before it dies — the demo control below does that ")
                   _T("in its destructor.")));
    {
        std::unique_ptr<ThemeSubscriberView> view(new ThemeSubscriberView());
        pSubscriberView = view.get();

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(12);

        std::unique_ptr<FnButton> btnSubscribe(new FnButton());
        btnSubscribe->SetText(Txt(_T("订阅"), _T("Subscribe")));
        ThemeSubscribeHandler subscribeHandler;
        subscribeHandler.m_pView = pSubscriberView;
        subscribeHandler.m_bSubscribe = true;
        btnSubscribe->onClick = subscribeHandler;
        buttonRow->AddChild(std::move(btnSubscribe), DuiLayout::Hint().Fixed(120));

        std::unique_ptr<FnButton> btnUnsubscribe(new FnButton());
        btnUnsubscribe->SetText(Txt(_T("退订"), _T("Unsubscribe")));
        ThemeSubscribeHandler unsubscribeHandler;
        unsubscribeHandler.m_pView = pSubscriberView;
        unsubscribeHandler.m_bSubscribe = false;
        btnUnsubscribe->onClick = unsubscribeHandler;
        buttonRow->AddChild(std::move(btnUnsubscribe), DuiLayout::Hint().Fixed(120));

        std::unique_ptr<FnButton> btnMutate(new FnButton());
        btnMutate->SetText(Txt(_T("改一个色槽"), _T("Change one slot")));
        ThemeMutateHandler mutateHandler;
        mutateHandler.m_pStatusView = pSubscriberView;
        btnMutate->onClick = mutateHandler;
        buttonRow->AddChild(std::move(btnMutate), DuiLayout::Hint().Fixed(160));

        AddVariantRow(page.get(), std::move(buttonRow), 32);

        std::unique_ptr<DuiHBox> viewRow(new DuiHBox());
        viewRow->AddChild(std::move(view), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(viewRow), kTextLineH * 3 + kDemoPadding);
    }

    // ---- 默认字体钩子 ----

    AddSection(page.get(),
               Txt(_T("默认字体钩子"), _T("Default font hooks")),
               Txt(_T("SetDefaultFontPt / SetDefaultFontFace 把字号与字体名存进主题，")
                   _T("字号会被限制在 6 到 96 磅之间。头文件写的是「实际的 HFONT 来自 ")
                   _T("DuiResMgr，在这里设置会让它重建」，但实现并非如此：")
                   _T("DuiResMgr::GetDefaultFont 里的 9 磅与 Microsoft YaHei 都是写死的，")
                   _T("它根本不读 DuiTheme。所以拖动下面的滑块时，左边这个自己去读")
                   _T("字号的标签会跟着变大变小，右边用库默认字体的标签始终不动。"),
                   _T("SetDefaultFontPt / SetDefaultFontFace store a point size and a ")
                   _T("face name in the theme, the size clamped to 6..96. The header ")
                   _T("says \"the actual HFONT comes from DuiResMgr; setting these ")
                   _T("here rebuilds it\", but the implementation does not: ")
                   _T("DuiResMgr::GetDefaultFont hardcodes 9pt and Microsoft YaHei ")
                   _T("and never reads DuiTheme. So the left label below, which reads ")
                   _T("the point size itself, follows the slider, while the right one ")
                   _T("on the library default font never moves.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);

        std::unique_ptr<DuiLabel> caption(new DuiLabel());
        caption->SetText(_T("SetDefaultFontPt"));
        caption->SetTextColor(kDemoTextColor);
        row->AddChild(std::move(caption), DuiLayout::Hint().Fixed(140));

        std::unique_ptr<DuiSlider> slider(new DuiSlider());
        slider->SetCtrlId(kIdThemeFontSlider);
        // 上限取 24 磅：再大就会撑破下面那一行的高度，看不出对比。
        slider->SetRange(6, 24);
        slider->SetPos(DuiTheme::Inst().GetDefaultFontPt(), false);
        slider->SetLineSize(1);
        slider->SetTickFrequency(2);
        row->AddChild(std::move(slider), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiLabel> valueLabel(new DuiLabel());
        CString valueText;
        valueText.Format(_T("%d pt"), DuiTheme::Inst().GetDefaultFontPt());
        valueLabel->SetText(valueText);
        valueLabel->SetTextColor(kDemoAccentColor);
        DuiLabel* pValueLabel = valueLabel.get();
        row->AddChild(std::move(valueLabel), DuiLayout::Hint().Fixed(70));

        AddVariantRow(page.get(), std::move(row), 32);

        std::unique_ptr<DuiHBox> previewRow(new DuiHBox());
        previewRow->SetGap(12);

        std::unique_ptr<ThemeFontPreviewLabel> preview(new ThemeFontPreviewLabel());
        preview->SetText(Txt(_T("这个标签自己去读主题的字号"),
                             _T("This label reads the theme's point size")));
        DuiControl* pPreview = preview.get();
        previewRow->AddChild(std::move(preview), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiLabel> fixedLabel(new DuiLabel());
        fixedLabel->SetText(Txt(_T("这个标签用库的默认字体，不受影响"),
                                _T("This one uses the library default font")));
        fixedLabel->SetTextColor(kDemoSubtleColor);
        previewRow->AddChild(std::move(fixedLabel), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(previewRow), 44);

        ThemeFontSliderHook hook;
        hook.m_pPreview = pPreview;
        hook.m_pValueLabel = pValueLabel;
        hook.m_pStatusView = pSubscriberView;
        g_pageNotifyHook = hook;
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiDpi 高 DPI =================================================

std::unique_ptr<DuiControl> Build_Dpi()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // ---- 换算表 ----

    AddSection(page.get(),
               Txt(_T("Scale / Unscale 换算表"), _T("Scale / Unscale table")),
               Txt(_T("Scale(逻辑值, dpi) 把 96 dpi 基线下的设计尺寸换成当前显示器的")
                   _T("物理像素，Unscale 反向换回来。两个都是纯函数、DPI 由参数传入，")
                   _T("所以下面这张表不需要改动任何系统设置就能列出四种 DPI 下的结果。")
                   _T("括号里是 Unscale(Scale(v)) 的往返结果，与原值不同的格子会标成")
                   _T("红色。头文件说 Scale 四舍五入而 Unscale 向零取整，但实现里两个")
                   _T("方向用的都是 MulDiv（四舍五入），所以表内这些常见取值往返都能")
                   _T("回到原值。"),
                   _T("Scale(logical, dpi) converts a design size measured at the ")
                   _T("96 dpi baseline into physical pixels for a given monitor, and ")
                   _T("Unscale converts back. Both are pure functions taking the DPI ")
                   _T("as an argument, so this table needs no system setting changed. ")
                   _T("The value in parentheses is the Unscale(Scale(v)) round trip; ")
                   _T("cells where it differs from the original are drawn in red. The ")
                   _T("header claims Scale rounds while Unscale truncates toward zero, ")
                   _T("but both directions use MulDiv (round to nearest), so every ")
                   _T("value in this table round-trips exactly.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::unique_ptr<DuiControl>(new DpiScaleTableView()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTableRowH * (kLogicalRowCount + 1) + 4);
    }

    // ---- 本机实测 ----

    AddSection(page.get(),
               Txt(_T("本机实测"), _T("Measured on this machine")),
               Txt(_T("GetSystemDpi 查的是主显示器，GetWindowDpi 查的是指定窗口当前")
                   _T("所在的那块显示器（在 Windows 10 1703 之前或接口缺失时退回主")
                   _T("显示器），DuiResMgr 保存的那一份是宿主窗口在创建时以及每次收到")
                   _T("系统 DPI 变化通知时写进去的，字体就按它来建。把窗口拖到另一块")
                   _T("缩放比例不同的显示器上，这三个数会跟着变。"),
                   _T("GetSystemDpi queries the primary monitor; GetWindowDpi queries ")
                   _T("the monitor a given window currently sits on (falling back to ")
                   _T("the primary one before Windows 10 1703 or when the API is ")
                   _T("missing); the value inside DuiResMgr is written by the host on ")
                   _T("creation and on every system DPI-change notification, and the ")
                   _T("shared fonts are built from it. Drag the window onto a monitor ")
                   _T("with a different scaling factor and all three follow.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::unique_ptr<DuiControl>(new DpiRuntimeView()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextLineH * 4 + kDemoPadding);
    }

    // ---- 物理方块 ----

    AddSection(page.get(),
               Txt(_T("同一个逻辑边长在四种 DPI 下的物理大小"),
                   _T("One logical size at four DPI values")),
               Txt(_T("四个方块的逻辑边长都是 24，只是分别按 96 / 120 / 144 / 192 dpi ")
                   _T("换算成了物理像素。这就是「控件代码里写 8 像素内边距，在任何")
                   _T("缩放比例下都是 8 个逻辑像素」的含义 —— 设计稿上的数字不变，")
                   _T("落到屏幕上的像素数随显示器变。"),
                   _T("All four squares are 24 logical pixels wide; they differ only ")
                   _T("in the DPI used to convert them into physical pixels — 96, 120, ")
                   _T("144 and 192. This is what \"an 8 px padding written in control ")
                   _T("code stays 8 logical pixels at any scaling factor\" means: the ")
                   _T("number on the design is fixed, the pixels on screen are not.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(12);
        for (int i = 0; i < kDpiColumnCount; ++i)
        {
            row->AddChild(std::unique_ptr<DuiControl>(
                new PhysicalSquareTile(kPhysicalTileLogicalSide, kDpiColumns[i])),
                DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), kPhysicalTileRowH);
    }

    // ---- 库内真正用到 DPI 的地方 ----

    AddSection(page.get(),
               Txt(_T("库内真正按 DPI 缩放的只有两处"),
                   _T("Only two places in the library actually scale by DPI")),
               Txt(_T("这一点值得单独说明，免得对 DPI 支持的完备程度产生误解。")
                   _T("全库检索下来，除去 DuiDpi 自身，用到 DPI 的只有三处：")
                   _T("DuiHost 在创建时和收到系统 DPI 变化通知时缓存一份 DPI 并转告 ")
                   _T("DuiResMgr；DuiResMgr 按它建默认字体与按磅值缓存的字体；")
                   _T("DuiFrameWindow 按它缩放窗口边框的拖拽宽度。除此之外，")
                   _T("控件的内边距、间距、圆角半径、图标尺寸全是未经缩放的常量，")
                   _T("在高 DPI 显示器上它们的物理尺寸会比设计意图偏小。"),
                   _T("Worth stating on its own so the state of DPI support is not ")
                   _T("overestimated. Searching the whole library, only three places ")
                   _T("use DPI besides DuiDpi itself: DuiHost caches it on creation ")
                   _T("and on every system DPI-change notification and forwards it to ")
                   _T("DuiResMgr; DuiResMgr builds the default and per-point-size ")
                   _T("fonts from it; DuiFrameWindow scales the window's resize border ")
                   _T("width. Everything else — control padding, gaps, corner radii, ")
                   _T("icon sizes — is an unscaled constant, so on a high-DPI monitor ")
                   _T("those come out physically smaller than intended.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiAnimation 动画与缓动 =======================================

std::unique_ptr<DuiControl> Build_Animation()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 清理哨兵必须是第一个子控件，理由见类注释。
    page->AddChild(std::unique_ptr<DuiControl>(new AnimPageGuard()),
                   DuiLayout::Hint().Fixed(0));

    // ---- 七条曲线同时跑 ----
    //
    // 管理器状态显示控件在这里就建出来，因为轨道的启动按钮要在自己的处理器
    // 里刷新它；控件本身则要等到后面「管理器状态」那一段才挂进页面。

    std::unique_ptr<AnimStatusView> statusView(new AnimStatusView());
    AnimStatusView* pStatusView = statusView.get();

    StartEaseTracksHandler startHandler;
    for (int i = 0; i < kEaseCurveCount; ++i)
    {
        startHandler.m_blocks[i] = NULL;
    }
    startHandler.m_pStatusView = pStatusView;

    AddSection(page.get(),
               Txt(_T("七条缓动曲线同时跑"), _T("All seven easing curves at once")),
               Txt(_T("点下面的按钮，七个方块同时出发、同时到达，用的却是 DuiEase ")
                   _T("里七条不同的曲线，所以中途任何一个时刻它们的位置都不一样：")
                   _T("EaseIn 系列起步慢、末段快，EaseOut 系列反过来，EaseInOut ")
                   _T("两头慢中间快，Linear 匀速。动画时长都是 1.5 秒。")
                   _T("写这段代码时有一条容易踩的顺序要求：SetEasing 与 SetOnComplete ")
                   _T("必须在 Add 之前调用 —— Add 之后动画的所有权已经交给管理器，")
                   _T("调用方手里的智能指针已经为空。"),
                   _T("Press the button and seven blocks leave and arrive together, ")
                   _T("each driven by a different curve from DuiEase, so at any ")
                   _T("moment in between they sit at different positions: the EaseIn ")
                   _T("family starts slow and finishes fast, EaseOut is the reverse, ")
                   _T("EaseInOut is slow at both ends, and Linear is constant speed. ")
                   _T("Every animation lasts 1.5 seconds. One ordering rule is easy to ")
                   _T("get wrong: SetEasing and SetOnComplete must be called before ")
                   _T("Add, because after Add the manager owns the animation and the ")
                   _T("caller's smart pointer is empty.")));
    {
        for (int i = 0; i < kEaseCurveCount; ++i)
        {
            std::unique_ptr<DuiHBox> row(new DuiHBox());
            row->SetGap(10);

            std::unique_ptr<DuiLabel> name(new DuiLabel());
            name->SetText(kEaseCurves[i].name);
            name->SetTextColor(kDemoTextColor);
            row->AddChild(std::move(name), DuiLayout::Hint().Fixed(kEaseNameColW));

            std::unique_ptr<EaseTrackBlock> block(new EaseTrackBlock(kEaseCurves[i].color));
            startHandler.m_blocks[i] = block.get();
            row->AddChild(std::move(block), DuiLayout::Hint().Weight(1));

            AddVariantRow(page.get(), std::move(row), kEaseTrackRowH);
        }

        AddGap(page.get(), 8);

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        std::unique_ptr<FnButton> btnStart(new FnButton());
        btnStart->SetText(Txt(_T("七条一起跑"), _T("Run all seven")));
        btnStart->onClick = startHandler;
        buttonRow->AddChild(std::move(btnStart), DuiLayout::Hint().Fixed(200));
        AddVariantRow(page.get(), std::move(buttonRow), 32);
    }

    // ---- 管理器状态 ----

    AddSection(page.get(),
               Txt(_T("管理器状态：一个进程只有一个脉冲定时器"),
                   _T("Manager state: one pulse timer per process")),
               Txt(_T("DuiAnimMgr 是进程单例，活跃列表从空变成非空时装上一个 16 毫秒")
                   _T("（约 60 赫兹）的线程定时器，列表清空时立刻卸掉，空闲期不会有")
                   _T("定时器长期挂着。反复点「再加 5 个动画」可以看到活跃个数不断")
                   _T("上涨而定时器编号始终是同一个 —— 这就是「一个进程一个定时器，")
                   _T("而不是一个动画一个定时器」；等全部跑完，个数归零、")
                   _T("IsSelfDriving 变回 false、编号也回到 0。"),
                   _T("DuiAnimMgr is a process singleton. It installs one 16 ms ")
                   _T("(about 60 Hz) thread timer when its active list goes from empty ")
                   _T("to non-empty and drops it the moment the list empties, so an ")
                   _T("idle process carries no timer. Press \"add 5 more\" repeatedly ")
                   _T("and the active count climbs while the timer id stays the same — ")
                   _T("that is \"one timer per process, not one per animation\". Once ")
                   _T("everything finishes the count returns to zero, IsSelfDriving ")
                   _T("goes back to false and the id back to 0.")));
    {
        std::unique_ptr<DuiHBox> statusRow(new DuiHBox());
        statusRow->AddChild(std::move(statusView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(statusRow), kTextLineH * 3 + kDemoPadding);

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(12);

        std::unique_ptr<FnButton> btnExtra(new FnButton());
        btnExtra->SetText(Txt(_T("再加 5 个动画"), _T("Add 5 more animations")));
        AddExtraAnimsHandler extraHandler;
        extraHandler.m_pStatusView = pStatusView;
        btnExtra->onClick = extraHandler;
        buttonRow->AddChild(std::move(btnExtra), DuiLayout::Hint().Fixed(200));

        AddVariantRow(page.get(), std::move(buttonRow), 32);
    }

    // ---- 曲线折线图 ----

    AddSection(page.get(),
               Txt(_T("七条曲线的形状"), _T("The shape of the seven curves")),
               Txt(_T("横轴是时间进度 t，纵轴是缓动之后的进度值，两者都在 0 到 1 之间。")
                   _T("七条曲线都精确经过 (0, 0) 与 (1, 1)，差别全在中间。这张图是对")
                   _T("曲线函数直接采样画出来的，与上面那段真跑动画互为印证：某一时刻")
                   _T("七个方块的左右次序，应当与同一横坐标处七条曲线的上下次序一致。"),
                   _T("The horizontal axis is the time progress t, the vertical axis ")
                   _T("is the eased progress, both in the 0..1 range. All seven curves ")
                   _T("pass exactly through (0, 0) and (1, 1); they differ only in ")
                   _T("between. The plot samples the curve functions directly and ")
                   _T("cross-checks the running demo above: at any instant the ")
                   _T("left-to-right order of the seven blocks matches the ")
                   _T("bottom-to-top order of the seven curves at that abscissa.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::unique_ptr<DuiControl>(new EaseCurveChart()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kChartRowH);
    }

    // ---- Finish 与 Clear ----

    AddSection(page.get(),
               Txt(_T("Finish 与 Clear 的差别"), _T("Finish versus Clear")),
               Txt(_T("两条动画各跑 6 秒。跑到一半时点 Finish，上面那条会立刻跳到")
                   _T("终点并触发它的完成回调（下面的计数加一）；点 Clear 则两条都")
                   _T("停在半路，完成回调一次也不触发。这个区别在实际使用中很关键：")
                   _T("淡出动画用 Finish 收尾能保证「最终状态被应用」，而窗口销毁前")
                   _T("必须用 Clear，因为那时回调里引用的控件已经不能再碰了。")
                   _T("另外 Clear 是进程级的，会把本页面别处正在跑的动画一并取消。"),
                   _T("Two animations, six seconds each. Press Finish halfway and the ")
                   _T("top one jumps straight to its end and fires its completion ")
                   _T("callback (the counter below goes up); press Clear and both stop ")
                   _T("where they are with no callback at all. The distinction matters ")
                   _T("in practice: finishing a fade with Finish guarantees the final ")
                   _T("state is applied, while a window must Clear before it is ")
                   _T("destroyed because the controls its callbacks reference are no ")
                   _T("longer safe to touch. Note that Clear is process-wide and also ")
                   _T("cancels whatever else on this page is running.")));
    {
        std::unique_ptr<EaseTrackBlock> topBlock(new EaseTrackBlock(kDemoAccentColor));
        EaseTrackBlock* pTopBlock = topBlock.get();
        std::unique_ptr<DuiHBox> topRow(new DuiHBox());
        topRow->SetGap(10);
        std::unique_ptr<DuiLabel> topName(new DuiLabel());
        topName->SetText(Txt(_T("上面这条"), _T("Top")));
        topName->SetTextColor(kDemoTextColor);
        topRow->AddChild(std::move(topName), DuiLayout::Hint().Fixed(kEaseNameColW));
        topRow->AddChild(std::move(topBlock), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(topRow), kEaseTrackRowH);

        std::unique_ptr<EaseTrackBlock> bottomBlock(new EaseTrackBlock(kDemoWarnColor));
        EaseTrackBlock* pBottomBlock = bottomBlock.get();
        std::unique_ptr<DuiHBox> bottomRow(new DuiHBox());
        bottomRow->SetGap(10);
        std::unique_ptr<DuiLabel> bottomName(new DuiLabel());
        bottomName->SetText(Txt(_T("下面这条"), _T("Bottom")));
        bottomName->SetTextColor(kDemoTextColor);
        bottomRow->AddChild(std::move(bottomName), DuiLayout::Hint().Fixed(kEaseNameColW));
        bottomRow->AddChild(std::move(bottomBlock), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(bottomRow), kEaseTrackRowH);

        std::unique_ptr<FinishClearView> view(new FinishClearView(pTopBlock, pBottomBlock));
        FinishClearView* pView = view.get();

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(12);

        std::unique_ptr<FnButton> btnStart(new FnButton());
        btnStart->SetText(Txt(_T("两条一起开始"), _T("Start both")));
        FinishClearHandler startAction;
        startAction.m_pView = pView;
        startAction.m_action = 0;
        btnStart->onClick = startAction;
        buttonRow->AddChild(std::move(btnStart), DuiLayout::Hint().Fixed(160));

        std::unique_ptr<FnButton> btnFinish(new FnButton());
        btnFinish->SetText(_T("Finish()"));
        FinishClearHandler finishAction;
        finishAction.m_pView = pView;
        finishAction.m_action = 1;
        btnFinish->onClick = finishAction;
        buttonRow->AddChild(std::move(btnFinish), DuiLayout::Hint().Fixed(140));

        std::unique_ptr<FnButton> btnClear(new FnButton());
        btnClear->SetText(_T("Clear()"));
        FinishClearHandler clearAction;
        clearAction.m_pView = pView;
        clearAction.m_action = 2;
        btnClear->onClick = clearAction;
        buttonRow->AddChild(std::move(btnClear), DuiLayout::Hint().Fixed(140));

        AddVariantRow(page.get(), std::move(buttonRow), 32);

        std::unique_ptr<DuiHBox> viewRow(new DuiHBox());
        viewRow->AddChild(std::move(view), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(viewRow), kTextLineH * 3 + kDemoPadding);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiTrace 运行期跟踪 ===========================================

std::unique_ptr<DuiControl> Build_Trace()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // ---- 当前状态 ----

    AddSection(page.get(),
               Txt(_T("当前状态与开启方法"), _T("Current state and how to switch it on")),
               Txt(_T("DuiTrace 把运行期的关键时刻按顺序、带高精度时间戳记到一个文件里，")
                   _T("专门用来排查「只在交互时出现、单元测试复现不了」的时序问题。")
                   _T("它默认关闭，关闭时的开销只有一次布尔判断，所以可以放心留在")
                   _T("正式代码里。开关是环境变量 BUI_DUI_TRACE，取值 1 表示开启，")
                   _T("而且必须在进程启动之前设好 —— 开关状态在首次判断时就被缓存下来，")
                   _T("运行期改不了。"),
                   _T("DuiTrace records key runtime moments in order with ")
                   _T("high-resolution timestamps, for timing problems that only show ")
                   _T("up during interaction and cannot be reproduced in a unit test. ")
                   _T("It is off by default and costs one boolean test when off, so it ")
                   _T("is safe to leave in shipping code. The switch is the ")
                   _T("BUI_DUI_TRACE environment variable, set to 1, and it must be in ")
                   _T("place before the process starts — the state is cached on the ")
                   _T("first check and cannot be changed at runtime.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiLabel> stateLabel(new DuiLabel());
        CString stateText;
        if (DuiTrace::IsEnabled())
        {
            stateText = Txt(_T("DuiTrace::IsEnabled() = true　（跟踪已开启）"),
                            _T("DuiTrace::IsEnabled() = true  (tracing is on)"));
            stateLabel->SetTextColor(kDemoAccentColor);
        }
        else
        {
            stateText = Txt(_T("DuiTrace::IsEnabled() = false　（跟踪未开启）"),
                            _T("DuiTrace::IsEnabled() = false  (tracing is off)"));
            stateLabel->SetTextColor(kDemoWarnColor);
        }
        stateLabel->SetText(stateText);
        row->AddChild(std::move(stateLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextLineH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiLabel> pathLabel(new DuiLabel());
        CString pathText;
        pathText.Format(Txt(_T("日志文件：%s"), _T("Log file: %s")),
                        (LPCTSTR)GetTraceLogPath());
        pathLabel->SetText(pathText);
        pathLabel->SetTextColor(kDemoTextColor);
        row->AddChild(std::move(pathLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextLineH);
    }
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiLabel> howLabel(new DuiLabel());
        howLabel->SetText(Txt(
            _T("开启方法：$env:BUI_DUI_TRACE = \"1\"　然后重新启动 DuiGallery.exe"),
            _T("To switch on: $env:BUI_DUI_TRACE = \"1\"  then restart DuiGallery.exe")));
        howLabel->SetTextColor(kDemoSubtleColor);
        row->AddChild(std::move(howLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTextLineH);
    }

    // ---- 写几条记录并回读 ----

    ClippedTextBlock* pOutput = NULL;
    AddSection(page.get(),
               Txt(_T("写几条记录，再把日志末尾读回来"),
                   _T("Write a few entries, then read the tail back")),
               Txt(_T("日志文件每次进程启动时覆盖重建，全程保持打开、每写一条就刷盘，")
                   _T("并且是以允许他人同时读写的方式打开的，所以可以边跑边读 —— ")
                   _T("下面这个按钮就是这么做的。日志每一行的第一列是进程启动以来的")
                   _T("毫秒数，第二列是距上一条的间隔；排查时序问题主要看第二列。")
                   _T("按仓库约定，日志正文一律是英文。"),
                   _T("The log file is recreated on every process start, kept open ")
                   _T("throughout, flushed after every entry, and opened in a way that ")
                   _T("lets other processes read it at the same time — which is exactly ")
                   _T("what the button below does. The first column of each line is ")
                   _T("milliseconds since process start, the second is the gap since ")
                   _T("the previous line; that second column is what timing ")
                   _T("investigations look at. Log text is always English, per the ")
                   _T("repository convention.")));
    {
        std::unique_ptr<ClippedTextBlock> output(new ClippedTextBlock());
        pOutput = output.get();
        RefreshTraceOutput(pOutput);

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(12);

        std::unique_ptr<FnButton> btnWrite(new FnButton());
        btnWrite->SetText(Txt(_T("写 4 条记录并回读"), _T("Write 4 entries and read back")));
        WriteTraceSampleHandler writeHandler;
        writeHandler.m_pOutput = pOutput;
        btnWrite->onClick = writeHandler;
        buttonRow->AddChild(std::move(btnWrite), DuiLayout::Hint().Fixed(220));

        std::unique_ptr<FnButton> btnRepaint(new FnButton());
        btnRepaint->SetText(Txt(_T("强制整窗重绘并回读"),
                                _T("Force a full repaint and read back")));
        CaptureRepaintTraceHandler repaintHandler;
        repaintHandler.m_pOutput = pOutput;
        btnRepaint->onClick = repaintHandler;
        buttonRow->AddChild(std::move(btnRepaint), DuiLayout::Hint().Fixed(240));

        AddVariantRow(page.get(), std::move(buttonRow), 32);

        std::unique_ptr<DuiHBox> outputRow(new DuiHBox());
        outputRow->AddChild(std::move(output),
                            DuiLayout::Hint().Weight(1).Margin(0, 4, 0, 0));
        AddVariantRow(page.get(), std::move(outputRow), kTraceOutputH);
    }

    // ---- 库内现成的埋点 ----

    AddSection(page.get(),
               Txt(_T("用库内现成的埋点看一次绘制"),
                   _T("Watch one paint through the library's own trace points")),
               Txt(_T("DuiHost::OnPaint 里已经埋了三条记录：HOST-PAINT-BEGIN 记下系统")
                   _T("判定的待重画区域与客户区尺寸，HOST-PAINT-BG-DONE 记下本次实际")
                   _T("重画的区域以及是否退回了整块重画，HOST-PAINT-TREE-DONE 表示")
                   _T("控件树画完。上面那个「强制整窗重绘并回读」按钮会先让整个宿主")
                   _T("窗口作废、同步完成一次绘制，再把日志末尾读回来，于是这三条")
                   _T("以及它们之间的实测间隔就都能看到了 —— 中间那一段间隔就是这一次")
                   _T("把整棵控件树画完花的时间。"),
                   _T("DuiHost::OnPaint already carries three trace points: ")
                   _T("HOST-PAINT-BEGIN records the update region the system computed ")
                   _T("plus the client size, HOST-PAINT-BG-DONE records the region ")
                   _T("actually repainted and whether it fell back to a full repaint, ")
                   _T("and HOST-PAINT-TREE-DONE marks the end of the control tree. The ")
                   _T("\"force a full repaint\" button above invalidates the whole host ")
                   _T("window, completes one paint synchronously, then reads the tail ")
                   _T("back, so all three lines and their measured gaps show up — the ")
                   _T("middle gap is how long painting the whole tree took this time.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ==============================================

const PageEntry* GetEnginePages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("theme"),     _T("DuiTheme　主题"),           _T("DuiTheme"),  &Build_Theme,     true },
        { _T("dpi"),       _T("DuiDpi　高 DPI"),           _T("DuiDpi"),    &Build_Dpi,       true },
        { _T("animation"), _T("DuiAnimation　动画与缓动"), _T("Animation"), &Build_Animation, true },
        { _T("trace"),     _T("DuiTrace　运行期跟踪"),     _T("DuiTrace"),  &Build_Trace,     true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
