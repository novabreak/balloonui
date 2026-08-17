#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_IMAGEOLE

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// CDuiImageOle —— 把 HBITMAP 包成 OLE 对象嵌入 RichEdit
// =================================================================
//
// 用途：富文本控件在文档里插图所用的"图片对象"。比"CF_BITMAP 剪贴板 +
// EM_PASTESPECIAL 借道 hack"更干净 —— 用真 OLE 对象插，图片在选区 /
// 拖动 / 复制时不会污染用户剪贴板。
//
// 只实现 RichEdit 真正会调到的接口子集：
//   · IOleObject             —— 大多空 stub；SetClientSite / Close 实做
//   · IViewObject2::Draw     —— 把 bitmap 画到目标 HDC。走 GDI+ 的
//                                HighQualityBicubic 从<u>全分辨率源图</u>一次性
//                                重采样到目标矩形，并按预乘 alpha 混合；调用方
//                                因此不需要（也不应该）预先把位图缩小 —— 预缩
//                                小再被放大回去，正是内联小图发糊的成因。
//   · IViewObject2::GetExtent —— 返 himetric 范围
//   · IDataObject::GetData(CF_BITMAP) —— EM_PASTE / drag-out 能取到位图
//   · IPersistStorage stubs  —— Save/Load 空实现（RichEdit 用内存里的
//                                位图重画即可；RTF 序列化不在本次迭代
//                                范围）
//
// 代码用法：
//
//     // 单 STA 线程；ULONG 引用计数。
//     auto* obj = new CDuiImageOle(hbm, /*ownsHbm*/ true);
//     // 然后把 obj QueryInterface 成 IOleObject* 塞进 REOBJECT，调
//     // IRichEditOle::InsertObject 插入。RichEdit 会自己加 ref；caller
//     // InsertObject 后 Release 本地指针即可。
//     // ownsHbm=true 时 OLE 对象在 dtor 里 DeleteObject HBITMAP。
//
// 不是 DuiControl 子类，<u>不</u>参与 XML / DUI 树。完全是富文本图文
// 混排的 plumbing。

#include <windows.h>
#include <ole2.h>
#include <richole.h>

namespace balloonwjui {

class CDuiImageOle : public IOleObject,
                     public IViewObject2,
                     public IDataObject,
                     public IPersistStorage
{
public:
    // hbm：要显示的位图，ownsHbm=true 表示所有权转移给本对象（析构时删除）。
    //   hasPremultipliedAlpha：hbm 是否为"32bpp 顶朝下、预乘 alpha"的
    //       DIBSection。true 时 Draw 会按带透明通道的方式混合到目标 DC，
    //       表情/图标的柔和边缘才不会烤上一圈白底；false 走不透明路径。
    //       传 true 而位图并非该格式属调用方错误，渲染结果不可预期。
    //   displayW / displayH：期望的显示边长（逻辑像素）。<=0 表示"按源位图
    //       的像素尺寸显示"。RichEdit 依据本尺寸换算出的 HIMETRIC 排版，
    //       而 Draw 始终从<u>全分辨率源图</u>一次性重采样到实际绘制矩形 ——
    //       这样源图不必预先缩小，避免"先缩小丢信息、再放大补像素"。
    CDuiImageOle(HBITMAP hbm, bool ownsHbm,
                 bool hasPremultipliedAlpha = false,
                 int displayW = 0, int displayH = 0);
    virtual ~CDuiImageOle();


    // 把一张位图作为内嵌对象插入富文本文档。
    //
    // 收的是 RichEdit 的 OLE 接口而不是窗口句柄 —— 插入的全过程（取客户站点、
    // 建存储、填 REOBJECT、调 InsertObject）只跟这个接口打交道，与窗口毫无
    // 关系。无窗口的富文本控件正是靠这一点插图：它没有窗口可发消息，但能
    // 通过排版引擎的接口拿到同一个 OLE 接口，再调本方法。
    //
    //   pRichEditOle：RichEdit 的 OLE 接口。**调用方持有其引用计数**，本方法
    //                 只借用、不释放。为空时直接失败。
    //   hbm / ownsHbm：位图；ownsHbm 为真表示所有权转移，插入失败时由本方法
    //                 负责删除。
    //   dwUser：写入 REOBJECT.dwUser 的应用自定义标记（默认 0）。RichEdit
    //           原样保存，日后可经 IRichEditOle::GetObject 读回，用于把内嵌
    //           图片还原成业务标识（如聊天表情的编号）。本类不解释其含义。
    //   hasPremultipliedAlpha / displayW / displayH：含义同构造函数。
    //   返回：true 插入成功。
    static bool InsertIntoRichEditOle(IRichEditOle* pRichEditOle,
                                      HBITMAP hbm, bool ownsHbm,
                                      DWORD_PTR dwUser = 0,
                                      bool hasPremultipliedAlpha = false,
                                      int displayW = 0, int displayH = 0);

    // 从磁盘文件解码出一张**预乘 alpha、底朝上**的 32 位位图，正是本类
    // 期望的输入格式。
    //
    // 两处讲究，写错了都不报错、只是画出来不对：
    //   · **预乘 alpha**：绘制走的是带 alpha 的位块传输，它要求颜色分量事先
    //     乘过 alpha。给非预乘的数据，半透明像素会发白发灰。
    //   · **底朝上**（位图高度取正值）：内存里的第 0 行是图像的最后一行。
    //     库内取位图信息的地方只能表达这一种行序，给顶朝下的数据，画出来
    //     上下颠倒。
    //
    //   path：   图片磁盘路径（PNG / JPG / BMP 等 GDI+ 可解码的格式）。
    //   outSize：出参，解码后的像素尺寸；可为空。
    //   返回：   新建的位图，**所有权归调用方**（用完自行删除）；
    //            路径为空、文件解不开、尺寸非法时返回空。
    static HBITMAP LoadPremultipliedDibFromFile(LPCTSTR path, SIZE* outSize);

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef () override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- IOleObject ----
    HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite* pSite) override;
    HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite** ppSite) override;
    HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR, LPCOLESTR) override;
    HRESULT STDMETHODCALLTYPE Close(DWORD dwSaveOption) override;
    HRESULT STDMETHODCALLTYPE SetMoniker(DWORD, IMoniker*) override;
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD, DWORD, IMoniker**) override;
    HRESULT STDMETHODCALLTYPE InitFromData(IDataObject*, BOOL, DWORD) override;
    HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD, IDataObject**) override;
    HRESULT STDMETHODCALLTYPE DoVerb(LONG, LPMSG, IOleClientSite*, LONG, HWND, LPCRECT) override;
    HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB**) override;
    HRESULT STDMETHODCALLTYPE Update() override;
    HRESULT STDMETHODCALLTYPE IsUpToDate() override;
    HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID*) override;
    HRESULT STDMETHODCALLTYPE GetUserType(DWORD, LPOLESTR*) override;
    HRESULT STDMETHODCALLTYPE SetExtent(DWORD, SIZEL*) override;
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, SIZEL*) override;
    HRESULT STDMETHODCALLTYPE Advise(IAdviseSink*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override;
    HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA**) override;
    HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD, DWORD*) override;
    HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE*) override;

    // ---- IViewObject2 ----
    HRESULT STDMETHODCALLTYPE Draw(DWORD dwDrawAspect, LONG lindex,
                                   void* pvAspect, DVTARGETDEVICE* ptd,
                                   HDC hdcTargetDev, HDC hdcDraw,
                                   LPCRECTL lprcBounds, LPCRECTL lprcWBounds,
                                   BOOL (STDMETHODCALLTYPE* pfnContinue)(ULONG_PTR),
                                   ULONG_PTR dwContinue) override;
    HRESULT STDMETHODCALLTYPE GetColorSet(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LOGPALETTE**) override;
    HRESULT STDMETHODCALLTYPE Freeze(DWORD, LONG, void*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE Unfreeze(DWORD) override;
    HRESULT STDMETHODCALLTYPE SetAdvise(DWORD, DWORD, IAdviseSink*) override;
    HRESULT STDMETHODCALLTYPE GetAdvise(DWORD*, DWORD*, IAdviseSink**) override;
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD, LONG, DVTARGETDEVICE*, LPSIZEL) override;

    // ---- IDataObject ----
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC*, STGMEDIUM*) override;
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override;
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC*) override;
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override;
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override;
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override;
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override;
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override;

    // ---- IPersist / IPersistStorage ----
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*) override;
    HRESULT STDMETHODCALLTYPE IsDirty() override;
    HRESULT STDMETHODCALLTYPE InitNew(IStorage*) override;
    HRESULT STDMETHODCALLTYPE Load(IStorage*) override;
    HRESULT STDMETHODCALLTYPE Save(IStorage*, BOOL) override;
    HRESULT STDMETHODCALLTYPE SaveCompleted(IStorage*) override;
    HRESULT STDMETHODCALLTYPE HandsOffStorage() override;

    // ---- accessors / test helpers ----
    HBITMAP GetBitmap() const { return m_hbm; }
    SIZE    GetPixelSize() const { return m_pxSize; }
    // 排版尺寸（逻辑像素）。构造时未指定 displayW/H 时等于源位图像素尺寸。
    SIZE    GetDisplaySize() const { return m_displayPx; }
    LONG    GetRefCount() const { return m_ref; }

private:
    void    ComputeSizes();   // fills m_pxSize / m_displayPx / m_himetric

    // Draw 的 GDI+ 实现：把全分辨率源位图一次性重采样到 (x, y, w, h)。
    //   hdcDraw：RichEdit 传进来的目标 DC，函数不改变其选中对象。
    //   x / y / w / h：目标矩形（设备像素），w / h 已保证 > 0。
    //   返回：true 表示已画完；false 表示 GDI+ 路径不可用，调用方应回退
    //         到 StretchBlt。
    bool    DrawWithGdiplus(HDC hdcDraw, int x, int y, int w, int h);

private:
    LONG             m_ref;
    HBITMAP          m_hbm;
    bool             m_ownsHbm;
    // m_hbm 是否为 32bpp 顶朝下预乘 alpha 的 DIBSection。决定 Draw 走
    // 透明混合还是不透明拷贝；由构造函数入参给定，本类不自行探测
    // （GDI 无从判断一张 32bpp 位图的 alpha 是否已预乘）。
    bool             m_hasAlpha;
    SIZE             m_pxSize;       // 源位图的像素尺寸
    // 排版尺寸（逻辑像素）。m_himetric 由它换算而来，Draw 与它无关 ——
    // Draw 只认 RichEdit 传进来的 lprcBounds。
    SIZE             m_displayPx;
    SIZEL            m_himetric;     // 0.01mm units
    IOleClientSite*  m_pSite;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_IMAGEOLE
