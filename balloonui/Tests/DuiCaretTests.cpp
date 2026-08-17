/**
 *  DuiCaret 的单元测试实现。用例说明见 DuiCaretTests.h。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "DuiCaretTests.h"

namespace balloonwjui {

namespace DuiCaretTests {

namespace {

// 测试用光标的宽高（像素）。取值本身无意义，只用于验证 DuiCaret 是否
// 原样记录下来；刻意取两个不相等的值，以便发现宽高写反的错误。
static const int kTestCaretWidth  = 2;
static const int kTestCaretHeight = 16;

// 测试用的光标位置（宿主客户区坐标，像素）。同样取两个不等的值，
// 以便发现横纵坐标写反的错误。
static const int kTestCaretX = 30;
static const int kTestCaretY = 44;

// 隐藏的临时宿主窗口。系统光标接口要求一个属于当前线程的窗口句柄，
// 本类负责在用例期间提供一个，析构时销毁。
//
// 窗口刻意不加 WS_VISIBLE：本组用例只检查 DuiCaret 的内部状态，
// 不需要真的把光标画到屏幕上，不显示窗口可以避免测试运行时屏幕闪烁。
class HiddenHostWnd
{
public:
    HiddenHostWnd()
        : m_hwnd(nullptr)
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiCaretTestHost");
        ::RegisterClassEx(&wc);

        m_hwnd = ::CreateWindowEx(
            0, _T("DuiCaretTestHost"), _T(""),
            WS_POPUP, 0, 0, 200, 100,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }

    ~HiddenHostWnd()
    {
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;
};

struct Result
{
    CString name;
    bool    ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok   = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name   = n;
    r.ok     = false;
    r.detail = d;
    return r;
}

// 断言宏。**把子项名字拼进 detail** 是有意为之：本库的报告只打印用例表里的
// 名字和 detail 两样，Fail 的第一个参数并不会出现在输出里。若不把子项名字
// 写进 detail，用例一旦失败就只知道"某个用例挂了"，不知道挂在哪一条断言上。
#define EXPECT_BOOL(actual, expected, name) \
    do { \
        bool _a = (actual); \
        bool _e = (expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e ? 1 : 0, _a ? 1 : 0); \
            return Fail(name, _d); \
        } \
    } while (0)

#define EXPECT_INT(actual, expected, name) \
    do { \
        int _a = (int)(actual); \
        int _e = (int)(expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e, _a); \
            return Fail(name, _d); \
        } \
    } while (0)

#define EXPECT_PTR_NULL(actual, name) \
    do { \
        if ((actual) != nullptr) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected null pointer"), name); \
            return Fail(name, _d); \
        } \
    } while (0)

//刚构造出来的对象：既没占有光标，也不处于显示状态，各项数据全部归零。
static Result Test_Defaults()
{
    DuiCaret caret;
    EXPECT_BOOL(caret.IsCreated(), false, _T("Defaults/notCreated"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Defaults/notVisible"));
    EXPECT_PTR_NULL(caret.GetOwner(), _T("Defaults/ownerNull"));
    EXPECT_INT(caret.GetPos().x,  0, _T("Defaults/posX"));
    EXPECT_INT(caret.GetPos().y,  0, _T("Defaults/posY"));
    EXPECT_INT(caret.GetSize().cx, 0, _T("Defaults/sizeCx"));
    EXPECT_INT(caret.GetSize().cy, 0, _T("Defaults/sizeCy"));
    return OK(_T("Defaults"));
}

//窗口句柄非法时创建应当失败，且不留下任何"已占有"的痕迹。
//传空句柄是调用方最容易犯的错（控件还没挂进 DUI 树就想建光标）。
static Result Test_CreateWithBadHwndFails()
{
    DuiCaret caret;
    EXPECT_BOOL(caret.Create(nullptr, nullptr, kTestCaretWidth, kTestCaretHeight),
                false, _T("BadHwnd/nullFails"));
    EXPECT_BOOL(caret.IsCreated(), false, _T("BadHwnd/stillNotCreated"));

    //一个已经销毁的窗口句柄同样应当被拒绝。
    HWND stale = nullptr;
    {
        HiddenHostWnd wnd;
        stale = wnd.get();
    }
    EXPECT_BOOL(caret.Create(stale, nullptr, kTestCaretWidth, kTestCaretHeight),
                false, _T("BadHwnd/staleFails"));
    EXPECT_BOOL(caret.IsCreated(), false, _T("BadHwnd/stillNotCreated2"));
    return OK(_T("CreateWithBadHwndFails"));
}

//创建成功后：占有标志置起、宽高被记下，但**默认不可见** —— 这是系统的行为，
//新建出来的光标必须再显式 Show(true) 才会出现，容易被误以为建完就能看见。
static Result Test_CreateSetsStateButStaysHidden()
{
    HiddenHostWnd wnd;
    DuiCaret caret;

    EXPECT_BOOL(caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight),
                true, _T("Create/ok"));
    EXPECT_BOOL(caret.IsCreated(), true,  _T("Create/created"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Create/hiddenByDefault"));
    EXPECT_INT(caret.GetSize().cx, kTestCaretWidth,  _T("Create/width"));
    EXPECT_INT(caret.GetSize().cy, kTestCaretHeight, _T("Create/height"));
    EXPECT_BOOL(caret.GetOwner() == wnd.get(), true, _T("Create/owner"));
    return OK(_T("CreateSetsStateButStaysHidden"));
}

//显示与隐藏的往返，以及**重复调用同一个值应当被折叠掉**。
//折叠这件事必须测：系统的显示状态是可叠加的计数器，隐藏两次就要显示两次才能
//恢复。若把重复调用原样透传下去，计数会越叠越深，最终表现为"光标怎么也不出来"。
static Result Test_ShowHideRoundTripAndFolding()
{
    HiddenHostWnd wnd;
    DuiCaret caret;
    caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight);

    EXPECT_BOOL(caret.Show(true),  true, _T("Show/firstShowOk"));
    EXPECT_BOOL(caret.IsVisible(), true, _T("Show/visible"));

    //连续两次要求显示：第二次应当被折叠，状态不变。
    EXPECT_BOOL(caret.Show(true),  true, _T("Show/secondShowOk"));
    EXPECT_BOOL(caret.IsVisible(), true, _T("Show/stillVisible"));

    EXPECT_BOOL(caret.Show(false), true,  _T("Show/hideOk"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Show/hidden"));

    //连续两次要求隐藏：同样折叠。
    EXPECT_BOOL(caret.Show(false), true,  _T("Show/secondHideOk"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Show/stillHidden"));

    //折叠若失效，这里恢复显示就会失败（系统计数被叠到 -2，一次 Show 不够）。
    EXPECT_BOOL(caret.Show(true),  true, _T("Show/reshowOk"));
    EXPECT_BOOL(caret.IsVisible(), true, _T("Show/reshowVisible"));
    return OK(_T("ShowHideRoundTripAndFolding"));
}

//位置被原样记录；且**隐藏状态下也允许设置位置** —— 输入法要靠系统光标的
//位置决定候选条弹在哪里，所以插入点一变就得设，不能等到光标可见时才设。
static Result Test_SetPosRecordsPositionEvenWhenHidden()
{
    HiddenHostWnd wnd;
    DuiCaret caret;
    caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight);

    //此时光标尚未显示。
    EXPECT_BOOL(caret.IsVisible(), false, _T("SetPos/hiddenPrecondition"));
    EXPECT_BOOL(caret.SetPos(kTestCaretX, kTestCaretY), true, _T("SetPos/okWhileHidden"));
    EXPECT_INT(caret.GetPos().x, kTestCaretX, _T("SetPos/x"));
    EXPECT_INT(caret.GetPos().y, kTestCaretY, _T("SetPos/y"));

    //显示之后再设一次，同样生效。
    caret.Show(true);
    EXPECT_BOOL(caret.SetPos(kTestCaretX + 5, kTestCaretY + 7), true, _T("SetPos/okWhileVisible"));
    EXPECT_INT(caret.GetPos().x, kTestCaretX + 5, _T("SetPos/x2"));
    EXPECT_INT(caret.GetPos().y, kTestCaretY + 7, _T("SetPos/y2"));
    return OK(_T("SetPosRecordsPositionEvenWhenHidden"));
}

//尚未创建时调用各接口应当安全空转、返回失败，而不是崩溃或误操作。
//这条对应控件生命周期的真实场景：控件建出来了但还没获得焦点，
//排版引擎却已经开始要求挪光标。
static Result Test_OpsBeforeCreateAreSafe()
{
    DuiCaret caret;
    EXPECT_BOOL(caret.Show(true),  false, _T("BeforeCreate/showFails"));
    EXPECT_BOOL(caret.Show(false), false, _T("BeforeCreate/hideFails"));
    EXPECT_BOOL(caret.SetPos(kTestCaretX, kTestCaretY), false, _T("BeforeCreate/setPosFails"));
    EXPECT_BOOL(caret.IsCreated(), false, _T("BeforeCreate/stillNotCreated"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("BeforeCreate/stillNotVisible"));
    EXPECT_INT(caret.GetPos().x, 0, _T("BeforeCreate/posUntouched"));
    return OK(_T("OpsBeforeCreateAreSafe"));
}

//销毁后状态全部归零，且后续操作安全空转。
//失去焦点时必须销毁，把线程唯一的那份系统光标让出来给别的控件。
static Result Test_DestroyResetsState()
{
    HiddenHostWnd wnd;
    DuiCaret caret;
    caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight);
    caret.Show(true);
    caret.SetPos(kTestCaretX, kTestCaretY);

    caret.Destroy();
    EXPECT_BOOL(caret.IsCreated(), false, _T("Destroy/notCreated"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Destroy/notVisible"));
    EXPECT_PTR_NULL(caret.GetOwner(), _T("Destroy/ownerNull"));
    EXPECT_INT(caret.GetPos().x,   0, _T("Destroy/posX"));
    EXPECT_INT(caret.GetSize().cy, 0, _T("Destroy/sizeCy"));

    //销毁之后各接口继续安全空转。
    EXPECT_BOOL(caret.Show(true), false, _T("Destroy/showFailsAfter"));
    EXPECT_BOOL(caret.SetPos(1, 2), false, _T("Destroy/setPosFailsAfter"));
    return OK(_T("DestroyResetsState"));
}

//重复销毁是幂等的。若不加占有标志的守卫，第二次销毁会把别的控件刚建好的
//光标误销毁掉 —— 因为系统的销毁接口作用于"当前线程拥有的光标"，不认句柄。
static Result Test_DestroyIsIdempotent()
{
    HiddenHostWnd wnd;
    DuiCaret caret;
    caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight);

    caret.Destroy();
    caret.Destroy();
    caret.Destroy();
    EXPECT_BOOL(caret.IsCreated(), false, _T("DestroyTwice/notCreated"));

    //未创建过就直接销毁，同样安全。
    DuiCaret fresh;
    fresh.Destroy();
    EXPECT_BOOL(fresh.IsCreated(), false, _T("DestroyFresh/notCreated"));
    return OK(_T("DestroyIsIdempotent"));
}

//已占有光标时再次创建：应当先让出旧的再建新的，显示状态随之复位。
//若直接叠着创建，内部记录的显示状态会与系统的显示计数脱节，
//后续的折叠逻辑就不准了。
static Result Test_RecreateWhileCreated()
{
    HiddenHostWnd wnd;
    DuiCaret caret;
    caret.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight);
    caret.Show(true);
    EXPECT_BOOL(caret.IsVisible(), true, _T("Recreate/visibleBefore"));

    //换一个尺寸重建。
    EXPECT_BOOL(caret.Create(wnd.get(), nullptr, kTestCaretWidth + 1, kTestCaretHeight + 4),
                true, _T("Recreate/ok"));
    EXPECT_BOOL(caret.IsCreated(), true,  _T("Recreate/created"));
    EXPECT_BOOL(caret.IsVisible(), false, _T("Recreate/hiddenAgain"));
    EXPECT_INT(caret.GetSize().cx, kTestCaretWidth + 1,  _T("Recreate/newWidth"));
    EXPECT_INT(caret.GetSize().cy, kTestCaretHeight + 4, _T("Recreate/newHeight"));
    return OK(_T("RecreateWhileCreated"));
}

//析构会自动让出光标：前一个对象出作用域后，后一个对象应当能正常创建。
//这一条守的是"线程唯一的系统光标不会被某个对象一直占着"。
static Result Test_DestructorReleasesCaret()
{
    HiddenHostWnd wnd;
    {
        DuiCaret first;
        EXPECT_BOOL(first.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight),
                    true, _T("Dtor/firstCreated"));
        first.Show(true);
        //刻意不调 Destroy，交给析构处理。
    }

    DuiCaret second;
    EXPECT_BOOL(second.Create(wnd.get(), nullptr, kTestCaretWidth, kTestCaretHeight),
                true, _T("Dtor/secondCreated"));
    EXPECT_BOOL(second.IsVisible(), false, _T("Dtor/secondHidden"));
    second.Destroy();
    return OK(_T("DestructorReleasesCaret"));
}

} // 匿名命名空间

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn  fn;
    };

    Entry tests[] = {
        { _T("Defaults"),                          &Test_Defaults                          },
        { _T("CreateWithBadHwndFails"),            &Test_CreateWithBadHwndFails            },
        { _T("CreateSetsStateButStaysHidden"),     &Test_CreateSetsStateButStaysHidden     },
        { _T("ShowHideRoundTripAndFolding"),       &Test_ShowHideRoundTripAndFolding       },
        { _T("SetPosRecordsPositionEvenWhenHidden"), &Test_SetPosRecordsPositionEvenWhenHidden },
        { _T("OpsBeforeCreateAreSafe"),            &Test_OpsBeforeCreateAreSafe            },
        { _T("DestroyResetsState"),                &Test_DestroyResetsState                },
        { _T("DestroyIsIdempotent"),               &Test_DestroyIsIdempotent               },
        { _T("RecreateWhileCreated"),              &Test_RecreateWhileCreated              },
        { _T("DestructorReleasesCaret"),           &Test_DestructorReleasesCaret           },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); ++i)
    {
        Result r = tests[i].fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), tests[i].name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), tests[i].name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }

    CString summary;
    summary.Format(_T("[summary] DuiCaretTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

#undef EXPECT_BOOL
#undef EXPECT_INT
#undef EXPECT_PTR_NULL

} // namespace DuiCaretTests

} // namespace balloonwjui
