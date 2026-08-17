/**
 *  画廊左侧导航栏的实现。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "GalleryNav.h"
#include "GalleryText.h"
#include "PageRegistry.h"
#include "DuiTheme.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// ---- 版式常量 ----

// 导航栏四边的内边距（像素）。
const int kNavPadding = 8;
// 搜索框与下方页面树之间的间距（像素）。
const int kNavGap = 8;
// 搜索框的高度（像素）。
const int kSearchHeight = 30;
// 页面树的行高（像素）。比控件默认的 28 略紧凑，让一屏能装下更多条目。
const int kTreeRowHeight = 26;
// 页面树每一级的缩进（像素）。
const int kTreeIndent = 14;

// 分组节点的 LPARAM 取值。分组本身不是页面，用它与页面节点区分开。
const LPARAM kGroupNodeParam = -1;

// 页面树的过滤谓词。
//
// 树的过滤有一处必须小心的地方：被过滤掉的节点会连同它的整棵子树一起隐藏。
// 因此谓词不能只判断"这个节点自己匹配吗"—— 分组标题通常不匹配搜索词，
// 一旦返回假，它底下所有匹配的页面也会跟着消失。这里的做法是先把需要保留的
// 节点编号（匹配的页面，以及它们各自的分组）全部算进一个集合，谓词只做查表。
struct NavFilterPredicate
{
    // 要保留的节点编号集合。不持有所有权，指向导航栏的成员变量，
    // 生命期与导航栏相同。
    const std::set<int>* keep;

    bool operator()(int nodeId) const
    {
        if (keep == NULL)
        {
            return true;
        }
        return keep->find(nodeId) != keep->end();
    }
};

} // 匿名命名空间

GalleryNav::GalleryNav()
    : m_pSearch(NULL)
    , m_pTreeScroll(NULL)
    , m_pTree(NULL)
{
    SetPadding(kNavPadding);
    SetGap(kNavGap);
    // 底色从主题现取，与页面版式保持一致。切换主题预设之后需要重建导航才会
    // 生效，宿主窗口已经这样做了。
    SetBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceBg));
}

void GalleryNav::BuildContents()
{
    // 内部控件只建一次。语言切换等需要刷新内容的场合走 RebuildTree，
    // 它只重建树的节点，不动搜索框与滚动视图。
    if (m_pTree != NULL)
    {
        return;
    }

    std::unique_ptr<DuiSearchBox> search(new DuiSearchBox());
    search->SetCtrlId(kIdNavSearch);
    search->SetPlaceholder(Txt(_T("搜索页面"), _T("Search pages")));
    m_pSearch = search.get();
    AddChild(std::move(search), DuiLayout::Hint().Fixed(kSearchHeight));

    std::unique_ptr<DuiScrollView> scroll(new DuiScrollView());
    scroll->SetCtrlId(kIdNavTreeScroll);
    m_pTreeScroll = scroll.get();

    std::unique_ptr<DuiTreeView> tree(new DuiTreeView());
    tree->SetCtrlId(kIdNavTree);
    tree->SetRowHeight(kTreeRowHeight);
    tree->SetIndentPx(kTreeIndent);
    // 树控件默认用自己那套浅色配色，并且会把行背景整片填满，所以不给它换色
    // 的话，切到深色主题时整个左栏仍是亮的、与其余部分割裂。这里逐项换成
    // 主题里的对应颜色，让导航栏跟着主题走。
    tree->SetRowBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceBg));
    tree->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextDefault));
    tree->SetSelTextColor(DuiTheme::Inst().Get(DuiTheme::TextOnRowSel));
    tree->SetRowSelColor(DuiTheme::Inst().Get(DuiTheme::RowSel));
    tree->SetRowHoverColor(DuiTheme::Inst().Get(DuiTheme::RowHover));
    tree->SetGlyphColor(DuiTheme::Inst().Get(DuiTheme::TextSubtle));
    m_pTree = tree.get();
    m_pTreeScroll->SetContent(std::move(tree));

    AddChild(std::move(scroll), DuiLayout::Hint().Weight(1));

    RebuildTree();
}

void GalleryNav::RebuildTree()
{
    if (m_pTree == NULL)
    {
        return;
    }

    // 节点编号是自增的，清空之后新树的编号与旧树不同，所以页面表要一起重建。
    m_pTree->Clear();
    m_pages.clear();
    m_filterKeep.clear();

    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    for (int g = 0; g < groupCount; ++g)
    {
        const PageGroup& group = groups[g];

        // 先数一下这个分组里有几个页面要出现在导航中。一个都没有的分组
        // （例如只装了文档配图夹具的那一组）连分组标题都不要建出来。
        int visibleCount = 0;
        for (int p = 0; p < group.pageCount; ++p)
        {
            if (group.pages[p].showInNav)
            {
                ++visibleCount;
            }
        }
        if (visibleCount == 0)
        {
            continue;
        }

        LPCTSTR groupTitle = Txt(group.titleZh, group.titleEn);
        int groupNodeId = m_pTree->AddRoot(groupTitle, NULL, kGroupNodeParam);
        // 分组标题不是页面，点它只应当展开或折叠，不应当变成"当前选中项"。
        m_pTree->SetItemSelectable(groupNodeId, false);

        for (int p = 0; p < group.pageCount; ++p)
        {
            const PageEntry& page = group.pages[p];
            if (!page.showInNav)
            {
                continue;
            }
            NavPage navPage;
            navPage.idName = page.idName;
            navPage.titleZh = page.titleZh;
            navPage.titleEn = page.titleEn;
            navPage.groupNodeId = groupNodeId;
            // 先占位，节点建出来之后再回填。
            navPage.nodeId = -1;

            int pageIndex = (int)m_pages.size();
            LPCTSTR pageTitle = Txt(page.titleZh, page.titleEn);
            int nodeId = m_pTree->AddChild(groupNodeId, pageTitle, NULL, (LPARAM)pageIndex);
            navPage.nodeId = nodeId;
            m_pages.push_back(navPage);
        }
    }

    // 重建之后把此前选中的页面重新选上。页面标识不随重建改变，所以这一步
    // 在语言切换时能让选中项原地不动。
    CString wanted = m_selectedPageId;
    if (wanted.IsEmpty())
    {
        wanted = GetDefaultPageId();
    }
    SelectPage(wanted);

    ApplyFilter();
}

bool GalleryNav::HandleNotify(const DuiNotify* pNotify)
{
    if (pNotify == NULL || m_pTree == NULL)
    {
        return false;
    }

    // 页面树的选中项发生变化。分组标题被设成不可选中，所以走到这里的一定是
    // 页面节点，但仍然按 LPARAM 判一次，避免今后加了别的节点类型时误判。
    if (pNotify->code == DUIN_VALUECHANGED && pNotify->ctrlId == kIdNavTree)
    {
        int nodeId = (int)pNotify->extra;
        LPARAM param = m_pTree->GetItemParam(nodeId);
        if (param == kGroupNodeParam)
        {
            return false;
        }
        int pageIndex = (int)param;
        if (pageIndex < 0 || pageIndex >= (int)m_pages.size())
        {
            return false;
        }
        CString newPageId = m_pages[pageIndex].idName;
        if (newPageId == m_selectedPageId)
        {
            return false;
        }
        m_selectedPageId = newPageId;
        return true;
    }

    // 搜索框内容变化，重新过滤。选中的页面不因过滤而改变，所以返回假。
    if (pNotify->code == DUIN_VALUECHANGED && pNotify->ctrlId == kIdNavSearch)
    {
        if (m_pSearch != NULL)
        {
            m_keyword = m_pSearch->GetText();
            m_keyword.MakeLower();
            m_keyword.Trim();
        }
        ApplyFilter();
        return false;
    }

    // 分组展开或折叠，可见行数变了，滚动范围要跟着更新。
    if (pNotify->code == (UINT)DuiTreeView::DUITVN_EXPAND_TOGGLED
        && pNotify->ctrlId == kIdNavTree)
    {
        SyncTreeScrollRange();
        return false;
    }

    return false;
}

CString GalleryNav::GetSelectedPageId() const
{
    return m_selectedPageId;
}

void GalleryNav::SelectPage(LPCTSTR pageId)
{
    if (pageId == NULL || m_pTree == NULL)
    {
        return;
    }
    for (size_t i = 0; i < m_pages.size(); ++i)
    {
        if (_tcscmp(m_pages[i].idName, pageId) == 0)
        {
            m_selectedPageId = pageId;
            // 这里不要让树发通知：选中动作是调用方发起的，它自己知道选了哪一页，
            // 再回传一次通知只会绕一圈又回到调用方。
            m_pTree->SetCurSel(m_pages[i].nodeId, false);
            return;
        }
    }
}

bool NavMatchesKeyword(LPCTSTR idName, LPCTSTR titleZh, LPCTSTR titleEn,
                       LPCTSTR lowerKeyword)
{
    if (lowerKeyword == NULL || lowerKeyword[0] == _T('\0'))
    {
        return true;
    }

    CString haystack;
    if (idName != NULL)
    {
        haystack += idName;
    }
    haystack += _T(" ");
    if (titleZh != NULL)
    {
        haystack += titleZh;
    }
    haystack += _T(" ");
    if (titleEn != NULL)
    {
        haystack += titleEn;
    }
    haystack.MakeLower();

    return haystack.Find(lowerKeyword) >= 0;
}

bool GalleryNav::MatchesKeyword(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= (int)m_pages.size())
    {
        return false;
    }
    const NavPage& page = m_pages[pageIndex];
    return NavMatchesKeyword(page.idName, page.titleZh, page.titleEn, m_keyword);
}

void GalleryNav::ApplyFilter()
{
    if (m_pTree == NULL)
    {
        return;
    }

    if (m_keyword.IsEmpty())
    {
        m_pTree->ClearFilter();
        SyncTreeScrollRange();
        return;
    }

    m_filterKeep.clear();
    for (size_t i = 0; i < m_pages.size(); ++i)
    {
        if (!MatchesKeyword((int)i))
        {
            continue;
        }
        m_filterKeep.insert(m_pages[i].nodeId);
        // 分组标题本身通常不匹配搜索词，但它必须一并保留 —— 分组被隐藏时
        // 它底下匹配的页面会跟着一起消失。
        m_filterKeep.insert(m_pages[i].groupNodeId);
    }

    NavFilterPredicate predicate;
    predicate.keep = &m_filterKeep;
    m_pTree->SetFilter(predicate);
    // 过滤之后被折叠起来的分组仍然看不见内容，所以搜索时把所有分组展开。
    m_pTree->ExpandAll();
    SyncTreeScrollRange();
}

void GalleryNav::SyncTreeScrollRange()
{
    if (m_pTree == NULL || m_pTreeScroll == NULL)
    {
        return;
    }
    // 树控件没有覆写期望尺寸，滚动视图的自动内容高度对它不起作用，只能
    // 每次内容变化后把行数乘行高算出来的总高显式告诉滚动视图。
    m_pTreeScroll->SetContentHeight(m_pTree->GetContentHeight());
}

} // namespace Gallery
