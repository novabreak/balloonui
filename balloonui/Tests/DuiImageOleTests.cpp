#include "stdafx.h"
#include "DuiImageOleTests.h"

#if BUI_FEATURE_IMAGEOLE

#include <ole2.h>

namespace balloonwjui {

namespace DuiImageOleTests {

namespace {

struct Result { CString name; bool ok; CString detail; };
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

#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)
#define EXPECT_HR(hr, name) \
    do { HRESULT _h = (hr); \
         if (FAILED(_h)) { CString _d; _d.Format(_T("HRESULT=0x%08X"), (unsigned)_h); return Fail(name, _d); } \
    } while (0)

static HBITMAP MakeFlatBitmap(int w, int h, BYTE r, BYTE g, BYTE b)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i*4 + 0] = b;
        p[i*4 + 1] = g;
        p[i*4 + 2] = r;
        p[i*4 + 3] = 255;
    }
    return hbm;
}

// 造一张 w×h、竖向 1px 黑白相间条纹的不透明位图（32bpp 顶朝下 DIBSection）。
// 用途：这种"最高频"的图案是重采样质量的照妖镜 —— 按 2:1 缩小时，只要做了
// 面积平均就该得到均匀的中灰；若是点采样 / 像素丢弃，结果会是纯黑或纯白。
static HBITMAP MakeStripeBitmap(int w, int h)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            BYTE v = (x % 2 == 0) ? 255 : 0;
            BYTE* px = p + ((size_t)y * w + x) * 4;
            px[0] = v;
            px[1] = v;
            px[2] = v;
            px[3] = 255;
        }
    }
    ::GdiFlush();
    return hbm;
}

// 造一张 w×h 全透明的位图（32bpp 预乘 alpha，四个通道全 0）。
// 用途：验证绘制时确实按 alpha 混合 —— 全透明的图画上去应当"什么都没画"。
static HBITMAP MakeTransparentBitmap(int w, int h)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    ::memset(bits, 0, (size_t)w * h * 4);
    ::GdiFlush();
    return hbm;
}

// 造一张上下不对称的带 alpha 位图：图像上半部纯白、下半部纯黑，均不透明。
// 用途：抓"上下颠倒"。竖条纹之类左右对称的图案对垂直翻转不敏感，测不出来。
//
// 位图按<u>底朝上</u>（biHeight 传正数）建，与 LoadPremultipliedDib 的产出一致 ——
// 这不是随手选的：GetObject 取回 DIBSECTION 时不保留"顶朝下"信息（用负
// biHeight 建出来，取回来的 dsBmih.biHeight 仍是正数），所以读像素的一侧只能
// 按底朝上解释，产出的一侧就必须按底朝上来写。
static HBITMAP MakeTopWhiteBottomBlackBitmap(int w, int h)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = h;      // 正数 = 底朝上
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int imgY = 0; imgY < h; ++imgY)      // imgY：图像自上而下的行号
    {
        BYTE v = (imgY < h / 2) ? 255 : 0;    // 上半白、下半黑
        int memY = h - 1 - imgY;              // 底朝上：图像首行写到内存末行
        for (int x = 0; x < w; ++x)
        {
            BYTE* px = p + ((size_t)memY * w + x) * 4;
            px[0] = v;
            px[1] = v;
            px[2] = v;
            px[3] = 255;
        }
    }
    ::GdiFlush();
    return hbm;
}

// 一块可读像素的离屏画布，供 Draw 的结果做断言。
class OffscreenCanvas
{
public:
    OffscreenCanvas(int w, int h)
        : m_w(w), m_h(h), m_hbm(nullptr), m_bits(nullptr)
        , m_hdc(nullptr), m_old(nullptr)
    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth    = w;
        bi.bmiHeader.biHeight   = -h;
        bi.bmiHeader.biPlanes   = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        m_hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
        if (m_hbm)
        {
            m_hdc = ::CreateCompatibleDC(nullptr);
            m_old = ::SelectObject(m_hdc, m_hbm);
        }
    }
    ~OffscreenCanvas()
    {
        if (m_hdc)
        {
            ::SelectObject(m_hdc, m_old);
            ::DeleteDC(m_hdc);
        }
        if (m_hbm)
        {
            ::DeleteObject(m_hbm);
        }
    }
    bool ok() const { return m_hdc != nullptr && m_bits != nullptr; }
    HDC  dc() const { return m_hdc; }

    // 用纯色铺满整块画布（alpha 一并写成 255，便于事后区分"没画过"）。
    void Fill(BYTE r, BYTE g, BYTE b)
    {
        BYTE* p = (BYTE*)m_bits;
        for (int i = 0; i < m_w * m_h; ++i)
        {
            p[i*4 + 0] = b;
            p[i*4 + 1] = g;
            p[i*4 + 2] = r;
            p[i*4 + 3] = 255;
        }
        ::GdiFlush();
    }

    // 取 (x, y) 处的分量。GDI 对 DIBSection 的写入有缓存，读之前须先冲。
    BYTE R(int x, int y) const { return Comp(x, y, 2); }
    BYTE G(int x, int y) const { return Comp(x, y, 1); }
    BYTE B(int x, int y) const { return Comp(x, y, 0); }

private:
    BYTE Comp(int x, int y, int idx) const
    {
        ::GdiFlush();
        const BYTE* p = (const BYTE*)m_bits;
        return p[((size_t)y * m_w + x) * 4 + idx];
    }

    int    m_w;
    int    m_h;
    HBITMAP m_hbm;
    void*  m_bits;
    HDC    m_hdc;
    HGDIOBJ m_old;
};

// ----- 绘制质量：缩小时必须做面积平均，而不是丢像素 -------------------
//
// 这条用例锁的是本次修复的核心：Draw 以前走 GDI 的 StretchBlt(HALFTONE)，
// 缩放质量差、放大时更是退化成像素复制；现在走 GDI+ 的 HighQualityBicubic。
// 拿 1px 黑白条纹按 2:1 缩小，正确的重采样会把相邻黑白平均成中灰。
static Result Test_DrawDownscaleAverages()
{
    const int kSrcSide = 16;
    const int kDstSide = 8;
    HBITMAP hbm = MakeStripeBitmap(kSrcSide, kSrcSide);
    EXPECT_TRUE(hbm != nullptr, _T("DownAvg/src"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true);

    OffscreenCanvas cv(kDstSide, kDstSide);
    EXPECT_TRUE(cv.ok(), _T("DownAvg/canvas"));
    cv.Fill(0, 255, 0);   // 铺成绿色：若 Draw 没画，断言会明显失败

    RECTL rc = { 0, 0, kDstSide, kDstSide };
    EXPECT_HR(p->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr,
                      cv.dc(), cv.dc(), &rc, nullptr, nullptr, 0),
              _T("DownAvg/hr"));

    // 逐像素检查中间一行：每个目标像素都应落在中灰附近。容差放得比较宽，
    // 因为不同 Windows 版本的 GDI+ 双三次实现有细微差异；但"点采样"会给出
    // 0 或 255，无论如何进不了这个区间。
    const int kGrayLo = 80;
    const int kGrayHi = 175;
    const int kProbeRow = kDstSide / 2;
    for (int x = 1; x < kDstSide - 1; ++x)   // 跳过两端，边界像素本就取不到完整邻域
    {
        BYTE v = cv.R(x, kProbeRow);
        if (v < kGrayLo || v > kGrayHi)
        {
            CString d;
            d.Format(_T("x=%d expected gray in [%d,%d] got=%d"),
                     x, kGrayLo, kGrayHi, (int)v);
            p->Release();
            return Fail(_T("DownAvg/gray"), d);
        }
    }

    p->Release();
    return OK(_T("DrawDownscaleAverages"));
}

// ----- 绘制质量：alpha 必须参与混合，而不是被当成不透明像素覆盖 -------
//
// 全透明的源画到白底上，白底应当原样保留。老实现用 StretchBlt(SRCCOPY)，
// 会把源里的 0 当成黑色直接盖上去 —— 这正是表情边缘发脏的成因。
static Result Test_DrawHonorsAlpha()
{
    const int kSide = 8;
    HBITMAP hbm = MakeTransparentBitmap(kSide, kSide);
    EXPECT_TRUE(hbm != nullptr, _T("Alpha/src"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true,
                                       /*hasPremultipliedAlpha=*/true);

    OffscreenCanvas cv(kSide, kSide);
    EXPECT_TRUE(cv.ok(), _T("Alpha/canvas"));
    cv.Fill(255, 255, 255);

    RECTL rc = { 0, 0, kSide, kSide };
    EXPECT_HR(p->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr,
                      cv.dc(), cv.dc(), &rc, nullptr, nullptr, 0),
              _T("Alpha/hr"));

    const int kMid = kSide / 2;
    EXPECT_INT((int)cv.R(kMid, kMid), 255, _T("Alpha/keepR"));
    EXPECT_INT((int)cv.G(kMid, kMid), 255, _T("Alpha/keepG"));
    EXPECT_INT((int)cv.B(kMid, kMid), 255, _T("Alpha/keepB"));

    p->Release();
    return OK(_T("DrawHonorsAlpha"));
}

// ----- 绘制方向：不许上下颠倒 -----------------------------------------
//
// 这条用例锁的是一个真实踩过的坑：带 alpha 的位图走的是"把 DIBSection 的像素
// 内存直接套成 GDI+ 位图"这条路，行序全靠 GetObject 回报的 biHeight 符号判断。
// 而 GetObject 并不保留"顶朝下"信息，一旦产出端建成顶朝下，画出来就整个上下
// 翻转。用上白下黑的源图一测便知 —— 左右对称的图案（如竖条纹）测不出这个问题。
static Result Test_DrawNotVerticallyFlipped()
{
    const int kSide = 16;
    HBITMAP hbm = MakeTopWhiteBottomBlackBitmap(kSide, kSide);
    EXPECT_TRUE(hbm != nullptr, _T("Flip/src"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true,
                                       /*hasPremultipliedAlpha=*/true);

    OffscreenCanvas cv(kSide, kSide);
    EXPECT_TRUE(cv.ok(), _T("Flip/canvas"));
    cv.Fill(0, 255, 0);   // 绿底：没画上去的话断言会明显失败

    RECTL rc = { 0, 0, kSide, kSide };
    EXPECT_HR(p->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr,
                      cv.dc(), cv.dc(), &rc, nullptr, nullptr, 0),
              _T("Flip/hr"));

    // 取上下各四分之一处采样，避开中间的过渡带。
    const int kNearWhite = 200;
    const int kNearBlack = 55;
    const int kUpperY = kSide / 4;
    const int kLowerY = kSide * 3 / 4;
    const int kProbeX = kSide / 2;

    BYTE upper = cv.R(kProbeX, kUpperY);
    BYTE lower = cv.R(kProbeX, kLowerY);
    if (upper < kNearWhite || lower > kNearBlack)
    {
        CString d;
        d.Format(_T("upper=%d (expect >=%d) lower=%d (expect <=%d) —— 疑似上下颠倒"),
                 (int)upper, kNearWhite, (int)lower, kNearBlack);
        p->Release();
        return Fail(_T("Flip/orientation"), d);
    }

    p->Release();
    return OK(_T("DrawNotVerticallyFlipped"));
}

// ----- 排版尺寸与源位图尺寸解耦 ---------------------------------------
//
// 表情靠这条特性工作：源图保持 56×56 全分辨率（绘制时才缩放，只缩一次），
// 但只占 28×28 的版面。GetExtent 必须报告排版尺寸换算出的 himetric，
// 而不是源位图尺寸。
static Result Test_DisplaySizeDecoupled()
{
    const int kSrcSide = 56;
    const int kDisplaySide = 28;
    HBITMAP hbm = MakeFlatBitmap(kSrcSide, kSrcSide, 10, 20, 30);
    EXPECT_TRUE(hbm != nullptr, _T("Disp/src"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true,
                                       /*hasPremultipliedAlpha=*/false,
                                       kDisplaySide, kDisplaySide);

    EXPECT_INT((int)p->GetPixelSize().cx,   kSrcSide,     _T("Disp/srcKept"));
    EXPECT_INT((int)p->GetDisplaySize().cx, kDisplaySide, _T("Disp/display"));

    IViewObject2* pView = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IViewObject2, (void**)&pView), _T("Disp/qi"));
    SIZEL sz = {};
    EXPECT_HR(pView->GetExtent(DVASPECT_CONTENT, -1, nullptr, &sz), _T("Disp/extHr"));
    pView->Release();

    // 28 px @ 96dpi 换算成 himetric：28 * 2540 / 96 = 741。
    const int kExpectHimetric = 741;
    EXPECT_INT((int)sz.cx, kExpectHimetric, _T("Disp/extCx"));
    EXPECT_INT((int)sz.cy, kExpectHimetric, _T("Disp/extCy"));

    // 不指定排版尺寸时退回"按源位图像素排版"的老行为。
    HBITMAP hbm2 = MakeFlatBitmap(kSrcSide, kSrcSide, 10, 20, 30);
    CDuiImageOle* p2 = new CDuiImageOle(hbm2, true);
    EXPECT_INT((int)p2->GetDisplaySize().cx, kSrcSide, _T("Disp/defaultsToSrc"));
    p2->Release();

    p->Release();
    return OK(_T("DisplaySizeDecoupled"));
}

// ----- ref counting ---------------------------------------------------

static Result Test_AddRefRelease()
{
    HBITMAP hbm = MakeFlatBitmap(8, 8, 200, 100, 50);
    EXPECT_TRUE(hbm != nullptr, _T("Ref/srcDib"));
    CDuiImageOle* p = new CDuiImageOle(hbm, /*ownsHbm=*/true);
    EXPECT_INT(p->GetRefCount(), 1, _T("Ref/init"));
    p->AddRef();
    EXPECT_INT(p->GetRefCount(), 2, _T("Ref/+1"));
    p->Release();
    EXPECT_INT(p->GetRefCount(), 1, _T("Ref/-1"));
    p->Release();   // deletes; we no longer touch p (and hbm is freed by ownsHbm)
    return OK(_T("AddRefRelease"));
}

// ----- QueryInterface lands on every advertised iface -----------------

static Result Test_QueryInterface()
{
    HBITMAP hbm = MakeFlatBitmap(4, 4, 0, 0, 0);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    IOleObject*      o1 = nullptr;
    IViewObject2*    o2 = nullptr;
    IDataObject*     o3 = nullptr;
    IPersist*        o4 = nullptr;
    IPersistStorage* o5 = nullptr;
    IUnknown*        o6 = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IOleObject,      (void**)&o1), _T("QI/Ole"));
    EXPECT_HR(p->QueryInterface(IID_IViewObject2,    (void**)&o2), _T("QI/View"));
    EXPECT_HR(p->QueryInterface(IID_IDataObject,     (void**)&o3), _T("QI/Data"));
    EXPECT_HR(p->QueryInterface(IID_IPersist,        (void**)&o4), _T("QI/Persist"));
    EXPECT_HR(p->QueryInterface(IID_IPersistStorage, (void**)&o5), _T("QI/PersistStg"));
    EXPECT_HR(p->QueryInterface(IID_IUnknown,        (void**)&o6), _T("QI/Unk"));
    o1->Release();
    o2->Release();
    o3->Release();
    o4->Release();
    o5->Release();
    o6->Release();

    // Bogus IID returns E_NOINTERFACE.
    void* nothing = nullptr;
    HRESULT hr = p->QueryInterface(IID_IClassFactory, &nothing);
    EXPECT_INT(hr, E_NOINTERFACE, _T("QI/none"));
    EXPECT_TRUE(nothing == nullptr, _T("QI/zeroOut"));
    p->Release();
    return OK(_T("QueryInterface"));
}

// ----- IViewObject2::GetExtent returns himetric size -----------------

static Result Test_GetExtent()
{
    HBITMAP hbm = MakeFlatBitmap(96, 48, 0, 0, 0);    // 1in × 0.5in @ 96dpi
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    // Both IOleObject and IViewObject2 expose a GetExtent method with
    // different arity. Go through QueryInterface so the call binds to
    // the right vtable slot without name-lookup ambiguity.
    IViewObject2* pView = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IViewObject2, (void**)&pView), _T("Ext/qi"));
    SIZEL sz = {};
    EXPECT_HR(pView->GetExtent(DVASPECT_CONTENT, -1, nullptr, &sz), _T("Ext/hr"));
    pView->Release();

    // 96px @ 96dpi = 1 inch = 2540 himetric units.
    EXPECT_INT(sz.cx, 2540, _T("Ext/cx"));
    EXPECT_INT(sz.cy, 1270, _T("Ext/cy"));

    // Also exercise the IOleObject::GetExtent variant (2 args).
    SIZEL sz2 = {};
    IOleObject* pOle = nullptr;
    EXPECT_HR(p->QueryInterface(IID_IOleObject, (void**)&pOle), _T("Ext/qiOle"));
    EXPECT_HR(pOle->GetExtent(DVASPECT_CONTENT, &sz2), _T("Ext/oleHr"));
    pOle->Release();
    EXPECT_INT(sz2.cx, 2540, _T("Ext/oleCx"));
    EXPECT_INT(sz2.cy, 1270, _T("Ext/oleCy"));

    p->Release();
    return OK(_T("GetExtent"));
}

// ----- IDataObject::QueryGetData / GetData CF_BITMAP ------------------

static Result Test_DataObjectBitmap()
{
    HBITMAP hbm = MakeFlatBitmap(16, 16, 80, 160, 240);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    FORMATETC fmt = {};
    fmt.cfFormat = CF_BITMAP;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex   = -1;
    fmt.tymed    = TYMED_GDI;
    EXPECT_HR(p->QueryGetData(&fmt), _T("Data/qOk"));

    fmt.cfFormat = CF_TEXT;
    EXPECT_INT(p->QueryGetData(&fmt), S_FALSE, _T("Data/qNotText"));

    fmt.cfFormat = CF_BITMAP;
    STGMEDIUM med = {};
    EXPECT_HR(p->GetData(&fmt, &med), _T("Data/get"));
    EXPECT_INT((int)med.tymed, (int)TYMED_GDI, _T("Data/tymed"));
    EXPECT_TRUE(med.hBitmap != nullptr, _T("Data/hbm"));
    EXPECT_TRUE(med.hBitmap != hbm, _T("Data/copy"));     // CopyImage returns a new HBITMAP
    if (med.hBitmap)
    {
        ::DeleteObject(med.hBitmap);
    }
    p->Release();
    return OK(_T("DataObjectBitmap"));
}

// ----- Draw smoke onto a memory DC ------------------------------------

static Result Test_DrawSmoke()
{
    HBITMAP hbm = MakeFlatBitmap(16, 16, 200, 50, 50);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 32;
    bi.bmiHeader.biHeight   = -32;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP dst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    EXPECT_TRUE(dst != nullptr, _T("Draw/dst"));
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);

    RECTL rc = { 0, 0, 32, 32 };
    HRESULT hr = p->Draw(DVASPECT_CONTENT, -1, nullptr, nullptr,
                         hdc, hdc, &rc, nullptr, nullptr, 0);
    EXPECT_HR(hr, _T("Draw/hr"));

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);
    p->Release();
    return OK(_T("DrawSmoke"));
}

// ----- SetClientSite / GetClientSite ref counting --------------------

class FakeSite : public IOleClientSite
{
public:
    LONG ref = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IOleClientSite)
        {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef () override { return (ULONG)InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { return (ULONG)InterlockedDecrement(&ref); }
    HRESULT STDMETHODCALLTYPE SaveObject() override                                      { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override               { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer**) override                      { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE ShowObject() override                                       { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override                                 { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override                           { return E_NOTIMPL; }
};

static Result Test_ClientSiteRef()
{
    HBITMAP hbm = MakeFlatBitmap(2, 2, 0, 0, 0);
    CDuiImageOle* p = new CDuiImageOle(hbm, true);
    FakeSite site;     // ref = 1 (initial)
    p->SetClientSite(&site);
    EXPECT_INT((int)site.ref, 2, _T("Site/+1"));
    p->Release();      // drops site ref
    EXPECT_INT((int)site.ref, 1, _T("Site/-1onClose"));
    return OK(_T("ClientSiteRef"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_HR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("AddRefRelease"),   &Test_AddRefRelease   },
        { _T("QueryInterface"),  &Test_QueryInterface  },
        { _T("GetExtent"),       &Test_GetExtent       },
        { _T("DataObjectBitmap"),&Test_DataObjectBitmap},
        { _T("DrawSmoke"),       &Test_DrawSmoke       },
        { _T("DrawDownscaleAverages"), &Test_DrawDownscaleAverages },
        { _T("DrawHonorsAlpha"),       &Test_DrawHonorsAlpha       },
        { _T("DrawNotVerticallyFlipped"), &Test_DrawNotVerticallyFlipped },
        { _T("DisplaySizeDecoupled"),  &Test_DisplaySizeDecoupled  },
        { _T("ClientSiteRef"),   &Test_ClientSiteRef   },
    };

    CString out;
    int passed = 0, failed = 0;
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
    summary.Format(_T("[summary] DuiImageOleTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiImageOleTests

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
