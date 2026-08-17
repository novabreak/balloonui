/**
 *  画廊的总览页。启动后默认停在这一页，用来回答三个问题：这个程序是什么、
 *  怎么用它、看完之后还能去哪里找更详细的资料。
 *
 *  重构之前画廊没有这样一页 —— 启动直接落在按钮页上，既不介绍 balloonui
 *  本身，也没有任何地方提到仓库里还有十来个独立的示例工程。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Basic/DuiLabel.h"
#include "Controls/Layout/DuiLayout.h"
#include "DuiTheme.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// 条目文字的行高（像素）。
const int kEntryRowHeight = 22;
// 条目左侧名称栏的宽度（像素）。要装得下最长的那个示例工程名。
const int kEntryNameWidth = 210;
// 统计数字那一行的高度（像素）。
const int kStatRowHeight = 26;

// 往当前段落里添加一行"名称 + 说明"的条目。
//   page：页面容器。
//   name：左侧的名称，通常是工程名或功能名。
//   desc：右侧的说明文字。
void AddEntryRow(GalleryPageBox* page, LPCTSTR name, LPCTSTR desc)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());
    row->SetGap(10);

    std::unique_ptr<DuiLabel> nameLabel(new DuiLabel());
    nameLabel->SetText(name);
    nameLabel->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextDefault));
    row->AddChild(std::move(nameLabel), DuiLayout::Hint().Fixed(kEntryNameWidth));

    std::unique_ptr<DuiLabel> descLabel(new DuiLabel());
    descLabel->SetText(desc);
    descLabel->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextSubtle));
    row->AddChild(std::move(descLabel), DuiLayout::Hint().Weight(1));

    AddVariantRow(page, std::move(row), kEntryRowHeight);
}

} // 匿名命名空间

// ===== 总览 ===========================================================

std::unique_ptr<DuiControl> Build_About()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("balloonui 是什么"), _T("What is balloonui")),
               Txt(_T("balloonui 是一套 Windows 平台的自绘界面控件库。整棵控件树只占用一个真正的窗口句柄，")
                   _T("所有控件都由库自己绘制，因此不受系统主题限制，也不会因为控件数量多而耗尽窗口资源。")
                   _T("它同时被这个仓库里的即时通讯客户端与管理端使用。"),
                   _T("balloonui is a self-drawn UI control library for Windows. An entire control tree lives ")
                   _T("inside a single real window handle and every control paints itself, so the look is not ")
                   _T("constrained by the system theme and a dense UI does not exhaust window resources. ")
                   _T("It is shared by the chat client and the admin tool in this repository.")));

    AddSection(page.get(),
               Txt(_T("怎么用这个画廊"), _T("How to use this gallery")),
               Txt(_T("左侧是按功能分组的页面列表，点开分组即可看到里面的控件。"),
                   _T("The left pane lists every page grouped by purpose. Expand a group to see its controls.")));
    AddEntryRow(page.get(),
                Txt(_T("搜索框"), _T("Search box")),
                Txt(_T("输入关键词即时过滤页面列表。中文名、英文名、内部标识都参与匹配。"),
                    _T("Type to filter the page list. Chinese names, English names and internal ids all match.")));
    AddEntryRow(page.get(),
                Txt(_T("语言按钮"), _T("Language button")),
                Txt(_T("右上角。按钮上写的是点击之后会切换到哪种语言。"),
                    _T("Top right. The button label names the language you will get if you click it.")));
    AddEntryRow(page.get(),
                Txt(_T("主题按钮"), _T("Theme buttons")),
                Txt(_T("右上角三档预设。切换之后只有读取主题的控件会变色，详见引擎分组的主题页。"),
                    _T("Three presets, top right. Only theme-aware controls change; see the Theme page.")));
    AddEntryRow(page.get(),
                Txt(_T("分隔条"), _T("Splitter")),
                Txt(_T("左右两栏之间可以拖动，调整导航栏的宽度。"),
                    _T("Drag the divider between the two panes to resize the navigation pane.")));

    // 统计数字直接从注册表算，加了页面之后这里自动跟着变，不会说谎。
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    int visibleGroups = 0;
    int visiblePages = 0;
    for (int g = 0; g < groupCount; ++g)
    {
        int inThisGroup = 0;
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            if (groups[g].pages[p].showInNav)
            {
                ++inThisGroup;
            }
        }
        if (inThisGroup > 0)
        {
            ++visibleGroups;
            visiblePages += inThisGroup;
        }
    }

    AddSection(page.get(),
               Txt(_T("这个画廊有多少内容"), _T("How much is in here")),
               Txt(_T("下面的数字是运行时从页面注册表数出来的，新增页面之后会自动更新。"),
                   _T("The numbers below are counted from the page registry at run time.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(10);

        CString stat;
        stat.Format(Txt(_T("共 %d 个分组、%d 个演示页面。"),
                        _T("%d groups, %d demo pages.")),
                    visibleGroups, visiblePages);

        std::unique_ptr<DuiLabel> statLabel(new DuiLabel());
        statLabel->SetText(stat);
        statLabel->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextDefault));
        row->AddChild(std::move(statLabel), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), kStatRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("更详细的资料"), _T("Further reading")),
               Txt(_T("画廊演示的是实际效果，接口的完整说明在文档里。"),
                   _T("This gallery shows what things look like; the full API reference lives in the docs.")));
    AddEntryRow(page.get(),
                _T("docs/guides.md"),
                Txt(_T("使用文档正篇：控件清单、XML 布局、事件路由、自绘控件、完整布局示例。"),
                    _T("Main guide: control reference, XML layout, event routing, custom controls, samples.")));
    AddEntryRow(page.get(),
                _T("docs/guides_en.md"),
                Txt(_T("上面那份文档的英文版。"), _T("English translation of the guide above.")));
    AddEntryRow(page.get(),
                _T("balloonui/BalloonUiFeatures.h"),
                Txt(_T("按需裁剪开关：只编译用到的控件，减小最终可执行文件的体积。"),
                    _T("Feature strip switches: compile only the controls you use to shrink the binary.")));
    AddEntryRow(page.get(),
                _T("balloonui/Tests/"),
                Txt(_T("库自己的单元测试。画廊启动时会全部跑一遍，结果写到临时目录下的日志文件。"),
                    _T("The library's own unit tests. The gallery runs them all at startup and logs the result.")));

    AddSection(page.get(),
               Txt(_T("仓库里的独立示例工程"), _T("Standalone sample projects")),
               Txt(_T("除了这个画廊，仓库里还有若干个可以单独编译运行的示例程序。")
                   _T("它们演示的是把控件拼成一个完整应用，而不是单个控件的用法，")
                   _T("都收在 Demos.sln 里。"),
                   _T("Besides this gallery the repository contains several standalone sample applications ")
                   _T("that show how controls compose into a complete program rather than how one control ")
                   _T("behaves. All of them are in Demos.sln.")));
    AddEntryRow(page.get(), _T("XChat"),
                Txt(_T("聊天程序外壳：登录窗与主窗。"), _T("Chat application shell: login and main window.")));
    AddEntryRow(page.get(), _T("NewChatDemo"),
                Txt(_T("聊天界面的另一版实现。"), _T("An alternative chat interface implementation.")));
    AddEntryRow(page.get(), _T("CloudMelodyDesktop"),
                Txt(_T("音乐播放器界面。"), _T("A music player interface.")));
    AddEntryRow(page.get(), _T("DemoTaskManager"),
                Txt(_T("任务管理器界面，重点是多列树控件与大量数据。"),
                    _T("Task manager UI: multi-column tree view with a lot of rows.")));
    AddEntryRow(page.get(), _T("DemoTreeViewLargeData"),
                Txt(_T("树控件的大数据量表现。"), _T("Tree view behaviour with a large data set.")));
    AddEntryRow(page.get(), _T("DemoNinePatchBg"),
                Txt(_T("九宫格背景图，可以拖动窗口边角观察不变形的效果。"),
                    _T("Nine-patch window background; drag the corners to see it stay undistorted.")));
    AddEntryRow(page.get(), _T("DemoChatBubble"),
                Txt(_T("自绘控件示例：聊天气泡。"), _T("Custom control sample: chat bubble.")));
    AddEntryRow(page.get(), _T("DemoCircularProgress"),
                Txt(_T("自绘控件示例：环形进度。"), _T("Custom control sample: circular progress.")));
    AddEntryRow(page.get(), _T("DemoFileTypeIcon"),
                Txt(_T("自绘控件示例：文件类型图标。"), _T("Custom control sample: file type icon.")));
    AddEntryRow(page.get(), _T("DemoTextBadgeTile"),
                Txt(_T("自绘控件示例：带徽标的文字瓦片。"), _T("Custom control sample: text tile with a badge.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ===============================================

const PageEntry* GetOverviewPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("about"), _T("关于 balloonui"), _T("About balloonui"), &Build_About, true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
