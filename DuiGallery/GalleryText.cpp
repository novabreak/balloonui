/**
 *  画廊界面文案中英文切换的实现。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "GalleryText.h"

namespace Gallery {

namespace {

// 当前生效的语言。画廊只在界面线程里读写它，因此不需要加锁。
Language g_currentLanguage = LangChinese;

// 两份文案都缺失时返回的空串。返回它而不是空指针，免得调用方每次都要判空。
const TCHAR g_emptyText[] = _T("");

} // 匿名命名空间

Language CurrentLanguage()
{
    return g_currentLanguage;
}

void SetCurrentLanguage(Language lang)
{
    g_currentLanguage = lang;
}

LPCTSTR Txt(LPCTSTR zh, LPCTSTR en)
{
    // 两份都没有给时返回空串，绝不返回空指针。
    if (zh == NULL && en == NULL)
    {
        return g_emptyText;
    }
    // 只给了一份时不论当前是哪种语言都用它。这条分支主要服务于那些本身
    // 就与语言无关的文字，例如控件类名、代码片段。
    if (zh == NULL)
    {
        return en;
    }
    if (en == NULL)
    {
        return zh;
    }
    return (g_currentLanguage == LangEnglish) ? en : zh;
}

} // namespace Gallery
