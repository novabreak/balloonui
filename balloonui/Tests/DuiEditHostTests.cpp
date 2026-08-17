/**
 *  DuiEditHostTests 的实现。
 *
 *  本文件现在只覆盖普通输入框的兼容外壳 DuiEditHost 自己新增的那几样东西；
 *  控件本体的用例在 DuiEditTests.cpp。取舍的理由见 DuiEditHostTests.h。
 *
 *  balloonwj@qq.com   2026-05-20
 */
#include "stdafx.h"
#include "DuiEditHostTests.h"

#if BUI_FEATURE_EDIT

namespace balloonwjui {

namespace DuiEditHostTests {

namespace {

// 用例里反复用到的两段占位文字。取两段是为了验证"开关关着时设的新文字，开关
// 一开就能生效"，一段文字分辨不出这件事。
LPCTSTR const kPlaceholderA = _T("请输入用户名");

// 一条用例的执行结果。
struct Result
{
    CString name;      // 失败时用于定位的断言名
    bool ok;           // 是否通过
    CString detail;    // 失败原因；通过时为空
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// ─── 兼容用的 EnsureCreated ────────────────────────────────────────

// 旧实现在 EnsureCreated 里创建内部子窗口，无窗口控件没有这一步，方法退化成
// 空实现并恒返回成功。这条用例钉住三件事：控件不调它也能用、调了恒成功、
// 重复调用不出问题；参数是被忽略的，传空句柄同样成功。
static Result Test_EnsureCreatedIsNoOp()
{
    DuiEditHost e;

    // 一次都没调过就直接用 —— 这正是无窗口实现相对旧实现的关键差别。
    e.SetText(_T("kept"));
    if (e.GetText() != _T("kept"))
    {
        return Fail(_T("Ensure/usableBeforeCall"), _T("control not usable before EnsureCreated"));
    }

    EXPECT_BOOL(e.EnsureCreated(NULL), true, _T("Ensure/firstCall"));
    EXPECT_BOOL(e.EnsureCreated(NULL), true, _T("Ensure/repeatedCall"));

    // 调用不应当碰控件已有的状态。
    if (e.GetText() != _T("kept"))
    {
        return Fail(_T("Ensure/textIntact"), _T("text changed by EnsureCreated"));
    }
    return OK(_T("EnsureCreatedIsNoOp"));
}

// ─── 占位文字的读写 ───────────────────────────────────────────────

// SetPlaceholder / GetPlaceholder 的往返：默认为空、设过能读回、传空指针
// 等同于清空。
static Result Test_PlaceholderRoundTrip()
{
    DuiEditHost e;
    if (!e.GetPlaceholder().IsEmpty())
    {
        return Fail(_T("PH/default"), _T("placeholder should be empty by default"));
    }

    e.SetPlaceholder(kPlaceholderA);
    if (e.GetPlaceholder() != kPlaceholderA)
    {
        return Fail(_T("PH/setGet"), _T("set/get mismatch"));
    }

    e.SetPlaceholder(NULL);
    if (!e.GetPlaceholder().IsEmpty())
    {
        return Fail(_T("PH/nullClears"), _T("null pointer should clear the placeholder"));
    }
    return OK(_T("PlaceholderRoundTrip"));
}

// ─── 旧通知码别名 ─────────────────────────────────────────────────

// 存量代码比较的是 DUIEN_LEFT_ICON_CLICK / DUIEN_RIGHT_ICON_CLICK 这两个符号，
// 它们必须与控件本体的对应通知码等值 —— 不等值的话，业务代码里的判断就再也
// 不会成立，症状是点了图标没反应，不报错也不崩溃。
static Result Test_IconClickCodeAliases()
{
    EXPECT_INT((int)DuiEditHost::DUIEN_LEFT_ICON_CLICK,
               (int)DuiEdit::DUIN_EDIT_LEFT_ICON_CLICK,
               _T("Alias/left"));
    EXPECT_INT((int)DuiEditHost::DUIEN_RIGHT_ICON_CLICK,
               (int)DuiEdit::DUIN_EDIT_RIGHT_ICON_CLICK,
               _T("Alias/right"));

    // 左右两个别名必须互不相同，否则派发端分不出点的是哪一侧。
    if ((int)DuiEditHost::DUIEN_LEFT_ICON_CLICK
        == (int)DuiEditHost::DUIEN_RIGHT_ICON_CLICK)
    {
        return Fail(_T("Alias/distinct"), _T("left and right alias have the same value"));
    }
    return OK(_T("IconClickCodeAliases"));
}

#undef EXPECT_BOOL
#undef EXPECT_INT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    // 这张表必须与上面保留下来的用例一一对应：本框架不会因为漏登记而报错，
    // 漏掉的用例只是永远不执行。
    Entry tests[] = {
        { _T("EnsureCreatedIsNoOp"),      &Test_EnsureCreatedIsNoOp      },
        { _T("PlaceholderRoundTrip"),     &Test_PlaceholderRoundTrip     },
        { _T("IconClickCodeAliases"),     &Test_IconClickCodeAliases     },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), e.name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiEditHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiEditHostTests

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
