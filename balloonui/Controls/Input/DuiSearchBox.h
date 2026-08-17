/**
 *  DuiSearchBox —— 带放大镜与清除叉号的搜索框，本体是无窗口的普通输入框。
 *
 *  它在普通输入框之上只加一层预设：左侧固定画一个放大镜，右侧在文字非空时
 *  出现一个清除叉号、点它就地清空文本。文本编辑、输入法、占位文字、选区、
 *  边框与底色等能力全部由基类提供，本类不重复实现。典型用法见下方类注释。
 *
 *  balloonwj@qq.com   2026-05-20
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SEARCHBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "DuiEditHost.h"

namespace balloonwjui {

// =================================================================
// DuiSearchBox —— 带放大镜与清除叉号的搜索框
// =================================================================
//
// 本控件是 DuiEditHost（普通输入框的兼容外壳，本体为无窗口的 DuiEdit）的
// 预设包装：直接继承它，在构造函数里把基类原生的左右内联图标接口配置成搜索
// 框该有的样子。全部输入行为（文本编辑、输入法、占位文字、多行、最大长度、
// 边框与底色等）都从基类继承，本类没有任何重复实现。
//
// ─────────────────────────────────────────────────────────────────
// 历史
// ─────────────────────────────────────────────────────────────────
//
// 本控件早期是一个独立类，自己绘制放大镜与叉号、自己做命中判定、自己在布局
// 里内嵌一个输入框控件。基类补上左右内联图标接口之后，本类重构成现在这层预
// 设包装，原先的绘制、布局、命中判定代码全部删除；对调用方暴露的接口保持不
// 变。2026-08-17 输入框改为无窗口实现，本类随之去掉了依赖子窗口通知的那条
// 同步路径，改为覆写基类的 OnTextChanged 钩子。
//
// ─────────────────────────────────────────────────────────────────
// 工作机制
// ─────────────────────────────────────────────────────────────────
//
//   · 构造函数通过 SetIcon(LeftIcon, ...) 安装抗锯齿绘制的放大镜图标。放大镜
//     只是装饰，不可点击，点在它上面等同于点在文本区上。
//   · 覆写基类的 OnTextChanged 钩子同步右侧图标：文字为空时 ClearIcon
//     (RightIcon)，非空时 SetIcon(RightIcon, ..., 叉号) 并标记为可点击。
//     用户编辑与业务代码调 SetText 两种情况都会走到这个钩子，因此不需要另设
//     同步点。
//   · 覆写基类的 OnIconClicked 钩子拦下右侧图标的点击：就地把文本清空，并
//     返回 true 表示这次点击已由本类消化，不再向宿主发出图标点击通知 ——
//     业务代码关心的是"搜索文字变了"，那件事由 DUIN_VALUECHANGED 承载。
//
// ─────────────────────────────────────────────────────────────────
// 代码用法
// ─────────────────────────────────────────────────────────────────
//
//     std::unique_ptr<DuiSearchBox> sb(new DuiSearchBox());
//     sb->SetPlaceholder(_T("搜索联系人"));
//     sb->SetMaxLength(64);
//     sb->SetCtrlId(IDC_SEARCH);
//     DuiSearchBox* raw = sb.get();
//     parent->AddChild(std::move(sb));
//     // 不需要任何"创建"调用，构造完就能用。
//
//     // 父窗口的通知处理：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_SEARCH)
//     //       FilterContacts(raw->GetText());
//
// XML 用法（详细属性见 guides.html §3.3.13）：
//
//     <searchbox id="100"
//                placeholder="搜索联系人"
//                max-length="64"
//                fixedHeight="28"/>
//
// 事件：
//   · DUIN_VALUECHANGED —— 文字变化（含点击清除叉号导致的清空）；extra 恒为 0。
class BUI_API DuiSearchBox : public DuiEditHost
{
public:
    DuiSearchBox();

    // 设置左侧放大镜区域的宽度（像素）。默认 24。
    //   px：宽度；负数按 0 处理，取 0 等效于不显示放大镜。
    void    SetGlyphStripWidth(int px);
    int     GetGlyphStripWidth() const { return GetIconWidth(LeftIcon); }

    // 设置右侧清除叉号区域的宽度（像素）。默认 22，小于 14 时按 14 处理。
    //   px：宽度。
    // 说明：这里设的只是"目标宽度"，叉号实际是否显示由文字是否非空决定
    // （见 IsClearShowing）。
    void    SetClearStripWidth(int px);
    int     GetClearStripWidth() const { return m_clearW; }

    // 当前清除叉号是否显示，等价于"文字非空"。
    bool    IsClearShowing() const;

    // 清除叉号的矩形，宿主窗口客户区坐标。叉号未显示时返回空矩形。
    RECT    GetClearRect() const;

    // 取内部输入框控件指针。
    //
    // 历史接口：本类早期是聚合关系（持有一个输入框子控件），返回的是那个子
    // 控件。重构之后本类自己就是输入框，直接返回 this，供存量调用方沿用。
    DuiEditHost* GetEdit() { return this; }

protected:
    // 文字内容变化时同步清除叉号的显隐。用户编辑与业务代码调 SetText 都会
    // 走到这里，实现内部先调基类实现再同步。
    void    OnTextChanged() override;

    // 拦下右侧清除叉号的点击。
    //   slot：被点击的槽位。
    //   返回：true = 本类已消化这次点击（右侧叉号，就地清空文本，不再向宿主
    //         发出图标点击通知）；false = 交回基类按常规发通知。
    bool    OnIconClicked(IconSlot slot) override;

private:
    // 构造时安装左侧放大镜图标。
    void    InstallMagnifier_();

    // 按当前文字是否为空，切换右侧清除叉号的显隐。
    void    SyncClear_();

    int     m_clearW = 22;   // 清除叉号区域的目标宽度（像素），默认 22、下限 14；实际显隐由 SyncClear_ 决定
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SEARCHBOX
