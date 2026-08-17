/**
 *  画廊的左侧导航栏：顶部一个搜索框，下面一棵按分组展开的页面树。
 *
 *  它取代了原先顶部那条横向标签条 —— 32 个页面平铺在一条标签条里，窗口
 *  1500 像素宽也只显示得下 19 个，其余要靠两端的箭头一格一格滚，而且一旦
 *  滚动，当前选中的标签就滚出了可视范围。改成分组树之后，11 个分组一屏
 *  全部可见，当前位置也始终看得见。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <set>
#include <vector>
#include <atlstr.h>

#include "Controls/Layout/DuiLayout.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/Input/DuiSearchBox.h"
#include "Controls/Window/DuiScrollBar.h"
#include "DuiNotify.h"

namespace Gallery {

// 导航栏内部各控件的编号。
//
// balloonui 的通知码是按控件各自编号的，搜索框、树、滚动条发出来的都是同一个
// DUIN_VALUECHANGED，只能靠控件编号区分，所以每个都必须显式编号、不能留成 0。
enum NavCtrlId
{
    // 顶部的搜索框。
    kIdNavSearch = 101,
    // 页面树。
    kIdNavTree = 102,
    // 包住页面树的滚动视图。
    kIdNavTreeScroll = 103,
};

// 判断一个页面是否匹配搜索关键词。
//   idName：页面的稳定标识。
//   titleZh：页面的中文标题。
//   titleEn：页面的英文标题。
//   lowerKeyword：**已经转成小写**的搜索关键词；空串表示不过滤，一律返回真。
// 返回：真表示该页面应当保留在过滤结果里。
//
// 三样东西都参与匹配，所以无论使用者当前切到哪种语言，输入 tree 或者"树"
// 都能找到同一页。抽成自由函数是为了能单独写用例 —— 它是整个搜索行为里
// 唯一有判断逻辑的地方。
bool NavMatchesKeyword(LPCTSTR idName, LPCTSTR titleZh, LPCTSTR titleEn,
                       LPCTSTR lowerKeyword);

// 画廊左侧的导航栏。
//
// 它自己就是一个竖直布局容器，构造之后调一次 BuildContents 建出内部控件。
// 宿主窗口收到通知时转交给 HandleNotify，返回真表示当前选中的页面发生了
// 变化，宿主据此重建右侧内容区。
class GalleryNav : public balloonwjui::DuiVBox
{
public:
    GalleryNav();

    // 建出内部控件（搜索框、页面树及其滚动视图），并按当前语言填好树的内容。
    // 只需要调用一次，重复调用直接返回。此后要刷新内容走 RebuildTree。
    void BuildContents();

    // 按当前语言重建整棵树。语言切换之后调用。
    // 重建会保留当前选中的页面：树节点编号会变，但页面标识不变。
    void RebuildTree();

    // 处理一条界面通知。
    //   pNotify：通知结构，为空时直接返回假。
    // 返回：真表示当前选中的页面发生了变化，调用方应当读取 GetSelectedPageId()
    //       并重建内容区；假表示这条通知与导航栏无关，或者虽然与导航栏有关
    //       （例如搜索框内容变化）但选中的页面没变。
    bool HandleNotify(const balloonwjui::DuiNotify* pNotify);

    // 取当前选中的页面标识。没有任何页面被选中时返回空串。
    CString GetSelectedPageId() const;

    // 选中指定页面。
    //   pageId：页面标识，即 PageEntry::idName。找不到时本函数不做任何事。
    // 本函数不会触发选中变化通知，调用方自己知道选了哪一页。
    void SelectPage(LPCTSTR pageId);

private:
    // 按当前的搜索关键词重新计算要保留哪些节点，并应用到树上。
    // 关键词为空时清除过滤。
    void ApplyFilter();

    // 判断一个页面是否匹配当前搜索关键词。
    //   pageIndex：页面在 m_pageIds 里的下标。
    // 返回：真表示匹配。关键词为空时一律返回真。
    bool MatchesKeyword(int pageIndex) const;

    // 把树的内容高度同步给它外面的滚动视图。
    // 树控件自己不报告期望尺寸，也不处理滚轮（单列模式下它把滚轮事件让给
    // 外层容器），所以每次增删节点、展开折叠、改过滤条件之后都要调一次。
    void SyncTreeScrollRange();

    // 一个页面节点的信息。
    struct NavPage
    {
        // 页面标识，指向注册表里的静态字符串，不持有所有权。
        LPCTSTR idName;
        // 中文标题，指向注册表里的静态字符串。搜索时一并参与匹配。
        LPCTSTR titleZh;
        // 英文标题，指向注册表里的静态字符串。搜索时一并参与匹配。
        LPCTSTR titleEn;
        // 这个页面在树里对应的节点编号。重建树时会变。
        int nodeId;
        // 这个页面所属分组在树里的节点编号。过滤时要连分组一起保留，
        // 否则分组被隐藏会把底下匹配的页面一并带走。
        int groupNodeId;
    };

    // 顶部的搜索框。所有权在基类的子控件列表里，这里只记指针。
    balloonwjui::DuiSearchBox* m_pSearch;
    // 包住页面树的滚动视图。所有权同上。
    balloonwjui::DuiScrollView* m_pTreeScroll;
    // 页面树。所有权在滚动视图里，这里只记指针。
    balloonwjui::DuiTreeView* m_pTree;
    // 全部页面节点。下标即节点的 LPARAM 值，用来从节点反查页面。
    std::vector<NavPage> m_pages;
    // 当前的搜索关键词，已转成小写。空串表示不过滤。
    CString m_keyword;
    // 当前选中的页面标识。空串表示没有选中任何页面。
    CString m_selectedPageId;
    // 过滤时要保留的节点编号集合。树的过滤谓词直接查这个集合。
    // 它必须是成员而不是局部变量，因为谓词会被树长期持有。
    std::set<int> m_filterKeep;
};

} // namespace Gallery
