/**
 *  DuiEdit —— 无窗口的普通文本输入框（单行 / 多行）。
 *
 *  它是 DuiRichEdit 的子类：排版、光标、选区、输入法、剪贴板、右键菜单这些
 *  能力全部由基类提供，本类只补上"普通输入框"相对于富文本框多出来的那部分
 *  语义 —— 单行时回车不换行、左右两侧的内联图标栏、密码框的显隐切换按钮、
 *  单行文字垂直居中，以及与旧输入框一致的边框配色与通知时机。
 *
 *  典型用法见下方类注释。
 *
 *  balloonwj@qq.com   2026-08-17
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

#include "DuiRichEdit.h"
#include <functional>

namespace balloonwjui {

// =================================================================
// DuiEdit —— 无窗口普通文本输入框
// =================================================================
//
// 用途：一切"让用户敲一行字"的场景 —— 登录窗的账号密码、资料页的各个字段、
// 搜索框、对话框里的单行输入等。多行纯文本（如备注、签名）同样用它；需要
// 富文本（加粗、配色、内嵌图片）时才直接用基类 DuiRichEdit。
//
// ─────────────────────────────────────────────────────────────────
// 与基类的分工
// ─────────────────────────────────────────────────────────────────
//
// 基类 DuiRichEdit 已经提供：文本读写、只读、多行、自动换行、占位文字、
// 选区、撤销重做、剪贴板（含强制纯文本粘贴）、右键菜单、滚动条、查找、
// 密码属性位与最大长度、输入法、光标。本类<u>不重复实现</u>这些，只做四件事：
//
//   一、单行语义。单行模式下回车不再插入换行，而是发出 DUIN_EDIT_ENTER
//       通知，交给宿主窗口去执行"默认按钮"那类动作；Esc 同理发出
//       DUIN_EDIT_ESCAPE。基类没有这两条 —— 富文本框的回车就该换行。
//   二、左右内联图标栏。在文本区两侧各让出一条固定宽度的区域用来画图标
//       （放大镜、清除叉号、锁形等），可点击、可接管拖动。
//   三、密码框的显隐切换按钮（俗称"小眼睛"），点一下在遮蔽与明文之间切换。
//   四、单行文字垂直居中，以及与旧输入框一致的边框配色。
//
// ─────────────────────────────────────────────────────────────────
// 本类替代的旧控件
// ─────────────────────────────────────────────────────────────────
//
// 本类替代早先内嵌真 Win32 输入框子窗口的 DuiEditHost。旧实现的子窗口永远
// 画在 DUI 合成层之上：不受父容器裁剪、进不了滚动容器、做不了圆角与半透明，
// 多行 / 密码这类属性还是创建期固化的窗口风格位、改一次就得销毁重建整个
// 子窗口。这些限制在无窗口实现里全部消失。
//
// DuiEditHost 现已降级为本类的兼容外壳，只为让存量调用方零改动通过编译；
// 新代码一律直接用 DuiEdit。
//
// ─────────────────────────────────────────────────────────────────
// 通知时机：程序设值也会通知
// ─────────────────────────────────────────────────────────────────
//
// 基类 DuiRichEdit 只在<u>用户编辑</u>时发 DUIN_VALUECHANGED，程序调 SetText
// 换内容不发。本类<u>刻意与之不同</u>：SetText 同样发一条 DUIN_VALUECHANGED，
// 与旧输入框的行为保持一致 —— 旧实现是靠系统的文字变化通知转发的，程序设值
// 一样会触发，存量业务代码里有多处依赖这一点（例如搜索框点清除叉号之后要靠
// 这条通知去重新过滤列表）。
//
// 确实需要"设值但不通知"时（典型是下拉框把选中项回填进输入框，回填本身不
// 应当再触发一次增量搜索），用 SetTextNoNotify。
//
// ─────────────────────────────────────────────────────────────────
// 代码用法
// ─────────────────────────────────────────────────────────────────
//
//     std::unique_ptr<DuiEdit> edit(new DuiEdit());
//     edit->SetCtrlId(IDC_USERNAME);
//     edit->SetPlaceholder(_T("请输入用户名"));
//     edit->SetMaxLength(32);
//     DuiEdit* raw = edit.get();
//     parent->AddChild(std::move(edit));
//     // 不需要任何"创建"调用，构造完就能用：
//     raw->SetText(_T("hello"));
//
//     // 父窗口的通知处理：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_USERNAME)
//     //       OnUserNameChanged(raw->GetText());
//
// XML 用法（标签仍是 edit，属性见 guides.html §3.3.11）：
//
//     <edit id="100" placeholder="请输入用户名"
//           password="false" multiline="false" fixedHeight="32"/>
//
// 事件：
//   · DUIN_VALUECHANGED        —— 文字改变（用户编辑或 SetText 均触发）。
//   · DUIN_SETFOCUS / DUIN_KILLFOCUS —— 焦点进出（由基类的基类发出）。
//   · DUIN_EDIT_ENTER          —— 单行模式下按了回车。
//   · DUIN_EDIT_ESCAPE         —— 按了 Esc。
//   · DUIN_EDIT_LEFT_ICON_CLICK / DUIN_EDIT_RIGHT_ICON_CLICK
//                              —— 点击了对应侧的可点击图标。
class BUI_API DuiEdit : public DuiRichEdit
{
public:
    // 本控件特有的通知码。
    //
    // 取值段从 DUIN_CUSTOM + 80 起算，而不是沿用旧输入框的 +1 / +2。库里的
    // 自定义通知码是按控件各自编号的，每个控件都从 DUIN_CUSTOM 起算，于是
    // 不同控件的第一个自定义码数值必然相同，已经有十余个控件挤在 +1 那一档
    // 上。另开取值段能从源头减少撞号。（这只是缓解，派发端该判的控件编号仍
    // 然要判 —— 见仓库约定里关于自定义通知码的那一条。）
    enum
    {
        DUIN_EDIT_BASE             = DUIN_CUSTOM + 80,   // 本控件通知码的起点，本身不作为通知发出
        //点击了左侧图标栏里的图标。仅在该图标被 SetIconClickable(true) 标记
        //为可点击时才会发出；extra 恒为 0。
        DUIN_EDIT_LEFT_ICON_CLICK  = DUIN_EDIT_BASE + 1,
        //点击了右侧图标栏里的图标。密码显隐按钮占用右侧位置时，右侧图标整体
        //让位，本通知不会发出。extra 恒为 0。
        DUIN_EDIT_RIGHT_ICON_CLICK = DUIN_EDIT_BASE + 2,
        //单行模式下用户按了回车。多行模式不发（那里回车是换行）。宿主窗口
        //收到后一般执行"确定"按钮的动作。extra 恒为 0。
        DUIN_EDIT_ENTER            = DUIN_EDIT_BASE + 3,
        //用户按了 Esc（单行多行都发）。宿主窗口收到后一般执行"取消"动作，
        //例如收起弹出层、退出内联编辑。extra 恒为 0。
        DUIN_EDIT_ESCAPE           = DUIN_EDIT_BASE + 4,
    };

    // 内联图标的槽位。左右各一个，互不影响。
    enum IconSlot
    {
        LeftIcon  = 0,    // 文本区左侧的图标栏，典型用途是搜索框的放大镜、密码框的锁形图标
        RightIcon = 1,    // 文本区右侧的图标栏，典型用途是搜索框的清除叉号
    };

    // 自定义图标画法。
    //   hdc：宿主的绘制目标，坐标系是宿主窗口客户区坐标。
    //   rcIcon：本次该画的图标矩形（已扣掉边框与上下内边距），同一坐标系。
    // 约定：回调内部若改动了 hdc 的状态（字体、颜色、背景模式等），必须自己
    // 恢复 —— 调用方不做保存与还原。
    typedef std::function<void(HDC hdc, const RECT& rcIcon)> IconPainter;

    // 图标栏的鼠标接管回调。装上之后该侧图标栏不再发点击通知，鼠标的按下、
    // 移动、抬起三类消息原样交给回调，典型用途是在图标栏里自绘一条可拖动的
    // 滚动条。
    //   msg：WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_LBUTTONUP 三者之一。
    //   pt：鼠标位置，宿主窗口客户区坐标。拖动过程中可能落在控件之外。
    //   gutterRect：该侧图标栏当前的矩形，便于回调把坐标换算成比例。
    typedef std::function<void(UINT msg, POINT pt, const RECT& gutterRect)> IconDragHandler;

    DuiEdit();
    ~DuiEdit() override;

    // ---- 文本读写（覆盖基类的通知时机）----

    // 设置文本，并发出一条 DUIN_VALUECHANGED。
    //   sz：新文本；传空指针等同于空串。
    // 说明：基类的同名方法不发通知，本类刻意发 —— 理由见类注释"通知时机"。
    void    SetText(LPCTSTR sz);

    // 设置文本但不发通知。用于"回填"这类程序侧动作，避免触发本不该有的
    // 业务响应（例如下拉框把选中项写回输入框时，不应再触发一次增量搜索）。
    //   sz：新文本；传空指针等同于空串。
    void    SetTextNoNotify(LPCTSTR sz);

    // ---- 密码模式 ----

    // 是否密码框（输入内容以遮蔽字符显示）。
    //   b：true = 密码框；false（默认）= 普通文本。
    // 说明：与旧输入框不同，本设置可以在任何时候调，不需要重建控件。
    void    SetPassword(bool b);
    bool    IsPassword() const { return m_bPassword; }

    // 是否在右侧显示"小眼睛"显隐切换按钮。仅密码框下生效。
    //   b：true = 显示按钮；false（默认）= 不显示。
    // 说明：关闭按钮时若当前正处于明文状态，会先恢复成遮蔽再关，避免密码
    // 停留在明文上无法收回。
    void    SetShowEyeToggle(bool b);
    bool    HasEyeToggle() const { return m_bShowEye; }

    // 密码当前是否以明文显示。
    //   b：true = 明文；false = 遮蔽。非密码框时调用无效果。
    void    SetPasswordRevealed(bool b);
    bool    IsPasswordRevealed() const { return m_bPwdRevealed; }

    // ---- 左右内联图标 ----

    // 用自定义画法设置某侧图标。
    //   slot：左侧还是右侧。
    //   gutterWidth：该侧让出的宽度（像素）；<= 0 或 painter 为空等同于清除。
    //   painter：绘制回调，由本控件在每次重绘时调用；本控件持有该回调的副本。
    void    SetIcon(IconSlot slot, int gutterWidth, IconPainter painter);

    // 用一张位图设置某侧图标，在图标矩形内居中绘制。
    //   slot：左侧还是右侧。
    //   gutterWidth：该侧让出的宽度（像素）。
    //   hbm：位图句柄；<u>所有权仍归调用方</u>，本控件不复制也不销毁，调用方
    //        必须保证它的生存期覆盖控件的生存期。传空句柄等同于清除。
    // 说明：按原始尺寸整块拷贝，不缩放、不做透明处理 —— 位图比图标矩形大时
    // 会被截掉，带透明通道的位图会连同底色一起画出来。需要抗锯齿或透明效果
    // 时请改用 SetIcon 自己画。
    void    SetIconBitmap(IconSlot slot, int gutterWidth, HBITMAP hbm);

    // 用一小段文字（字形符号）设置某侧图标，在图标矩形内居中绘制。
    //   slot：左侧还是右侧。
    //   gutterWidth：该侧让出的宽度（像素）。
    //   szGlyph：要画的文字，一般是一个符号字符；空串等同于清除。
    //   crText：文字颜色。
    void    SetIconGlyph(IconSlot slot, int gutterWidth, LPCTSTR szGlyph, COLORREF crText);

    // 清除某侧图标，该侧让出的宽度归还给文本区。
    //   slot：左侧还是右侧。
    void    ClearIcon(IconSlot slot);

    // 某侧图标当前占用的宽度（像素）。没有图标时返回 0。
    //   slot：左侧还是右侧。
    int     GetIconWidth(IconSlot slot) const;

    // 设置某侧图标是否可点击。
    //   slot：左侧还是右侧。
    //   clickable：true = 可点击（鼠标指针变为手形，点击发出对应通知）；
    //              false（默认）= 不可点击，点击穿透到文本区去定位光标。
    void    SetIconClickable(IconSlot slot, bool clickable);
    bool    IsIconClickable(IconSlot slot) const;

    // 让某侧图标栏接管鼠标。装上回调之后该侧不再发点击通知。
    //   slot：左侧还是右侧。
    //   handler：接管回调；传空回调即取消接管。
    void    SetIconDragHandler(IconSlot slot, IconDragHandler handler);

    // 计算某侧图标矩形。静态纯函数，不依赖控件状态，便于调用方按同一套规则
    // 定位自己画的内容（搜索框就靠它对齐清除叉号的命中区）。
    //   rc：控件矩形，宿主窗口客户区坐标。
    //   slot：左侧还是右侧。
    //   gutterWidth：该侧让出的宽度（像素）；<= 0 时返回空矩形。
    //   borderPx：边框宽度（像素），图标矩形从边框内侧开始。
    //   marginVPx：图标矩形相对控件上下边的额外内缩（像素）。
    //   返回：图标矩形，与 rc 同一坐标系。
    static RECT ComputeIconRect(const RECT& rc, IconSlot slot,
                                int gutterWidth, int borderPx, int marginVPx);

    // ---- 单行文字垂直居中 ----

    // 单行模式下文字是否在控件高度内垂直居中。
    //   b：true（默认）= 居中；false = 从顶部开始排。
    // 说明：多行模式下本设置无效果 —— 多行永远从顶部开始排。
    void    SetVerticalCenter(bool b);
    bool    IsVerticalCenter() const { return m_bVCenter; }

    // ---- 与旧输入框对齐的便捷接口 ----

    // 设置整体背景色。等价于基类的 SetBackgroundColor，保留本名字是为了让
    // 存量调用方零改动。
    //   c：背景色。控件禁用时忽略本设置，改用固定的禁用底色。
    void    SetBgColor(COLORREF c);
    COLORREF GetBgColor() const;

    // 按字体名与像素字号设置控件字体。
    //   family：字体名，空指针表示沿用当前字体名。
    //   sizePx：字号（像素高，即 LOGFONT::lfHeight 的绝对值）；<= 0 表示沿用当前字号。
    //   bBold / bItalic / bUnderline / bStrikeOut：四个字形开关。
    // 说明：内部建出字体后交给基类，字体对象由本控件持有并在析构时销毁。
    void    SetCtlFont(LPCTSTR family, int sizePx,
                       bool bBold = false, bool bItalic = false,
                       bool bUnderline = false, bool bStrikeOut = false);

    // ---- 通知抑制（供复合控件内嵌输入框时使用）----

    // 设置本控件是否停止向宿主窗口发送通知。
    //   b：true = 停止发送全部通知；false（默认）= 正常发送。
    // 用途：下拉框、数字框这类复合控件内部嵌有一个输入框时，对外应当只发送
    // 复合控件自身的通知。由于通知直接发送到宿主窗口、不沿控件树逐层传递，
    // 外层控件无法拦截内层控件的通知，只能由内层控件自己停止发送。
    // 注意：本设置只影响发送给宿主窗口的通知，不影响控件内部的回调
    // （OnTextChanged 仍会照常调用），外层控件依然可以感知内部变化。
    void    SetNotificationsSuppressed(bool b) { m_bNotifySuppressed = b; }
    bool    AreNotificationsSuppressed() const { return m_bNotifySuppressed; }

    // 发通知。覆写基类以支持上面的抑制开关。
    //   code：通知码。
    //   extra：随通知附带的数据，含义由通知码决定。
    //   返回：宿主的处理结果；被抑制时恒返回 0。
    LRESULT NotifyParent(UINT code, LPARAM extra = 0) override;

    // ---- DuiControl / DuiRichEdit 覆写 ----

    // 设置文字与控件边界之间的内边距（像素）。
    //
    // 本类把调用方给的值记为"内容内边距"，真正交给排版引擎的是它<u>再加上</u>
    // 图标栏与密码显隐按钮占用的宽度。如果直接把调用方的值交给引擎，图标栏
    // 预留的宽度就会被覆盖，文字将与图标重叠。
    void    SetMargins(int left, int top, int right, int bottom) override;

    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    void    Layout(const RECT& rcAvail) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp(POINT pt, UINT mkFlags) override;
    bool    OnMouseMove(POINT pt, UINT mkFlags) override;
    bool    OnMouseEnter() override;
    bool    OnMouseLeave() override;
    bool    OnSetCursor(POINT pt) override;
    bool    OnChar(TCHAR ch) override;
    bool    OnKeyDown(UINT vk, UINT flags) override;

protected:
    // 图标被点击时的内部拦截钩子，先于通知发出。
    //   slot：被点击的槽位。
    //   返回：true = 本类（或子类）已经处理完毕，不再向宿主发通知；
    //         false（默认）= 继续发 DUIN_EDIT_LEFT_ICON_CLICK / RIGHT。
    // 用途：搜索框用它把清除叉号的点击就地变成"清空文本"，不让这条点击冒泡
    // 到业务代码去。
    virtual bool OnIconClicked(IconSlot slot);

    // 文本内容变化时被调用（用户编辑与程序 SetText 都会走到）。
    // 子类覆写它来同步自身状态，例如搜索框据此决定清除叉号是否显示。
    // 覆写时必须先调基类实现。
    virtual void OnTextChanged();

    // 引擎回传通知的入口。本类覆写它是为了截获"内容已改变"这一条，转成上面
    // 的 OnTextChanged 钩子。
    //   iNotify：引擎的通知码。
    //   pv：随通知附带的数据，具体含义由通知码决定，可能为空。
    //   返回：S_OK 表示已处理。
    HRESULT TxSiteNotify(DWORD iNotify, void* pv) override;

    // 边框颜色。覆写基类是为了沿用无窗口化之前那套灰蓝配色 —— 两个程序里
    // 现有的输入框全是这个观感，换成基类的品牌蓝会让每一个输入框都变样。
    COLORREF BorderColor() const override;

    // 整体底色。同上，禁用态沿用旧输入框的浅灰。
    COLORREF FillColor() const override;

private:
    // 一侧图标栏的全部状态。左右各持有一份。
    struct IconState
    {
        int             m_nWidth;        // 该侧让出的宽度（像素）；0 表示没有图标
        IconPainter     m_painter;       // 自定义画法；三种画法互斥，非空时优先
        HBITMAP         m_hBitmap;       // 位图画法用的位图；所有权归调用方，本控件不销毁
        CString         m_strGlyph;      // 字形画法要画的文字
        COLORREF        m_crGlyph;       // 字形画法的文字颜色
        IconDragHandler m_dragHandler;   // 鼠标接管回调；非空时该侧不发点击通知
        bool            m_bClickable;    // 是否可点击（影响鼠标指针形状与点击命中）
        bool            m_bHover;        // 鼠标是否正悬停在该图标上，用于悬停变色

        IconState();
    };

    // 按当前的图标宽度、密码显隐按钮占位与垂直居中设置，重新算出文本区应有
    // 的内边距并推给基类。布局与这些设置发生变化时都要调一次。
    //
    // 必须在基类布局<u>之前</u>调 —— 基类要按文本区矩形摆放滚动条，内边距晚
    // 一步生效的话滚动条会停在上一次的位置上。
    //
    //   nItemHeight：控件本次的高度（像素）；垂直居中要按它算上下内边距。
    //                传 0 表示沿用控件当前矩形的高度。
    void    ApplyTextInsets(int nItemHeight);

    // 画某一侧的图标。painter / 位图 / 字形三选一，都没有则不画。
    //   hdc：绘制目标。
    //   slot：要画的槽位。
    void    PaintIcon(HDC hdc, IconSlot slot);

    // 画密码显隐按钮（小眼睛）。仅在按钮可见时调用。
    //   hdc：绘制目标。
    void    PaintEyeToggle(HDC hdc);

    // 密码显隐按钮当前的矩形，宿主窗口客户区坐标。按钮不可见时返回空矩形。
    RECT    EyeRect() const;

    // 密码显隐按钮当前是否可见 —— 只有"是密码框"且"开了按钮"时才可见。
    bool    EyeVisible() const { return m_bShowEye && m_bPassword; }

    // 某侧图标当前的矩形，宿主窗口客户区坐标。该侧无图标时返回空矩形。
    //   slot：左侧还是右侧。
    RECT    IconRect(IconSlot slot) const;

    // 取某侧图标状态的引用。
    //   slot：左侧还是右侧。
    IconState&       Icon(IconSlot slot);
    const IconState& Icon(IconSlot slot) const;

    bool     m_bPassword;        // 是否密码框；决定输入内容是否遮蔽显示
    bool     m_bShowEye;         // 是否显示密码显隐按钮；仅密码框下有意义
    bool     m_bPwdRevealed;     // 密码当前是否以明文显示
    bool     m_bEyeHover;        // 鼠标是否悬停在密码显隐按钮上，用于按钮变色
    bool     m_bVCenter;         // 单行模式下文字是否垂直居中
    int      m_nPadL;            // 内容内边距 —— 左（像素）；图标栏宽度在此之上另加
    int      m_nPadT;            // 内容内边距 —— 上（像素）；开启垂直居中时本值被居中量取代
    int      m_nPadR;            // 内容内边距 —— 右（像素）；图标栏或密码按钮宽度在此之上另加
    int      m_nPadB;            // 内容内边距 —— 下（像素）
    bool     m_bSuppressNotify;  // 为真时 SetText 不发文字变化通知，供 SetTextNoNotify 使用
    bool     m_bNotifySuppressed; // 为真时本控件对外一条通知都不发，供复合控件内嵌输入框使用
    int      m_nDragSlot;        // 正在被拖动接管的槽位；-1 表示没有
    int      m_nPressedZone;     // 鼠标按下时落在哪个可点区域，见 .cpp 里的 kZoneXxx 常量；用于配对按下与抬起
    IconState m_iconL;           // 左侧图标栏状态
    IconState m_iconR;           // 右侧图标栏状态
    HFONT    m_hOwnFont;         // SetCtlFont 建出来的字体，由本控件持有并在析构时销毁；未设过为空
};

}   // namespace balloonwjui

#endif  // BUI_FEATURE_EDIT
