/**
 *  画廊页面注册表的实现。本文件只做汇总，具体页面在各分组自己的
 *  Pages_*.cpp 里定义。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageRegistry.h"

namespace Gallery {

namespace {

// 分组表。顺序即导航树里从上到下的顺序，按"先看得见的控件、再看不见的
// 引擎、最后完整示例"排列，与 docs/guides.md 第 7 章的章节顺序一致。
//
// 每一项的页面数组在首次调用 GetPageGroups 时才向各分组索取，因为跨翻译
// 单元的静态对象初始化顺序没有保证，不能在静态初始化阶段就去读别的文件
// 里的数组。
struct GroupDef
{
    // 分组标识。
    LPCTSTR idName;
    // 中文分组名。
    LPCTSTR titleZh;
    // 英文分组名。
    LPCTSTR titleEn;
    // 取本分组页面列表的函数。
    const PageEntry* (*fetch)(int&);
};

const GroupDef g_groupDefs[] = {
    { _T("overview"), _T("总览"),       _T("Overview"),          &GetOverviewPages    },
    { _T("layout"),   _T("布局容器"),   _T("Layout"),            &GetLayoutPages      },
    { _T("basic"),    _T("基础控件"),   _T("Basics"),            &GetBasicPages       },
    { _T("input"),    _T("输入"),       _T("Input"),             &GetInputPages       },
    { _T("list"),     _T("列表与导航"), _T("Lists & Navigation"),&GetListPages        },
    { _T("feedback"), _T("反馈与浮层"), _T("Feedback & Popups"), &GetFeedbackPages    },
    { _T("media"),    _T("媒体"),       _T("Media"),             &GetMediaPages       },
    { _T("window"),   _T("窗口与宿主"), _T("Windows & Hosting"), &GetWindowPages      },
    { _T("engine"),   _T("引擎"),       _T("Engine"),            &GetEnginePages      },
    { _T("tools"),    _T("工具"),       _T("Tools"),             &GetToolPages        },
    { _T("samples"),  _T("完整示例"),   _T("Full Samples"),      &GetSamplePages      },
    // 文档配图夹具。整组的页面都不出现在导航里，只有命令行截图模式会遍历到。
    { _T("doccapture"), _T("文档配图"), _T("Doc Captures"),      &GetDocCapturePages  },
};

// 分组个数。
const int kGroupCount = (int)(sizeof(g_groupDefs) / sizeof(g_groupDefs[0]));

// 首次访问时组装出来的分组表。之后一直复用，指向的都是各翻译单元里的
// 静态数组，生命期与进程相同。
PageGroup* BuildGroups()
{
    static PageGroup s_groups[kGroupCount];
    static bool s_built = false;
    if (s_built)
    {
        return s_groups;
    }
    for (int i = 0; i < kGroupCount; ++i)
    {
        int count = 0;
        const PageEntry* pages = g_groupDefs[i].fetch(count);
        s_groups[i].idName = g_groupDefs[i].idName;
        s_groups[i].titleZh = g_groupDefs[i].titleZh;
        s_groups[i].titleEn = g_groupDefs[i].titleEn;
        s_groups[i].pages = pages;
        s_groups[i].pageCount = (pages != NULL) ? count : 0;
    }
    s_built = true;
    return s_groups;
}

} // 匿名命名空间

const PageGroup* GetPageGroups(int& outCount)
{
    outCount = kGroupCount;
    return BuildGroups();
}

const PageEntry* FindPageById(LPCTSTR idName)
{
    if (idName == NULL)
    {
        return NULL;
    }
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            if (_tcscmp(groups[g].pages[p].idName, idName) == 0)
            {
                return &groups[g].pages[p];
            }
        }
    }
    return NULL;
}

LPCTSTR GetDefaultPageId()
{
    // 启动后默认停在总览页，它会说明这个程序是什么、怎么用。
    return _T("about");
}

} // namespace Gallery
