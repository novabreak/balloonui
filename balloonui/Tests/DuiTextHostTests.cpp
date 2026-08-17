/**
 *  DuiTextHost 的单元测试实现。用例说明见 DuiTextHostTests.h。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiTextHostTests.h"
#include "../Controls/Input/DuiTextServices.h"

namespace balloonwjui {

namespace DuiTextHostTests {

namespace {

// 测试用的文本区尺寸（像素）。取一个足够窄的宽度，好让长文本必然换行，
// 从而能验证"内容自然高度随宽度变化"。
const int kTestClientWidth  = 200;
const int kTestClientHeight = 100;

// 测试用的内边距（像素）。四个值刻意互不相等，以便发现边被写串的错误。
const int kInsetLeft   = 4;
const int kInsetTop    = 2;
const int kInsetRight  = 6;
const int kInsetBottom = 3;

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

// 断言宏。把子项名字拼进 detail —— 本库的报告只打印用例表里的名字和
// detail 两样，Fail 的第一个参数不会出现在输出里。
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

#define EXPECT_GT(actual, threshold, name) \
    do { \
        int _a = (int)(actual); \
        int _t = (int)(threshold); \
        if (!(_a > _t)) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected > %d, got %d"), name, _t, _a); \
            return Fail(name, _d); \
        } \
    } while (0)

// 去掉文档末尾的回车与换行，再与期望值比较。
//
// 为什么需要这一步：排版引擎会在文档末尾维护一个结尾标记，读回来的文本
// 因此比写进去的多一个回车符。这是引擎的既定行为，不是缺陷 —— 库内现有
// 的富文本控件测试同样做了这个处理。直接按原样比较会得到"内容不匹配"，
// 而实际内容是对的，容易把人往错误方向带。
static CString TrimTrailingEol(const wchar_t* psz)
{
    if (psz == nullptr)
    {
        return CString();
    }
    CString s(psz);
    while (!s.IsEmpty())
    {
        TCHAR ch = s[s.GetLength() - 1];
        if (ch != _T('\r') && ch != _T('\n'))
        {
            break;
        }
        s.Delete(s.GetLength() - 1);
    }
    return s;
}

// 建一个不接控件、不建窗口的宿主实例，并把文本区尺寸设好。
// 失败时返回 nullptr。用完必须 Release（它是引用计数对象，不能 delete）。
static DuiTextHost* MakeHeadlessHost()
{
    DuiTextHost* pHost = new DuiTextHost();
    if (!pHost->Init(nullptr))
    {
        pHost->Release();
        return nullptr;
    }
    RECT rc;
    ::SetRect(&rc, 0, 0, kTestClientWidth, kTestClientHeight);
    pHost->SetClientRect(rc);
    return pHost;
}

//引擎库在本机可用。这条不过后面全都无从谈起，所以单列一条，
//失败时能一眼看出是环境问题而不是代码问题。
static Result Test_EngineLibraryAvailable()
{
    EXPECT_BOOL(DuiTextServices::IsAvailable(), true, _T("Engine/available"));
    return OK(_T("EngineLibraryAvailable"));
}

//**本组最关键的一条**：不接控件、不建任何窗口，引擎照样能创建出来。
//这条成立，无窗口路线的地基才是实的，绝大部分行为才能写成纯单元测试。
static Result Test_CreateWithoutSiteOrWindow()
{
    DuiTextHost* pHost = new DuiTextHost();

    //回调接口传空 —— 完全没有控件。
    bool bInit = pHost->Init(nullptr);
    if (!bInit)
    {
        pHost->Release();
        return Fail(_T("CreateWithoutSiteOrWindow"),
                    _T("Init(nullptr) failed: engine cannot be created headless"));
    }

    bool bReady = pHost->IsReady();
    bool bHasServices = (pHost->GetTextServices() != nullptr);
    pHost->Release();

    EXPECT_BOOL(bInit,       true, _T("Headless/initOk"));
    EXPECT_BOOL(bReady,      true, _T("Headless/ready"));
    EXPECT_BOOL(bHasServices, true, _T("Headless/servicesNotNull"));
    return OK(_T("CreateWithoutSiteOrWindow"));
}

//引擎在无窗口状态下真的能干活：灌进去的文本能原样读回来。
//能创建只说明对象建出来了，能读写才说明它真的在工作。
static Result Test_TextRoundTripWithoutWindow()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("TextRoundTripWithoutWindow"), _T("headless host init failed"));
    }

    ITextServices* pSvc = pHost->GetTextServices();
    const wchar_t* kInput = L"hello richedit";
    HRESULT hrSet = pSvc->TxSetText(kInput);

    BSTR bstr = nullptr;
    HRESULT hrGet = pSvc->TxGetText(&bstr);

    bool bMatch = false;
    CString got;
    if (bstr != nullptr)
    {
        got = TrimTrailingEol(bstr);
        bMatch = (got == CString(kInput));
        ::SysFreeString(bstr);
    }
    else
    {
        got = _T("<null bstr>");
    }
    pHost->Release();

    EXPECT_BOOL(SUCCEEDED(hrSet), true, _T("TextRT/setOk"));
    EXPECT_BOOL(SUCCEEDED(hrGet), true, _T("TextRT/getOk"));
    if (!bMatch)
    {
        // 失败时把实际拿到的内容与两个返回码一并带出来，否则只知道"不匹配"，
        // 还得再改一轮代码才能知道到底拿到了什么。
        CString d;
        d.Format(_T("TextRT/contentMatches: hrSet=0x%08X hrGet=0x%08X got=[%s] len=%d"),
                 (unsigned)hrSet, (unsigned)hrGet, (LPCTSTR)got, got.GetLength());
        return Fail(_T("TextRT/contentMatches"), d);
    }
    return OK(_T("TextRoundTripWithoutWindow"));
}

//中文文本同样能正确往返，确认没有编码问题。
static Result Test_ChineseTextRoundTrip()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("ChineseTextRoundTrip"), _T("headless host init failed"));
    }

    ITextServices* pSvc = pHost->GetTextServices();
    const wchar_t* kInput = L"你好，富文本";   // "你好，富文本"
    pSvc->TxSetText(kInput);

    BSTR bstr = nullptr;
    HRESULT hr = pSvc->TxGetText(&bstr);
    bool bMatch = false;
    if (SUCCEEDED(hr) && bstr != nullptr)
    {
        bMatch = (TrimTrailingEol(bstr) == CString(kInput));
        ::SysFreeString(bstr);
    }
    pHost->Release();

    EXPECT_BOOL(bMatch, true, _T("ChineseRT/contentMatches"));
    return OK(_T("ChineseTextRoundTrip"));
}

//**自动增高的地基**：引擎能在无窗口状态下算出"刚好装下内容需要多高"。
//这条成立，控件才能把这个高度报给布局体系，做出随内容长高的输入框。
static Result Test_NaturalSizeWithoutWindow()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("NaturalSizeWithoutWindow"), _T("headless host init failed"));
    }

    ITextServices* pSvc = pHost->GetTextServices();

    //范围参数要传真实值。传全零时引擎会把出参一并写成 0，量不出东西来
    //（实测过：传 {0,0} 时宽高都返回 0）。这里取宿主算好的那份，
    //单位是百分之毫米。
    SIZEL szExtent = { 0, 0 };
    pHost->TxGetExtent(&szExtent);

    //目标设备参数也要给。两个设备上下文都传屏幕的即可 —— 引擎只用它们
    //查询字体度量。
    HDC hdc = ::GetDC(nullptr);

    //先量空内容的高度作为基准。宽度是入参（可用宽度），高度是出参。
    LONG wEmpty = kTestClientWidth;
    LONG hEmpty = 0;
    HRESULT hr1 = pSvc->TxGetNaturalSize(DVASPECT_CONTENT, hdc, nullptr, nullptr,
                                         TXTNS_FITTOCONTENT, &szExtent,
                                         &wEmpty, &hEmpty);

    //再灌一段必然要换很多行的长文本，重新量。
    pSvc->TxSetText(L"The quick brown fox jumps over the lazy dog. "
                    L"The quick brown fox jumps over the lazy dog. "
                    L"The quick brown fox jumps over the lazy dog.");
    LONG wLong = kTestClientWidth;
    LONG hLong = 0;
    HRESULT hr2 = pSvc->TxGetNaturalSize(DVASPECT_CONTENT, hdc, nullptr, nullptr,
                                         TXTNS_FITTOCONTENT, &szExtent,
                                         &wLong, &hLong);
    ::ReleaseDC(nullptr, hdc);
    pHost->Release();

    EXPECT_BOOL(SUCCEEDED(hr1), true, _T("NaturalSize/emptyOk"));
    EXPECT_BOOL(SUCCEEDED(hr2), true, _T("NaturalSize/longOk"));
    //空内容也应当有一行的高度，不能是 0。
    if (hEmpty <= 0)
    {
        CString d;
        d.Format(_T("NaturalSize/emptyHeightPositive: hr1=0x%08X hr2=0x%08X ")
                 _T("wEmpty=%d hEmpty=%d wLong=%d hLong=%d"),
                 (unsigned)hr1, (unsigned)hr2,
                 (int)wEmpty, (int)hEmpty, (int)wLong, (int)hLong);
        return Fail(_T("NaturalSize/emptyHeightPositive"), d);
    }
    //长文本换行后必然比空内容高。这条同时验证了"引擎确实按我们给的宽度
    //在排版"——若它没拿到宽度，就不会换行，高度也就不会变高。
    EXPECT_GT(hLong, hEmpty, _T("NaturalSize/longerThanEmpty"));
    return OK(_T("NaturalSizeWithoutWindow"));
}

//属性位的读写往返，并验证掩码过滤：只返回问到的那几位。
static Result Test_PropertyBitsRoundTrip()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("PropertyBitsRoundTrip"), _T("headless host init failed"));
    }

    //默认应当是富文本 + 多行 + 自动换行。
    DWORD dwBits = pHost->GetPropertyBits_();
    bool bRich     = (dwBits & TXTBIT_RICHTEXT) != 0;
    bool bMulti    = (dwBits & TXTBIT_MULTILINE) != 0;
    bool bWrap     = (dwBits & TXTBIT_WORDWRAP) != 0;
    bool bReadOnly = (dwBits & TXTBIT_READONLY) != 0;

    //置上只读，再确认只有这一位变了。
    pHost->SetPropertyBits(TXTBIT_READONLY, TXTBIT_READONLY);
    DWORD dwAfter = pHost->GetPropertyBits_();
    bool bReadOnlyAfter = (dwAfter & TXTBIT_READONLY) != 0;
    bool bMultiStill    = (dwAfter & TXTBIT_MULTILINE) != 0;

    //再取消只读。
    pHost->SetPropertyBits(TXTBIT_READONLY, 0);
    DWORD dwFinal = pHost->GetPropertyBits_();
    bool bReadOnlyFinal = (dwFinal & TXTBIT_READONLY) != 0;

    //掩码过滤：只问只读那一位时，返回值里不该带上别的位。
    DWORD dwMasked = 0;
    pHost->TxGetPropertyBits(TXTBIT_READONLY, &dwMasked);

    pHost->Release();

    EXPECT_BOOL(bRich,          true,  _T("PropBits/defaultRich"));
    EXPECT_BOOL(bMulti,         true,  _T("PropBits/defaultMultiline"));
    EXPECT_BOOL(bWrap,          true,  _T("PropBits/defaultWordWrap"));
    EXPECT_BOOL(bReadOnly,      false, _T("PropBits/defaultNotReadOnly"));
    EXPECT_BOOL(bReadOnlyAfter, true,  _T("PropBits/readOnlySet"));
    EXPECT_BOOL(bMultiStill,    true,  _T("PropBits/multilineUntouched"));
    EXPECT_BOOL(bReadOnlyFinal, false, _T("PropBits/readOnlyCleared"));
    EXPECT_INT(dwMasked, 0, _T("PropBits/maskFiltersOthers"));
    return OK(_T("PropertyBitsRoundTrip"));
}

//内边距的单位换算：传进去的是像素，引擎拿到的必须是百分之毫米。
//这是本类最容易搞错的一处，用一条用例钉住。
static Result Test_ViewInsetUnitConversion()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("ViewInsetUnitConversion"), _T("headless host init failed"));
    }

    pHost->SetViewInsetPixels(kInsetLeft, kInsetTop, kInsetRight, kInsetBottom);

    RECT rcInset;
    ::SetRect(&rcInset, 0, 0, 0, 0);
    HRESULT hr = pHost->TxGetViewInset(&rcInset);

    //取当前屏幕的每英寸点数，按同一公式算出期望值。
    //不写死数字是因为高分屏下换算结果本来就不同，写死会在别的机器上误报。
    int dpiX = 96;
    int dpiY = 96;
    HDC hdc = ::GetDC(nullptr);
    if (hdc != nullptr)
    {
        int x = ::GetDeviceCaps(hdc, LOGPIXELSX);
        int y = ::GetDeviceCaps(hdc, LOGPIXELSY);
        ::ReleaseDC(nullptr, hdc);
        if (x > 0) { dpiX = x; }
        if (y > 0) { dpiY = y; }
    }
    const int kHimetricPerInch = 2540;
    LONG expectLeft   = ::MulDiv(kInsetLeft,   kHimetricPerInch, dpiX);
    LONG expectTop    = ::MulDiv(kInsetTop,    kHimetricPerInch, dpiY);
    LONG expectRight  = ::MulDiv(kInsetRight,  kHimetricPerInch, dpiX);
    LONG expectBottom = ::MulDiv(kInsetBottom, kHimetricPerInch, dpiY);

    pHost->Release();

    EXPECT_BOOL(SUCCEEDED(hr), true, _T("Inset/getOk"));
    EXPECT_INT(rcInset.left,   expectLeft,   _T("Inset/left"));
    EXPECT_INT(rcInset.top,    expectTop,    _T("Inset/top"));
    EXPECT_INT(rcInset.right,  expectRight,  _T("Inset/right"));
    EXPECT_INT(rcInset.bottom, expectBottom, _T("Inset/bottom"));
    return OK(_T("ViewInsetUnitConversion"));
}

//文本区矩形按**像素**原样返回，不做任何换算。
//与上一条配成一对：相邻的两个方法，一个像素、一个百分之毫米。
static Result Test_ClientRectStaysInPixels()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("ClientRectStaysInPixels"), _T("headless host init failed"));
    }

    RECT rc;
    ::SetRect(&rc, 0, 0, 0, 0);
    HRESULT hr = pHost->TxGetClientRect(&rc);
    pHost->Release();

    EXPECT_BOOL(SUCCEEDED(hr), true, _T("ClientRect/getOk"));
    EXPECT_INT(rc.right - rc.left,  kTestClientWidth,  _T("ClientRect/width"));
    EXPECT_INT(rc.bottom - rc.top,  kTestClientHeight, _T("ClientRect/height"));
    return OK(_T("ClientRectStaysInPixels"));
}

//背景样式：默认透明（由控件自己画背景），可切换成不透明。
static Result Test_BackStyleDefaultsToTransparent()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("BackStyleDefaultsToTransparent"), _T("headless host init failed"));
    }

    TXTBACKSTYLE style = TXTBACK_OPAQUE;
    pHost->TxGetBackStyle(&style);
    bool bTransparentByDefault = (style == TXTBACK_TRANSPARENT);

    pHost->SetBackTransparent(false);
    pHost->TxGetBackStyle(&style);
    bool bOpaqueAfter = (style == TXTBACK_OPAQUE);

    pHost->Release();

    EXPECT_BOOL(bTransparentByDefault, true, _T("BackStyle/defaultTransparent"));
    EXPECT_BOOL(bOpaqueAfter,          true, _T("BackStyle/switchToOpaque"));
    return OK(_T("BackStyleDefaultsToTransparent"));
}

//选区配色：默认跟随系统，设了覆盖色之后返回覆盖色。
static Result Test_SelectionColorOverride()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("SelectionColorOverride"), _T("headless host init failed"));
    }

    COLORREF crSysHighlight = ::GetSysColor(COLOR_HIGHLIGHT);
    COLORREF crBefore = pHost->TxGetSysColor(COLOR_HIGHLIGHT);

    const COLORREF kCustomBack = RGB(10, 20, 30);
    const COLORREF kCustomText = RGB(40, 50, 60);
    pHost->SetSelectionColors(kCustomBack, kCustomText);
    COLORREF crAfterBack = pHost->TxGetSysColor(COLOR_HIGHLIGHT);
    COLORREF crAfterText = pHost->TxGetSysColor(COLOR_HIGHLIGHTTEXT);

    //没被覆盖的颜色项仍然走系统。
    COLORREF crWindowText = pHost->TxGetSysColor(COLOR_WINDOWTEXT);
    COLORREF crSysWindowText = ::GetSysColor(COLOR_WINDOWTEXT);

    pHost->Release();

    EXPECT_BOOL(crBefore == crSysHighlight, true, _T("SelColor/defaultFollowsSystem"));
    EXPECT_BOOL(crAfterBack == kCustomBack, true, _T("SelColor/backOverridden"));
    EXPECT_BOOL(crAfterText == kCustomText, true, _T("SelColor/textOverridden"));
    EXPECT_BOOL(crWindowText == crSysWindowText, true, _T("SelColor/othersUntouched"));
    return OK(_T("SelectionColorOverride"));
}

//没有控件时，所有需要操作控件的回调都安全空转，不崩溃。
//这条对应"控件还没挂进 DUI 树、引擎却已经开始回调"的真实场景。
static Result Test_CallbacksWithoutSiteAreSafe()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("CallbacksWithoutSiteAreSafe"), _T("headless host init failed"));
    }

    //以下每一个都依赖控件回调接口，此刻它是空的。
    pHost->TxInvalidateRect(nullptr, TRUE);
    pHost->TxViewChange(TRUE);
    pHost->TxScrollWindowEx(0, 0, nullptr, nullptr, nullptr, nullptr, 0);
    pHost->TxSetCapture(TRUE);
    pHost->TxSetCapture(FALSE);
    pHost->TxSetFocus();
    pHost->TxShowScrollBar(SB_VERT, TRUE);
    pHost->TxEnableScrollBar(SB_BOTH, 0);
    pHost->TxSetScrollRange(SB_VERT, 0, 100, TRUE);
    pHost->TxSetScrollPos(SB_VERT, 10, TRUE);

    //需要窗口句柄的几个应当返回失败而不是崩溃。
    BOOL bCaret = pHost->TxCreateCaret(nullptr, 2, 16);
    POINT pt = { 0, 0 };
    BOOL bS2C = pHost->TxScreenToClient(&pt);
    HIMC himc = pHost->TxImmGetContext();

    pHost->Release();

    EXPECT_BOOL(bCaret == FALSE, true, _T("NoSite/createCaretFails"));
    EXPECT_BOOL(bS2C == FALSE,   true, _T("NoSite/screenToClientFails"));
    EXPECT_BOOL(himc == nullptr, true, _T("NoSite/immContextNull"));
    return OK(_T("CallbacksWithoutSiteAreSafe"));
}

//断开是幂等的，断开之后各接口安全空转。
static Result Test_ShutdownIsIdempotent()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("ShutdownIsIdempotent"), _T("headless host init failed"));
    }

    pHost->Shutdown();
    bool bReadyAfter = pHost->IsReady();

    //再断开两次，不应出问题。
    pHost->Shutdown();
    pHost->Shutdown();

    //断开之后查询滚动状态应当返回失败并把出参清零。
    int nMin = 7;
    int nMax = 7;
    int nPage = 7;
    int nPos = 7;
    bool bEnabled = true;
    bool bQuery = pHost->QueryScrollInfo(true, nMin, nMax, nPage, nPos, bEnabled);

    pHost->Release();

    EXPECT_BOOL(bReadyAfter, false, _T("Shutdown/notReady"));
    EXPECT_BOOL(bQuery,      false, _T("Shutdown/queryFails"));
    EXPECT_INT(nMin,  0, _T("Shutdown/outParamsCleared"));
    EXPECT_BOOL(bEnabled, false, _T("Shutdown/enabledCleared"));
    return OK(_T("ShutdownIsIdempotent"));
}

//只读属性生效：置上只读之后，模拟输入的字符不应改变内容。
//这条同时说明引擎在无窗口状态下也正确地执行了属性位。
static Result Test_ReadOnlyBlocksTypedInput()
{
    DuiTextHost* pHost = MakeHeadlessHost();
    if (pHost == nullptr)
    {
        return Fail(_T("ReadOnlyBlocksTypedInput"), _T("headless host init failed"));
    }

    ITextServices* pSvc = pHost->GetTextServices();
    pSvc->TxSetText(L"abc");
    pHost->SetPropertyBits(TXTBIT_READONLY, TXTBIT_READONLY);

    //把光标放到末尾再模拟按下一个字符键。
    pSvc->TxSendMessage(EM_SETSEL, (WPARAM)-1, (LPARAM)-1, nullptr);
    pSvc->TxSendMessage(WM_CHAR, (WPARAM)L'X', 0, nullptr);

    BSTR bstr = nullptr;
    pSvc->TxGetText(&bstr);
    bool bUnchanged = (bstr != nullptr && TrimTrailingEol(bstr) == CString(_T("abc")));
    if (bstr != nullptr)
    {
        ::SysFreeString(bstr);
    }
    pHost->Release();

    EXPECT_BOOL(bUnchanged, true, _T("ReadOnly/textUnchanged"));
    return OK(_T("ReadOnlyBlocksTypedInput"));
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
        { _T("EngineLibraryAvailable"),         &Test_EngineLibraryAvailable         },
        { _T("CreateWithoutSiteOrWindow"),      &Test_CreateWithoutSiteOrWindow      },
        { _T("TextRoundTripWithoutWindow"),     &Test_TextRoundTripWithoutWindow     },
        { _T("ChineseTextRoundTrip"),           &Test_ChineseTextRoundTrip           },
        { _T("NaturalSizeWithoutWindow"),       &Test_NaturalSizeWithoutWindow       },
        { _T("PropertyBitsRoundTrip"),          &Test_PropertyBitsRoundTrip          },
        { _T("ViewInsetUnitConversion"),        &Test_ViewInsetUnitConversion        },
        { _T("ClientRectStaysInPixels"),        &Test_ClientRectStaysInPixels        },
        { _T("BackStyleDefaultsToTransparent"), &Test_BackStyleDefaultsToTransparent },
        { _T("SelectionColorOverride"),         &Test_SelectionColorOverride         },
        { _T("CallbacksWithoutSiteAreSafe"),    &Test_CallbacksWithoutSiteAreSafe    },
        { _T("ShutdownIsIdempotent"),           &Test_ShutdownIsIdempotent           },
        { _T("ReadOnlyBlocksTypedInput"),       &Test_ReadOnlyBlocksTypedInput       },
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
    summary.Format(_T("[summary] DuiTextHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

#undef EXPECT_BOOL
#undef EXPECT_INT
#undef EXPECT_GT

} // namespace DuiTextHostTests

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
