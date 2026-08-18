#include "stdafx.h"
#include "DuiImageOle.h"

#if BUI_FEATURE_IMAGEOLE

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

namespace {

// 带 alpha 的位图必须是每像素 32 位（BGRA 四个通道各 8 位）；低于此位深的
// 位图没有 alpha 通道可言，只能走不透明路径。
const int kAlphaBitsPerPixel = 32;

// GDI+ 一次性启动。Draw 可能在任何一次富文本重绘里被调到，未必先经过本文件
// 的插入路径（截图那条路可以直接传位图进来），所以本文件自己兜一次底。
// token 故意不释放，与本代码库其余处的 GDI+ 用法一致。
void EnsureGdiplusForImageOle()
{
    static ULONG_PTR s_token = 0;
    if (s_token != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&s_token, &in, nullptr);
}

} // anonymous

// =====================================================================
// CDuiImageOle: minimal OLE wrapper around an HBITMAP for embedding into
// a RichEdit. Only the methods RichEdit actually uses do work — the rest
// return E_NOTIMPL or S_OK to satisfy the interfaces. This is enough for
// in-memory render of the image; on-disk RTF persistence is out of scope.
// =====================================================================

CDuiImageOle::CDuiImageOle(HBITMAP hbm, bool ownsHbm,
                           bool hasPremultipliedAlpha,
                           int displayW, int displayH)
    : m_ref(1)
    , m_hbm(hbm)
    , m_ownsHbm(ownsHbm)
    , m_hasAlpha(hasPremultipliedAlpha)
    , m_pxSize{ 0, 0 }
    , m_displayPx{ displayW, displayH }
    , m_himetric{ 0, 0 }
    , m_pSite(nullptr)
{
    ComputeSizes();
}

CDuiImageOle::~CDuiImageOle()
{
    if (m_pSite)
    {
        m_pSite->Release();
        m_pSite = nullptr;
    }
    if (m_ownsHbm && m_hbm)
    {
        ::DeleteObject(m_hbm);
        m_hbm = nullptr;
    }
}

void CDuiImageOle::ComputeSizes()
{
    BITMAP bm = {};
    if (m_hbm && ::GetObject(m_hbm, sizeof(bm), &bm) == sizeof(bm))
    {
        m_pxSize.cx = bm.bmWidth;
        m_pxSize.cy = bm.bmHeight;
    }
    else
    {
        m_pxSize.cx = m_pxSize.cy = 0;
    }

    // 未显式指定排版尺寸时按源位图的像素尺寸排版（历史行为）。指定了就用
    // 指定值 —— 表情走的正是这条：源图保持 56×56 全分辨率，只按 28×28 排版。
    if (m_displayPx.cx <= 0 || m_displayPx.cy <= 0)
    {
        m_displayPx = m_pxSize;
    }

    // Convert px -> himetric (0.01mm). 1 inch = 96 px (DPI-agnostic
    // baseline), 1 inch = 2540 himetric units. RichEdit uses these for
    // layout.
    m_himetric.cx = ::MulDiv(m_displayPx.cx, 2540, 96);
    m_himetric.cy = ::MulDiv(m_displayPx.cy, 2540, 96);
}

// ----- IUnknown -------------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IOleObject*>(this);
    }
    else if (riid == IID_IOleObject)
    {
        *ppv = static_cast<IOleObject*>(this);
    }
    else if (riid == IID_IViewObject)
    {
        *ppv = static_cast<IViewObject2*>(this);
    }
    else if (riid == IID_IViewObject2)
    {
        *ppv = static_cast<IViewObject2*>(this);
    }
    else if (riid == IID_IDataObject)
    {
        *ppv = static_cast<IDataObject*>(this);
    }
    else if (riid == IID_IPersist)
    {
        *ppv = static_cast<IPersistStorage*>(this);
    }
    else if (riid == IID_IPersistStorage)
    {
        *ppv = static_cast<IPersistStorage*>(this);
    }
    if (!*ppv)
    {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE CDuiImageOle::AddRef()
{
    return (ULONG)InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE CDuiImageOle::Release()
{
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0)
    {
        delete this;
    }
    return (ULONG)r;
}

// ----- IOleObject (mostly stubs) --------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetClientSite(IOleClientSite* pSite)
{
    if (m_pSite)
    {
        m_pSite->Release();
    }
    m_pSite = pSite;
    if (m_pSite)
    {
        m_pSite->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClientSite(IOleClientSite** ppSite)
{
    if (!ppSite)
    {
        return E_POINTER;
    }
    *ppSite = m_pSite;
    if (m_pSite)
    {
        m_pSite->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::Close(DWORD)
{
    if (m_pSite)
    {
        m_pSite->Release();
        m_pSite = nullptr;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetHostNames(LPCOLESTR, LPCOLESTR)            { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetMoniker(DWORD, IMoniker*)                  { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetMoniker(DWORD, DWORD, IMoniker**)          { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::InitFromData(IDataObject*, BOOL, DWORD)       { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClipboardData(DWORD, IDataObject**)        { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::DoVerb(LONG, LPMSG, IOleClientSite*, LONG,
                                               HWND, LPCRECT)                          { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumVerbs(IEnumOLEVERB**)                     { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Update()                                      { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::IsUpToDate()                                  { return S_OK; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetUserClassID(CLSID* pClsid)
{
    if (pClsid)
    {
        *pClsid = CLSID_NULL;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetUserType(DWORD, LPOLESTR*)                 { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::SetExtent(DWORD, SIZEL* pSize)
{
    if (pSize)
    {
        m_himetric = *pSize;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetExtent(DWORD, SIZEL* pSize)
{
    if (!pSize)
    {
        return E_POINTER;
    }
    *pSize = m_himetric;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::Advise(IAdviseSink*, DWORD*)                  { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Unadvise(DWORD)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumAdvise(IEnumSTATDATA**)                   { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetMiscStatus(DWORD, DWORD* pdw)
{
    if (pdw)
    {
        *pdw = OLEMISC_RECOMPOSEONRESIZE | OLEMISC_INSIDEOUT;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetColorScheme(LOGPALETTE*)                   { return E_NOTIMPL; }

// ----- IViewObject2 ---------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::Draw(
    DWORD /*dwAspect*/, LONG /*lindex*/, void* /*pvAspect*/,
    DVTARGETDEVICE* /*ptd*/,
    HDC /*hdcTargetDev*/, HDC hdcDraw,
    LPCRECTL lprcBounds, LPCRECTL /*lprcWBounds*/,
    BOOL (STDMETHODCALLTYPE* /*pfnContinue*/)(ULONG_PTR), ULONG_PTR /*dwContinue*/)
{
    if (!hdcDraw || !lprcBounds || !m_hbm)
    {
        return E_INVALIDARG;
    }

    const int x  = lprcBounds->left;
    const int y  = lprcBounds->top;
    const int w  = lprcBounds->right  - lprcBounds->left;
    const int h  = lprcBounds->bottom - lprcBounds->top;
    const int sw = m_pxSize.cx;
    const int sh = m_pxSize.cy;
    if (sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
    {
        return S_OK;
    }

    // 走 GDI+ 高质量重采样，而不是 GDI 的 StretchBlt。原因：StretchBlt 即便
    // 设成 HALFTONE，缩小时质量也一般，放大时更是退化成像素复制（源像素直接
    // 摊成色块），表情这种带柔和边缘的小图会明显发糊、出现台阶。GDI+ 从
    // <u>全分辨率源图</u>一次采样到 lprcBounds，无论 RichEdit 最终算出多大的
    // 矩形都只经历一次缩放。
    if (DrawWithGdiplus(hdcDraw, x, y, w, h))
    {
        return S_OK;
    }

    // GDI+ 不可用（启动失败 / 位图格式意外）时退回原来的 StretchBlt，
    // 保证"画得不够好"而不是"画不出来"。
    HDC mem = ::CreateCompatibleDC(hdcDraw);
    if (!mem)
    {
        return E_FAIL;
    }
    HGDIOBJ old = ::SelectObject(mem, m_hbm);
    ::SetStretchBltMode(hdcDraw, HALFTONE);
    ::SetBrushOrgEx(hdcDraw, 0, 0, nullptr);
    ::StretchBlt(hdcDraw, x, y, w, h, mem, 0, 0, sw, sh, SRCCOPY);
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    return S_OK;
}

bool CDuiImageOle::DrawWithGdiplus(HDC hdcDraw, int x, int y, int w, int h)
{
    EnsureGdiplusForImageOle();

    Gdiplus::Bitmap* pSrc = nullptr;

    if (m_hasAlpha)
    {
        // 带预乘 alpha：不能用 Bitmap::FromHBITMAP —— 它会丢掉 alpha 通道。
        // 直接在 DIBSection 的像素内存上"套"一个 GDI+ 位图（不复制像素），
        // 声明格式为 32bppPARGB，GDI+ 便会按预乘语义正确混合。
        DIBSECTION ds;
        ::ZeroMemory(&ds, sizeof(ds));
        if (::GetObject(m_hbm, sizeof(ds), &ds) == sizeof(ds)
            && ds.dsBm.bmBits != nullptr
            && ds.dsBm.bmBitsPixel == kAlphaBitsPerPixel)
        {
            // 按 biHeight 的符号判断行序：>0 为底朝上（内存首行是图像末行），
            // 此时图像首行位于内存末行处、行序反向，正是 GDI+ "负 stride +
            // scan0 指向图像首行" 所表达的布局。
            //
            // 注意：实测 GetObject 对<u>顶朝下</u>建出来的 DIBSection 也回报正的
            // biHeight（用负 biHeight 调 CreateDIBSection，取回来仍是正数），
            // 也就是说方向信息在 GetObject 这一层就丢了，实际总是走这个分支。
            // 因此产出位图的一侧必须统一建成底朝上 —— 见本类的
            // LoadPremultipliedDibFromFile。下面的顶朝下分支只作防御保留。
            INT   stride = ds.dsBm.bmWidthBytes;
            BYTE* scan0  = (BYTE*)ds.dsBm.bmBits;
            if (ds.dsBmih.biHeight > 0)
            {
                scan0  += (size_t)stride * (m_pxSize.cy - 1);
                stride  = -stride;
            }
            pSrc = new Gdiplus::Bitmap(m_pxSize.cx, m_pxSize.cy, stride,
                                       PixelFormat32bppPARGB, scan0);
        }
    }
    else
    {
        // 不透明位图：FromHBITMAP 会复制一份像素，本次绘制用完即弃。
        pSrc = Gdiplus::Bitmap::FromHBITMAP(m_hbm, nullptr);
    }

    if (pSrc == nullptr)
    {
        return false;
    }
    if (pSrc->GetLastStatus() != Gdiplus::Ok)
    {
        delete pSrc;
        return false;
    }

    Gdiplus::Graphics g(hdcDraw);
    // HighQualityBicubic：缩小时带预滤波，小图缩放后边缘干净；
    // PixelOffsetModeHalf：把采样点对到像素中心，避免整体偏半个像素。
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    // 指定源矩形（而不是只给目标矩形），否则 GDI+ 会按 DC 的分辨率把源图
    // 当成"有物理尺寸的图像"换算，得到的缩放比例并非我们要的像素级映射。
    Gdiplus::Status st = g.DrawImage(
        pSrc, Gdiplus::Rect(x, y, w, h),
        0, 0, m_pxSize.cx, m_pxSize.cy, Gdiplus::UnitPixel);
    g.Flush(Gdiplus::FlushIntentionSync);

    delete pSrc;
    return st == Gdiplus::Ok;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetColorSet(
    DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LOGPALETTE**)
{
    return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::Freeze(DWORD, LONG, void*, DWORD*)            { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Unfreeze(DWORD)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetAdvise(DWORD, DWORD, IAdviseSink*)         { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetAdvise(DWORD* pa, DWORD* pf, IAdviseSink** ps)
{
    if (pa)
    {
        *pa = 0;
    }
    if (pf)
    {
        *pf = 0;
    }
    if (ps)
    {
        *ps = nullptr;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetExtent(DWORD, LONG, DVTARGETDEVICE*, LPSIZEL pSize)
{
    if (!pSize)
    {
        return E_POINTER;
    }
    *pSize = m_himetric;
    return S_OK;
}

// ----- IDataObject ----------------------------------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetData(FORMATETC* pFmt, STGMEDIUM* pMed)
{
    if (!pFmt || !pMed)
    {
        return E_POINTER;
    }
    if (pFmt->cfFormat != CF_BITMAP)
    {
        return DV_E_FORMATETC;
    }
    if (!(pFmt->tymed & TYMED_GDI))
    {
        return DV_E_TYMED;
    }
    if (!m_hbm)
    {
        return E_FAIL;
    }

    // 必须真正复制一份：调用方（RichEdit 的复制 / 拖出流程）会自行
    // DeleteObject 拿到的句柄。原来带 LR_COPYRETURNORG，一旦源位图恰好满足
    // "尺寸与色深都不用变"就会把<u>本对象持有的那张位图</u>原样交出去，等对方
    // 删掉就成了二次释放 —— 表情改用 DIBSection 之后正好会落进这种情形。
    // 代价是拿到的是 DDB、alpha 丢失，这本就是 CF_BITMAP 的固有限制。
    pMed->tymed = TYMED_GDI;
    pMed->hBitmap = (HBITMAP)::CopyImage(m_hbm, IMAGE_BITMAP, 0, 0, 0);
    pMed->pUnkForRelease = nullptr;
    return pMed->hBitmap ? S_OK : E_FAIL;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::GetDataHere(FORMATETC*, STGMEDIUM*)           { return E_NOTIMPL; }

HRESULT STDMETHODCALLTYPE CDuiImageOle::QueryGetData(FORMATETC* pFmt)
{
    if (!pFmt)
    {
        return E_POINTER;
    }
    if (pFmt->cfFormat == CF_BITMAP && (pFmt->tymed & TYMED_GDI))
    {
        return S_OK;
    }
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetCanonicalFormatEtc(FORMATETC*, FORMATETC* pOut)
{
    if (pOut)
    {
        ZeroMemory(pOut, sizeof(*pOut));
        pOut->cfFormat = CF_BITMAP;
        pOut->dwAspect = DVASPECT_CONTENT;
        pOut->lindex   = -1;
        pOut->tymed    = TYMED_GDI;
    }
    return DATA_S_SAMEFORMATETC;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::SetData(FORMATETC*, STGMEDIUM*, BOOL)         { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumFormatEtc(DWORD, IEnumFORMATETC**)        { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*)
{
    return OLE_E_ADVISENOTSUPPORTED;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::DUnadvise(DWORD)                              { return OLE_E_ADVISENOTSUPPORTED; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::EnumDAdvise(IEnumSTATDATA**)                  { return OLE_E_ADVISENOTSUPPORTED; }

// ----- IPersist / IPersistStorage (no-op) -----------------------------

HRESULT STDMETHODCALLTYPE CDuiImageOle::GetClassID(CLSID* pClsid)
{
    if (pClsid)
    {
        *pClsid = CLSID_NULL;
    }
    return S_OK;
}
HRESULT STDMETHODCALLTYPE CDuiImageOle::IsDirty()                                     { return S_FALSE; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::InitNew(IStorage*)                            { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Load(IStorage*)                               { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::Save(IStorage*, BOOL)                         { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::SaveCompleted(IStorage*)                      { return S_OK; }
HRESULT STDMETHODCALLTYPE CDuiImageOle::HandsOffStorage()                             { return S_OK; }

// =====================================================================
// InsertIntoRichEditOle: full plumbing path. Returns true on success.
// =====================================================================

HBITMAP CDuiImageOle::LoadPremultipliedDibFromFile(LPCTSTR path, SIZE* outSize)
{
    if (path == nullptr || *path == _T('\0'))
    {
        return nullptr;
    }
    EnsureGdiplusForImageOle();

    Gdiplus::Bitmap src(path);
    if (src.GetLastStatus() != Gdiplus::Ok)
    {
        return nullptr;
    }
    const int w = (int)src.GetWidth();
    const int h = (int)src.GetHeight();
    if (w <= 0 || h <= 0)
    {
        return nullptr;
    }

    // 高度取正数 = 底朝上：内存里的第 0 行是图像的**最后**一行。
    // 理由见头文件里本方法的注释 —— 库内取位图信息的地方只能表达这一种行序。
    BITMAPINFO bi;
    ::ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                     &pBits, nullptr, 0);
    if (hbm == nullptr || pBits == nullptr)
    {
        if (hbm != nullptr)
        {
            ::DeleteObject(hbm);
        }
        return nullptr;
    }

    // 32 位色每行天然 4 字节对齐，行字节数就是宽度乘 4。
    const int kBytesPerPixel = 4;
    const int dstStride = w * kBytesPerPixel;

    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    ::ZeroMemory(&bd, sizeof(bd));
    if (src.LockBits(&rc, Gdiplus::ImageLockModeRead,
                     PixelFormat32bppPARGB, &bd) != Gdiplus::Ok)
    {
        ::DeleteObject(hbm);
        return nullptr;
    }
    // GDI+ 的第 y 行是图像自上而下的第 y 行；目标位图底朝上，故写到倒数第
    // y 行去，等于在拷贝的同时完成上下翻转。
    for (int y = 0; y < h; ++y)
    {
        const BYTE* srcRow = (const BYTE*)bd.Scan0 + (INT_PTR)bd.Stride * y;
        BYTE* dstRow = (BYTE*)pBits + (INT_PTR)dstStride * (h - 1 - y);
        ::memcpy(dstRow, srcRow, dstStride);
    }
    src.UnlockBits(&bd);

    // 系统对这类位图的写入有缓存，交给别的绘图调用之前须冲一下，
    // 否则首次绘制可能读到尚未落盘的像素。
    ::GdiFlush();

    if (outSize != nullptr)
    {
        outSize->cx = w;
        outSize->cy = h;
    }
    return hbm;
}

bool CDuiImageOle::InsertIntoRichEditOle(IRichEditOle* pRiche, HBITMAP hbm,
                                         bool ownsHbm, DWORD_PTR dwUser,
                                         bool hasPremultipliedAlpha,
                                         int displayW, int displayH)
{
    if (!pRiche || !hbm)
    {
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }

    IOleClientSite* pSite = nullptr;
    pRiche->GetClientSite(&pSite);

    IStorage* pStg = nullptr;
    LPLOCKBYTES pLock = nullptr;
    if (FAILED(::CreateILockBytesOnHGlobal(NULL, TRUE, &pLock)))
    {
        if (pSite)
        {
            pSite->Release();
        }
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }
    if (FAILED(::StgCreateDocfileOnILockBytes(pLock,
            STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &pStg)))
    {
        pLock->Release();
        if (pSite)
        {
            pSite->Release();
        }
        if (ownsHbm)
        {
            ::DeleteObject(hbm);
        }
        return false;
    }

    CDuiImageOle* obj = new CDuiImageOle(hbm, ownsHbm, hasPremultipliedAlpha,
                                         displayW, displayH);   // ref=1
    obj->SetClientSite(pSite);
    ::OleSetContainedObject(static_cast<IOleObject*>(obj), TRUE);

    REOBJECT reo = {};
    reo.cbStruct = sizeof(REOBJECT);
    reo.clsid    = CLSID_NULL;
    reo.cp       = REO_CP_SELECTION;
    reo.dvaspect = DVASPECT_CONTENT;
    reo.dwFlags  = REO_BELOWBASELINE;
    reo.poleobj  = static_cast<IOleObject*>(obj);
    reo.pstg     = pStg;
    reo.polesite = pSite;
    // 排版尺寸取 GetDisplaySize（未指定时它等于源位图像素尺寸），而不是源
    // 位图尺寸 —— 表情的源图是 56×56 全分辨率，但只应占 28×28 的版面。
    SIZEL ext = obj->GetDisplaySize().cx > 0
        ? SIZEL{ ::MulDiv(obj->GetDisplaySize().cx, 2540, 96),
                 ::MulDiv(obj->GetDisplaySize().cy, 2540, 96) }
        : SIZEL{ 0, 0 };
    reo.sizel    = ext;
    // 应用自定义标记（聊天表情走这里存 faceId）。REOBJECT.dwUser 由 Win32 定义为
    // DWORD，即便 64 位下本方法的入参是 DWORD_PTR，能被 RichEdit 保存并读回的也
    // 只有低 32 位，因此这里显式收窄，不会丢失实际可用的信息。
    reo.dwUser   = static_cast<DWORD>(dwUser);

    HRESULT hr = pRiche->InsertObject(&reo);

    obj->Release();   // RichEdit AddRef'd what it needs
    if (pStg)
    {
        pStg->Release();
    }
    if (pLock)
    {
        pLock->Release();
    }
    if (pSite)
    {
        pSite->Release();
    }
    // **不释放 pRiche** —— 它是调用方借给我们的，谁取的谁释放。
    return SUCCEEDED(hr);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
