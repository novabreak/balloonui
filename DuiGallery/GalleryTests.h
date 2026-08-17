/**
 *  画廊自身的单元测试。测的不是 balloonui 控件（那些用例在 balloonui/Tests/
 *  下），而是这次重构新加的三样东西：页面注册表、中英文切换、导航搜索的
 *  匹配规则，外加段落说明文字按宽度自动换行这一机制。
 *
 *  用法与库内其它测试一致：调 RunAll() 拿到一份文本报告，画廊启动时会把它
 *  连同库的测试报告一起写到临时目录下的 DuiGallery_tests.log。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#pragma once

#include <atlstr.h>

namespace Gallery {

namespace GalleryTests
{
    // 把本文件里的全部用例跑一遍。
    // 返回：每条用例一行的文本报告，最后一行是汇总的通过与失败条数。
    CString RunAll();
}

} // namespace Gallery
