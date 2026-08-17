/**
 *  画廊「输入控件」分组的全部演示页面：DuiEdit（输入框）、DuiRichEdit（富文本）、
 *  DuiSearchBox 与 DuiSpinBox（搜索框与数字微调框）、DuiSlider（滑块）、
 *  DuiSwitch（开关）、DuiComboBox（下拉框）。
 *
 *  每个页面对应一个 Build_* 函数，文件末尾的 GetInputPages 把它们汇总成本分组
 *  的页面列表交给页面注册表。页面的版式与卡片由 PageKit.h 统一提供，面向读者
 *  的文字一律经 Txt() 取出，切换语言之后整页重建即可换成另一种语言。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include <functional>

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Input/DuiEdit.h"
#include "Controls/Input/DuiRichEdit.h"
#include "Controls/Input/DuiSearchBox.h"
#include "Controls/Input/DuiSpinBox.h"
#include "Controls/Input/DuiComboBox.h"
#include "Controls/Input/DuiSlider.h"
#include "Controls/Input/DuiSwitch.h"
#include "DuiPaintAA.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 本文件公用的版式常量
// =====================================================================

// 演示行内相邻控件之间的水平间距（像素）。本分组所有并排摆放的演示行统一
// 用这个值，改一处全分组同时生效。
const int kRowGapPx = 12;

// 单行控件演示行的行高（像素）。搜索框与数字微调框、下拉框、滑块、开关这几页
// 的演示行都用它。
const int kCompactRowHeightPx = 28;

// 输入框那一页演示行的行高（像素）。比上面高 2 像素，是为了让控件
// 自带的上下内边距完整露出来，看得清边框与文字之间的留白。
const int kEditRowHeightPx = 30;

// 输入框页「胶囊样式」那一组演示行的行高（像素）。比普通演示行再高 2 像素，
// 因为这一组去掉了边框、改用整块底色，行矮了看起来会显得局促。
const int kPillRowHeightPx = 32;

// 状态提示标签的行高（像素）。用于「最近一次收到的通知是什么」这类一行提示。
const int kStatusRowHeightPx = 20;

// 状态提示标签的文字色。比正文浅一档，避免与被演示的控件抢注意力。
const COLORREF kStatusTextColor = RGB(120, 120, 120);

// 品牌蓝。富文本页的遮挡色块与下拉框页的箭头换色都用它。
const COLORREF kBrandBlueColor = RGB(45, 108, 223);

#if BUI_FEATURE_EDIT

// =====================================================================
// 输入框页的内联图标画法
// =====================================================================

// 演示图标统一使用的颜色：偏冷的中性灰。在白色底上看得清，又不会比输入的
// 文字更抢眼。
const COLORREF kDemoIconColor = RGB(120, 120, 130);

// 放大镜镜片的半径（像素）。
const int kDemoMagnifierRadiusPx = 5;

// 放大镜镜柄相对镜片圆心的起止偏移（像素），斜向右下甩出一小段。
const int kDemoHandleBeginPx = 3;
const int kDemoHandleEndPx   = 7;

// 叉号一笔的半边长（像素），即从图标中心到笔画端点的水平 / 垂直距离。
const int kDemoCrossHalfPx = 5;

// 演示图标线条的笔宽（像素），与库内 DuiSearchBox 的取值保持一致。
const float kDemoStrokeWidth = 1.5f;

// 自绘图标栏的宽度（像素）。放大镜、叉号这类需要留出笔画空间的图标用它。
const int kPainterGutterWidthPx = 28;

// 字形图标栏的宽度（像素）。SetIconGlyph 直接画一个字符，比自绘图形窄一档。
const int kGlyphGutterWidthPx = 24;

// 「胶囊样式」那一组图标栏的宽度（像素）。比自绘图标栏再宽一档，让放大镜
// 离控件左边缘远一些，与整块底色配合起来才不显得贴边。
const int kPillGutterWidthPx = 32;

// 左侧图标的画法：画一个放大镜，表示这是一个搜索用的输入框。
// 本函数作为 DuiEdit::IconPainter 交给控件，控件在每次重绘时调用它。
//   hdc：绘制目标，坐标系是宿主窗口客户区坐标。
//   rc：本次该画的图标矩形（控件已扣掉边框与上下内边距），同一坐标系。
void PaintDemoMagnifier(HDC hdc, const RECT& rc)
{
    const int nCenterX = (rc.left + rc.right) / 2;
    const int nCenterY = (rc.top + rc.bottom) / 2;
    const int nRadius  = kDemoMagnifierRadiusPx;

    // 镜片：填充色传 CLR_INVALID 表示只描边、不填充。整体上移一像素，
    // 让加上镜柄之后的图形在视觉上仍然居中。
    RECT rcCircle = { nCenterX - nRadius, nCenterY - nRadius - 1,
                      nCenterX + nRadius, nCenterY + nRadius - 1 };
    DuiAA::FillEllipse(hdc, rcCircle, CLR_INVALID, kDemoIconColor, kDemoStrokeWidth);

    // 镜柄：从镜片右下方斜着甩出去的一小段直线。
    DuiAA::DrawLine(hdc,
                    nCenterX + kDemoHandleBeginPx, nCenterY + kDemoHandleBeginPx,
                    nCenterX + kDemoHandleEndPx,   nCenterY + kDemoHandleEndPx,
                    kDemoIconColor, kDemoStrokeWidth);
}

// 右侧图标的画法：画一个叉号。演示页把它设成可点击，点击后清空文本。
//   hdc：绘制目标，坐标系是宿主窗口客户区坐标。
//   rc：本次该画的图标矩形，同一坐标系。
void PaintDemoClearCross(HDC hdc, const RECT& rc)
{
    const int nCenterX = (rc.left + rc.right) / 2;
    const int nCenterY = (rc.top + rc.bottom) / 2;
    const int nHalf    = kDemoCrossHalfPx;

    DuiAA::DrawLine(hdc, nCenterX - nHalf, nCenterY - nHalf,
                    nCenterX + nHalf, nCenterY + nHalf,
                    kDemoIconColor, kDemoStrokeWidth);
    DuiAA::DrawLine(hdc, nCenterX + nHalf, nCenterY - nHalf,
                    nCenterX - nHalf, nCenterY + nHalf,
                    kDemoIconColor, kDemoStrokeWidth);
}

// 输入框页「右侧可点击图标」那一组的通知接收器。
//
// 演示程序里控件的通知走 GalleryFrame::OnDuiNotify → Gallery::g_pageNotifyHook
// 这条路，与业务窗口自己收 WM_DUI_NOTIFY 是同一回事。这里写成具名仿函数而不是
// lambda，是为了让判定条件与它持有的控件指针一目了然。
class EditNotifyWatcher
{
public:
    // pEdit：带右侧叉号的那个输入框，非空；生存期由页面持有，本类只借用，
    //        既不复制也不释放。
    // uCtrlId：pEdit 的控件编号，用来把它的通知与页内其它控件区分开。
    // pStatus：显示最近一次收到的通知的标签，非空；同样只借用不释放。
    EditNotifyWatcher(DuiEdit* pEdit, UINT uCtrlId, DuiLabel* pStatus)
        : m_pEdit(pEdit)
        , m_uCtrlId(uCtrlId)
        , m_pStatus(pStatus)
    {
    }

    // 通知入口，由页面钩子逐条转发进来。
    //   n：通知结构，可能为空（钩子的调用方不保证非空）。
    void operator()(const balloonwjui::DuiNotify* n)
    {
        if (n == NULL || m_pEdit == NULL || m_pStatus == NULL)
        {
            return;
        }

        // 控件编号必须与通知码写在同一个条件里。库里的自定义通知码是按控件
        // 各自编号的，每个控件都从 DUIN_CUSTOM 起算，不同控件的同一档自定义
        // 码数值必然相同 —— 只比通知码就会把别的控件的通知一并收下。
        if (n->code == (UINT)DuiEdit::DUIN_EDIT_RIGHT_ICON_CLICK
            && n->ctrlId == m_uCtrlId)
        {
            m_pEdit->SetText(_T(""));
            m_pStatus->SetText(
                Txt(_T("收到 DUIN_EDIT_RIGHT_ICON_CLICK：已清空文本"),
                    _T("DUIN_EDIT_RIGHT_ICON_CLICK received: the text was cleared.")));
        }
        else if (n->code == (UINT)DuiEdit::DUIN_EDIT_ENTER
                 && n->ctrlId == m_uCtrlId)
        {
            m_pStatus->SetText(
                Txt(_T("收到 DUIN_EDIT_ENTER：单行模式下回车不换行，改发通知"),
                    _T("DUIN_EDIT_ENTER received: in single-line mode Enter sends ")
                    _T("a notification instead of inserting a line break.")));
        }
        else if (n->code == (UINT)DuiEdit::DUIN_EDIT_ESCAPE
                 && n->ctrlId == m_uCtrlId)
        {
            m_pStatus->SetText(
                Txt(_T("收到 DUIN_EDIT_ESCAPE：按下了 Esc"),
                    _T("DUIN_EDIT_ESCAPE received: Esc was pressed.")));
        }
    }

private:
    DuiEdit*  m_pEdit;      // 被演示的输入框，只借用不释放
    UINT      m_uCtrlId;    // m_pEdit 的控件编号，用于区分通知来源
    DuiLabel* m_pStatus;    // 显示通知文字的标签，只借用不释放
};

// 「右侧可点击图标」那一组里输入框的控件编号。本分组只有两处需要按编号区分
// 通知，取的值避开其它页面已经在用的 700 / 900 段即可。
const UINT kIdClearableEdit = 1101;

#endif // BUI_FEATURE_EDIT

#if BUI_FEATURE_RICHTEXT

// =====================================================================
// 富文本页专用的辅助类与常量
// =====================================================================

// 富文本页「层叠顺序」那一组用的容器：把两个子控件叠在同一块区域上。
//
// 普通的竖直 / 水平布局会把区域切开分给各个子控件，叠不起来，所以这里自己
// 排一次：第一个子控件铺满整块区域，第二个子控件只占右半边。第二个后加入，
// 因此画在上层 —— 这正是内嵌真子窗口的控件做不到的事。
class OverlapBox : public DuiControl
{
public:
    // 排列子控件。
    //   rcAvail：本容器这一次拿到的区域，宿主窗口客户区坐标。
    void Layout(const RECT& rcAvail) override
    {
        m_rcItem = rcAvail;

        // 给子控件定位要用 SetRect —— Layout 只负责把区域分给子控件的子控件，
        // 不设置自身矩形。用错的话子控件矩形恒为空，什么也画不出来。
        if (m_children.size() >= 1)
        {
            m_children[0]->SetRect(rcAvail);
        }
        if (m_children.size() >= 2)
        {
            RECT rc = rcAvail;
            rc.left = rcAvail.left + (rcAvail.right - rcAvail.left) / 2;
            rc.top += 12;
            rc.bottom -= 12;
            m_children[1]->SetRect(rc);
        }
    }
};

// 富文本页「层叠顺序」那一组用的纯色色块。它没有任何交互，只是一块用来压在
// 编辑框上面的不透明像素，证明无窗口控件确实会被后加入的兄弟控件遮住。
class ColorStrip : public DuiControl
{
public:
    // 绘制自身。
    //   hdc：绘制目标。
    //   第二个参数是脏矩形，本控件整块重画，不需要它。
    void OnPaint(HDC hdc, const RECT&) override
    {
        HBRUSH hbr = ::CreateSolidBrush(kBrandBlueColor);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
    }
};

// 富文本页「右键菜单定制」那一组里右边那个编辑框的类型：在默认菜单的基础上
// 删项与改文案。
//
// 这是「第二层定制」的写法 —— 先调基类拿到默认菜单，再在结果上任意增删改。
// 分隔条不必自己操心，控件在弹出之前会统一规整，删项留下的悬空分隔条会被
// 自动去掉。
class TrimmedMenuRichEdit : public DuiRichEdit
{
protected:
    // 构建右键菜单的模型。
    //   items：菜单项列表，进来时是空的，基类先填默认项，本函数再改。
    void OnBuildContextMenu(
        std::vector<balloonwjui::DuiRichEditMenuItem>& items) override
    {
        DuiRichEdit::OnBuildContextMenu(items);

        // 去掉「粘贴为纯文本」和「删除」两项。倒着遍历，删掉元素之后后面的
        // 下标不会错位。
        for (int i = (int)items.size() - 1; i >= 0; --i)
        {
            if (items[(size_t)i].m_separator)
            {
                continue;
            }
            if (items[(size_t)i].m_id == balloonwjui::kRichEditCmdPastePlain
                || items[(size_t)i].m_id == balloonwjui::kRichEditCmdDelete)
            {
                items.erase(items.begin() + i);
            }
        }

        // 把「全选」改个说法，演示改文案。
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (!items[i].m_separator
                && items[i].m_id == balloonwjui::kRichEditCmdSelectAll)
            {
                items[i].m_text = Txt(_T("选中全文(&A)"), _T("Select &all text"));
            }
        }
    }
};

// 富文本页「运行期改属性」那一组三个按钮共用的响应体。
//
// 三个按钮除了改的属性不同以外做的事完全一样，所以合成一个仿函数，用构造
// 参数区分。写成具名类而不是 lambda，是为了让它持有的控件指针一目了然。
class RichEditPropertyToggler
{
public:
    // 本响应体负责翻转哪一个属性。
    enum Property
    {
        // 多行开关，对应 SetMultiLine / IsMultiLine。
        PropMultiLine = 0,
        // 自动换行开关，对应 SetWordWrap / IsWordWrap。
        PropWordWrap = 1,
        // 只读开关，对应 SetReadOnly / IsReadOnly。
        PropReadOnly = 2,
    };

    // pEdit：被切换属性的富文本控件，非空；生存期由页面持有，本类只借用，
    //        既不复制也不释放。
    // prop：本响应体负责翻转的属性。
    RichEditPropertyToggler(DuiRichEdit* pEdit, Property prop)
        : m_pEdit(pEdit)
        , m_prop(prop)
    {
    }

    // 按钮点击入口，由 FnButton 在一次完整点击之后调用。
    //   参数是被点击的按钮自身，本响应体用不到。
    void operator()(FnButton*)
    {
        if (m_pEdit == NULL)
        {
            return;
        }

        switch (m_prop)
        {
        //翻转多行开关：多行与单行之间来回切，内容保持不变
        case PropMultiLine:
            m_pEdit->SetMultiLine(!m_pEdit->IsMultiLine());
            break;

        //翻转自动换行开关：关掉之后长行不再折回，改为横向溢出
        case PropWordWrap:
            m_pEdit->SetWordWrap(!m_pEdit->IsWordWrap());
            break;

        //翻转只读开关：只读时不显示光标、不接受输入，但仍可选中复制
        case PropReadOnly:
            m_pEdit->SetReadOnly(!m_pEdit->IsReadOnly());
            break;

        //取值超出上面三种：构造时传了未定义的枚举值，什么也不做
        default:
            break;
        }
    }

private:
    DuiRichEdit* m_pEdit;    // 被切换属性的控件，只借用不释放
    Property     m_prop;     // 本响应体负责翻转的属性
};

// 富文本页「右键菜单定制」那一组追加的两个自定义命令编号。
//
// 必须不小于 kRichEditMenuCustomBase —— 低于它会与内置命令撞号，控件会拒绝
// 登记。这两个值本身没有特别含义，业务自己定即可。
const UINT kCmdInsertStamp = balloonwjui::kRichEditMenuCustomBase + 1;
const UINT kCmdClearAll    = balloonwjui::kRichEditMenuCustomBase + 2;

// 追加了自定义菜单项的那个富文本控件的编号，用于把它的菜单命令通知与同一页
// 里另外两个控件区分开。
const UINT kIdAppendedRichEdit = 1102;

// 富文本页「右键菜单定制」那一组的通知接收器。
//
// 自定义菜单项被选中时，控件把编号作为通知发给父窗口 —— 这是「第一层定制」
// 的完整路径，业务不必子类化控件。演示程序把通知转给页面钩子，本类据编号执行
// 对应动作。
class RichEditMenuWatcher
{
public:
    // pEdit：追加了自定义菜单项的那个富文本控件，非空；生存期由页面持有，
    //        本类只借用，既不复制也不释放。
    // uCtrlId：pEdit 的控件编号，用来把它的通知与页内其它控件区分开。
    // pStatus：显示最近一次执行了哪条自定义命令的标签，非空；同样只借用。
    RichEditMenuWatcher(DuiRichEdit* pEdit, UINT uCtrlId, DuiLabel* pStatus)
        : m_pEdit(pEdit)
        , m_uCtrlId(uCtrlId)
        , m_pStatus(pStatus)
    {
    }

    // 通知入口，由页面钩子逐条转发进来。
    //   n：通知结构，可能为空（钩子的调用方不保证非空）。
    void operator()(const balloonwjui::DuiNotify* n)
    {
        if (n == NULL || m_pEdit == NULL || m_pStatus == NULL)
        {
            return;
        }
        // 通知码要与控件编号写在同一个条件里，理由同 EditNotifyWatcher。
        if (n->code != (UINT)DuiRichEdit::DUIN_RICHTEXT_MENUCOMMAND
            || n->ctrlId != m_uCtrlId)
        {
            return;
        }

        // 被选中的自定义命令编号由控件放在 extra 里回带。
        const UINT nCmd = (UINT)n->extra;
        if (nCmd == kCmdInsertStamp)
        {
            SYSTEMTIME st;
            ::GetLocalTime(&st);
            CString text;
            text.Format(_T("[%02d:%02d:%02d]"), st.wHour, st.wMinute, st.wSecond);
            m_pEdit->ReplaceSel(text);
            m_pStatus->SetText(Txt(_T("自定义命令：插入时间戳"),
                                   _T("custom command: insert timestamp")));
        }
        else if (nCmd == kCmdClearAll)
        {
            m_pEdit->SetText(_T(""));
            m_pStatus->SetText(Txt(_T("自定义命令：清空全文"),
                                   _T("custom command: clear all")));
        }
    }

private:
    DuiRichEdit* m_pEdit;      // 被演示的富文本控件，只借用不释放
    UINT         m_uCtrlId;    // m_pEdit 的控件编号，用于区分通知来源
    DuiLabel*    m_pStatus;    // 显示命令执行结果的标签，只借用不释放
};

#endif // BUI_FEATURE_RICHTEXT

// =====================================================================
// 下拉框页专用的辅助类型与常量
// =====================================================================

// 「箭头颜色」那一组里每一个下拉框的演示参数。
struct ComboArrowDemo
{
    // 下拉框正下方那一行说明文字。
    LPCTSTR caption;
    // 下拉箭头的颜色；取 CLR_INVALID 表示不调用 SetArrowColor，保留控件默认色。
    COLORREF arrow;
};

// 「长列表」那一组往下拉框里塞多少项。弹出列表默认最多显示 8 行，取 25 是为了
// 明显超过这个上限，才看得出长到上限之后改为滚动的效果。
const int kLongListItemCount = 25;

// 开关页演示行内相邻开关之间的水平间距（像素）。开关本身比较小，间距比其它
// 页面大一档才不至于挤成一片。
const int kSwitchRowGapPx = 20;

// 开关的默认宽度（像素）。与库内 DuiSwitch 的默认尺寸一致。
const int kSwitchWidthPx = 46;

} // 匿名命名空间

// ===== DuiEdit　输入框 ===============================================
//
// 本页演示的是普通输入框 DuiEdit，它是无窗口实现，没有自己的 HWND。它是
// DuiRichEdit 的子类：排版、光标、选区、输入法、剪贴板、右键菜单全部由基类提供，
// 本类只补上「普通输入框」多出来的语义 —— 单行回车不换行、左右内联图标栏、
// 密码显隐切换按钮、单行文字垂直居中。
//
// 本页由两个页面合并而来。库里早先有两个普通输入框控件，一个内嵌真 Win32 输入框
// 子窗口、一个是无窗口实现，各自占一页；旧的那个控件删除之后两页演示的成了同一个
// 控件，因此合并成本页，两页原有的演示组都保留下来。

std::unique_ptr<DuiControl> Build_Edit()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

#if BUI_FEATURE_EDIT
    AddSection(page.get(),
               Txt(_T("单行输入框 + 占位文字"), _T("Single line and placeholder")),
               Txt(_T("构造完即可用，不需要任何创建调用。空且未获得焦点时显示占位文字；")
                   _T("单行模式下文字默认在控件高度内垂直居中。右边那个另外调了 ")
                   _T("SetMaxLength(32)，超出长度的字符直接拒绝输入。"),
                   _T("Ready to use right after construction - there is no create step. The ")
                   _T("placeholder shows only while the box is empty and unfocused, and in ")
                   _T("single-line mode the text is vertically centred within the control ")
                   _T("height by default. The box on the right also calls SetMaxLength(32), so ")
                   _T("characters beyond that length are simply rejected.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> ePlaceholder(new DuiEdit());
        ePlaceholder->SetPlaceholder(Txt(_T("请输入用户名"), _T("Enter your user name")));
        row->AddChild(std::move(ePlaceholder), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiEdit> ePrefilled(new DuiEdit());
        ePrefilled->SetText(Txt(_T("已经填好的内容"), _T("Pre-filled value")));
        // 最大长度由基类提供，超出部分直接拒绝输入。
        ePrefilled->SetMaxLength(32);
        row->AddChild(std::move(ePrefilled), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kEditRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("多行输入框（关掉垂直居中）"),
                   _T("Multi-line, and turning vertical centring off")),
               Txt(_T("左：SetMultiLine(true) + SetWordWrap(true)，多行永远从顶部开始排，")
                   _T("垂直居中设置对它没有效果。右：单行但 SetVerticalCenter(false)，文字贴着")
                   _T("顶边排，与上一组的居中效果正好对照。"),
                   _T("Left: SetMultiLine(true) plus SetWordWrap(true). Multi-line text always ")
                   _T("starts at the top, so the vertical-centring setting has no effect on it. ")
                   _T("Right: a single-line box with SetVerticalCenter(false), whose text sits ")
                   _T("against the top edge - the exact contrast to the centred boxes above.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> eMulti(new DuiEdit());
        eMulti->SetMultiLine(true);
        eMulti->SetWordWrap(true);
        // 多行模式下本设置不起作用，写出来是为了明示这一组演示的就是它。
        eMulti->SetVerticalCenter(false);
        eMulti->SetPlaceholder(
            Txt(_T("多行纯文本：可以换行，可以用输入法，可以拖动选择"),
                _T("Multi-line plain text: line breaks, IME input and drag-selection")));
        row->AddChild(std::move(eMulti), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiEdit> eTopAligned(new DuiEdit());
        eTopAligned->SetVerticalCenter(false);
        eTopAligned->SetText(Txt(_T("单行，文字贴顶"),
                                 _T("Single line, text against the top")));
        row->AddChild(std::move(eTopAligned), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 84);
    }

    AddSection(page.get(),
               Txt(_T("密码框 + 显隐切换按钮"), _T("Password field with a reveal toggle")),
               Txt(_T("左：SetPassword(true)，输入内容以遮蔽字符显示。右：再加 ")
                   _T("SetShowEyeToggle(true)，右侧出现小眼睛按钮，点一下在遮蔽与明文之间")
                   _T("切换。两项设置随时可改，不必重建控件。"),
                   _T("Left: SetPassword(true) masks every character. Right: additionally ")
                   _T("SetShowEyeToggle(true), which puts a small eye button on the right; ")
                   _T("clicking it switches between the masked and the plain text. Both ")
                   _T("settings can be changed at any time without rebuilding the control.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> ePwd(new DuiEdit());
        ePwd->SetPassword(true);
        ePwd->SetPlaceholder(Txt(_T("密码"), _T("Password")));
        row->AddChild(std::move(ePwd), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiEdit> ePwdEye(new DuiEdit());
        ePwdEye->SetPassword(true);
        ePwdEye->SetShowEyeToggle(true);
        ePwdEye->SetText(_T("secret-value"));
        row->AddChild(std::move(ePwdEye), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kEditRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("左侧内联图标"), _T("Inline icon on the left")),
               Txt(_T("SetIcon(LeftIcon, 宽度, 画法) 在文本左侧让出一条图标栏，文本区自动")
                   _T("内缩相应宽度。画法是一个普通的绘制回调，用任何 GDI / GDI+ 接口画都行。")
                   _T("默认不可点击，鼠标会穿透到文本区定位光标。"),
                   _T("SetIcon(LeftIcon, width, painter) reserves an icon gutter to the left ")
                   _T("of the text and insets the text area by that width. The painter is an ")
                   _T("ordinary drawing callback, so any GDI / GDI+ API will do. The gutter is ")
                   _T("not clickable by default - a click falls through to the text area and ")
                   _T("places the caret.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> eSearch(new DuiEdit());
        eSearch->SetPlaceholder(Txt(_T("搜索联系人、消息、文件"),
                                    _T("Search contacts, messages and files")));
        eSearch->SetIcon(DuiEdit::LeftIcon, kPainterGutterWidthPx, &PaintDemoMagnifier);
        row->AddChild(std::move(eSearch), DuiLayout::Hint().Weight(1));

        // 另一种画法：直接给一小段文字当图标，省掉自己画的功夫。
        std::unique_ptr<DuiEdit> eGlyph(new DuiEdit());
        eGlyph->SetPlaceholder(Txt(_T("邮箱地址"), _T("Email address")));
        eGlyph->SetIconGlyph(DuiEdit::LeftIcon, kGlyphGutterWidthPx,
                             _T("@"), kDemoIconColor);
        row->AddChild(std::move(eGlyph), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kEditRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("右侧可点击图标（点击发通知）"),
                   _T("Clickable icon on the right (it fires a notification)")),
               Txt(_T("SetIconClickable(RightIcon, true) 之后，鼠标移上去变成手形，点击发出 ")
                   _T("DUIN_EDIT_RIGHT_ICON_CLICK。本页收到后把文本清空，下方标签显示最近")
                   _T("一次收到的通知。在框内按回车或 Esc 还能看到 DUIN_EDIT_ENTER 与 ")
                   _T("DUIN_EDIT_ESCAPE —— 单行模式下回车不换行，改发通知。"),
                   _T("After SetIconClickable(RightIcon, true) the cursor turns into a hand ")
                   _T("over the gutter and a click sends DUIN_EDIT_RIGHT_ICON_CLICK. This page ")
                   _T("clears the text when it receives one, and the label underneath shows ")
                   _T("the most recent notification. Pressing Enter or Esc inside the box ")
                   _T("produces DUIN_EDIT_ENTER and DUIN_EDIT_ESCAPE - in single-line mode ")
                   _T("Enter does not insert a line break, it sends a notification instead.")));
    {
        std::unique_ptr<DuiVBox> stack(new DuiVBox());
        stack->SetGap(8);

        std::unique_ptr<DuiEdit> eClearable(new DuiEdit());
        eClearable->SetCtrlId(kIdClearableEdit);
        eClearable->SetText(Txt(_T("点右边的叉号清空这里"),
                                _T("Click the cross on the right to clear this")));
        eClearable->SetIcon(DuiEdit::RightIcon, kGlyphGutterWidthPx, &PaintDemoClearCross);
        eClearable->SetIconClickable(DuiEdit::RightIcon, true);
        DuiEdit* pClearable = eClearable.get();
        stack->AddChild(std::move(eClearable), DuiLayout::Hint().Fixed(kEditRowHeightPx));

        std::unique_ptr<DuiLabel> status(new DuiLabel());
        status->SetText(Txt(_T("（还没有收到通知）"), _T("(no notification received yet)")));
        status->SetTextColor(kStatusTextColor);
        DuiLabel* pStatus = status.get();
        stack->AddChild(std::move(status), DuiLayout::Hint().Fixed(kStatusRowHeightPx));

        // 这一组是输入框加一行状态标签的竖直组合，不是普通的演示行，所以直接
        // 加进当前段落的卡片，而不走 AddVariantRow。
        page->CurrentCard()->AddChild(std::move(stack), DuiLayout::Hint().Fixed(58));

        // 注册页面通知钩子。切换到别的页面时画廊窗口会把它清空，因此上面两个
        // 裸指针不会在页面销毁之后被再次使用。
        g_pageNotifyHook = EditNotifyWatcher(pClearable, kIdClearableEdit, pStatus);
    }

    AddSection(page.get(),
               Txt(_T("两侧同时带图标"), _T("Icons on both sides at once")),
               Txt(_T("左右两条图标栏互不影响，可以同时装。这里左侧放一个 @ 符号，右侧放一个")
                   _T("叉号并标记为可点击。两侧让出的宽度都会从文本区里扣掉，文字始终排在剩下")
                   _T("的中间部分。图标栏宽度默认为 0，此时的行为与不带图标完全一致。"),
                   _T("The left and right gutters are independent of each other, so both can be ")
                   _T("used at the same time. Here the left one holds an @ sign and the right one ")
                   _T("a cross that is marked clickable. The width reserved on each side is taken ")
                   _T("out of the text area, so the text always sits in what is left in between. ")
                   _T("The gutter width defaults to 0, which behaves exactly as if there were no ")
                   _T("icon at all.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> eBoth(new DuiEdit());
        eBoth->SetPlaceholder(Txt(_T("邮箱地址"), _T("Email address")));
        eBoth->SetIconGlyph(DuiEdit::LeftIcon, kGlyphGutterWidthPx,
                            _T("@"), kDemoIconColor);
        eBoth->SetIconGlyph(DuiEdit::RightIcon, kGlyphGutterWidthPx,
                            _T("✕"), kDemoIconColor);
        eBoth->SetIconClickable(DuiEdit::RightIcon, true);
        row->AddChild(std::move(eBoth), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kEditRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("胶囊样式：底色 + 去边框 + 左侧图标"),
                   _T("Pill style: background colour, no border, left icon")),
               Txt(_T("三项设置的组合用法：SetBgColor 换一个浅灰底色，SetShowBorder(false) ")
                   _T("去掉边框，再加一个左侧放大镜。实际界面里外层容器还会画一块同色的圆角底")
                   _T("把它包住，这里没有画，演示行直接落在卡片底色上。"),
                   _T("A combination of three settings: SetBgColor for a light grey fill, ")
                   _T("SetShowBorder(false) to drop the border, plus a magnifier on the left. ")
                   _T("In a real screen an outer container would paint a rounded pill of the ")
                   _T("same grey around it; that is not done here, so the row sits directly on ")
                   _T("the card background.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> e(new DuiEdit());
        e->SetPlaceholder(Txt(_T("搜索任意内容"), _T("Search anything...")));
        e->SetBgColor(RGB(0xF3, 0xF3, 0xF4));
        e->SetShowBorder(false);
        // 胶囊样式的图标栏比普通的宽一点，让放大镜离左边缘远一些。
        e->SetIcon(DuiEdit::LeftIcon, kPillGutterWidthPx, &PaintDemoMagnifier);
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));

        // 胶囊样式比其它演示行略高一点，圆角底色铺开之后观感才对。
        AddVariantRow(page.get(), std::move(row), kPillRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("只读 / 禁用"), _T("Read-only and disabled")),
               Txt(_T("左：SetReadOnly(true)，不能改，但仍可点击、选择、复制，配色不变。")
                   _T("右：SetEnabled(false)，整体换成禁用配色（浅灰底 + 浅灰边框），不接受")
                   _T("任何输入。两者放在一起便于对比配色差别。"),
                   _T("Left: SetReadOnly(true) - the content cannot be changed, but it can ")
                   _T("still be clicked, selected and copied, and the colours stay the same. ")
                   _T("Right: SetEnabled(false) - the whole control switches to the disabled ")
                   _T("palette (light grey fill and border) and accepts no input. The two sit ")
                   _T("side by side so the difference in colour is easy to compare.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiEdit> eReadOnly(new DuiEdit());
        eReadOnly->SetText(Txt(_T("只读：可以选中复制，不能修改"),
                               _T("Read-only: selectable and copyable, but not editable")));
        eReadOnly->SetReadOnly(true);
        row->AddChild(std::move(eReadOnly), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiEdit> eDisabled(new DuiEdit());
        eDisabled->SetText(Txt(_T("禁用：连焦点都拿不到"),
                               _T("Disabled: it cannot even take focus")));
        eDisabled->SetEnabled(false);
        row->AddChild(std::move(eDisabled), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kEditRowHeightPx);
    }
#else
    AddSection(page.get(),
               _T("DuiEdit"),
               Txt(_T("该控件在本次构建中被裁剪（BUI_FEATURE_EDIT 已关闭）。"),
                   _T("This control was trimmed out of the current build ")
                   _T("(BUI_FEATURE_EDIT is disabled).")));
#endif // BUI_FEATURE_EDIT

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiRichEdit　富文本 ===========================================
//
// DuiRichEdit 是无窗口的富文本控件：它取的是系统排版引擎的无窗口接口，文字画在
// DUI 合成层上，因此它是控件树里的普通一员 —— 能被别的控件遮住、能被父容器裁剪、
// 能进滚动容器，多行 / 只读 / 换行这些属性还能在运行期随时改。

std::unique_ptr<DuiControl> Build_RichText()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

#if BUI_FEATURE_RICHTEXT
    AddSection(page.get(),
               Txt(_T("基本编辑"), _T("Basic editing")),
               Txt(_T("点击定位光标、直接输入、按住鼠标拖动选择文字。整个过程没有任何子窗口")
                   _T("参与，像素由控件自己画在宿主的绘制目标上。"),
                   _T("Click to place the caret, type, and drag with the mouse to select. No ")
                   _T("child window is involved at any point - the control paints its own ")
                   _T("pixels onto the host's drawing target.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiRichEdit> re(new DuiRichEdit());
        re->SetMultiLine(true);
        re->SetWordWrap(true);
        re->SetText(Txt(_T("点这里就可以开始输入，按住鼠标拖动可以选择文字。"),
                        _T("Click here and start typing. Select with the mouse.")));
        row->AddChild(std::move(re), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 100);
    }

    AddSection(page.get(),
               Txt(_T("占位文字 / 只读 / 去边框"),
                   _T("Placeholder, read-only and borderless")),
               Txt(_T("左：内容为空时显示占位文字。中：只读，不显示光标，文字仍然可以选中")
                   _T("复制。右：去掉边框并换上一个接近页面底色的背景，整块融进背景里，")
                   _T("同时 SetFocusable(false) 让它不参与键盘焦点。"),
                   _T("Left: the placeholder shows while the box is empty. Middle: read-only ")
                   _T("- the caret is hidden but the text can still be selected and copied. ")
                   _T("Right: the border is removed and the background is set close to the ")
                   _T("page colour so the box blends in; SetFocusable(false) also keeps it out ")
                   _T("of the keyboard focus order.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiRichEdit> reEmpty(new DuiRichEdit());
        reEmpty->SetPlaceholder(Txt(_T("输入一条消息"), _T("type a message...")));
        row->AddChild(std::move(reEmpty), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiRichEdit> reReadOnly(new DuiRichEdit());
        reReadOnly->SetText(Txt(_T("只读：不显示光标，文字仍然可以选中。"),
                                _T("Read-only: caret hidden, text still selectable.")));
        reReadOnly->SetReadOnly(true);
        row->AddChild(std::move(reReadOnly), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiRichEdit> reNoBorder(new DuiRichEdit());
        reNoBorder->SetText(Txt(_T("没有边框，融进页面背景里。"),
                                _T("No border, blends into the page background.")));
        reNoBorder->SetShowBorder(false);
        reNoBorder->SetBackgroundColor(RGB(245, 246, 248));
        reNoBorder->SetFocusable(false);
        row->AddChild(std::move(reNoBorder), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 80);
    }

    AddSection(page.get(),
               Txt(_T("层叠顺序：会被后加入的兄弟控件遮住"),
                   _T("Z-order: it can be covered by later siblings")),
               Txt(_T("蓝色色块是一个普通的 DuiControl，它在编辑框之后加入，因此画在编辑框")
                   _T("上层。内嵌真子窗口的富文本控件做不到这一点 —— 子窗口永远画在最上层，")
                   _T("盖不住也被盖不住。"),
                   _T("The blue strip is a plain DuiControl added after the editor, so it ")
                   _T("paints on top of it. A rich text control backed by a real child window ")
                   _T("cannot do this - a child window always wins the z-order.")));
    {
        std::unique_ptr<OverlapBox> box(new OverlapBox());

        std::unique_ptr<DuiRichEdit> re(new DuiRichEdit());
        re->SetMultiLine(true);
        re->SetWordWrap(true);
        re->SetText(
            Txt(_T("这段文字从右半边的蓝色色块下面穿过。无窗口控件与其它控件一样参与 DUI ")
                _T("的层叠顺序，所以后加入的色块画在它上面。"),
                _T("This text runs under the blue strip on the right half. A windowless ")
                _T("control participates in the DUI z-order like any other control, so the ")
                _T("strip added after it paints on top.")));
        box->AddChild(std::move(re));
        box->AddChild(std::unique_ptr<DuiControl>(new ColorStrip()));

        // 这一组是两个控件叠在同一块区域上，不是普通的并排演示行，所以直接
        // 加进当前段落的卡片。
        page->CurrentCard()->AddChild(std::move(box), DuiLayout::Hint().Fixed(90));
    }

    AddSection(page.get(),
               Txt(_T("悬浮式滚动条"), _T("Overlay scroll bar")),
               Txt(_T("用滚轮滚动看效果。滚动条浮在文字之上，不占用内容宽度，因此它出现时")
                   _T("文字的折行位置不会跟着变。左：自动（默认），需要时才出现。中：")
                   _T("始终显示。右：从不显示，但滚轮仍然可以滚动。"),
                   _T("Scroll with the wheel. The bar floats on top of the text and never ")
                   _T("takes content width, so the wrap positions do not shift when it ")
                   _T("appears. Left: automatic (the default), shown only when needed. ")
                   _T("Middle: always visible. Right: never shown, though the wheel still ")
                   _T("scrolls.")));
    {
        // 三个编辑框共用同一段长文本，才能直接对比三种策略的差别。
        LPCTSTR pszLongText =
            Txt(_T("无窗口富文本控件同样可以滚动，滚动条浮在文字之上，不占内容宽度。")
                _T("因此滚动条出现的那一刻，文字的折行位置一个都不会变。")
                _T("这段文字被刻意写长，为的是让内容高度超过控件高度，滚动条才有机会出现。")
                _T("这段文字被刻意写长，为的是让内容高度超过控件高度，滚动条才有机会出现。")
                _T("这段文字被刻意写长，为的是让内容高度超过控件高度，滚动条才有机会出现。"),
                _T("Windowless rich text can scroll, and the bar floats on top of the text ")
                _T("without taking any content width, so the moment it appears not a single ")
                _T("wrap position moves. ")
                _T("The quick brown fox jumps over the lazy dog. ")
                _T("The quick brown fox jumps over the lazy dog. ")
                _T("The quick brown fox jumps over the lazy dog. ")
                _T("The quick brown fox jumps over the lazy dog. ")
                _T("The quick brown fox jumps over the lazy dog."));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiRichEdit> reAuto(new DuiRichEdit());
        reAuto->SetMultiLine(true);
        reAuto->SetWordWrap(true);
        reAuto->SetText(pszLongText);
        row->AddChild(std::move(reAuto), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiRichEdit> reAlways(new DuiRichEdit());
        reAlways->SetMultiLine(true);
        reAlways->SetWordWrap(true);
        reAlways->SetVScrollPolicy(DuiRichEdit::kScrollBarAlways);
        reAlways->SetText(pszLongText);
        row->AddChild(std::move(reAlways), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiRichEdit> reNever(new DuiRichEdit());
        reNever->SetMultiLine(true);
        reNever->SetWordWrap(true);
        reNever->SetVScrollPolicy(DuiRichEdit::kScrollBarNever);
        reNever->SetText(pszLongText);
        row->AddChild(std::move(reNever), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 90);
    }

    AddSection(page.get(),
               Txt(_T("运行期改属性（不必重建控件）"),
                   _T("Switching properties at run time, without a rebuild")),
               Txt(_T("点下面三个按钮分别翻转多行、自动换行、只读三个属性，编辑框里的内容在")
                   _T("每一次切换之后都还在。内嵌真子窗口的控件要改这些属性，必须销毁并重建")
                   _T("它的子窗口。"),
                   _T("The three buttons below toggle multi-line, word wrap and read-only. ")
                   _T("The content of the editor survives every toggle. A control backed by a ")
                   _T("real child window has to destroy and recreate that window to change ")
                   _T("any of these.")));
    {
        std::unique_ptr<DuiVBox> stack(new DuiVBox());
        stack->SetGap(8);

        std::unique_ptr<DuiRichEdit> edit(new DuiRichEdit());
        edit->SetMultiLine(true);
        edit->SetWordWrap(true);
        edit->SetText(Txt(_T("点下面的按钮切换属性，这段文字在每一次切换之后都必须还在。"),
                          _T("Toggle the switches below - this text must survive every ")
                          _T("toggle.")));
        DuiRichEdit* pEdit = edit.get();
        stack->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiHBox> btnRow(new DuiHBox());
        btnRow->SetGap(10);

        std::unique_ptr<FnButton> bMulti(new FnButton());
        bMulti->SetText(Txt(_T("多行"), _T("Multi-line")));
        bMulti->onClick =
            RichEditPropertyToggler(pEdit, RichEditPropertyToggler::PropMultiLine);
        btnRow->AddChild(std::move(bMulti), DuiLayout::Hint().Fixed(110));

        std::unique_ptr<FnButton> bWrap(new FnButton());
        bWrap->SetText(Txt(_T("自动换行"), _T("Word wrap")));
        bWrap->onClick =
            RichEditPropertyToggler(pEdit, RichEditPropertyToggler::PropWordWrap);
        btnRow->AddChild(std::move(bWrap), DuiLayout::Hint().Fixed(110));

        std::unique_ptr<FnButton> bRead(new FnButton());
        bRead->SetText(Txt(_T("只读"), _T("Read-only")));
        bRead->onClick =
            RichEditPropertyToggler(pEdit, RichEditPropertyToggler::PropReadOnly);
        btnRow->AddChild(std::move(bRead), DuiLayout::Hint().Fixed(110));

        // 末尾垫一个空控件把三个按钮顶到左边，否则它们会被平分整行宽度。
        btnRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                         DuiLayout::Hint().Weight(1));
        stack->AddChild(std::move(btnRow), DuiLayout::Hint().Fixed(32));

        page->CurrentCard()->AddChild(std::move(stack), DuiLayout::Hint().Fixed(160));
    }

    AddSection(page.get(),
               Txt(_T("右键菜单：默认 / 追加项 / 定制"),
                   _T("Context menu: default, appended and customized")),
               Txt(_T("在任意一个框里点右键，或者在它获得焦点时按菜单键（Shift+F10）。")
                   _T("左：内置菜单，一行代码都不用写。中：追加了两个自定义项，选中之后")
                   _T("编号会作为通知发到父窗口，本页收到后执行对应动作。右：子类化控件，")
                   _T("在默认菜单的基础上删项与改文案。"),
                   _T("Right-click any of the boxes, or press the menu key (Shift+F10) while ")
                   _T("it has focus. Left: the built-in menu, which needs no code at all. ")
                   _T("Middle: two appended custom items - the chosen id arrives at the parent ")
                   _T("window as a notification, and this page acts on it. Right: a subclass ")
                   _T("that removes items from the built-in menu and renames one of them.")));
    {
        std::unique_ptr<DuiVBox> stack(new DuiVBox());
        stack->SetGap(8);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        // 其一：默认菜单，一行代码都不用写。
        std::unique_ptr<DuiRichEdit> reDefault(new DuiRichEdit());
        reDefault->SetMultiLine(true);
        reDefault->SetWordWrap(true);
        reDefault->SetText(
            Txt(_T("在这里点右键：撤销 / 重做 / 剪切 / 复制 / 粘贴 / 粘贴为纯文本 / ")
                _T("删除 / 全选。"),
                _T("Right-click me: undo / redo / cut / copy / paste / paste as plain text / ")
                _T("delete / select all.")));
        row->AddChild(std::move(reDefault), DuiLayout::Hint().Weight(1));

        // 其二：默认菜单加两个追加项。
        std::unique_ptr<DuiRichEdit> reAppended(new DuiRichEdit());
        reAppended->SetCtrlId(kIdAppendedRichEdit);
        reAppended->SetMultiLine(true);
        reAppended->SetWordWrap(true);
        reAppended->SetText(Txt(_T("在这里点右键：菜单底部多了两个自定义项。"),
                                _T("Right-click me: two extra items at the bottom.")));
        reAppended->AppendContextMenuItem(kCmdInsertStamp,
                                          Txt(_T("插入时间戳"), _T("Insert timestamp")));
        reAppended->AppendContextMenuItem(kCmdClearAll,
                                          Txt(_T("清空全文"), _T("Clear all")));
        DuiRichEdit* pAppended = reAppended.get();
        row->AddChild(std::move(reAppended), DuiLayout::Hint().Weight(1));

        // 其三：子类裁剪过的菜单。
        std::unique_ptr<TrimmedMenuRichEdit> reTrimmed(new TrimmedMenuRichEdit());
        reTrimmed->SetMultiLine(true);
        reTrimmed->SetWordWrap(true);
        reTrimmed->SetText(
            Txt(_T("在这里点右键：没有「粘贴为纯文本」和「删除」，「全选」也换了文案。"),
                _T("Right-click me: no 'paste as plain text', no 'delete', and 'select all' ")
                _T("is renamed.")));
        row->AddChild(std::move(reTrimmed), DuiLayout::Hint().Weight(1));

        stack->AddChild(std::move(row), DuiLayout::Hint().Weight(1));

        // 状态行：显示中间那个编辑框最近一次执行了哪条自定义命令。
        std::unique_ptr<DuiLabel> status(new DuiLabel());
        status->SetText(Txt(_T("（在中间那个框里选一个自定义项）"),
                            _T("(pick a custom item in the middle box)")));
        status->SetTextColor(kStatusTextColor);
        DuiLabel* pStatus = status.get();
        stack->AddChild(std::move(status), DuiLayout::Hint().Fixed(kStatusRowHeightPx));

        page->CurrentCard()->AddChild(std::move(stack), DuiLayout::Hint().Fixed(150));

        // 注册页面通知钩子。同一时刻只有一个页面存在，切换页面时画廊窗口会把
        // 钩子清空，因此上面两个裸指针不会在页面销毁之后被再次使用。
        g_pageNotifyHook = RichEditMenuWatcher(pAppended, kIdAppendedRichEdit, pStatus);
    }
#else
    AddSection(page.get(),
               _T("DuiRichEdit"),
               Txt(_T("该控件在本次构建中被裁剪（BUI_FEATURE_RICHTEXT 已关闭）。"),
                   _T("This control was trimmed out of the current build ")
                   _T("(BUI_FEATURE_RICHTEXT is disabled).")));
#endif // BUI_FEATURE_RICHTEXT

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiSearchBox / DuiSpinBox　搜索框与数字微调框 =================
//
// 这两个控件都是在输入框之上做的预设包装：搜索框固定配好了左侧放大镜与右侧
// 清除叉号，数字微调框在输入框右边接了一条上下箭头。放在同一页是因为它们的
// 演示都只有一行，各自单开一页反而看不出关系。

std::unique_ptr<DuiControl> Build_SearchSpin()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("DuiSearchBox —— 放大镜图标 + 有内容时出现的清除叉号"),
                   _T("DuiSearchBox - magnifier glyph, plus a clear cross when text is ")
                   _T("present")),
               Txt(_T("往框里输入文字，右侧会出现清除叉号，点它清空内容。这两个图标都是")
                   _T("输入框自带的左右内联图标栏，本控件只是在构造函数里把它们配置好，")
                   _T("没有另写一份绘制与命中判定。"),
                   _T("Type into the field and a clear cross appears on the right; click it to ")
                   _T("wipe the text. Both glyphs sit in the inline icon gutters that the text ")
                   _T("box itself provides - this control only configures them in its ")
                   _T("constructor, it does not reimplement any drawing or hit-testing.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiSearchBox> sb(new DuiSearchBox());
        sb->SetPlaceholder(Txt(_T("搜索联系人"), _T("Search contacts...")));
        row->AddChild(std::move(sb), DuiLayout::Hint().Fixed(280));

        AddVariantRow(page.get(), std::move(row), 32);
    }

    AddSection(page.get(),
               Txt(_T("DuiSpinBox —— 带上下箭头的整数输入框"),
                   _T("DuiSpinBox - an integer field with up / down buttons")),
               Txt(_T("取值范围 0 到 50，每次增减 5。点上下箭头会立刻改数值并发出 ")
                   _T("DUIN_VALUECHANGED；左侧也可以手工输入，但手输入的内容不会自动提交")
                   _T("为新数值，需要调用方在合适的时机（如输入框失去焦点时）自己调 SetValue ")
                   _T("提交。"),
                   _T("The range is 0 to 50 with a step of 5. Clicking the up / down arrows ")
                   _T("changes the value immediately and sends DUIN_VALUECHANGED. The field on ")
                   _T("the left also accepts typing, but typed text is not committed ")
                   _T("automatically - the caller has to call SetValue at a suitable moment, ")
                   _T("for example when the field loses focus.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(8);

        std::unique_ptr<DuiSpinBox> sp(new DuiSpinBox());
        sp->SetRange(0, 50);
        sp->SetStep(5);
        sp->SetValue(20, false);
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(120));

        AddVariantRow(page.get(), std::move(row), 32);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiSlider　滑块 ===============================================

std::unique_ptr<DuiControl> Build_Slider()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("取值 0 到 100，步长 1"), _T("Range 0..100, line size 1")),
               Txt(_T("轨道左侧到滑块之间是品牌蓝的已填充段，右侧是浅灰的未填充段，滑块是一个")
                   _T("抗锯齿画出来的白色圆球。拖动滑块、点击轨道空白处、滚轮、方向键、")
                   _T("Home / End 都可以改变取值。"),
                   _T("The part of the rail from its left end to the thumb is filled in brand ")
                   _T("blue, the rest stays light grey, and the thumb is an anti-aliased white ")
                   _T("circle. Dragging the thumb, clicking the empty part of the rail, the ")
                   _T("mouse wheel, the arrow keys and Home / End all change the value.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiSlider> s(new DuiSlider());
        s->SetRange(0, 100);
        s->SetPos(35, false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("取值 0 到 1000，步长 50（大步长）"),
                   _T("Range 0..1000, line size 50 (a large step)")),
               Txt(_T("SetLineSize(50) 之后，方向键、滚轮与点击轨道每次都按 50 个单位移动。")
                   _T("量程大而精度要求不高时用这一档，免得用户按几十下方向键才挪得动。"),
                   _T("After SetLineSize(50) the arrow keys, the wheel and a click on the rail ")
                   _T("each move the value by 50 units. This suits a wide range that does not ")
                   _T("need fine precision, so the user is not left pressing an arrow key ")
                   _T("dozens of times.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiSlider> s(new DuiSlider());
        s->SetRange(0, 1000);
        s->SetLineSize(50);
        s->SetPos(500, false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("禁用"), _T("Disabled")),
               Txt(_T("SetEnabled(false) 之后轨道与滑块换成灰色，鼠标与键盘都不响应。"),
                   _T("After SetEnabled(false) the rail and the thumb turn grey and neither ")
                   _T("the mouse nor the keyboard has any effect.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiSlider> s(new DuiSlider());
        s->SetRange(0, 100);
        s->SetPos(70, false);
        s->SetEnabled(false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiSwitch　开关 ===============================================

std::unique_ptr<DuiControl> Build_Switch()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("四种状态并排"), _T("The four states side by side")),
               Txt(_T("关闭、打开、禁用且关闭、禁用且打开四种状态并排画出来，便于一眼比较，")
                   _T("也便于文档截图。下面几组是可以真的点的。"),
                   _T("Off, on, disabled-off and disabled-on rendered next to each other, both ")
                   _T("for comparison at a glance and for documentation screenshots. The ")
                   _T("groups below are interactive.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kSwitchRowGapPx);

        std::unique_ptr<DuiSwitch> sOff(new DuiSwitch());

        std::unique_ptr<DuiSwitch> sOn(new DuiSwitch());
        sOn->SetChecked(true, /*animated=*/false, /*notify=*/false);

        std::unique_ptr<DuiSwitch> sDisabledOff(new DuiSwitch());
        sDisabledOff->SetEnabled(false);

        std::unique_ptr<DuiSwitch> sDisabledOn(new DuiSwitch());
        sDisabledOn->SetChecked(true, /*animated=*/false, /*notify=*/false);
        sDisabledOn->SetEnabled(false);

        row->AddChild(std::move(sOff), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(sOn), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(sDisabledOff), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(sDisabledOn), DuiLayout::Hint().Fixed(kSwitchWidthPx));

        AddVariantRowCapture(page.get(), _T("switch-states"),
                             std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("可交互 —— 默认带动画"), _T("Interactive - animated by default")),
               Txt(_T("点一下翻转状态。注意滑块 150 毫秒的缓出滑动，以及底色从关闭色渐变到")
                   _T("打开色的过程。"),
                   _T("Click to toggle. Watch the 150 ms ease-out slide of the knob and the ")
                   _T("background fading from the off colour to the on colour.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kSwitchRowGapPx);

        std::unique_ptr<DuiSwitch> swOff(new DuiSwitch());

        std::unique_ptr<DuiSwitch> swOn(new DuiSwitch());
        swOn->SetChecked(true, /*animated=*/false, /*notify=*/false);

        row->AddChild(std::move(swOff), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(swOn), DuiLayout::Hint().Fixed(kSwitchWidthPx));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("可交互 —— 关掉动画"), _T("Interactive - SetAnimated(false)")),
               Txt(_T("同一个控件调 SetAnimated(false)，点击后直接跳到目标状态，没有过渡")
                   _T("过程。列表里成批出现的开关适合关掉动画，免得同时跑几十份动画。"),
                   _T("The same control with SetAnimated(false): a click snaps straight to the ")
                   _T("target state with no transition. Turning the animation off suits ")
                   _T("switches that appear in bulk inside a list, where dozens of animations ")
                   _T("would otherwise run at once.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kSwitchRowGapPx);

        std::unique_ptr<DuiSwitch> sw(new DuiSwitch());
        sw->SetAnimated(false);
        row->AddChild(std::move(sw), DuiLayout::Hint().Fixed(kSwitchWidthPx));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("自定义配色"), _T("Custom colours")),
               Txt(_T("SetOnColor / SetOffColor / SetKnobColor 分别换掉打开态底色、关闭态")
                   _T("底色与滑块颜色。左边两个换了打开态底色，右边那个换的是关闭态底色与")
                   _T("滑块颜色。"),
                   _T("SetOnColor, SetOffColor and SetKnobColor replace the on-state ")
                   _T("background, the off-state background and the knob colour ")
                   _T("respectively. The two on the left change the on-state background; the ")
                   _T("one on the right changes the off-state background and the knob.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kSwitchRowGapPx);

        std::unique_ptr<DuiSwitch> orange(new DuiSwitch());
        orange->SetOnColor(RGB(255, 140, 0));
        orange->SetChecked(true, /*animated=*/false, /*notify=*/false);

        std::unique_ptr<DuiSwitch> purple(new DuiSwitch());
        purple->SetOnColor(RGB(155, 89, 182));
        purple->SetChecked(true, /*animated=*/false, /*notify=*/false);

        std::unique_ptr<DuiSwitch> darkOff(new DuiSwitch());
        darkOff->SetOffColor(RGB(80, 80, 80));
        darkOff->SetKnobColor(RGB(220, 220, 220));

        row->AddChild(std::move(orange), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(purple), DuiLayout::Hint().Fixed(kSwitchWidthPx));
        row->AddChild(std::move(darkOff), DuiLayout::Hint().Fixed(kSwitchWidthPx));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("放大尺寸（100 × 32）"), _T("A larger size (100 x 32)")),
               Txt(_T("几何形状完全由控件拿到的矩形决定：胶囊圆角半径等于高度的一半，")
                   _T("滑块的行程随宽度变长。因此放大只需要在布局上给它更大的矩形，")
                   _T("不需要另设任何尺寸参数。"),
                   _T("The geometry follows the rectangle the control is given: the pill ")
                   _T("radius is half the height and the knob travel grows with the width. ")
                   _T("Enlarging it therefore only takes a bigger rectangle in the layout, ")
                   _T("with no extra size parameters.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kSwitchRowGapPx);

        std::unique_ptr<DuiSwitch> big(new DuiSwitch());
        big->SetChecked(true, /*animated=*/false, /*notify=*/false);
        row->AddChild(std::move(big), DuiLayout::Hint().Fixed(100));

        AddVariantRow(page.get(), std::move(row), 32);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== DuiComboBox　下拉框 ===========================================

std::unique_ptr<DuiControl> Build_ComboBox()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("只读下拉（等价于 CBS_DROPDOWNLIST）"),
                   _T("Read-only drop-down (equivalent to CBS_DROPDOWNLIST)")),
               Txt(_T("主体是自绘的，显示当前选中项，不能手工输入；点控件的任意位置都会")
                   _T("弹出列表。弹出的列表是一个独立的顶层窗口，可以超出父窗口的客户区，")
                   _T("与系统原生下拉框一致。"),
                   _T("The body is owner-drawn and shows the selected item; it cannot be ")
                   _T("typed into, and clicking anywhere on it opens the popup. That popup is ")
                   _T("a separate top-level window, so it may extend beyond the parent ")
                   _T("window's client area just as a native drop-down does.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiComboBox> cb(new DuiComboBox());
        cb->AddString(Txt(_T("在线"), _T("Online")));
        cb->AddString(Txt(_T("离开"), _T("Away")));
        cb->AddString(Txt(_T("忙碌"), _T("Busy")));
        cb->AddString(Txt(_T("隐身"), _T("Invisible")));
        cb->AddString(Txt(_T("离线"), _T("Offline")));
        cb->SetCurSel(0, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("可编辑下拉（等价于 CBS_DROPDOWN）"),
                   _T("Editable drop-down (equivalent to CBS_DROPDOWN)")),
               Txt(_T("SetEditable(true) 之后，主体左侧嵌一个无窗口输入框，右侧约 20 像素是")
                   _T("箭头区：在左边可以手工输入（输入法正常可用），点右边的箭头才弹出列表。")
                   _T("选中列表里的某一项时，项的文字会写回左边的输入框。"),
                   _T("With SetEditable(true) the body embeds a windowless text box on the ")
                   _T("left and keeps about 20 pixels on the right as the arrow zone: the left ")
                   _T("part accepts typing (IME included) and the arrow opens the popup. ")
                   _T("Picking an item writes its text back into the text box.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiComboBox> cb(new DuiComboBox());
        cb->SetEditable(true);
        cb->AddString(Txt(_T("苹果"), _T("Apple")));
        cb->AddString(Txt(_T("香蕉"), _T("Banana")));
        cb->AddString(Txt(_T("樱桃"), _T("Cherry")));
        cb->AddString(Txt(_T("榴莲"), _T("Durian")));
        cb->SetText(Txt(_T("苹果"), _T("Apple")));
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("长列表（弹出列表可滚动）"), _T("A long list (the popup scrolls)")),
               Txt(_T("项数超过最多可见行数（默认 8 行）时，弹出列表长到这个上限就不再变高，")
                   _T("改为滚动。上限可以用 SetMaxVisibleItems 调整。"),
                   _T("When there are more items than the maximum visible row count (8 by ")
                   _T("default), the popup stops growing at that cap and scrolls instead. The ")
                   _T("cap itself can be changed with SetMaxVisibleItems.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGapPx);

        std::unique_ptr<DuiComboBox> cb(new DuiComboBox());
        for (int i = 1; i <= kLongListItemCount; ++i)
        {
            CString strItem;
            strItem.Format(Txt(_T("第 %02d 项"), _T("Item %02d")), i);
            cb->AddString(strItem);
        }
        cb->SetCurSel(0, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kCompactRowHeightPx);
    }

    AddSection(page.get(),
               Txt(_T("箭头颜色与抗锯齿"), _T("Arrow colour and anti-aliasing")),
               Txt(_T("下拉箭头走 DuiAA::FillPolygon 抗锯齿绘制，斜边上看不到锯齿；")
                   _T("SetArrowColor 一行换色，与底色、边框、列表数据互不影响。")
                   _T("三个下拉框并排展示默认的蓝灰、品牌蓝与危险红。"),
                   _T("The drop-down arrow is drawn through DuiAA::FillPolygon, so its ")
                   _T("diagonal edges show no jaggies. SetArrowColor changes its colour in a ")
                   _T("single line, independently of the background, the border and the item ")
                   _T("data. The three combo boxes show the default blue-grey, the brand blue ")
                   _T("and a danger red.")));
    {
        const ComboArrowDemo demos[] =
        {
            { Txt(_T("默认（RGB 80,100,140）"), _T("Default (RGB 80,100,140)")), CLR_INVALID },
            { Txt(_T("品牌蓝"), _T("Brand blue")), kBrandBlueColor },
            { Txt(_T("危险红"), _T("Danger red")), RGB(220, 60, 60) },
        };
        const int kDemoCount = (int)(sizeof(demos) / sizeof(demos[0]));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(16);
        for (int i = 0; i < kDemoCount; ++i)
        {
            // 每一列是一个下拉框加它下方的说明文字。
            std::unique_ptr<DuiVBox> col(new DuiVBox());
            col->SetGap(4);

            std::unique_ptr<DuiComboBox> cb(new DuiComboBox());
            cb->AddString(Txt(_T("选项 A"), _T("Option A")));
            cb->AddString(Txt(_T("选项 B"), _T("Option B")));
            cb->AddString(Txt(_T("选项 C"), _T("Option C")));
            cb->SetCurSel(0, false);
            if (demos[i].arrow != CLR_INVALID)
            {
                cb->SetArrowColor(demos[i].arrow);
            }

            std::unique_ptr<DuiLabel> caption(new DuiLabel());
            caption->SetText(demos[i].caption);
            caption->SetTextColor(RGB(80, 80, 80));

            col->AddChild(std::move(cb), DuiLayout::Hint().Fixed(kCompactRowHeightPx));
            col->AddChild(std::move(caption), DuiLayout::Hint().Fixed(18));
            row->AddChild(std::move(col), DuiLayout::Hint().Weight(1));
        }

        AddVariantRow(page.get(), std::move(row), 60);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ==============================================

const PageEntry* GetInputPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("edit"),        _T("DuiEdit　输入框"),                 _T("DuiEdit"),       &Build_Edit,           true },
        { _T("rich-edit"),   _T("DuiRichEdit　富文本"),             _T("DuiRichEdit"),   &Build_RichText,       true },
        { _T("search-spin"), _T("搜索框与数字微调框"), _T("SearchBox & SpinBox"), &Build_SearchSpin, true },
        { _T("slider"),      _T("DuiSlider　滑块"),                 _T("DuiSlider"),     &Build_Slider,         true },
        { _T("switch"),      _T("DuiSwitch　开关"),                 _T("DuiSwitch"),     &Build_Switch,         true },
        { _T("combo-box"),   _T("DuiComboBox　下拉框"),             _T("DuiComboBox"),   &Build_ComboBox,       true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
