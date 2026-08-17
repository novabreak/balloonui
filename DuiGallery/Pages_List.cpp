/**
 *  画廊「列表与导航」分组的演示页面：列表框（DuiListBox）、虚拟列表
 *  （DuiVirtualList）、树形列表（DuiTreeView）、标签栏（DuiTab）、菜单
 *  （DuiMenu）与菜单栏（DuiMenuBar）。
 *
 *  本文件只负责把这六个页面搭出来，版式（页边距、卡片、标题与说明的字号）
 *  统一由 PageKit 提供，文案统一经 GalleryText 的 Txt() 在中英文之间选取。
 *  文件末尾的 GetListPages() 把这六个页面导出给页面注册表。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/List/DuiListBox.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/List/DuiTab.h"
#include "Controls/List/DuiMenu.h"
#include "Controls/List/DuiMenuBar.h"
#include "Controls/Window/DuiScrollBar.h"
#include "DuiHost.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 通用小工具
// =====================================================================

// 一条同时给出中英文两种写法的示例文案。
// 示例数据（人名、部门名、列表项）需要跟着界面语言一起切换，把两份文案
// 摆在一起，取用时经 Txt() 挑一份即可。
struct DemoTextPair
{
    // 中文文案。必须是字符串字面量，Txt() 直接返回该指针、不做复制。
    LPCTSTR zh;
    // 英文文案。要求同上。
    LPCTSTR en;
};

// 合成一张纵向双色渐变的 32×32 位图，用作树节点图标的测试素材。
//   r0 / g0 / b0：顶端颜色的红、绿、蓝分量，取值 0..255。
//   r1 / g1 / b1：底端颜色的红、绿、蓝分量，取值 0..255。
// 返回：新建的 DIBSection 位图句柄；创建失败时返回空句柄。**所有权归调用
//       方**，本演示把返回值存进函数内的静态变量，随进程退出一并释放。
// 树控件只借用 HBITMAP，不会复制也不会删除它，所以位图的生命期必须长于
// 使用它的控件。
HBITMAP MakeGradientIconBitmap(BYTE r0, BYTE g0, BYTE b0,
                               BYTE r1, BYTE g1, BYTE b1)
{
    // 图标位图的边长（像素）。树控件按自己的图标槽尺寸缩放绘制，这里取一个
    // 比显示尺寸大一档的值，缩小时比放大好看。
    const int kIconSize = 32;
    // 32 位色深下每个像素占的字节数。
    const int kBytesPerPixel = 4;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kIconSize;
    // 高度取负表示自上而下的行序，扫描线顺序与下面的循环一致。
    bi.bmiHeader.biHeight = -kIconSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hBitmap == NULL)
    {
        return NULL;
    }

    BYTE* pixels = static_cast<BYTE*>(bits);
    for (int y = 0; y < kIconSize; ++y)
    {
        // 从上到下把插值系数从 0 推到 1。
        float t = y / (float)(kIconSize - 1);
        BYTE r = (BYTE)(r0 + (r1 - r0) * t);
        BYTE g = (BYTE)(g0 + (g1 - g0) * t);
        BYTE b = (BYTE)(b0 + (b1 - b0) * t);
        for (int x = 0; x < kIconSize; ++x)
        {
            BYTE* pixel = pixels + (y * kIconSize + x) * kBytesPerPixel;
            // DIBSection 的通道顺序是蓝、绿、红、alpha。
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = 255;
        }
    }
    return hBitmap;
}

// =====================================================================
// 列表框（DuiListBox）
// =====================================================================

// 第一段演示用的示例项。取水果名是因为它们长度相近、中英文都好读，与控件
// 本身的能力无关。
const DemoTextPair kFruitItems[] =
{
    { _T("苹果"),   _T("Apple")    },
    { _T("香蕉"),   _T("Banana")   },
    { _T("樱桃"),   _T("Cherry")   },
    { _T("榴莲"),   _T("Durian")   },
    { _T("茄子"),   _T("Eggplant") },
    { _T("无花果"), _T("Fig")      },
    { _T("葡萄"),   _T("Grape")    },
    { _T("榛子"),   _T("Hazel")    },
    { _T("鸢尾"),   _T("Iris")     },
    { _T("红枣"),   _T("Jujube")   },
};
const int kFruitItemCount = (int)(sizeof(kFruitItems) / sizeof(kFruitItems[0]));

// 第二段演示用的行数。取 50 是为了必然超过下面那个 220 像素高的显示区，
// 从而让内嵌滚动条出现。
const int kScrollDemoItemCount = 50;

// 列表框演示行的高度（像素）。列表要足够高才看得出滚动与选中的效果。
const int kListBoxRowHeight = 220;

// 两个列表框各自的控件编号。演示控件本身不接收通知，但它们选中项变化时会
// 发出通用的 DUIN_VALUECHANGED，编号留成 0 的话就与其它匿名控件混在一起，
// 排查通知去向时无从分辨，所以逐个编号。
const UINT kIdListBoxBasic = 910;
const UINT kIdListBoxScroll = 911;

// =====================================================================
// 虚拟列表（DuiVirtualList）
// =====================================================================

// 虚拟列表的控件编号。理由同上面两个列表框。
const UINT kIdVirtualList = 915;

// 虚拟列表演示的总行数。取一万行是为了说明绘制开销与总行数无关。
const int kVirtualRowCount = 10000;
// 虚拟列表的单行高度（像素）。
const int kVirtualRowHeight = 28;
// 虚拟列表演示行的高度（像素）。
const int kVirtualListRowHeight = 360;
// 行文字距行左边界的缩进（像素）。
const int kVirtualRowTextIndent = 6;

// 选中行的底色。
const COLORREF kVirtualRowBgSelected = RGB(180, 210, 245);
// 鼠标停留行的底色。
const COLORREF kVirtualRowBgHover = RGB(232, 240, 252);
// 奇数行的底色。与偶数行差一点点，用来看清行与行的边界。
const COLORREF kVirtualRowBgOdd = RGB(248, 248, 252);
// 偶数行的底色。
const COLORREF kVirtualRowBgEven = RGB(255, 255, 255);
// 选中行的文字色。
const COLORREF kVirtualRowTextSelected = RGB(10, 30, 90);
// 普通行的文字色。
const COLORREF kVirtualRowText = RGB(40, 40, 40);

// 虚拟列表的行绘制回调。控件不保存任何行数据，滚动到哪几行就对哪几行回调
// 一次本函数。
//   user：注册回调时一并传入的用户指针，本演示不需要额外数据，固定为空。
//   hdc：宿主后台缓冲区的设备上下文。
//   rowIndex：行索引，取值范围 [0, 总行数)。
//   rowRect：本行的矩形，宿主客户区坐标，已经应用过滚动位置。
//   selected：本行是否处于选中状态。
//   hover：鼠标是否正停在本行上。
// 选中与鼠标停留的状态由控件维护，本函数只负责按状态挑颜色并画出内容。
void PaintVirtualRow(void* /*user*/, HDC hdc, int rowIndex,
                     const RECT& rowRect, bool selected, bool hover)
{
    COLORREF bgColor = kVirtualRowBgEven;
    if (selected)
    {
        bgColor = kVirtualRowBgSelected;
    }
    else if (hover)
    {
        bgColor = kVirtualRowBgHover;
    }
    else if ((rowIndex & 1) != 0)
    {
        bgColor = kVirtualRowBgOdd;
    }

    HBRUSH brush = ::CreateSolidBrush(bgColor);
    ::FillRect(hdc, &rowRect, brush);
    ::DeleteObject(brush);

    int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTextColor = ::SetTextColor(hdc,
        selected ? kVirtualRowTextSelected : kVirtualRowText);

    CString text;
    text.Format(Txt(_T("第 %05d 行（虚拟）"), _T("Row #%05d (virtual)")), rowIndex);
    RECT rcText = rowRect;
    rcText.left += kVirtualRowTextIndent;
    ::DrawText(hdc, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    ::SetTextColor(hdc, oldTextColor);
    ::SetBkMode(hdc, oldBkMode);
}

// =====================================================================
// 树形列表（DuiTreeView）
// =====================================================================

// 本页九个树控件各自的控件编号。通知按控件编号路由，两个控件用同一个编号
// 会让处理分支收到不属于它的通知，所以这里逐个错开。
const UINT kIdTreeBasic = 900;         // 第 1 段：基础用法
const UINT kIdTreeMultiColumn = 901;   // 第 4 段：多列表格模式
const UINT kIdTreeSubLabel = 902;      // 第 8 段：副标签
const UINT kIdTreeRightText = 903;     // 第 5 段：右侧辅助文字与灰显图标
const UINT kIdTreeFilter = 904;        // 第 7 段：展开快照与过滤
const UINT kIdTreeHover = 905;         // 第 6 段：悬停通知
const UINT kIdTreeIcon = 907;          // 第 3 段：节点图标
const UINT kIdTreeMultiLevel = 908;    // 第 2 段：多层级嵌套
const UINT kIdTreeCustomRow = 909;     // 第 9 段：节点自绘控件

// 在线状态点的四种颜色。与 DuiAvatar 用的一套配色一致，这里就地写出来，
// 免得本页面只为四个常量把 DuiAvatar.h 拉进来。
const COLORREF kTreeStatusOnline = RGB(60, 175, 80);
const COLORREF kTreeStatusAway = RGB(240, 175, 40);
const COLORREF kTreeStatusBusy = RGB(220, 60, 60);
const COLORREF kTreeStatusOffline = RGB(150, 150, 150);

// 树演示行的几档高度（像素）。树要足够高才看得出层级与滚动。
const int kTreeRowHeightTall = 320;
const int kTreeRowHeightMedium = 280;
const int kTreeRowHeightHover = 260;
const int kTreeRowHeightShort = 250;

// 树演示里工具按钮的宽度（像素）。六个按钮取同一个宽度，中英文都装得下。
const int kTreeToolButtonWidth = 120;
// 工具按钮之间的水平间距（像素）。
const int kTreeToolButtonGap = 8;
// 工具按钮所在那一行的高度（像素）。
const int kTreeToolBarHeight = 30;
// 工具按钮行与下方树之间的竖直间距（像素）。
const int kTreeToolBarGap = 8;

// 第 1 段的联系人示例数据。
struct TreeContact
{
    // 中文显示名。
    LPCTSTR zh;
    // 英文显示名。
    LPCTSTR en;
    // 行右端状态点的颜色。
    COLORREF status;
};

const TreeContact kBasicContacts[] =
{
    { _T("张伟"), _T("Alice"), kTreeStatusOnline  },
    { _T("李娜"), _T("Bob"),   kTreeStatusAway    },
    { _T("王强"), _T("Carol"), kTreeStatusBusy    },
    { _T("赵敏"), _T("Dave"),  kTreeStatusOffline },
};
const int kBasicContactCount = (int)(sizeof(kBasicContacts) / sizeof(kBasicContacts[0]));

// 第 1 段里同事分组的人数。用循环生成而不是逐个写死，是为了让这一组必然
// 撑出滚动条。
const int kBasicCoworkerCount = 8;

// 第 6 段悬停演示里的人数。行数够多才能一边移动鼠标一边看到滚动条淡入淡出。
const int kHoverDemoRowCount = 20;

// 第 7 段过滤演示里工程师分组的人数。
const int kFilterDemoEngineerCount = 5;

// 展开状态快照。第 7 段的「保存展开状态」按钮把当前展开的节点编号存进来，
// 「还原展开状态」按钮再读出去。放在这里是因为两个按钮的处理函数都要用到
// 它，且它必须活到页面被销毁之后（页面切换时按钮先于快照消失）。
std::vector<int> s_treeExpandSnapshot;

// 第 9 段用的业务自绘行控件：模拟一条即时通讯会话记录的视觉。
//
// 树控件把节点的内容区整个交给本控件绘制，自己只保留行背景、展开标志和
// 缩进。像未读消息角标这种只有具体业务才有的概念，就应该像这样放在业务侧
// 实现，不必进通用控件库。真实业务代码会把这样一个类放到自己的 .h / .cpp
// 里，这里为了让演示集中在一处才写在本文件内。
class ConversationRowCell : public DuiControl
{
public:
    // 构造。
    //   name：会话名称，显示在第一行。
    //   sub：会话摘要，显示在第二行，过长时以省略号截断。
    //   avatarColor：左侧头像方块的底色。
    //   unreadCount：未读条数；取 0 表示不画角标。
    ConversationRowCell(LPCTSTR name, LPCTSTR sub, COLORREF avatarColor, int unreadCount)
        : m_name(name)
        , m_sub(sub)
        , m_avatarColor(avatarColor)
        , m_unreadCount(unreadCount)
    {
    }

    // 绘制整行内容。
    //   hdc：宿主后台缓冲区的设备上下文。
    //   rcDirty：需要重绘的区域；本控件内容很少，整块重画即可，故未使用。
    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        // 头像方块的边长（像素）。
        const int kAvatarSize = 36;
        // 头像方块距行左边界的距离（像素）。
        const int kAvatarLeftMargin = 6;
        // 头像方块与右侧文字之间的间距（像素）。
        const int kAvatarTextGap = 10;
        // 有未读角标时在行右端预留的宽度（像素）。
        const int kBadgeReservedWidth = 36;
        // 文字区域距右侧预留区的间距（像素）。
        const int kTextRightGap = 8;
        // 第一行文字相对行顶端的上下边界（像素）。
        const int kNameTop = 5;
        const int kNameBottom = 25;
        // 第二行文字相对行顶端的上边界与距行底端的距离（像素）。
        const int kSubTop = 27;
        const int kSubBottomMargin = 5;
        // 未读角标的高度（像素）。
        const int kBadgeHeight = 18;
        // 未读角标内文字左右两侧的内边距之和（像素）。
        const int kBadgeTextPadding = 12;
        // 未读角标距行右边界的距离（像素）。
        const int kBadgeRightMargin = 4;
        // 未读条数显示的上限，超过它只显示 "99+"。
        const int kBadgeMaxCount = 99;
        // 未读角标的底色。
        const COLORREF kBadgeBgColor = RGB(0xE5, 0x4D, 0x4D);
        // 会话名称的文字色。
        const COLORREF kNameColor = RGB(30, 30, 30);
        // 会话摘要的文字色。比名称浅，拉开层级。
        const COLORREF kSubColor = RGB(140, 140, 140);

        const int left = m_rcItem.left;
        const int top = m_rcItem.top;
        const int right = m_rcItem.right;
        const int bottom = m_rcItem.bottom;

        //—— 头像：实心方块加名称首字
        const int avatarX = left + kAvatarLeftMargin;
        const int avatarY = top + (bottom - top - kAvatarSize) / 2;
        RECT rcAvatar = { avatarX, avatarY, avatarX + kAvatarSize, avatarY + kAvatarSize };
        HBRUSH avatarBrush = ::CreateSolidBrush(m_avatarColor);
        ::FillRect(hdc, &rcAvatar, avatarBrush);
        ::DeleteObject(avatarBrush);

        int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldTextColor = ::SetTextColor(hdc, RGB(255, 255, 255));
        HFONT guiFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
        HFONT oldFont = NULL;
        if (guiFont != NULL)
        {
            oldFont = (HFONT)::SelectObject(hdc, guiFont);
        }
        CString initial = m_name.IsEmpty() ? CString(_T("?")) : m_name.Left(1);
        ::DrawText(hdc, initial, -1, &rcAvatar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        //—— 名称与摘要：右侧给未读角标让出位置
        const int badgeReserved = (m_unreadCount > 0) ? kBadgeReservedWidth : 0;
        const int textLeft = avatarX + kAvatarSize + kAvatarTextGap;

        RECT rcName = { textLeft, top + kNameTop,
                        right - badgeReserved - kTextRightGap, top + kNameBottom };
        ::SetTextColor(hdc, kNameColor);
        ::DrawText(hdc, m_name, -1, &rcName,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT rcSub = { textLeft, top + kSubTop,
                       right - badgeReserved - kTextRightGap, bottom - kSubBottomMargin };
        ::SetTextColor(hdc, kSubColor);
        ::DrawText(hdc, m_sub, -1, &rcSub,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        //—— 未读角标：圆角矩形加数字，由业务自己画
        if (m_unreadCount > 0)
        {
            CString badgeText;
            if (m_unreadCount > kBadgeMaxCount)
            {
                badgeText = _T("99+");
            }
            else
            {
                badgeText.Format(_T("%d"), m_unreadCount);
            }

            SIZE textSize = {};
            ::GetTextExtentPoint32(hdc, badgeText, badgeText.GetLength(), &textSize);
            // 数字很短时角标会退化成竖条，所以宽度至少取一个高度，画出来是圆形。
            int badgeWidth = textSize.cx + kBadgeTextPadding;
            if (badgeWidth < kBadgeHeight)
            {
                badgeWidth = kBadgeHeight;
            }
            const int badgeY = top + (bottom - top) / 2 - kBadgeHeight / 2;
            const int badgeX = right - badgeWidth - kBadgeRightMargin;

            HBRUSH badgeBrush = ::CreateSolidBrush(kBadgeBgColor);
            HBRUSH oldBrush = (HBRUSH)::SelectObject(hdc, badgeBrush);
            HGDIOBJ oldPen = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
            // RoundRect 的右下边界不含端点，各加 1 才能填满 badgeWidth × kBadgeHeight。
            ::RoundRect(hdc, badgeX, badgeY,
                        badgeX + badgeWidth + 1, badgeY + kBadgeHeight + 1,
                        kBadgeHeight, kBadgeHeight);
            ::SelectObject(hdc, oldPen);
            ::SelectObject(hdc, oldBrush);
            ::DeleteObject(badgeBrush);

            ::SetTextColor(hdc, RGB(255, 255, 255));
            RECT rcBadge = { badgeX, badgeY, badgeX + badgeWidth, badgeY + kBadgeHeight };
            ::DrawText(hdc, badgeText, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        if (oldFont != NULL)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::SetTextColor(hdc, oldTextColor);
        ::SetBkMode(hdc, oldBkMode);
    }

private:
    // 会话名称，显示在第一行。构造时拷贝一份，生命期与本控件相同。
    CString m_name;
    // 会话摘要，显示在第二行。生命期同上。
    CString m_sub;
    // 头像方块的底色。
    COLORREF m_avatarColor;
    // 未读条数；0 表示不画角标。
    int m_unreadCount;
};

// =====================================================================
// 标签栏（DuiTab）
// =====================================================================

// 标签栏演示行的高度（像素）。
const int kTabStripRowHeight = 32;

// 本页五个标签栏各自的控件编号。标签栏切换当前标签时发出通用的
// DUIN_VALUECHANGED，与导航用的控件同码，靠编号区分。
const UINT kIdTabPlain = 920;
const UINT kIdTabCloseable = 921;
const UINT kIdTabWithIcon = 922;
const UINT kIdTabAutoFitOff = 923;
const UINT kIdTabAutoFitOn = 924;

// 合成一张 16×16 的圆点位图，用作标签图标的测试素材。
//   r / g / b：圆点的红、绿、蓝分量，取值 0..255。
// 返回：新建的 DIBSection 位图句柄；创建失败时返回空句柄。**所有权归调用
//       方**，本演示存进函数内的静态变量，随进程退出一并释放。
// 位图是 32 位预乘 alpha 格式，控件用 ::AlphaBlend 绘制它，所以圆点之外的
// 像素必须整个清零（alpha 为 0）。
HBITMAP MakeTabDotIcon(BYTE r, BYTE g, BYTE b)
{
    // 图标位图的边长（像素）。与 DuiTab 默认的图标尺寸一致。
    const int kDotIconSize = 16;
    // 32 位色深下每个像素占的字节数。
    const int kBytesPerPixel = 4;
    // 圆点的半径（像素）。
    const int kDotRadius = 6;
    // 半径的平方。判断像素是否落在圆内时比较平方值，免去开方运算。
    const int kDotRadiusSquared = kDotRadius * kDotRadius;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kDotIconSize;
    // 高度取负表示自上而下的行序。
    bi.bmiHeader.biHeight = -kDotIconSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hBitmap == NULL)
    {
        return NULL;
    }

    BYTE* pixels = static_cast<BYTE*>(bits);
    ::ZeroMemory(pixels, kDotIconSize * kDotIconSize * kBytesPerPixel);

    const int centerX = kDotIconSize / 2;
    const int centerY = kDotIconSize / 2;
    for (int y = 0; y < kDotIconSize; ++y)
    {
        for (int x = 0; x < kDotIconSize; ++x)
        {
            int dx = x - centerX;
            int dy = y - centerY;
            if (dx * dx + dy * dy >= kDotRadiusSquared)
            {
                continue;
            }
            BYTE* pixel = pixels + (y * kDotIconSize + x) * kBytesPerPixel;
            // alpha 取 255 时预乘不改变颜色分量，直接写入即可（顺序为蓝、绿、红）。
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
            pixel[3] = 255;
        }
    }
    return hBitmap;
}

// =====================================================================
// 菜单（DuiMenu）
// =====================================================================

// 弹出菜单演示按钮的宽度（像素）。
const int kMenuButtonWidth = 180;
// 演示行内控件之间的水平间距（像素）。
const int kMenuRowGap = 12;

// 演示菜单里各项的命令编号。TrackPopup 同步返回被点中项的编号，本演示不
// 消费返回值，这些编号只用来把各项区分开。
const UINT kMenuIdNew = 101;
const UINT kMenuIdOpen = 102;
const UINT kMenuIdSave = 103;
const UINT kMenuIdExit = 104;
const UINT kMenuIdStayOnTop = 201;
const UINT kMenuIdMute = 202;
const UINT kMenuIdLanguage = 203;
const UINT kMenuIdDisabled = 204;
const UINT kMenuIdQuit = 299;
const UINT kMenuIdLangEnglish = 301;
const UINT kMenuIdLangChinese = 302;
const UINT kMenuIdLangJapanese = 303;

// 「语言」子菜单。DuiMenu 只借用子菜单的指针，要求它在 TrackPopup 的整个
// 调用期间保持存活，所以放在这里随进程存活，而不是放在构建函数的栈上。
DuiMenu s_menuLanguageSubMenu;

// 构建一个只有普通项和分隔条的菜单。
//   menu：待填充的菜单，调用方持有，本函数只往里追加项。
void BuildPlainDemoMenu(DuiMenu& menu)
{
    menu.AppendItem(kMenuIdNew, Txt(_T("新建"), _T("New")));
    menu.AppendItem(kMenuIdOpen, Txt(_T("打开..."), _T("Open...")));
    menu.AppendItem(kMenuIdSave, Txt(_T("保存"), _T("Save")));
    menu.AppendSeparator();
    menu.AppendItem(kMenuIdExit, Txt(_T("退出"), _T("Exit")));
}

// 构建一个用上了勾选项、子菜单与禁用项的菜单。
//   menu：待填充的菜单，调用方持有，本函数只往里追加项。
// 副作用：重建全局的「语言」子菜单，使它的文字跟随当前语言。
void BuildFullDemoMenu(DuiMenu& menu)
{
    menu.AppendChecked(kMenuIdStayOnTop, Txt(_T("窗口置顶"), _T("Stay on top")), true);
    menu.AppendChecked(kMenuIdMute, Txt(_T("静音"), _T("Mute")), false);
    menu.AppendSeparator();

    // 子菜单每次重建，切换界面语言之后它的文字才会跟着变。
    s_menuLanguageSubMenu.Clear();
    s_menuLanguageSubMenu.AppendItem(kMenuIdLangEnglish, Txt(_T("英语"), _T("English")));
    s_menuLanguageSubMenu.AppendItem(kMenuIdLangChinese, Txt(_T("中文"), _T("Chinese")));
    s_menuLanguageSubMenu.AppendItem(kMenuIdLangJapanese, Txt(_T("日语"), _T("Japanese")));
    menu.AppendSubMenu(kMenuIdLanguage, Txt(_T("语言"), _T("Language")), &s_menuLanguageSubMenu);

    menu.AppendDisabled(kMenuIdDisabled, Txt(_T("不可点击（已禁用）"), _T("Cannot click (disabled)")));
    menu.AppendSeparator();
    menu.AppendItem(kMenuIdQuit, Txt(_T("退出程序"), _T("Quit")));
}

// 点击后在自己下方弹出一张菜单的按钮。
//
// 菜单内容由调用方给的构建函数决定，这样同一个按钮类就能服务本页里的几段
// 演示。菜单对象建在 OnLButtonUp 的栈上：TrackPopup 是同步调用，返回时菜单
// 已经关闭，此后没有人再引用它。
class MenuPopupButton : public DuiButton
{
public:
    // 菜单内容的构建函数。参数是待填充的菜单。
    typedef void (*BuildFn)(DuiMenu& menu);

    // 指定本按钮弹出的菜单由哪个函数填充。
    //   fn：构建函数；传空指针表示点击不弹菜单。
    void SetBuilder(BuildFn fn)
    {
        m_pBuildFn = fn;
    }

    // 左键抬起。基类判定构成一次点击时，在按钮下方弹出菜单。
    //   pt：鼠标位置，宿主客户区坐标。
    //   mkFlags：鼠标消息附带的按键状态标志。
    // 返回：本控件是否消费了这次消息。
    bool OnLButtonUp(POINT pt, UINT mkFlags) override
    {
        bool consumed = DuiButton::OnLButtonUp(pt, mkFlags);
        if (!consumed || m_pBuildFn == NULL)
        {
            return consumed;
        }

        DuiMenu menu;
        m_pBuildFn(menu);

        // 期望的落点是按钮的左下角。菜单弹出前会自己把落点限制在工作区内，
        // 所以这里不必判断按钮是不是贴着屏幕边缘。
        POINT ptScreen;
        ptScreen.x = m_rcItem.left;
        ptScreen.y = m_rcItem.bottom;
        HWND hHostWnd = NULL;
        if (GetHost() != NULL)
        {
            hHostWnd = GetHost()->m_hWnd;
        }
        if (hHostWnd != NULL)
        {
            ::ClientToScreen(hHostWnd, &ptScreen);
        }
        menu.TrackPopup(ptScreen.x, ptScreen.y, hHostWnd);
        return consumed;
    }

private:
    // 菜单内容的构建函数；为空表示点击不弹菜单。
    BuildFn m_pBuildFn = NULL;
};

// =====================================================================
// 菜单栏（DuiMenuBar）
// =====================================================================

#if BUI_FEATURE_MENUBAR

// 菜单栏演示行的高度（像素）。
const int kMenuBarRowHeight = 30;
// 菜单栏控件的宽度（像素）。三个栏目的文字加起来大致就这么宽。
const int kMenuBarWidth = 180;

// 菜单栏控件自身的编号。它发出的 DUIMBN_DROPDOWN_OPEN / CLOSE 与别的控件的
// 第一、第二个自定义通知码数值相同，只能靠编号区分。
const UINT kIdMenuBarDemo = 700;

// 菜单栏三个栏目的编号。
const UINT kMenuBarIdFile = 710;
const UINT kMenuBarIdOptions = 720;
const UINT kMenuBarIdView = 740;

// 「文件」栏下拉项的编号。
const UINT kMenuBarIdFileOpen = 701;
const UINT kMenuBarIdFileSave = 702;
const UINT kMenuBarIdFileExit = 703;

// 「选项」栏下拉项的编号。
const UINT kMenuBarIdOptTopmost = 721;
const UINT kMenuBarIdOptHideOnMin = 722;
const UINT kMenuBarIdOptOnDemand = 723;

// 「查看」栏下拉项的编号。
const UINT kMenuBarIdViewRefresh = 741;
const UINT kMenuBarIdViewSpeedHigh = 742;
const UINT kMenuBarIdViewSpeedNormal = 743;
const UINT kMenuBarIdViewSpeedLow = 744;
const UINT kMenuBarIdViewSpeedPaused = 745;
const UINT kMenuBarIdViewGroupByType = 746;
const UINT kMenuBarIdViewExpandAll = 747;

// 菜单栏三个栏目各自的下拉菜单。DuiMenuBar 只借用指针，要求下拉菜单的生命期
// 长于菜单栏本身；而菜单栏随页面重建，所以这三个对象放在这里随进程存活。
DuiMenu s_menuBarFileMenu;
DuiMenu s_menuBarOptionsMenu;
DuiMenu s_menuBarViewMenu;

// 重建菜单栏的三张下拉菜单。
// 每次构建菜单栏页面时都调用一次，这样切换界面语言之后下拉项的文字也会跟着
// 变。项文字里的 & 是助记符标记，按 Alt 加该字母可以直接跳到对应栏目或项。
void RebuildMenuBarMenus()
{
    s_menuBarFileMenu.Clear();
    s_menuBarFileMenu.AppendItem(kMenuBarIdFileOpen, Txt(_T("打开(&O)..."), _T("&Open...")));
    s_menuBarFileMenu.AppendItem(kMenuBarIdFileSave, Txt(_T("保存(&S)"), _T("&Save")));
    s_menuBarFileMenu.AppendSeparator();
    s_menuBarFileMenu.AppendItem(kMenuBarIdFileExit, Txt(_T("退出(&X)"), _T("E&xit")));

    s_menuBarOptionsMenu.Clear();
    s_menuBarOptionsMenu.AppendChecked(kMenuBarIdOptTopmost,
                                       Txt(_T("窗口置顶(&T)"), _T("Always on &top")), false);
    s_menuBarOptionsMenu.AppendChecked(kMenuBarIdOptHideOnMin,
                                       Txt(_T("最小化时隐藏(&H)"), _T("&Hide on minimize")), true);
    s_menuBarOptionsMenu.AppendChecked(kMenuBarIdOptOnDemand,
                                       Txt(_T("按需显示(&D)"), _T("Show on &demand")), false);

    s_menuBarViewMenu.Clear();
    s_menuBarViewMenu.AppendItem(kMenuBarIdViewRefresh, Txt(_T("刷新(&R)\tF5"), _T("&Refresh\tF5")));
    s_menuBarViewMenu.AppendSeparator();
    s_menuBarViewMenu.AppendChecked(kMenuBarIdViewSpeedHigh,
                                    Txt(_T("刷新频率：高"), _T("Speed: high")), false);
    s_menuBarViewMenu.AppendChecked(kMenuBarIdViewSpeedNormal,
                                    Txt(_T("刷新频率：中"), _T("Speed: normal")), true);
    s_menuBarViewMenu.AppendChecked(kMenuBarIdViewSpeedLow,
                                    Txt(_T("刷新频率：低"), _T("Speed: low")), false);
    s_menuBarViewMenu.AppendChecked(kMenuBarIdViewSpeedPaused,
                                    Txt(_T("刷新频率：暂停"), _T("Speed: paused")), false);
    s_menuBarViewMenu.AppendSeparator();
    s_menuBarViewMenu.AppendChecked(kMenuBarIdViewGroupByType,
                                    Txt(_T("按类型分组(&G)"), _T("&Group by type")), true);
    s_menuBarViewMenu.AppendItem(kMenuBarIdViewExpandAll,
                                 Txt(_T("全部展开(&E)"), _T("&Expand all")));
}

#endif // BUI_FEATURE_MENUBAR

} // 匿名命名空间

// =====================================================================
// 列表框
// =====================================================================

std::unique_ptr<DuiControl> Build_ListBox()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("十项，单选"), _T("10 items, single select")),
               Txt(_T("点击一项把它变成选中项，鼠标经过的项高亮显示。键盘上的 Up、Down、Home、End、")
                   _T("PageUp、PageDown 都能移动选中项，选中项移出可见范围时列表自动滚动跟上。"),
                   _T("Click to select an item; the item under the mouse is highlighted. ")
                   _T("Up, Down, Home, End, PageUp and PageDown move the selection, and the list ")
                   _T("scrolls automatically to keep the selected item visible.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiListBox> listBox(new DuiListBox());
        listBox->SetCtrlId(kIdListBoxBasic);
        for (int i = 0; i < kFruitItemCount; ++i)
        {
            listBox->AddItem(Txt(kFruitItems[i].zh, kFruitItems[i].en));
        }
        listBox->SetCurSel(0, false);
        row->AddChild(std::move(listBox), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kListBoxRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("五十项，滚动条自动出现"), _T("50 items with a scroll bar")),
               Txt(_T("内容高度超过可见高度时，控件内嵌的 DuiScrollBar 自动出现；内容变少之后又自动消失。")
                   _T("滚轮、拖动滑块、点击轨道三种操作都能滚动。"),
                   _T("The embedded DuiScrollBar shows up as soon as the content is taller than the ")
                   _T("visible area, and disappears again when it is not. The wheel, dragging the thumb ")
                   _T("and clicking the track all scroll the list.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiListBox> listBox(new DuiListBox());
        listBox->SetCtrlId(kIdListBoxScroll);
        for (int i = 1; i <= kScrollDemoItemCount; ++i)
        {
            CString text;
            text.Format(Txt(_T("第 %02d 行"), _T("Row #%02d")), i);
            listBox->AddItem(text);
        }
        listBox->SetCurSel(0, false);
        row->AddChild(std::move(listBox), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kListBoxRowHeight);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 虚拟列表
// =====================================================================

std::unique_ptr<DuiControl> Build_VirtualList()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("一万行（绘制回调）"), _T("10000 rows (paint callback)")),
               Txt(_T("DuiVirtualList 不保存任何行数据，调用方只告诉它总共有多少行，再给一个绘制回调。")
                   _T("控件每次只对当前可见的那十几行回调，所以无论总行数是一万还是一百万，")
                   _T("绘制开销都是一样的。选中状态、滚动位置和鼠标停留状态由控件自己维护。"),
                   _T("DuiVirtualList stores no row data: the caller declares how many rows there are ")
                   _T("and supplies a paint callback. Only the dozen or so visible rows are painted on ")
                   _T("each pass, so the cost is the same whether there are ten thousand rows or a ")
                   _T("million. Selection, scroll position and hover state are maintained by the control.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiVirtualList> virtualList(new DuiVirtualList());
        virtualList->SetCtrlId(kIdVirtualList);
        virtualList->SetRowCount(kVirtualRowCount);
        virtualList->SetRowHeight(kVirtualRowHeight);
        // 回调不需要额外的用户数据，行内容完全由行号推出来，故第二个参数传空。
        virtualList->SetPaintRowCallback(&PaintVirtualRow, NULL);
        row->AddChild(std::move(virtualList), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kVirtualListRowHeight);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 树形列表
// =====================================================================

std::unique_ptr<DuiControl> Build_TreeView()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // ---- 1. 基础用法 ------------------------------------------------
    AddSection(page.get(),
               Txt(_T("1. 联系人列表（基础用法：点三角标志展开折叠，点行选中）"),
                   _T("1. Contact list (basics: click the arrow to expand, click a row to select)")),
               Txt(_T("树嵌在 DuiScrollView 里，展开或折叠改变内容高度时滚动条自动出现或消失。")
                   _T("默认行高 28 像素，每一层缩进 18 像素，行的右端可以挂一个表示在线状态的彩色圆点。"),
                   _T("The tree sits inside a DuiScrollView, so the scroll bar appears and disappears as ")
                   _T("expanding or collapsing changes the content height. Rows are 28 pixels high, each ")
                   _T("level is indented by 18 pixels, and a row may carry a colored status dot at its ")
                   _T("right end.")));
    {
        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeBasic);

        int groupContacts = tree->AddRoot(Txt(_T("联系人"), _T("Contacts")));
        for (int i = 0; i < kBasicContactCount; ++i)
        {
            int id = tree->AddChild(groupContacts,
                                    Txt(kBasicContacts[i].zh, kBasicContacts[i].en));
            tree->SetItemStatusColor(id, kBasicContacts[i].status);
        }

        int groupCoworkers = tree->AddRoot(Txt(_T("同事"), _T("Coworkers")));
        for (int i = 1; i <= kBasicCoworkerCount; ++i)
        {
            CString text;
            text.Format(Txt(_T("工程师 #%d"), _T("Engineer #%d")), i);
            int id = tree->AddChild(groupCoworkers, text);
            // 四种在线状态轮流出现，让这一组同时展示出四种颜色的状态点。
            COLORREF status = kTreeStatusOnline;
            if (i % 4 == 0)
            {
                status = kTreeStatusOffline;
            }
            else if (i % 3 == 0)
            {
                status = kTreeStatusBusy;
            }
            else if (i % 2 == 0)
            {
                status = kTreeStatusAway;
            }
            tree->SetItemStatusColor(id, status);
        }

        int groupProjects = tree->AddRoot(Txt(_T("项目"), _T("Projects")));
        int projectAlpha = tree->AddChild(groupProjects, _T("Alpha"));
        tree->AddChild(projectAlpha, Txt(_T("前端"), _T("frontend")));
        tree->AddChild(projectAlpha, Txt(_T("后端"), _T("backend")));
        tree->AddChild(projectAlpha, Txt(_T("移动端"), _T("mobile")));
        int projectBeta = tree->AddChild(groupProjects, _T("Beta"));
        tree->AddChild(projectBeta, Txt(_T("设计"), _T("design")));
        tree->AddChild(projectBeta, Txt(_T("基础设施"), _T("infra")));

        // 嵌进滚动视图，节点多到装不下时不至于把整个页面撑长。
        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        DuiTreeView* pTree = tree.get();
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightTall);
    }

    // ---- 2. 多层级嵌套 ----------------------------------------------
    AddSection(page.get(),
               Txt(_T("2. 多层级嵌套（任意深度）"), _T("2. Arbitrarily deep nesting")),
               Txt(_T("AddChild 可以挂到任意一个已经存在的节点下面，不限定挂在根节点上，因此层级深度没有上限。")
                   _T("绘制时按「层级 × 缩进」做视觉缩进，缩进默认 18 像素；折叠某个祖先节点时，")
                   _T("它下面的整棵子树一并跳过。本段画的是一棵五层的公司组织结构：公司、中心、部门、小组、项目。"),
                   _T("AddChild can attach a node under any existing node, not just under a root, so there ")
                   _T("is no limit on depth. Each level is indented by depth times the indent width ")
                   _T("(18 pixels by default), and collapsing an ancestor skips its whole subtree. This ")
                   _T("section builds a five-level org chart: company, center, department, team, project.")));
    {
        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeMultiLevel);

        //—— 第 0 层：公司
        int company = tree->AddRoot(Txt(_T("云海科技"), _T("CloudCorp")));

        //—— 第 1 层：两个中心
        int centerRD = tree->AddChild(company, Txt(_T("研发中心"), _T("R&D Center")));
        int centerOps = tree->AddChild(company, Txt(_T("运维中心"), _T("Operations Center")));

        //—— 第 2 层：每个中心下面两个部门
        int deptFrontend = tree->AddChild(centerRD, Txt(_T("前端部"), _T("Frontend")));
        int deptBackend = tree->AddChild(centerRD, Txt(_T("后端部"), _T("Backend")));
        int deptHosting = tree->AddChild(centerOps, Txt(_T("托管部"), _T("Hosting")));
        tree->AddChild(centerOps, Txt(_T("支持部"), _T("Support")));

        //—— 第 3 层：部门下面的小组
        int teamWeb = tree->AddChild(deptFrontend, Txt(_T("网页组"), _T("Web Team")));
        int teamMobile = tree->AddChild(deptFrontend, Txt(_T("移动组"), _T("Mobile Team")));
        int teamGateway = tree->AddChild(deptBackend, Txt(_T("网关组"), _T("Gateway Group")));
        tree->AddChild(deptBackend, Txt(_T("存储组"), _T("Datastore Group")));
        tree->AddChild(deptHosting, Txt(_T("内容分发组"), _T("CDN Group")));

        //—— 第 4 层：项目，都是叶节点
        tree->AddChild(teamWeb, Txt(_T("极光项目"), _T("Project Aurora")));
        tree->AddChild(teamWeb, Txt(_T("晨曦项目"), _T("Project Borealis")));
        tree->AddChild(teamMobile, Txt(_T("彗星项目"), _T("Project Comet")));
        tree->AddChild(teamMobile, Txt(_T("天龙项目"), _T("Project Drako")));
        tree->AddChild(teamGateway, Txt(_T("边缘代理"), _T("Edge Proxy")));
        tree->AddChild(teamGateway, Txt(_T("限流器"), _T("Rate Limiter")));

        DuiTreeView* pTree = tree.get();

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());
        DuiScrollView* pScrollView = scrollView.get();

        //—— 两个按钮批量展开与折叠，顺便看折叠祖先时整棵子树一起消失
        std::unique_ptr<DuiHBox> toolBar(new DuiHBox());
        toolBar->SetGap(kTreeToolButtonGap);

        std::unique_ptr<FnButton> btnExpand(new FnButton());
        btnExpand->SetText(Txt(_T("全部展开"), _T("Expand all")));
        btnExpand->onClick = [pTree, pScrollView](FnButton*)
        {
            pTree->ExpandAll();
            // 可见节点数变了，内容高度要重新告诉滚动视图，否则滚动范围是旧的。
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        std::unique_ptr<FnButton> btnCollapse(new FnButton());
        btnCollapse->SetText(Txt(_T("全部折叠"), _T("Collapse all")));
        btnCollapse->onClick = [pTree, pScrollView](FnButton*)
        {
            pTree->CollapseAll();
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        toolBar->AddChild(std::move(btnExpand), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnCollapse), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));

        std::unique_ptr<DuiVBox> group(new DuiVBox());
        group->SetGap(kTreeToolBarGap);
        group->AddChild(std::move(toolBar), DuiLayout::Hint().Fixed(kTreeToolBarHeight));
        group->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(group), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightTall);
    }

    // ---- 3. 节点图标 ------------------------------------------------
    AddSection(page.get(),
               Txt(_T("3. 节点图标（SetItemIcon 传入 HBITMAP）"),
                   _T("3. Per-node icons (SetItemIcon takes an HBITMAP)")),
               Txt(_T("每个节点都可以挂自己的图标，画在展开标志和标签之间，显示尺寸 18×18 像素。")
                   _T("位图的所有权归调用方，控件内部只保存裸指针，既不复制也不释放，")
                   _T("因此位图必须活得比控件久。一部分节点有图标、一部分没有可以混排；")
                   _T("单列模式和多列模式都支持，任意层级都可用。"),
                   _T("Any node can carry its own icon, drawn between the expand arrow and the label at ")
                   _T("18 by 18 pixels. The bitmap stays owned by the caller — the control keeps a raw ")
                   _T("pointer and neither copies nor frees it, so the bitmap must outlive the control. ")
                   _T("Nodes with and without icons can be mixed freely, in single-column and ")
                   _T("multi-column mode alike, at any depth.")));
    {
        //—— 四张不同色调的渐变位图当测试素材。静态变量只创建一次，
        //   页面重建时复用，随进程退出释放。
        static HBITMAP s_iconRedFolder = MakeGradientIconBitmap(220, 90, 60, 130, 30, 20);
        static HBITMAP s_iconBlueFolder = MakeGradientIconBitmap(80, 130, 220, 30, 60, 130);
        static HBITMAP s_iconGreenDoc = MakeGradientIconBitmap(80, 200, 110, 30, 110, 50);
        static HBITMAP s_iconAmberDoc = MakeGradientIconBitmap(255, 180, 70, 200, 110, 20);

        // 本段的行高（像素）。比默认的 28 高一点，18 像素的图标四周才有余量。
        const int kIconDemoRowHeight = 30;

        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeIcon);
        tree->SetRowHeight(kIconDemoRowHeight);

        //—— 两个根节点用「文件夹」风格的图标
        int projectAlpha = tree->AddRoot(_T("project-alpha"), s_iconRedFolder);
        int projectBeta = tree->AddRoot(_T("project-beta"), s_iconBlueFolder);

        //—— 子节点用「文档」风格的图标，其中两个不传图标，演示混排
        tree->AddChild(projectAlpha, _T("README.md"), s_iconGreenDoc);
        tree->AddChild(projectAlpha, _T("design.pdf"), s_iconAmberDoc);
        tree->AddChild(projectAlpha, Txt(_T("（这一项没有图标）"), _T("(no icon on this one)")));
        tree->AddChild(projectAlpha, _T("schema.sql"), s_iconGreenDoc);

        tree->AddChild(projectBeta, _T("docs.md"), s_iconGreenDoc);
        tree->AddChild(projectBeta, _T("logo.png"), s_iconAmberDoc);
        tree->AddChild(projectBeta, Txt(_T("（这一项也没有图标）"), _T("(no icon here either)")));

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        DuiTreeView* pTree = tree.get();
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightMedium);
    }

    // ---- 4. 多列表格模式 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("4. 多列表格模式（拖动列宽、Ctrl 加点击多选、双击文字进入编辑）"),
                   _T("4. Multi-column table mode (drag column widths, Ctrl-click to multi-select, double-click text to edit)")),
               Txt(_T("五列：名称列是树列并且被冻结，其余四列分别演示复选框、进度条、右对齐文本和超链接。")
                   _T("本段打开了编辑开关，冻结了第一列，并且初始就按名称列升序显示排序三角标志。")
                   _T("排序本身由业务侧完成，控件只发出「用户点了这一列」的通知并画三角标志。"),
                   _T("Five columns: the name column is the tree column and is frozen; the other four show ")
                   _T("a check box, a progress bar, right-aligned text and a hyperlink. Editing is enabled, ")
                   _T("the first column is frozen, and the sort indicator starts out ascending on the name ")
                   _T("column. Sorting itself is up to the caller — the control only reports that a column ")
                   _T("was clicked and draws the indicator.")));
    {
        // 各列的初始宽度与最小宽度（像素）。最小宽度是拖动列宽时的下限。
        const int kColNameWidth = 200;
        const int kColNameMinWidth = 80;
        const int kColDoneWidth = 70;
        const int kColDoneMinWidth = 50;
        const int kColProgressWidth = 160;
        const int kColProgressMinWidth = 90;
        const int kColSizeWidth = 90;
        const int kColSizeMinWidth = 60;
        const int kColLinkWidth = 180;
        const int kColLinkMinWidth = 80;
        // 冻结列数：名称列贴在左边，水平滚动时不跟着走。
        const int kFrozenColumnCount = 1;
        // 排序指示的方向，取 +1 表示升序。
        const int kSortAscending = 1;

        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeMultiColumn);

        int colName = tree->AddColumn(Txt(_T("名称"), _T("Name")),
                                      kColNameWidth, kColNameMinWidth, DT_LEFT);
        int colDone = tree->AddColumn(Txt(_T("完成"), _T("Done")),
                                      kColDoneWidth, kColDoneMinWidth, DT_CENTER);
        int colProgress = tree->AddColumn(Txt(_T("进度"), _T("Progress")),
                                          kColProgressWidth, kColProgressMinWidth, DT_LEFT);
        int colSize = tree->AddColumn(Txt(_T("大小"), _T("Size")),
                                      kColSizeWidth, kColSizeMinWidth, DT_RIGHT);
        int colLink = tree->AddColumn(Txt(_T("链接"), _T("Link")),
                                      kColLinkWidth, kColLinkMinWidth, DT_LEFT);

        tree->SetFrozenColumns(kFrozenColumnCount);
        tree->SetEditable(true);
        tree->SetSortIndicator(colName, kSortAscending);

        // 一行表格数据。名称、大小、网址这些不随语言变化，链接文字要跟着变。
        struct TableRow
        {
            // 名称列的文字，同时也是树节点的标签。
            LPCTSTR name;
            // 完成列的勾选状态。
            bool done;
            // 进度列的取值，范围 0 到 100。
            int progress;
            // 大小列的文字。
            LPCTSTR size;
            // 链接列显示出来的文字。
            LPCTSTR linkText;
            // 链接列对应的网址。
            LPCTSTR url;
        };
        // 本数组必须是局部变量：初始值里的 Txt() 要在每次构建页面时重新求值，
        // 写成静态变量会把第一次构建时的语言固定下来。
        const TableRow rows[] =
        {
            { _T("README.md"),  true,  100, _T("4.2 KB"),
              Txt(_T("打开"), _T("open")),     _T("https://example.com/readme") },
            { _T("design.pdf"), false,  60, _T("1.1 MB"),
              Txt(_T("预览"), _T("preview")),  _T("https://example.com/design") },
            { _T("logo.png"),   true,  100, _T("87 KB"),
              Txt(_T("下载"), _T("download")), _T("https://example.com/logo")   },
            { _T("notes.txt"),  false,  20, _T("612 B"),
              Txt(_T("编辑"), _T("edit")),     _T("https://example.com/notes")  },
            { _T("schema.sql"), false,  85, _T("32 KB"),
              Txt(_T("执行"), _T("run")),      _T("https://example.com/sql")    },
        };
        const int kTableRowCount = (int)(sizeof(rows) / sizeof(rows[0]));

        int projectAlpha = tree->AddRoot(_T("project-alpha"));
        for (int i = 0; i < kTableRowCount; ++i)
        {
            int id = tree->AddChild(projectAlpha, rows[i].name);
            tree->SetCellChecked(id, colDone, rows[i].done);
            tree->SetCellValue(id, colProgress, rows[i].progress);
            tree->SetCellText(id, colSize, rows[i].size);
            tree->SetCellLink(id, colLink, rows[i].linkText, rows[i].url);
        }

        // 第二个根节点只放一行，用来说明多列模式下同样可以有多棵子树。
        const int kDocsProgress = 45;
        int projectBeta = tree->AddRoot(_T("project-beta"));
        int idDocs = tree->AddChild(projectBeta, _T("docs.md"));
        tree->SetCellChecked(idDocs, colDone, false);
        tree->SetCellValue(idDocs, colProgress, kDocsProgress);
        tree->SetCellText(idDocs, colSize, _T("8.7 KB"));
        tree->SetCellLink(idDocs, colLink, Txt(_T("打开"), _T("open")),
                          _T("https://example.com/docs"));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(tree), DuiLayout::Hint().Weight(1));
        AddVariantRowCapture(page.get(), _T("treeview-multicol"),
                             std::move(row), kTreeRowHeightTall);
    }

    // ---- 5. 右侧辅助文字与灰显图标 ----------------------------------
    AddSection(page.get(),
               Txt(_T("5. 右侧辅助文字与灰显图标（仅单列模式）"),
                   _T("5. Trailing text and grayed icons (single-column mode only)")),
               Txt(_T("SetItemRightText 在行的右端画一段右对齐的辅助文字，位置在状态点的内侧，")
                   _T("典型用途是分组行上的在线人数、会话行上的最后消息时间。")
                   _T("SetItemIconGrayed 把图标按亮度公式转成灰度，用来表示「在列表里但当前不可用」，")
                   _T("例如离线的联系人、已冻结的项目。"),
                   _T("SetItemRightText draws a right-aligned piece of text at the end of a row, just ")
                   _T("inside the status dot — typically an online count on a group row or a timestamp on ")
                   _T("a conversation row. SetItemIconGrayed converts the icon to grayscale with a luma ")
                   _T("formula, which reads as \"present but currently unavailable\": an offline contact, ")
                   _T("a frozen project.")));
    {
        //—— 三张彩色渐变位图当测试图标，静态变量只创建一次
        static HBITMAP s_iconBlue = MakeGradientIconBitmap(80, 130, 220, 30, 60, 130);
        static HBITMAP s_iconOrange = MakeGradientIconBitmap(255, 170, 60, 200, 80, 20);
        static HBITMAP s_iconPurple = MakeGradientIconBitmap(170, 90, 200, 90, 30, 130);

        // 本段的行高（像素）。比默认的 28 高一点，图标、文字和右侧辅助文字
        // 才不显得挤。
        const int kRightTextRowHeight = 32;

        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeRightText);
        tree->SetRowHeight(kRightTextRowHeight);

        //—— 分组行的右侧写在线人数，这是最常见的用法
        int groupContacts = tree->AddRoot(Txt(_T("联系人"), _T("Contacts")));
        tree->SetItemRightText(groupContacts, _T("3/5"));

        int idFirst = tree->AddChild(groupContacts, Txt(_T("张伟"), _T("Alice")));
        tree->SetItemIcon(idFirst, s_iconBlue);
        tree->SetItemStatusColor(idFirst, kTreeStatusOnline);

        int idSecond = tree->AddChild(groupContacts, Txt(_T("李娜"), _T("Bob")));
        tree->SetItemIcon(idSecond, s_iconOrange);
        tree->SetItemStatusColor(idSecond, kTreeStatusAway);

        //—— 离线联系人：图标灰显
        int idThird = tree->AddChild(groupContacts, Txt(_T("王强（离线）"), _T("Carol (offline)")));
        tree->SetItemIcon(idThird, s_iconPurple);
        tree->SetItemIconGrayed(idThird, true);
        tree->SetItemStatusColor(idThird, kTreeStatusOffline);

        //—— 离线联系人：图标灰显，右侧再补一个最后在线时间
        int idFourth = tree->AddChild(groupContacts, Txt(_T("赵敏（离线）"), _T("Dave (offline)")));
        tree->SetItemIcon(idFourth, s_iconBlue);
        tree->SetItemIconGrayed(idFourth, true);
        tree->SetItemStatusColor(idFourth, kTreeStatusOffline);
        tree->SetItemRightText(idFourth, Txt(_T("今天 14:32"), _T("today 14:32")));

        //—— 第二组只有右侧文字、没有状态点，用来看辅助文字贴住右边距的效果
        int groupProjects = tree->AddRoot(Txt(_T("项目"), _T("Projects")));
        tree->SetItemRightText(groupProjects, Txt(_T("2 个进行中"), _T("2 active")));

        int idAlpha = tree->AddChild(groupProjects, _T("Alpha"));
        tree->SetItemIcon(idAlpha, s_iconOrange);
        tree->SetItemRightText(idAlpha, Txt(_T("3 个待办"), _T("3 issues")));

        int idBeta = tree->AddChild(groupProjects, Txt(_T("Beta（已冻结）"), _T("Beta (frozen)")));
        tree->SetItemIcon(idBeta, s_iconPurple);
        tree->SetItemIconGrayed(idBeta, true);
        tree->SetItemRightText(idBeta, Txt(_T("四季度发布"), _T("Q4 release")));

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        DuiTreeView* pTree = tree.get();
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightShort);
    }

    // ---- 6. 悬停通知与滚动条自动隐藏 --------------------------------
    AddSection(page.get(),
               Txt(_T("6. 悬停通知（DUITVN_HOVER_ENTER / DUITVN_HOVER_LEAVE）与滚动条自动隐藏"),
                   _T("6. Hover notifications (DUITVN_HOVER_ENTER / DUITVN_HOVER_LEAVE) and auto-hiding scroll bar")),
               Txt(_T("把鼠标移到行上，下方的标签会实时显示当前悬停的节点。这两个通知没有延时，")
                   _T("业务如果想要「停留 500 毫秒才弹出名片」这种效果，自己用定时器加延时即可。")
                   _T("滚动条这里调用了 SetAutoHide(true)：滚动时淡入，停止滚动约 800 毫秒后淡出。")
                   _T("这是 balloonui 滚动控件自带的行为，直接复用即可，不需要为此改动控件库。"),
                   _T("Move the mouse over the rows and the label below updates to show the node under the ")
                   _T("cursor. The two notifications carry no delay: if you want \"show a card after ")
                   _T("hovering for 500 ms\", add the delay yourself with a timer. The scroll bar here has ")
                   _T("SetAutoHide(true) on it, so it fades in while scrolling and fades out roughly 800 ms ")
                   _T("after scrolling stops. That behaviour is built into the balloonui scroll controls; ")
                   _T("reuse it as is rather than modifying the library.")));
    {
        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeHover);
        DuiTreeView* pTree = tree.get();

        //—— 行数多一些，既给鼠标留出移动的空间，也让滚动条必然出现
        int groupRoot = tree->AddRoot(Txt(_T("工程师"), _T("Engineers")));
        for (int i = 1; i <= kHoverDemoRowCount; ++i)
        {
            CString text;
            text.Format(Txt(_T("工程师 #%d"), _T("Eng #%d")), i);
            int id = tree->AddChild(groupRoot, text);
            tree->SetItemStatusColor(id, (i & 1) ? kTreeStatusOnline : kTreeStatusOffline);
        }

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());
        DuiScrollBar* pScrollBar = scrollView->GetScrollBar();
        if (pScrollBar != NULL)
        {
            pScrollBar->SetAutoHide(true);
        }

        //—— 显示当前悬停节点的标签
        // 悬停提示标签的高度（像素）。单行文字。
        const int kHoverLabelHeight = 20;
        // 标签与下方树之间的竖直间距（像素）。
        const int kHoverLabelGap = 6;

        std::unique_ptr<DuiLabel> hoverLabel(new DuiLabel());
        hoverLabel->SetText(Txt(_T("当前悬停：（无）—— 把鼠标移到行上试试"),
                                _T("Hovering: (none) — move the mouse over the rows")));
        hoverLabel->SetTextColor(RGB(60, 60, 60));
        DuiLabel* pHoverLabel = hoverLabel.get();

        //—— 登记本页面的通知钩子，画廊窗口会把控件通知转到这里。
        //   判断条件里必须连控件编号一起判：自定义通知码是每个控件各自从
        //   DUIN_CUSTOM 起算的，别的控件的第一个自定义码与这里的数值相同。
        g_pageNotifyHook = [pTree, pHoverLabel](const DuiNotify* pNotify)
        {
            if (pNotify == NULL)
            {
                return;
            }
            if (pNotify->code == (UINT)DuiTreeView::DUITVN_HOVER_ENTER
                && pNotify->ctrlId == kIdTreeHover)
            {
                int itemId = (int)pNotify->extra;
                CString label = pTree->GetItemLabel(itemId);
                CString text;
                text.Format(Txt(_T("当前悬停：%s（编号 %d）"), _T("Hovering: %s (id=%d)")),
                            (LPCTSTR)label, itemId);
                pHoverLabel->SetText(text);
            }
            else if (pNotify->code == (UINT)DuiTreeView::DUITVN_HOVER_LEAVE
                     && pNotify->ctrlId == kIdTreeHover)
            {
                pHoverLabel->SetText(Txt(_T("当前悬停：（无）"), _T("Hovering: (none)")));
            }
        };

        std::unique_ptr<DuiVBox> group(new DuiVBox());
        group->SetGap(kHoverLabelGap);
        group->AddChild(std::move(hoverLabel), DuiLayout::Hint().Fixed(kHoverLabelHeight));
        group->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(group), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightHover);
    }

    // ---- 7. 展开状态快照与节点过滤 ----------------------------------
    AddSection(page.get(),
               Txt(_T("7. 展开状态快照与节点过滤"), _T("7. Expand-state snapshots and node filtering")),
               Txt(_T("「保存展开状态」记下当前所有处于展开状态的节点编号。先「全部折叠」再「还原展开状态」，")
                   _T("可以看到中间的破坏性操作并不影响快照的还原结果。")
                   _T("「过滤工程师」用一个谓词函数过滤节点：只保留名字以「工程师」开头的节点及其后代。")
                   _T("「显示或隐藏」按钮单独翻转某一个节点的可见性，它与过滤器彼此独立，")
                   _T("最终是否可见由两者共同决定。"),
                   _T("\"Save expand\" records the ids of every currently expanded node. Collapse ")
                   _T("everything and then restore, and you can see that the destructive step in between ")
                   _T("does not affect the result. \"Filter Eng*\" installs a predicate that keeps only ")
                   _T("nodes whose label starts with \"Eng\", together with their descendants. The toggle ")
                   _T("button flips the visibility of one single node; that flag and the filter are ")
                   _T("independent, and a node is visible only when both allow it.")));
    {
        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeFilter);
        DuiTreeView* pTree = tree.get();

        //—— 三组示例数据：联系人、工程师、设计师
        int groupContacts = tree->AddRoot(Txt(_T("联系人"), _T("Contacts")));
        tree->AddChild(groupContacts, Txt(_T("张伟"), _T("Alice")));
        int idToggleTarget = tree->AddChild(groupContacts, Txt(_T("李娜"), _T("Bob")));
        tree->AddChild(groupContacts, Txt(_T("王强"), _T("Carol")));

        // 过滤用的前缀。分组名与组内每个节点都以它开头，所以过滤之后这一组
        // 整体保留，其余两组整体消失。
        LPCTSTR filterPrefix = Txt(_T("工程师"), _T("Eng"));

        int groupEngineers = tree->AddRoot(Txt(_T("工程师"), _T("Engineers")));
        for (int i = 1; i <= kFilterDemoEngineerCount; ++i)
        {
            CString text;
            text.Format(Txt(_T("工程师 #%d"), _T("Eng #%d")), i);
            tree->AddChild(groupEngineers, text);
        }

        int groupDesigners = tree->AddRoot(Txt(_T("设计师"), _T("Designers")));
        tree->AddChild(groupDesigners, Txt(_T("孙浩"), _T("Dan")));
        tree->AddChild(groupDesigners, Txt(_T("周雨"), _T("Dora")));

        //—— 嵌进滚动视图：节点显隐变化时滚动条自动出现或消失
        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());
        DuiScrollView* pScrollView = scrollView.get();

        //—— 六个按钮横排。每个按钮改完树之后都要重新告诉滚动视图内容高度，
        //   否则滚动范围停留在改动之前。
        std::unique_ptr<DuiHBox> toolBar(new DuiHBox());
        toolBar->SetGap(kTreeToolButtonGap);

        std::unique_ptr<FnButton> btnSave(new FnButton());
        btnSave->SetText(Txt(_T("保存展开状态"), _T("Save expand")));
        btnSave->onClick = [pTree](FnButton*)
        {
            s_treeExpandSnapshot = pTree->GetExpandedSnapshot();
        };

        std::unique_ptr<FnButton> btnCollapse(new FnButton());
        btnCollapse->SetText(Txt(_T("全部折叠"), _T("Collapse all")));
        btnCollapse->onClick = [pTree, pScrollView](FnButton*)
        {
            pTree->CollapseAll();
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        std::unique_ptr<FnButton> btnRestore(new FnButton());
        btnRestore->SetText(Txt(_T("还原展开状态"), _T("Restore expand")));
        btnRestore->onClick = [pTree, pScrollView](FnButton*)
        {
            pTree->RestoreExpanded(s_treeExpandSnapshot);
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        std::unique_ptr<FnButton> btnFilter(new FnButton());
        btnFilter->SetText(Txt(_T("过滤工程师"), _T("Filter Eng*")));
        btnFilter->onClick = [pTree, pScrollView, filterPrefix](FnButton*)
        {
            pTree->SetFilter([pTree, filterPrefix](int nodeId) -> bool
            {
                CString label = pTree->GetItemLabel(nodeId);
                return label.Find(filterPrefix) == 0;
            });
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        std::unique_ptr<FnButton> btnClearFilter(new FnButton());
        btnClearFilter->SetText(Txt(_T("清除过滤"), _T("Clear filter")));
        btnClearFilter->onClick = [pTree, pScrollView](FnButton*)
        {
            pTree->ClearFilter();
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        std::unique_ptr<FnButton> btnToggle(new FnButton());
        btnToggle->SetText(Txt(_T("显示或隐藏李娜"), _T("Toggle Bob")));
        btnToggle->onClick = [pTree, pScrollView, idToggleTarget](FnButton*)
        {
            bool wasVisible = pTree->IsItemVisible(idToggleTarget);
            pTree->SetItemVisible(idToggleTarget, !wasVisible);
            pScrollView->SetContentHeight(pTree->GetContentHeight());
        };

        toolBar->AddChild(std::move(btnSave), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnCollapse), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnRestore), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnFilter), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnClearFilter), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));
        toolBar->AddChild(std::move(btnToggle), DuiLayout::Hint().Fixed(kTreeToolButtonWidth));

        std::unique_ptr<DuiVBox> group(new DuiVBox());
        group->SetGap(kTreeToolBarGap);
        group->AddChild(std::move(toolBar), DuiLayout::Hint().Fixed(kTreeToolBarHeight));
        group->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(group), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightMedium);
    }

    // ---- 8. 副标签 --------------------------------------------------
    AddSection(page.get(),
               Txt(_T("8. 副标签（两行节点，仅单列模式）"),
                   _T("8. Sub-labels (two-line rows, single-column mode only)")),
               Txt(_T("SetItemSubLabel 在主标签下面再加一行字号略小的副标签。调用方应当把行高调到 40 像素")
                   _T("左右，两行文字才不显得挤。副标签为空字符串时这一行退回单行垂直居中绘制，")
                   _T("因此同一棵树里两行节点和单行节点可以混排。"),
                   _T("SetItemSubLabel adds a second, slightly smaller line under the main label. Raise the ")
                   _T("row height to around 40 pixels so the two lines are not cramped. A node whose ")
                   _T("sub-label is empty falls back to a single vertically centered line, so one- and ")
                   _T("two-line rows can be mixed in the same tree.")));
    {
        // 本段的行高（像素）。默认的 28 装不下两行文字。
        const int kSubLabelRowHeight = 40;

        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeSubLabel);
        tree->SetRowHeight(kSubLabelRowHeight);

        int groupContacts = tree->AddRoot(Txt(_T("联系人"), _T("Contacts")));

        //—— 三种情形：完整的副标签、过长需要截断的副标签、没有副标签
        int idFirst = tree->AddChild(groupContacts, Txt(_T("张伟"), _T("Alice")));
        tree->SetItemStatusColor(idFirst, kTreeStatusOnline);
        tree->SetItemSubLabel(idFirst, Txt(_T("在线 · 2 分钟前活跃"), _T("Online · last seen 2 min ago")));

        int idSecond = tree->AddChild(groupContacts, Txt(_T("李娜"), _T("Bob")));
        tree->SetItemStatusColor(idSecond, kTreeStatusBusy);
        tree->SetItemSubLabel(idSecond,
                              Txt(_T("忙碌 · 会议中，下午三点结束，有事请留言"),
                                  _T("Busy · in meeting until 3pm — please leave a message")));

        int idThird = tree->AddChild(groupContacts, Txt(_T("王强"), _T("Carol")));
        tree->SetItemStatusColor(idThird, kTreeStatusAway);
        //—— 这一项不设副标签，绘制时退回单行垂直居中

        int groupProjects = tree->AddRoot(Txt(_T("项目"), _T("Projects")));
        int idAlpha = tree->AddChild(groupProjects, _T("Alpha"));
        tree->SetItemSubLabel(idAlpha,
                              Txt(_T("3 个待办，1 个合并请求待评审"),
                                  _T("3 open issues, 1 PR pending review")));
        int idBeta = tree->AddChild(groupProjects, _T("Beta"));
        tree->SetItemSubLabel(idBeta,
                              Txt(_T("已冻结，等待四季度发布分支"),
                                  _T("Frozen for the Q4 release branch")));

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        DuiTreeView* pTree = tree.get();
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightMedium);
    }

    // ---- 9. 节点自绘控件 --------------------------------------------
    AddSection(page.get(),
               Txt(_T("9. 节点自绘控件（业务接管整行绘制）"),
                   _T("9. Custom node controls (the caller paints the whole row)")),
               Txt(_T("SetItemCustomControl 把节点的内容区交给业务自己提供的 DuiControl 绘制。")
                   _T("树仍然负责画行背景、展开标志和缩进，但跳过内置的标签、图标、副标签、")
                   _T("右侧文字和状态点。内容区里的鼠标事件先转发给这个控件，它返回真表示已经处理。")
                   _T("本段的行控件画了头像方块、名称、摘要和红色的未读条数角标 —— ")
                   _T("未读角标这种只有即时通讯业务才有的概念就应该这样放在业务代码里，不必进通用控件库。"),
                   _T("SetItemCustomControl hands the content area of a node to a DuiControl supplied by ")
                   _T("the caller. The tree still paints the row background, the expand arrow and the ")
                   _T("indent, but skips its own label, icon, sub-label, trailing text and status dot. ")
                   _T("Mouse events inside the content area go to that control first, and it returns true ")
                   _T("when it has handled them. The row control here paints an avatar square, a name, a ")
                   _T("summary line and a red unread badge — a concept specific to messaging apps, which ")
                   _T("belongs in application code rather than in a general-purpose control library.")));
    {
        // 本段的行高（像素）。行内容由业务自决，这里取大一些以容纳头像和两行文字。
        const int kCustomRowHeight = 56;

        std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
        tree->SetCtrlId(kIdTreeCustomRow);
        tree->SetRowHeight(kCustomRowHeight);

        // 一条会话的示例数据。
        struct ConversationRow
        {
            // 会话名称。
            LPCTSTR name;
            // 会话摘要，显示在第二行。
            LPCTSTR summary;
            // 头像方块的底色。
            COLORREF avatarColor;
            // 未读条数；0 表示不画角标。
            int unreadCount;
        };
        // 与第 4 段同理，初始值里有 Txt()，必须是局部变量。
        const ConversationRow rows[] =
        {
            { Txt(_T("张伟"),   _T("Alice")),
              Txt(_T("今天有空吗"),        _T("Free this afternoon?")),
              RGB(0xFF, 0x7A, 0x45),   3 },
            { Txt(_T("李娜"),   _T("Bob")),
              Txt(_T("好的"),              _T("ok")),
              RGB(0x6E, 0x5B, 0xE5),   0 },
            { Txt(_T("王强"),   _T("Charlie")),
              Txt(_T("[图片]"),            _T("[image]")),
              RGB(0x22, 0xC5, 0x5E),  99 },
            { Txt(_T("项目群"), _T("Project group")),
              Txt(_T("孙浩：文档已上传"),   _T("Dan: doc uploaded")),
              RGB(0xFF, 0xC1, 0x07),   5 },
            { Txt(_T("周雨"),   _T("Dora")),
              Txt(_T("回头聊"),            _T("talk later")),
              RGB(0x4A, 0x90, 0xE2), 120 },
        };
        const int kConversationRowCount = (int)(sizeof(rows) / sizeof(rows[0]));

        for (int i = 0; i < kConversationRowCount; ++i)
        {
            // 标签留空：这一行的内容全部由下面的自绘控件负责。
            int id = tree->AddRoot(_T(""));
            tree->SetItemCustomControl(id,
                std::unique_ptr<DuiControl>(
                    new ConversationRowCell(rows[i].name, rows[i].summary,
                                            rows[i].avatarColor, rows[i].unreadCount)));
        }

        std::unique_ptr<DuiScrollView> scrollView(new DuiScrollView());
        DuiTreeView* pTree = tree.get();
        scrollView->SetContent(std::move(tree));
        scrollView->SetContentHeight(pTree->GetContentHeight());

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::move(scrollView), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTreeRowHeightTall);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 标签栏
// =====================================================================

std::unique_ptr<DuiControl> Build_Tab()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("基本标签栏"), _T("Plain tabs")),
               Txt(_T("完全自绘的一条标签栏。当前标签用白色填充，其余标签与整条标签栏同色。")
                   _T("切换当前标签时发出 DUIN_VALUECHANGED 通知，附带的数值是新的标签序号。"),
                   _T("An owner-drawn tab strip. The selected tab is filled with white while the others ")
                   _T("share the strip color. Changing the selection fires DUIN_VALUECHANGED with the new ")
                   _T("tab index attached.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiTab> tab(new DuiTab());
        tab->SetCtrlId(kIdTabPlain);
        tab->AddTab(Txt(_T("联系人"), _T("Contacts")));
        tab->AddTab(Txt(_T("群组"), _T("Groups")));
        tab->AddTab(Txt(_T("最近"), _T("Recent")));
        tab->SetCurSel(0, false);
        row->AddChild(std::move(tab), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTabStripRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("可关闭标签与下拉标签"), _T("Closeable and dropdown tabs")),
               Txt(_T("AddTab 的 closeable 参数取真时，标签右侧画一个叉号；dropdown 参数取真时画一个三角。")
                   _T("点这两处分别发出 DUITN_CLOSE 与 DUITN_DROPDOWN 通知，附带的数值是标签序号。")
                   _T("控件本身不会自己把标签删掉，是否关闭由业务决定。"),
                   _T("Passing closeable=true to AddTab paints an X on the right of the tab; ")
                   _T("dropdown=true paints a triangle. Clicking either fires DUITN_CLOSE or ")
                   _T("DUITN_DROPDOWN with the tab index attached. The control never removes a tab on its ")
                   _T("own — whether to close it is up to the caller.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiTab> tab(new DuiTab());
        tab->SetCtrlId(kIdTabCloseable);
        // 第二个参数是可关闭，第三个参数是带下拉。
        tab->AddTab(Txt(_T("设置"), _T("Settings")), false, true);
        tab->AddTab(Txt(_T("会话：张伟"), _T("Chat: Alice")), true);
        tab->AddTab(Txt(_T("会话：李娜"), _T("Chat: Bob")), true);
        tab->SetCurSel(0, false);
        row->AddChild(std::move(tab), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTabStripRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("带前置图标的标签"), _T("Tabs with a leading icon")),
               Txt(_T("图标可以在 AddTab 时一并传入，也可以事后用 SetTabIcon 设置。")
                   _T("位图是 32 位预乘 alpha 格式，控件用 ::AlphaBlend 绘制，")
                   _T("所有权归调用方、控件不会释放它。SetIconSize 调整显示尺寸，")
                   _T("SetIconGap 调整图标与文字之间的间距。"),
                   _T("An icon can be passed to AddTab or set afterwards with SetTabIcon. The bitmap is ")
                   _T("32-bit premultiplied alpha and is drawn with ::AlphaBlend; it stays owned by the ")
                   _T("caller and the control never frees it. SetIconSize changes the drawn size and ")
                   _T("SetIconGap the space between icon and text.")));
    {
        //—— 三张不同颜色的圆点位图，静态变量只创建一次，页面重建时复用
        static HBITMAP s_dotRed = MakeTabDotIcon(220, 60, 60);
        static HBITMAP s_dotGreen = MakeTabDotIcon(60, 170, 80);
        static HBITMAP s_dotBlue = MakeTabDotIcon(60, 120, 220);

        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiTab> tab(new DuiTab());
        tab->SetCtrlId(kIdTabWithIcon);
        // 中间两个参数分别是可关闭与带下拉，本段都不需要；第四个参数是
        // 附带数据，这里用不到。
        tab->AddTab(Txt(_T("收件箱"), _T("Inbox")), false, false, 0, s_dotRed);
        tab->AddTab(Txt(_T("已发送"), _T("Sent")), false, false, 0, s_dotGreen);
        tab->AddTab(Txt(_T("归档"), _T("Archive")), false, false, 0, s_dotBlue);
        tab->SetCurSel(0, false);
        row->AddChild(std::move(tab), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTabStripRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("宽度自适应：关闭（默认）"), _T("Auto-fit width: off (the default)")),
               Txt(_T("默认状态下每个标签的宽度被限制在最小 60 像素、最大 200 像素之间：")
                   _T("文字很短的标签仍占满 60 像素，文字很长的标签截断到 200 像素并以省略号收尾。")
                   _T("适合设置对话框这类希望每个标签看起来宽度相近的场景。"),
                   _T("By default each tab is clamped between 60 and 200 pixels wide: a very short label ")
                   _T("still takes up 60 pixels, and a very long one is cut off at 200 pixels with an ")
                   _T("ellipsis. This suits places like a settings dialog where the tabs should look ")
                   _T("about equally wide.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiTab> tab(new DuiTab());
        tab->SetCtrlId(kIdTabAutoFitOff);
        // 文字加内边距远不足 60 像素，会被撑到下限。
        tab->AddTab(_T("A"));
        tab->AddTab(Txt(_T("你好"), _T("Hello")));
        tab->AddTab(Txt(_T("设置"), _T("Settings")));
        // 文字远超 200 像素，会被截断并以省略号收尾。
        tab->AddTab(Txt(_T("非常非常非常非常非常非常长的标签标题"),
                        _T("a very very very very very long tab title")));
        tab->SetCurSel(0, false);
        row->AddChild(std::move(tab), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTabStripRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("宽度自适应：开启"), _T("Auto-fit width: on")),
               Txt(_T("调用 SetAutoFitTabWidth(true) 之后跳过最小与最大宽度的限制，")
                   _T("标签宽度严格等于「文字宽度 + 左右内边距 + 图标宽度与间距 + 关闭或下拉标志的增量」。")
                   _T("适合分类条、过滤条这类希望一眼看出各标签长短的场景。"),
                   _T("After SetAutoFitTabWidth(true) the minimum and maximum are skipped and a tab is ")
                   _T("exactly as wide as its text plus padding, plus the icon and its gap, plus whatever ")
                   _T("the close or dropdown marker adds. This suits category or filter bars where the ")
                   _T("differing widths are the point.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiTab> tab(new DuiTab());
        tab->SetCtrlId(kIdTabAutoFitOn);
        tab->SetAutoFitTabWidth(true);
        // 同样的四个标签，这次短的可以窄于 60 像素、长的可以宽于 200 像素。
        tab->AddTab(_T("A"));
        tab->AddTab(Txt(_T("你好"), _T("Hello")));
        tab->AddTab(Txt(_T("设置"), _T("Settings")));
        tab->AddTab(Txt(_T("非常非常非常非常非常非常长的标签标题"),
                        _T("a very very very very very long tab title")));
        tab->SetCurSel(0, false);
        row->AddChild(std::move(tab), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kTabStripRowHeight);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 菜单
// =====================================================================

std::unique_ptr<DuiControl> Build_Menu()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("普通菜单"), _T("Plain menu")),
               Txt(_T("点按钮在它下方弹出一张四项的菜单。菜单是自绘的：白底、浅灰色的悬停高亮、")
                   _T("外围一圈投影。分隔条不参与鼠标命中判定。"),
                   _T("Click the button to pop a four-item menu below it. The menu is owner-drawn: white ")
                   _T("background, light gray hover highlight and a drop shadow around it. Separators are ")
                   _T("skipped by hit testing.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kMenuRowGap);
        std::unique_ptr<MenuPopupButton> button(new MenuPopupButton());
        button->SetText(Txt(_T("弹出普通菜单"), _T("Show plain menu")));
        button->SetBuilder(&BuildPlainDemoMenu);
        row->AddChild(std::move(button), DuiLayout::Hint().Fixed(kMenuButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("勾选项、子菜单与禁用项"), _T("Checked items, sub-menus and disabled items")),
               Txt(_T("AppendChecked 追加带勾选标记的项，AppendSubMenu 追加子菜单")
                   _T("（鼠标停留或点击都能展开），AppendDisabled 追加灰显且点不动的项。")
                   _T("子菜单对象的生命期归调用方，必须保证它在整个弹出期间存活。"),
                   _T("AppendChecked adds an item with a check mark, AppendSubMenu adds a sub-menu ")
                   _T("(hovering or clicking opens it), and AppendDisabled adds a grayed-out item that ")
                   _T("cannot be clicked. The sub-menu object stays owned by the caller and must stay ")
                   _T("alive for as long as the menu is up.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kMenuRowGap);
        std::unique_ptr<MenuPopupButton> button(new MenuPopupButton());
        button->SetText(Txt(_T("弹出完整菜单"), _T("Show full menu")));
        button->SetBuilder(&BuildFullDemoMenu);
        row->AddChild(std::move(button), DuiLayout::Hint().Fixed(kMenuButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("弹出位置自动限制在工作区内"), _T("The popup position is clamped to the work area")),
               Txt(_T("TrackPopup 是同步调用，一直阻塞到用户选中某一项、按下 ESC 或者点到菜单外面为止，")
                   _T("返回值是被选中项的编号，没有选中任何项时返回 0。")
                   _T("传进去的坐标只是期望的左上角位置：菜单在显示之前会把落点限制在锚点所在显示器的")
                   _T("工作区内，右边放不下就翻向左边，下边放不下就翻向上边，")
                   _T("所以贴着屏幕边缘调用也能看到完整的菜单。这段运算是幂等的，")
                   _T("调用方自己算好的落点不会被再挪动一次。"),
                   _T("TrackPopup is synchronous: it blocks until an item is chosen, ESC is pressed or the ")
                   _T("user clicks outside, and it returns the id of the chosen item (0 when nothing was ")
                   _T("chosen). The coordinates you pass are only the preferred top-left corner — before ")
                   _T("showing, the popup clamps that point to the work area of the monitor the anchor is ")
                   _T("on, flipping to the left when it does not fit on the right and upwards when it does ")
                   _T("not fit below, so a menu opened against the edge of the screen is still fully ")
                   _T("visible. The clamp is idempotent, so a position the caller already worked out is ")
                   _T("left untouched.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 菜单栏
// =====================================================================

std::unique_ptr<DuiControl> Build_MenuBar()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

#if BUI_FEATURE_MENUBAR
    RebuildMenuBarMenus();

    AddSection(page.get(),
               Txt(_T("DuiMenuBar —— 文件 / 选项 / 查看"), _T("DuiMenuBar — File / Options / View")),
               Txt(_T("常驻在客户区里的一条菜单栏。点某一栏弹出它的下拉菜单，")
                   _T("按 Alt 加栏目名里带下划线的那个字母可以直接跳到对应栏目。")
                   _T("下拉菜单打开的状态下，把鼠标移到相邻的栏目上会自动切换过去，与 Windows 一致。"),
                   _T("A menu bar living in the client area. Click a column to drop its menu, or press Alt ")
                   _T("plus the underlined letter to jump straight to it. While a dropdown is open, moving ")
                   _T("the mouse onto a neighbouring column switches to it, the same way Windows does.")));
    {
        // 菜单栏随页面一起重建，而三张下拉菜单是随进程存活的，
        // 满足「下拉菜单必须保活到弹出流程结束」这一约定。
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiMenuBar> menuBar(new DuiMenuBar());
        menuBar->SetCtrlId(kIdMenuBarDemo);
        menuBar->AppendItem(kMenuBarIdFile, Txt(_T("文件(&F)"), _T("&File")), &s_menuBarFileMenu);
        menuBar->AppendItem(kMenuBarIdOptions, Txt(_T("选项(&O)"), _T("&Options")), &s_menuBarOptionsMenu);
        menuBar->AppendItem(kMenuBarIdView, Txt(_T("查看(&V)"), _T("&View")), &s_menuBarViewMenu);
        row->AddChild(std::move(menuBar), DuiLayout::Hint().Fixed(kMenuBarWidth));
        AddVariantRow(page.get(), std::move(row), kMenuBarRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("人工核对清单"), _T("Manual verification checklist")),
               Txt(_T("一、鼠标停在「文件」上时该栏底色变浅灰；")
                   _T("二、点开「文件」之后把鼠标移到「选项」或「查看」上会自动切换过去；")
                   _T("三、Alt+F、Alt+O、Alt+V 能直接跳到对应栏目；")
                   _T("四、按 ESC 关闭下拉菜单；")
                   _T("五、在 125% 与 150% 缩放下，栏目文字和助记符下划线依然清晰。"),
                   _T("(1) hovering over File tints that column light gray; (2) with File open, moving onto ")
                   _T("Options or View switches to it; (3) Alt+F, Alt+O and Alt+V jump straight to a ")
                   _T("column; (4) ESC closes the dropdown; (5) at 125% and 150% scaling the column text ")
                   _T("and the mnemonic underline are still crisp.")));
#else
    AddSection(page.get(),
               _T("DuiMenuBar"),
               Txt(_T("本次编译关闭了 BUI_FEATURE_MENUBAR，菜单栏控件没有参与编译，因此无法演示。"),
                   _T("BUI_FEATURE_MENUBAR is disabled in this build, so the menu bar control is not ")
                   _T("compiled in and cannot be demonstrated.")));
#endif // BUI_FEATURE_MENUBAR

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 本分组的页面列表
// =====================================================================

const PageEntry* GetListPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("list-box"),     _T("DuiListBox　列表框"),       _T("DuiListBox"),     &Build_ListBox,     true },
        { _T("virtual-list"), _T("DuiVirtualList　虚拟列表"), _T("DuiVirtualList"), &Build_VirtualList, true },
        { _T("tree-view"),    _T("DuiTreeView　树形列表"),    _T("DuiTreeView"),    &Build_TreeView,    true },
        { _T("tab"),          _T("DuiTab　标签栏"),           _T("DuiTab"),         &Build_Tab,         true },
        { _T("menu"),         _T("DuiMenu　菜单"),            _T("DuiMenu"),        &Build_Menu,        true },
        { _T("menu-bar"),     _T("DuiMenuBar　菜单栏"),       _T("DuiMenuBar"),     &Build_MenuBar,     true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
