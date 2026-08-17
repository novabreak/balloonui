/**
 *  画廊界面文案的中英文切换。画廊里所有面向读者的文字（页面标题、段落
 *  标题、段落说明、演示控件上的示例文字）都必须经由本文件的 Txt() 取出，
 *  切换语言后重建导航与当前页面即可整体换成另一种语言。
 *
 *  典型用法：
 *      AddSection(page.get(),
 *                 Txt(_T("按钮"), _T("PushButton")),
 *                 Txt(_T("品牌蓝圆角主操作按钮。"),
 *                     _T("Brand-blue rounded primary action.")));
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <tchar.h>

namespace Gallery {

// 画廊界面文案使用的语言。
enum Language
{
    // 简体中文。程序启动时的默认语言。
    LangChinese = 0,
    // 英文。供不读中文的使用者切换。
    LangEnglish = 1,
};

// 读取当前语言。
// 返回：当前生效的语言枚举值。
Language CurrentLanguage();

// 切换当前语言。
//   lang：要切换到的语言。
// 本函数只更新全局状态，**不会**重建任何已经建好的控件 —— 已经取过文案
// 的控件仍然显示旧语言的文字。调用方负责在切换之后重建导航树与当前页面。
void SetCurrentLanguage(Language lang);

// 按当前语言在两份文案里挑出一份。
//   zh：中文文案。必须是字符串字面量或其它静态存储期的字符串，不能是栈上
//       的临时缓冲区 —— 本函数直接返回传入的指针，不做任何复制。
//   en：英文文案。要求同上。
// 返回：当前语言对应的那一份。任一入参为空指针时返回另一份；两份都为空
//       时返回空字符串，绝不返回空指针。
LPCTSTR Txt(LPCTSTR zh, LPCTSTR en);

} // namespace Gallery
