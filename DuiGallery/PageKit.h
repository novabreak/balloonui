/**
 *  画廊页面的公共构建工具。每一个演示页面都由本文件提供的这几个函数拼出来，
 *  版式（页边距、卡片外观、标题与说明的字号和颜色、演示行的高度）集中在这里
 *  定义，改一处全部页面同时生效。
 *
 *  一个页面的典型写法：
 *
 *      std::unique_ptr<DuiControl> Build_Button()
 *      {
 *          std::unique_ptr<GalleryPageBox> page = NewPage();
 *
 *          // AddSection 会新开一张卡片，此后的演示行都落在这张卡片里，
 *          // 直到下一次 AddSection 再开一张。
 *          AddSection(page.get(),
 *                     Txt(_T("主操作按钮"), _T("PushButton")),
 *                     Txt(_T("品牌蓝圆角按钮，悬停加深，禁用置灰。"),
 *                         _T("Brand-blue rounded primary action.")));
 *          {
 *              std::unique_ptr<DuiHBox> row(new DuiHBox());
 *              row->SetGap(12);
 *              // ... 往 row 里塞演示控件 ...
 *              AddVariantRow(page.get(), std::move(row));
 *          }
 *
 *          return std::unique_ptr<DuiControl>(page.release());
 *      }
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <atlstr.h>

#include "DuiControl.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Window/DuiScrollBar.h"

#include "GalleryText.h"

namespace Gallery {

// =====================================================================
// 版式常量
// =====================================================================

// 页面四边的内边距（像素）。
const int kPageMargin = 20;
// 段落标题的行高（像素）。标题是单行，不参与自动换行。
const int kHeaderH = 26;
// 演示行的默认高度（像素）。控件较高的段落可以在 AddVariantRow 里单独指定。
const int kRowH = 36;
// 段落卡片之间的竖直间距（像素）。由页面容器的 gap 统一控制，段落之间
// 不需要再手工插入空白控件。
const int kSectionGap = 14;
// 卡片内部的四边内边距（像素）。
const int kCardPadding = 16;
// 卡片圆角半径（像素）。
const int kCardRadius = 6;

// =====================================================================
// 页面容器
// =====================================================================

// 画廊页面的根容器。
//
// 它在竖直布局的基础上多做两件事：
//
// 一、**按当前宽度测量说明文字的高度。** 段落说明是可以换到多行的文本，
//     它需要多高完全取决于它有多宽，而宽度要等到排列时才知道。本类在
//     Layout 的开头按这一次的可用宽度把每条说明重新测一遍，把结果写回
//     它们各自的 layout 提示，然后才交给基类排列。因此窗口宽度变化时
//     说明文字的高度立刻就是对的，不会慢一次排列才纠正。
//
// 二、**把段落包进卡片。** AddSection 每调用一次就新建一张卡片容器，
//     此后添加的演示行都落在这张卡片里。卡片用的是 DuiVBox 自带的
//     底色 / 圆角 / 描边能力，没有自绘代码。
//
// 页面容器通常挂在一个 DuiScrollView 下。它按父控件的矩形推算自己这一次
// 能拿到多宽，因此不需要调用方做任何额外的关联工作。
class GalleryPageBox : public balloonwjui::DuiVBox
{
public:
    GalleryPageBox();

    // 切换到无卡片模式。
    //   plain：真表示段落不再各自包一张卡片，标题、说明与演示行直接排在页面上，
    //          页面也不画背景色。
    // 这个模式是给文档配图夹具用的：那一页的每个段落恰好一行，命令行截图模式
    // 按演示行的矩形裁图。包上卡片会让每张图窄 32 像素、空白处底色由灰变白，
    // 与已经发布在文档里的配图对不上。必须在往页面里添加任何内容之前调用。
    void SetPlainMode(bool plain);

    // 新开一张段落卡片，后续的演示行都添加到这张卡片里。
    // 无卡片模式下不新建容器，直接返回页面自身。
    //   返回：新建的卡片容器，所有权归本页面容器，调用方只借用指针。
    // 一般不直接调用，而是经由 AddSection 间接调用。
    balloonwjui::DuiVBox* BeginCard();

    // 取当前正在填充的卡片；一张都还没开时自动开一张。
    balloonwjui::DuiVBox* CurrentCard();

    // 登记一条高度随宽度变化的说明文字。
    //   pCard：这条说明所在的卡片容器。测量完成后要把高度写回它保存的
    //          布局提示里，所以必须一并登记。不持有所有权。
    //   pLabel：说明标签。所有权归它所在的卡片，本容器只记下裸指针用于
    //           测量，页面销毁时一并失效。
    // 一般不直接调用，而是经由 AddSection 间接调用。
    void RegisterWrapLabel(balloonwjui::DuiVBox* pCard, balloonwjui::DuiLabel* pLabel);

    // 报告本页面需要多高。供 DuiScrollView 的自动内容高度使用。
    SIZE GetDesiredSize() const override;

    // 取本页面直接子控件的个数。
    // 返回：直接子控件个数。带卡片时等于段落数，无卡片模式下等于标题、
    //       说明与演示行的总数。
    // 这个访问器只给单元测试用来核对段落有没有被正确地包进卡片 ——
    // DuiControl 没有公开子控件个数，而这正是版式是否成立的判据。
    int GetChildCountForTests() const;

protected:
    // 排列子控件。开头先按本次的可用宽度重新测量所有说明文字。
    void Layout(const RECT& rcAvail) override;

private:
    // 按给定的卡片内容宽度重新测量所有说明文字，并把结果写回它们的
    // layout 提示。
    //   cardContentWidth：卡片内部可供文字使用的宽度（像素）。小于 1 时
    //                     本函数直接返回，不做任何改动。
    void MeasureWrapLabels(int cardContentWidth) const;

    // 推算本页面这一次能拿到多宽。
    // 返回：页面容器自身的宽度（像素）；无法推算时返回 0。
    int ResolveOwnWidth() const;

    // 由页面宽度算出说明文字实际能用多宽。
    //   pageWidth：页面容器的宽度（像素）。
    // 返回：说明文字可用的宽度（像素）。
    int TextWidthFromPageWidth(int pageWidth) const;

    // 一条需要自动换行的说明文字，以及它所在的卡片。
    struct WrapLabelEntry
    {
        // 说明所在的卡片容器。测量出来的高度写回它保存的布局提示。
        balloonwjui::DuiVBox* card;
        // 说明标签本身。
        balloonwjui::DuiLabel* label;
    };

    // 当前正在填充的卡片。所有权在基类的子控件列表里，这里只记指针。
    // 为空表示还没有开过卡片。
    balloonwjui::DuiVBox* m_pCurrentCard;
    // 全部需要自动换行的说明标签。所有权在各自的卡片里，这里只记指针。
    std::vector<WrapLabelEntry> m_wrapLabels;
    // 上一次测量说明文字时使用的卡片内容宽度（像素）。宽度没有变化时
    // 跳过重复测量，避免每次排列都重算一遍文本高度。
    mutable int m_lastMeasuredWidth;
    // 是否处于无卡片模式。见 SetPlainMode 的说明。
    bool m_plainMode;
};

// =====================================================================
// 页面构建函数
// =====================================================================

// 新建一个空页面。
// 返回：页面容器，所有权交给调用方。
std::unique_ptr<GalleryPageBox> NewPage();

// 新建一个无卡片的空页面，供文档配图夹具使用。
// 返回：页面容器，所有权交给调用方。原因见 GalleryPageBox::SetPlainMode。
std::unique_ptr<GalleryPageBox> NewPlainPage();

// 新开一个段落：一张卡片，卡片里先放标题，再放说明。
//   page：页面容器，不能为空。
//   title：段落标题，单行，通常写控件名或能力名。
//   desc：段落说明，可以很长，会按当前宽度自动换到多行。允许传空指针或
//         空串表示这一段不需要说明。
// 调用之后，后续的 AddVariantRow / AddGap 都会落在这张新卡片里。
void AddSection(GalleryPageBox* page, LPCTSTR title, LPCTSTR desc);

// 往当前段落的卡片里添加一行演示控件。
//   page：页面容器，不能为空。
//   row：预先拼好的水平容器，所有权转移给卡片。
//   rowH：这一行的高度（像素），默认 kRowH。控件较高时自行加大。
// 返回：刚添加的这一行的容器指针，所有权归卡片，调用方只借用。截图标记
//       需要它作为锚点。
balloonwjui::DuiControl* AddVariantRow(GalleryPageBox* page,
                                       std::unique_ptr<balloonwjui::DuiHBox> row,
                                       int rowH = kRowH);

// 与 AddVariantRow 相同，另外登记一个文档截图标记。
//   captureName：截图文件名的主干部分，最终生成 ctl-<captureName>.png。
// 命令行的 --capture-all 模式会遍历这些标记逐个截图。
void AddVariantRowCapture(GalleryPageBox* page,
                          LPCTSTR captureName,
                          std::unique_ptr<balloonwjui::DuiHBox> row,
                          int rowH = kRowH);

// 在当前段落的卡片里插入一段竖直空白。
//   h：空白高度（像素）。
// 用于同一段落内两组演示行之间的分隔。**段落与段落之间不需要调用本函数**，
// 卡片之间的间距由页面容器统一控制。
void AddGap(GalleryPageBox* page, int h);

// =====================================================================
// 文档截图标记
// =====================================================================

// 一个文档截图标记。
struct CaptureMark
{
    // 截图文件名的主干部分。
    CString name;
    // 被截取的控件。不持有所有权；页面被销毁后即失效，因此标记必须在
    // 切换到下一个页面之前消费掉。
    balloonwjui::DuiControl* anchor;
};

// 取全部已登记的截图标记。命令行截图模式在每建完一个页面之后读取并清空。
std::vector<CaptureMark>& GetCaptureMarks();

// 登记一个截图标记。
//   name：截图文件名的主干部分，为空时本次登记被忽略。
//   anchor：被截取的控件，为空时本次登记被忽略。
void RegisterCapture(LPCTSTR name, balloonwjui::DuiControl* anchor);

// =====================================================================
// 演示用的小控件
// =====================================================================

// 点击时调用一个函数对象的按钮。
//
// 画廊里大量段落需要"点一下看看效果"，但画廊窗口的通知分发只按控件编号
// 路由，给每个演示按钮都分配编号并在窗口里加一条分支既繁琐又容易撞号。
// 本类把响应就地放在按钮自己身上，页面构建代码写完即生效。
//
// 按下状态用自己的成员记录，因为基类的同名状态是私有的。
class FnButton : public balloonwjui::DuiButton
{
public:
    // 点击后被调用的函数对象。为空表示点击不做任何事。参数是被点击的
    // 按钮自身，便于同一份处理逻辑服务多个按钮。
    std::function<void(FnButton*)> onClick;

    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;

private:
    // 本按钮上是否正处于按下状态。基类的按下状态不对子类开放，所以这里
    // 自己记一份，用来判断抬起时是否构成一次完整的点击。
    bool m_localPressed = false;
};

// 当前页面注册的通知钩子。
//
// 少数演示段落需要监听控件通知来实时更新自己的显示（例如树控件的悬停
// 演示要把当前悬停的节点名写到一个标签上）。页面在构建时按需给这个钩子
// 赋值，画廊窗口在处理完自己的通知之后转发给它。切换页面时会被清空，
// 因为钩子里持有的控件指针会随旧页面一起失效。
extern std::function<void(const balloonwjui::DuiNotify*)> g_pageNotifyHook;

} // namespace Gallery
