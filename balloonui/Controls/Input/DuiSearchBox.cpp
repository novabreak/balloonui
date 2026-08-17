#include "stdafx.h"
#include "DuiSearchBox.h"

#if BUI_FEATURE_SEARCHBOX

#include "../../DuiPaintAA.h"

namespace balloonwjui {

namespace {

// Magnifier glyph color: a slightly cool medium gray so the icon reads as
// "search affordance" without competing with whatever the EDIT renders.
const COLORREF kGlyphColor = RGB(110, 110, 120);

// Clear (x) button color: neutral gray so it's visible against the white
// EDIT background but doesn't shout for attention.
const COLORREF kClearColor = RGB(150, 150, 150);

// Left magnifier strip width default (px).
const int kDefaultGlyphW = 24;

// Glyph geometry (intra-strip).
const int kMagnifierRadiusPx = 5;
const int kClearGlyphHalfPx  = 5;

// Painter callback：在 left gutter RECT 内画 magnifier glyph
void PaintMagnifierGlyph(HDC hdc, const RECT& rc)
{
    int gx = (rc.left + rc.right) / 2;
    int gy = (rc.top  + rc.bottom) / 2;
    int r  = kMagnifierRadiusPx;
    RECT circle = { gx - r, gy - r - 1, gx + r, gy + r - 1 };
    DuiAA::FillEllipse(hdc, circle, CLR_INVALID, kGlyphColor, 1.5f);
    DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 7, gy + 7,
                    kGlyphColor, 1.5f);
}

// Painter callback：在 right gutter RECT 内画 × glyph
void PaintClearGlyph(HDC hdc, const RECT& rc)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top  + rc.bottom) / 2;
    int sz = kClearGlyphHalfPx;
    DuiAA::DrawLine(hdc, cx - sz, cy - sz, cx + sz, cy + sz,
                    kClearColor, 1.5f);
    DuiAA::DrawLine(hdc, cx + sz, cy - sz, cx - sz, cy + sz,
                    kClearColor, 1.5f);
}

} // anonymous

DuiSearchBox::DuiSearchBox()
{
    SetTabStop(true);   // 与基类默认一致，这里显式写出来
    InstallMagnifier_();
    // 右侧叉号不在构造函数里安装 —— 文字非空时 SyncClear_ 才装。这样空状
    // 态下叉号不画，与重构前的 IsClearShowing 行为一致。
}

void DuiSearchBox::InstallMagnifier_()
{
    SetIcon(LeftIcon, kDefaultGlyphW, &PaintMagnifierGlyph);
    // 放大镜只是装饰，标记为不可点击，让鼠标穿透到文本区去定位光标
    SetIconClickable(LeftIcon, false);
}

void DuiSearchBox::SetGlyphStripWidth(int px)
{
    if (px < 0) { px = 0; }
    SetIcon(LeftIcon, px, px > 0 ? &PaintMagnifierGlyph : nullptr);
}

void DuiSearchBox::SetClearStripWidth(int px)
{
    if (px < 14) { px = 14; }
    if (m_clearW == px) { return; }
    m_clearW = px;
    SyncClear_();   // 当前若已显示，宽度跟着改
}

bool DuiSearchBox::IsClearShowing() const
{
    return GetIconWidth(RightIcon) > 0;
}

RECT DuiSearchBox::GetClearRect() const
{
    if (!IsClearShowing())
    {
        RECT z = { 0, 0, 0, 0 };
        return z;
    }
    // 沿用基类的 ComputeIconRect 静态方法 —— 与基类绘制、命中判定用同一套
    // 计算，保证三者一致。
    // 后两个实参是边框宽度与图标上下内缩，取值必须与基类内部一致：它们对应
    // DuiEdit.cpp 里的 kBorderPx（当前为 1）与 kIconMarginV（当前为 2），
    // 那边改了这里要同步改，否则叉号的绘制位置与命中区会对不上。
    return ComputeIconRect(GetRect(), RightIcon, m_clearW, 1, 2);
}

void DuiSearchBox::OnTextChanged()
{
    DuiEdit::OnTextChanged();
    // 文字内容变了，刷新叉号的显隐。用户编辑与业务代码调 SetText 都会走到
    // 这里，不需要在 SetText 上另设一个同步点。
    SyncClear_();
}

void DuiSearchBox::SyncClear_()
{
    bool nonEmpty = !GetText().IsEmpty();
    if (nonEmpty)
    {
        SetIcon(RightIcon, m_clearW, &PaintClearGlyph);
        SetIconClickable(RightIcon, true);
    }
    else
    {
        ClearIcon(RightIcon);
    }
}

bool DuiSearchBox::OnIconClicked(IconSlot slot)
{
    if (slot == RightIcon)
    {
        // 点击清除叉号：本类自己消化这次点击，不让图标点击通知冒泡到宿主 ——
        // 业务代码关心的是"用户改了搜索文字"，那件事由下面 SetText 发出的
        // DUIN_VALUECHANGED 承载。
        SetText(_T(""));
        // SetText 会依次调用 OnTextChanged 与 SyncClear_，叉号随之
        // ClearIcon(RightIcon) 自行消失，这里不必再处理。
        return true;
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SEARCHBOX
