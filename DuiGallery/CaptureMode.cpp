#include "stdafx.h"
#include "CaptureMode.h"
#include "PageKit.h"
#include "PageRegistry.h"
#include "DuiHost.h"
#include "DuiDpi.h"

#include <vector>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace CaptureMode {

namespace {

// 离屏画布的尺寸（像素）。
//
// 高度必须装得下最长的那一页。文档配图夹具一页有 38 个段落，每段是
// "标题 + 说明 + 一行演示 + 段间空白"，实测总高已经超过 6000 —— 此前这个
// 值正是 6000，于是最后一段的配图长期是一张全黑的图，而且没有任何报错。
// 9000 给后续新增留出余量；对应的 32 位 DIB 约 40MB，现在的机器完全够用。
// 越界时的告警见下面截图循环里的判断。
const int kCanvasW = 1100;
const int kCanvasH = 9000;

// 每建完一页之后驱动重绘与消息循环的轮数。
// 前几轮让宿主把 DUI 控件树画进后台缓冲，后几轮让寄宿的真窗口子控件
// （输入框、富文本框）完成自己的排列与绘制。
const int kPumpRounds = 6;

// Owner window class for the headless host. Just provides an HWND that
// the DuiHost can attach to; does not paint anything itself.
class CaptureOwner : public CWindowImpl<CaptureOwner, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiGalleryCapture__"),
                         CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    BEGIN_MSG_MAP(CaptureOwner)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM, BOOL&) { return 0; }
};

// PNG encoder CLSID for GDI+ Bitmap::Save. Looked up once and cached.
bool GetPngEncoderClsid(CLSID& outClsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
    {
        return false;
    }
    BYTE* buf = new BYTE[size];
    Gdiplus::ImageCodecInfo* info =
        reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf);
    Gdiplus::GetImageEncoders(num, size, info);
    bool found = false;
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, L"image/png") == 0)
        {
            outClsid = info[i].Clsid;
            found = true;
            break;
        }
    }
    delete[] buf;
    return found;
}

// Save the contents of a DIB section (selected into srcDC) inside the
// rectangle (x, y, w, h) to outPath as a 32bpp PNG. Returns true on
// success.
bool SaveRegionAsPng(HDC srcDC, int x, int y, int w, int h, LPCTSTR outPath)
{
    if (w <= 0 || h <= 0)
    {
        return false;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;        // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
    if (!dib)
    {
        return false;
    }
    HDC memDC = ::CreateCompatibleDC(srcDC);
    HGDIOBJ old = ::SelectObject(memDC, dib);
    ::BitBlt(memDC, 0, 0, w, h, srcDC, x, y, SRCCOPY);
    ::SelectObject(memDC, old);
    ::DeleteDC(memDC);

    // Force opaque alpha - GDI's BitBlt does not write meaningful
    // alpha. Without this, GDI+ saves a partially-transparent PNG.
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i * 4 + 3] = 0xFF;
    }

    Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, p);
    CLSID clsid;
    bool ok = false;
    if (GetPngEncoderClsid(clsid))
    {
        ok = (bmp.Save(outPath, &clsid, nullptr) == Gdiplus::Ok);
    }
    ::DeleteObject(dib);
    return ok;
}

} // anonymous

// Snapshot composes:
//   1) BitBlt the host's DUI back buffer (covers all paint-only DUI
//      controls — buttons, labels, avatars, etc.).
//   2) Walk every visible child HWND of the host and paint each
//      child's content on top of the snapshot in its rect. No control
//      in the library hosts a real child window any more (the input
//      controls became windowless on 2026-08-17), so this step is
//      normally a no-op; it is kept so that a host which does create
//      its own child window still gets captured correctly.
//
// PrintWindow on a fully-off-screen window paints black on Win10+, so
// the "snapshot the whole window" path is not viable here; this
// composition trick gets the same end result.
//
// Caller must DeleteDC + DeleteObject the returned pair.
HDC SnapshotHostToDib(HWND host, HDC backDC, int w, int h,
                      HBITMAP& outDib)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
    if (!dib)
    {
        outDib = nullptr;
        return nullptr;
    }
    HDC scrDC  = ::GetDC(nullptr);
    HDC memDC  = ::CreateCompatibleDC(scrDC);
    ::ReleaseDC(nullptr, scrDC);
    ::SelectObject(memDC, dib);

    // 1) DUI back buffer.
    if (backDC)
    {
        ::BitBlt(memDC, 0, 0, w, h, backDC, 0, 0, SRCCOPY);
    }
    else
    {
        RECT rcAll = { 0, 0, w, h };
        ::FillRect(memDC, &rcAll, ::GetSysColorBrush(COLOR_BTNFACE));
    }

    // 2) Walk the host's child windows. Each child gets PrintWindow'd
    //    into its rect on top of the DUI paint. PrintWindow on a real
    //    HWND child (with its own paint cycle that already ran) does
    //    work even when the parent is off-screen, because the child
    //    is painting its own DC, not relying on the parent's
    //    composition.
    const UINT PW_CLIENTONLY_FLAG = 0x00000001;
    HWND child = ::GetWindow(host, GW_CHILD);
    while (child)
    {
        if (::IsWindowVisible(child))
        {
            RECT rChild;
            ::GetWindowRect(child, &rChild);
            POINT topLeft = { rChild.left, rChild.top };
            ::ScreenToClient(host, &topLeft);
            int cw = rChild.right  - rChild.left;
            int ch = rChild.bottom - rChild.top;
            if (cw > 0 && ch > 0)
            {
                // Save current DC origin, shift, print, restore.
                POINT oldOrg = {};
                ::GetViewportOrgEx(memDC, &oldOrg);
                ::SetViewportOrgEx(memDC,
                                   oldOrg.x + topLeft.x,
                                   oldOrg.y + topLeft.y,
                                   nullptr);
                ::PrintWindow(child, memDC, PW_CLIENTONLY_FLAG);
                ::SetViewportOrgEx(memDC, oldOrg.x, oldOrg.y, nullptr);
            }
        }
        child = ::GetWindow(child, GW_HWNDNEXT);
    }

    outDib = dib;
    return memDC;
}

int RunCaptureAll(LPCTSTR outDir)
{
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    ::CreateDirectory(outDir, nullptr);

    CaptureOwner owner;
    // WS_VISIBLE but positioned far off-screen so WM_PAINT actually
    // fires for the host and for any child window it creates. A truly
    // hidden window collapses paint and nothing would render.
    // WS_EX_TOOLWINDOW + WS_EX_NOACTIVATE keep it out of the taskbar
    // and prevent focus theft. WS_DISABLED stops accidental input.
    if (!owner.Create(NULL, CWindow::rcDefault, _T("DuiGallery Capture"),
                      WS_POPUP | WS_VISIBLE | WS_DISABLED,
                      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))
    {
        Gdiplus::GdiplusShutdown(gpToken);
        return -1;
    }
    owner.SetWindowPos(NULL, -32000, -32000, kCanvasW, kCanvasH,
                       SWP_NOZORDER | SWP_NOACTIVATE);

    DuiHost host;
    RECT rcClient = { 0, 0, kCanvasW, kCanvasH };
    host.Create(owner.m_hWnd, rcClient, nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);

    int totalSaved = 0;
    int groupCount = 0;
    const Gallery::PageGroup* groups = Gallery::GetPageGroups(groupCount);

    // 遍历全部分组下的全部页面。这里**不能**按 showInNav 过滤 —— 文档配图
    // 夹具那一页恰恰是不出现在导航里的，滤掉它就一张配图都出不来。
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const Gallery::PageEntry& entry = groups[g].pages[p];
            if (entry.build == NULL)
            {
                continue;
            }
            Gallery::GetCaptureMarks().clear();
            std::unique_ptr<DuiControl> content = entry.build();
            host.SetRoot(std::move(content));

            // Pump messages a few times. The first pumps let DuiHost paint
            // its DUI tree; the later pumps let the HWND-hosted children
            // (EDIT, RichEdit) lay out and paint themselves.
            for (int pump = 0; pump < kPumpRounds; ++pump)
            {
                ::RedrawWindow(host.m_hWnd, NULL, NULL,
                               RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW
                               | RDW_ALLCHILDREN);
                MSG msg;
                while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }

            // Snapshot the entire host's client area (DUI back buffer +
            // PrintWindow each HWND-hosted child) into one big DIB, then
            // crop per-mark.
            HBITMAP fullDib = nullptr;
            HDC fullDC = SnapshotHostToDib(host.m_hWnd,
                                           host.GetBackBufferDC(),
                                           kCanvasW, kCanvasH, fullDib);
            if (!fullDC)
            {
                continue;
            }

            const std::vector<Gallery::CaptureMark>& marks = Gallery::GetCaptureMarks();
            for (size_t m = 0; m < marks.size(); ++m)
            {
                const Gallery::CaptureMark& mark = marks[m];
                if (mark.anchor == NULL)
                {
                    continue;
                }
                const RECT& r = mark.anchor->GetRect();
                int w = r.right - r.left;
                int h = r.bottom - r.top;
                if (w <= 0 || h <= 0)
                {
                    continue;
                }
                // 超出画布的部分在快照 DIB 里根本不存在，此时 BitBlt 仍会
                // "成功"，写出来的却是一张全黑的图，而且不报任何错。这种
                // 失败此前一直没有征兆：画布高度写死 6000 像素，而文档配图
                // 夹具那一页实际已经长到 6000 以上，最后一张配图长期是黑的。
                // 这里显式判一次并输出告警，把静默失败变成看得见的失败。
                if (r.bottom > kCanvasH || r.right > kCanvasW)
                {
                    CString warn;
                    warn.Format(_T("DuiGallery: capture '%s' is outside the %d x %d canvas ")
                                _T("(rect right=%d bottom=%d), skipped.\n"),
                                (LPCTSTR)mark.name, kCanvasW, kCanvasH,
                                (int)r.right, (int)r.bottom);
                    ::OutputDebugString(warn);
                    continue;
                }
                CString path;
                path.Format(_T("%s\\ctl-%s.png"), outDir, (LPCTSTR)mark.name);
                if (SaveRegionAsPng(fullDC, r.left, r.top, w, h, path))
                {
                    ++totalSaved;
                    ::OutputDebugString(path);
                    ::OutputDebugString(_T("\n"));
                }
            }

            ::DeleteDC(fullDC);
            ::DeleteObject(fullDib);
        }
    }

    if (host.IsWindow())
    {
        host.DestroyWindow();
    }
    if (owner.IsWindow())
    {
        owner.DestroyWindow();
    }
    Gdiplus::GdiplusShutdown(gpToken);
    return totalSaved;
}

} // namespace CaptureMode
