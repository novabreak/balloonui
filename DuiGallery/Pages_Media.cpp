/**
 *  媒体分组的三个演示页面：动图（DuiGif / DuiGifControl）、富文本内嵌图片
 *  （CDuiImageOle 与 DuiRichEdit 的插图接口）、异步图片加载（DuiAsyncImage）。
 *
 *  三个页面都用到磁盘上的素材：动图页要 Image 目录下的两个 GIF，富文本页与
 *  异步加载页要 Face 目录下的表情 PNG。素材缺失时页面不会崩溃，只是把对应
 *  段落换成一行说明文字，其余段落照常演示。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "BalloonUiFeatures.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Input/DuiSlider.h"
#include "Controls/Input/DuiRichEdit.h"
#include "Controls/Media/DuiGif.h"
#include "Controls/Media/DuiImageOle.h"
#include "DuiAsyncImage.h"

#if BUI_FEATURE_RICHTEXT && BUI_FEATURE_IMAGEOLE
//取排版引擎的 OLE 接口要用到 EM_GETOLEINTERFACE。
#include <richedit.h>
//合成带柔和边缘的圆形位图时要算像素到圆心的距离。
#include <math.h>
#endif

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 三个页面共用的小工具
// =====================================================================

// 演示行里两个控件之间的水平间距（像素）。
const int kRowGap = 12;
// 演示按钮的宽度与高度（像素）。整组页面统一用这一对值，按钮才对得齐。
const int kButtonW = 128;
const int kButtonH = 28;
// 文字较长的按钮所用的宽度（像素）。
const int kWideButtonW = 168;
// 只放按钮的演示行高度（像素）。比按钮本身高一点，上下才不至于贴边。
const int kButtonRowH = kButtonH + 8;
// 同一段落内两组演示行之间的竖直间距（像素）。
const int kInnerGap = 8;
// 演示行下方那一行补充说明文字的高度（像素）。
const int kCaptionRowH = 22;
// 演示行内补充说明文字的颜色，与卡片里的段落说明同色系。
const COLORREF kCaptionColor = RGB(107, 114, 128);
// 素材缺失提示的文字颜色。用暖色与普通说明区分开，提示这是需要处理的情况。
const COLORREF kNoticeColor = RGB(180, 83, 9);
// 素材缺失提示行的高度（像素）。按两行文字留。
const int kNoticeRowH = 44;

// 取画廊可执行文件所在的目录。
// 返回：目录路径，结尾不带反斜杠；取不到时返回空串。
CString ExeDirectory()
{
    TCHAR szModule[MAX_PATH] = {};
    ::GetModuleFileName(NULL, szModule, MAX_PATH);
    CString strDir = szModule;
    int nSlash = strDir.ReverseFind(_T('\\'));
    if (nSlash >= 0)
    {
        strDir = strDir.Left(nSlash);
    }
    return strDir;
}

// 把一个相对画廊可执行文件目录的路径拼成绝对路径。
//   szRelative：相对路径，例如 _T("Image\\DownloadFailed.gif")。
// 返回：绝对路径。
CString AssetPath(LPCTSTR szRelative)
{
    CString strPath = ExeDirectory();
    strPath += _T("\\");
    strPath += szRelative;
    return strPath;
}

// 判断素材文件是否存在。
//   szAbsolutePath：绝对路径。
// 返回：存在且不是目录时返回 true。
bool AssetExists(LPCTSTR szAbsolutePath)
{
    DWORD dwAttr = ::GetFileAttributes(szAbsolutePath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 新建一个演示行里用的文字标签。
//   szText：标签文字。
//   crText：文字颜色。
//   dwAlign：DrawText 的对齐标志组合。
// 返回：标签控件，所有权交给调用方。
std::unique_ptr<DuiLabel> MakeLabel(LPCTSTR szText, COLORREF crText, DWORD dwAlign)
{
    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(szText);
    label->SetTextColor(crText);
    label->SetTextAlign(dwAlign);
    return label;
}

// 新建一个左对齐的普通说明标签。
//   szText：标签文字。
// 返回：标签控件，所有权交给调用方。
std::unique_ptr<DuiLabel> MakeCaption(LPCTSTR szText)
{
    return MakeLabel(szText, kCaptionColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// 新建一个点击后调用函数对象的按钮。
//   szText：按钮文字。
// 返回：按钮控件，所有权交给调用方；调用方自行给 onClick 赋值。
std::unique_ptr<FnButton> MakeButton(LPCTSTR szText)
{
    std::unique_ptr<FnButton> button(new FnButton());
    button->SetText(szText);
    return button;
}

// 往当前段落里加一行提示文字，用于素材缺失等降级情况。
//   page：页面容器，不能为空。
//   szText：提示文字，可以写满两行。
void AddNoticeRow(GalleryPageBox* page, LPCTSTR szText)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());
    std::unique_ptr<DuiLabel> label = MakeLabel(szText, kNoticeColor,
                                                DT_LEFT | DT_TOP);
    label->SetWordWrap(true);
    row->AddChild(std::move(label), DuiLayout::Hint().Weight(1));
    AddVariantRow(page, std::move(row), kNoticeRowH);
}

// 往当前段落里加一行普通说明文字。
//   page：页面容器，不能为空。
//   szText：说明文字，单行。
void AddCaptionRow(GalleryPageBox* page, LPCTSTR szText)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());
    row->AddChild(MakeCaption(szText), DuiLayout::Hint().Weight(1));
    AddVariantRow(page, std::move(row), kCaptionRowH);
}

} // 匿名命名空间

// =====================================================================
// 动图（DuiGif / DuiGifControl）
// =====================================================================

namespace {

#if BUI_FEATURE_GIF

// 多帧动图素材的相对路径（相对画廊可执行文件所在目录）：42 × 42、9 帧。
const LPCTSTR kGifAnimatedRelPath = _T("Image\\DownloadImageProgress.gif");
// 单帧动图素材的相对路径：42 × 42、1 帧，用于演示退化行为。
const LPCTSTR kGifSingleRelPath = _T("Image\\DownloadFailed.gif");

// 逐帧定位演示里滑块的控件编号。判定通知时必须连控件编号一起判，所以这里
// 必须给一个非零值；取 9100 段是为了避开画廊框架自己用掉的小编号。
const UINT kIdGifFrameSlider = 9101;

// 动图控件在普通演示行里占的宽度与高度（像素）。素材是 42 × 42，留一点余量。
const int kGifCellW = 56;
const int kGifCellH = 46;
// 普通动图演示行的高度（像素）。
const int kGifRowH = 56;
// 拉伸对照里每个动图控件占的宽度（像素）。故意远大于素材本身的 42 像素，
// 拉伸与不拉伸的差别才看得出来。
const int kGifStretchCellW = 150;
// 拉伸对照那一行的高度（像素）。
const int kGifStretchRowH = 96;
// 帧号 / 延时说明标签的宽度（像素）。
const int kGifInfoLabelW = 260;
// 元信息段落里最多列出多少帧的延时，超过的部分省略，避免一行过长。
const int kMaxListedFrameDelays = 12;

// 已经从页面里摘出来、长期保留的动图控件。
//
// 进程退出时不回收，与画廊里其它静态缓存的做法一致。
// 返回：保留列表的引用。
std::vector<std::unique_ptr<DuiControl> >& RetiredGifControls()
{
    static std::vector<std::unique_ptr<DuiControl> > s_retired;
    return s_retired;
}

// 加载一个动图素材。
//   szRelPath：相对画廊可执行文件目录的路径。
// 返回：加载好的动图，所有权交给调用方；文件不存在或解码失败时返回空指针。
std::unique_ptr<DuiGif> LoadDemoGif(LPCTSTR szRelPath)
{
    std::unique_ptr<DuiGif> gif(new DuiGif());
    if (!gif->LoadFromFile(AssetPath(szRelPath)))
    {
        gif.reset();
    }
    return gif;
}

// 把一个动图的元信息拼成一行文字。
//   szDisplayName：显示用的文件名。
//   gif：已经加载好的动图。
// 返回：形如「Image\xxx.gif：9 帧，42 × 42 像素，一个循环 900 毫秒」的文字。
CString FormatGifMetaLine(LPCTSTR szDisplayName, const DuiGif& gif)
{
    CString strLine;
    strLine.Format(Txt(_T("%s：%d 帧，%d × %d 像素，一个循环 %d 毫秒"),
                       _T("%s: %d frames, %d x %d pixels, %d ms per loop")),
                   szDisplayName,
                   gif.GetFrameCount(),
                   gif.GetWidth(),
                   gif.GetHeight(),
                   gif.GetTotalDurationMs());
    return strLine;
}

// 把一个动图的每帧延时拼成一行文字。
//   gif：已经加载好的动图。
// 返回：形如「每帧延时（毫秒）：100, 100, ...」的文字；帧数超过
//       kMaxListedFrameDelays 时后面用省略号收尾。
CString FormatGifDelayLine(const DuiGif& gif)
{
    CString strLine = Txt(_T("每帧延时（毫秒）："), _T("Per-frame delays (ms): "));
    int nCount = gif.GetFrameCount();
    int nListed = nCount;
    if (nListed > kMaxListedFrameDelays)
    {
        nListed = kMaxListedFrameDelays;
    }
    for (int i = 0; i < nListed; ++i)
    {
        CString strOne;
        strOne.Format(_T("%d"), gif.GetFrameDelayMs(i));
        if (i > 0)
        {
            strLine += _T(", ");
        }
        strLine += strOne;
    }
    if (nListed < nCount)
    {
        strLine += _T(" ...");
    }
    return strLine;
}

// 会把当前帧号回报到一个标签上的动图控件。
//
// DuiGifControl 播放时不对外发通知，而本页要把帧号显示出来。播放期间控件
// 每换一帧都会重绘，所以在绘制里比对帧号、发现变化时写到指定标签上，就能
// 得到与画面同步的帧号显示，不需要另外的定时器。
class FrameReportingGif : public DuiGifControl
{
public:
    FrameReportingGif();

    // 指定显示帧号的标签。
    //   pLabel：标签控件；所有权归它所在的容器，本控件只借用指针，因此该
    //           标签必须与本控件在同一个页面里，页面销毁时两者一并失效。
    //           允许传空指针，表示不回报帧号。
    void SetFrameLabel(DuiLabel* pLabel);

    // 绘制当前帧，并在帧号变化时刷新标签。
    //   hdc：目标绘制上下文。
    //   rcDirty：本次需要重绘的区域。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 把当前帧号写到标签上。帧号与上一次写的相同时什么都不做，避免每绘制
    // 一次就让标签重绘一次。
    void RefreshFrameLabel();

    // 显示帧号的标签。可能为空。不持有所有权。
    DuiLabel* m_pFrameLabel;
    // 上一次写到标签上的帧号，-1 表示还没有写过。
    int m_nLastReportedFrame;
};

FrameReportingGif::FrameReportingGif()
    : m_pFrameLabel(NULL)
    , m_nLastReportedFrame(-1)
{
}

void FrameReportingGif::SetFrameLabel(DuiLabel* pLabel)
{
    m_pFrameLabel = pLabel;
    m_nLastReportedFrame = -1;
    RefreshFrameLabel();
}

void FrameReportingGif::OnPaint(HDC hdc, const RECT& rcDirty)
{
    DuiGifControl::OnPaint(hdc, rcDirty);
    RefreshFrameLabel();
}

void FrameReportingGif::RefreshFrameLabel()
{
    if (m_pFrameLabel == NULL)
    {
        return;
    }
    DuiGif* pGif = GetGif();
    if (pGif == NULL || pGif->GetFrameCount() <= 0)
    {
        return;
    }
    int nFrame = GetFrameIndex();
    if (nFrame == m_nLastReportedFrame)
    {
        return;
    }
    m_nLastReportedFrame = nFrame;

    CString strText;
    strText.Format(Txt(_T("当前第 %d 帧，共 %d 帧"),
                       _T("Frame %d of %d")),
                   nFrame + 1,
                   pGif->GetFrameCount());
    m_pFrameLabel->SetText(strText);
}

// 承载动图控件的演示行。
//
// 它比普通的水平容器多做一件事：本行销毁时，先让登记过的动图控件停止播放，
// 再把它们从子控件列表里移出来交给一个进程级容器长期持有，而不是随本行
// 一起释放。
//
// 这样做是为了绕开库内播放机制的一处缺陷：DuiGifControl::Start 会向
// DuiAnimMgr 提交一个时长一小时的心跳动画，动画对象持有控件的裸指针，而
// Stop 与控件析构都不会把这个动画撤下（见 balloonui/Controls/Media/
// DuiGif.cpp 里的 GifTickAnim）。画廊切换页面时整棵页面子树都会被销毁，
// 动图控件若跟着释放，心跳动画下一次触发就会读到已经释放的内存。
//
// 停止播放之后，心跳动画在第一个判断（控件是否正在播放）处就返回，不再
// 访问控件的其它成员；而控件本身被长期保留，那一次读取落在仍然有效的内存
// 上。代价是每访问一次本页面就留下一份控件与它已解码的帧位图不再释放，
// 单份约几十 KB。库内提供撤下动画的接口之后应当删掉本类。
class GifKeepAliveRow : public DuiHBox
{
public:
    ~GifKeepAliveRow() override;

    // 登记一个需要在本行销毁时保留下来的动图控件。
    //   pGif：动图控件，必须是本行的**直接**子控件；所有权仍在本行，本方法
    //         只记下裸指针。允许在 AddChild 之前或之后调用。
    void RegisterGif(DuiGifControl* pGif);

private:
    // 判断一个子控件是否登记过。
    //   pChild：待判断的子控件。
    // 返回：登记过返回 true。
    bool IsRegistered(DuiControl* pChild) const;

    // 登记过的动图控件。不持有所有权，生命期与本行的子控件列表一致。
    std::vector<DuiGifControl*> m_gifs;
};

GifKeepAliveRow::~GifKeepAliveRow()
{
    for (size_t i = 0; i < m_gifs.size(); ++i)
    {
        if (m_gifs[i] != NULL)
        {
            m_gifs[i]->Stop();
        }
    }
    // 从后往前遍历：移走一个就要把它从子控件列表里删掉，倒序遍历时删除
    // 不会影响尚未访问到的下标。
    for (int i = (int)m_children.size() - 1; i >= 0; --i)
    {
        if (!IsRegistered(m_children[i].get()))
        {
            continue;
        }
        RetiredGifControls().push_back(std::move(m_children[i]));
        m_children.erase(m_children.begin() + i);
    }
}

void GifKeepAliveRow::RegisterGif(DuiGifControl* pGif)
{
    if (pGif == NULL)
    {
        return;
    }
    m_gifs.push_back(pGif);
}

bool GifKeepAliveRow::IsRegistered(DuiControl* pChild) const
{
    for (size_t i = 0; i < m_gifs.size(); ++i)
    {
        if (m_gifs[i] == pChild)
        {
            return true;
        }
    }
    return false;
}

// 动图页面里逐帧定位那一段需要的运行期状态。
struct GifPageState
{
    // 被滑块定位的动图控件。不持有所有权。
    DuiGifControl* m_pStepGif;
    // 显示当前帧号与该帧延时的标签。不持有所有权。
    DuiLabel* m_pStepLabel;
};

// 当前动图页面的状态。页面存在时指向 GifSliderRow 内的那一份，页面销毁后
// 为空。滑块通知的处理函数靠它找到要更新的控件。
GifPageState* g_pGifPageState = NULL;

// 逐帧定位演示所在的行。
//
// 除了保留动图控件之外，它还负责在自己存活期间把本页面的状态登记到
// g_pGifPageState 上 —— 滑块通知由一个自由函数处理，那个函数只能通过文件
// 级指针找到要更新的控件。
class GifSliderRow : public GifKeepAliveRow
{
public:
    GifSliderRow();
    ~GifSliderRow() override;

    // 取本行持有的页面状态，供页面构建代码填写控件指针。
    // 返回：状态结构的引用，生命期与本行相同。
    GifPageState& State() { return m_state; }

private:
    // 本页面的状态，随本行一起存亡。
    GifPageState m_state;
};

GifSliderRow::GifSliderRow()
{
    m_state.m_pStepGif = NULL;
    m_state.m_pStepLabel = NULL;
    g_pGifPageState = &m_state;
}

GifSliderRow::~GifSliderRow()
{
    // 只在全局指针仍然指向自己这一份时才清空。画廊切换页面时是**先建新
    // 页面、再销毁旧页面**，若无条件清空，旧页面的析构会把新页面刚登记
    // 上去的那一份抹掉。
    if (g_pGifPageState == &m_state)
    {
        g_pGifPageState = NULL;
    }
}

// 按动图控件的当前帧刷新逐帧定位那一段的说明文字。
//   state：页面状态。控件指针为空时本函数直接返回。
void RefreshGifStepLabel(const GifPageState& state)
{
    if (state.m_pStepGif == NULL || state.m_pStepLabel == NULL)
    {
        return;
    }
    DuiGif* pGif = state.m_pStepGif->GetGif();
    if (pGif == NULL || pGif->GetFrameCount() <= 0)
    {
        return;
    }
    int nFrame = state.m_pStepGif->GetFrameIndex();
    CString strText;
    strText.Format(Txt(_T("停在第 %d 帧（共 %d 帧），该帧延时 %d 毫秒"),
                       _T("Stopped at frame %d of %d, delay %d ms")),
                   nFrame + 1,
                   pGif->GetFrameCount(),
                   pGif->GetFrameDelayMs(nFrame));
    state.m_pStepLabel->SetText(strText);
}

// 动图页面的通知处理函数。
//
// 注册给 Gallery::g_pageNotifyHook，由画廊窗口在自己处理完通知之后转发
// 进来。滑块发的是通用的 DUIN_VALUECHANGED，与别的控件重号，所以控件编号
// 必须写进分支条件本身。
//   pNotify：收到的通知，可能为空。
void OnGifPageNotify(const DuiNotify* pNotify)
{
    if (pNotify == NULL || g_pGifPageState == NULL)
    {
        return;
    }
    if (pNotify->code == (UINT)DUIN_VALUECHANGED
        && pNotify->ctrlId == kIdGifFrameSlider)
    {
        if (g_pGifPageState->m_pStepGif != NULL)
        {
            g_pGifPageState->m_pStepGif->SetFrameIndex((int)pNotify->extra);
        }
        RefreshGifStepLabel(*g_pGifPageState);
    }
}

#endif // BUI_FEATURE_GIF

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_Gif()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

#if BUI_FEATURE_GIF
    const CString strAnimatedPath = AssetPath(kGifAnimatedRelPath);
    const CString strSinglePath = AssetPath(kGifSingleRelPath);
    const bool bHasAnimated = AssetExists(strAnimatedPath);
    const bool bHasSingle = AssetExists(strSinglePath);

    //—— 段落一：播放与暂停 ——
    AddSection(page.get(),
               Txt(_T("播放与暂停"), _T("Play and pause")),
               Txt(_T("DuiGif 只负责解码与缓存每一帧，它本身不显示；把它交给 ")
                   _T("DuiGifControl 之后才画得出来。调用顺序是：新建 DuiGif、")
                   _T("LoadFromFile 加载文件、新建 DuiGifControl、SetGif 把动图")
                   _T("交给控件、Start 开始播放。播放由 DuiAnimMgr 的共享脉冲")
                   _T("驱动，宿主窗口不需要自备定时器；GDI+ 也由 LoadFromFile ")
                   _T("内部启动，调用方不用管。"),
                   _T("DuiGif only decodes and caches the frames - it draws ")
                   _T("nothing by itself. Hand it to a DuiGifControl and the ")
                   _T("frames appear. The call order is: new DuiGif, ")
                   _T("LoadFromFile, new DuiGifControl, SetGif, Start. ")
                   _T("Playback rides on the shared pulse of DuiAnimMgr, so ")
                   _T("the host window needs no timer of its own, and ")
                   _T("LoadFromFile starts GDI+ on its own as well.")));
    if (!bHasAnimated)
    {
        AddNoticeRow(page.get(),
                     Txt(_T("找不到素材 Image\\DownloadImageProgress.gif，")
                         _T("本段无法演示。把该文件放到 DuiGallery.exe 所在")
                         _T("目录的 Image 子目录下即可。"),
                         _T("Asset Image\\DownloadImageProgress.gif is ")
                         _T("missing, so this section cannot run. Copy it ")
                         _T("into the Image folder next to DuiGallery.exe.")));
    }
    else
    {
        std::unique_ptr<GifKeepAliveRow> row(new GifKeepAliveRow());
        row->SetGap(kRowGap);

        std::unique_ptr<FrameReportingGif> gif(new FrameReportingGif());
        gif->SetGif(LoadDemoGif(kGifAnimatedRelPath));
        //按原始 42 × 42 居中画，避免拉伸掩盖帧与帧之间的差别。
        gif->SetStretch(false);
        FrameReportingGif* pGif = gif.get();
        row->RegisterGif(pGif);
        row->AddChild(std::move(gif),
                      DuiLayout::Hint().Fixed(kGifCellW, kGifCellH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonStart = MakeButton(Txt(_T("开始播放"),
                                                               _T("Start")));
        buttonStart->onClick = [pGif](FnButton*)
        {
            pGif->Start();
        };
        row->AddChild(std::move(buttonStart),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonStop = MakeButton(Txt(_T("暂停"),
                                                              _T("Stop")));
        buttonStop->onClick = [pGif](FnButton*)
        {
            pGif->Stop();
        };
        row->AddChild(std::move(buttonStop),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<DuiLabel> frameLabel = MakeCaption(_T(""));
        DuiLabel* pFrameLabel = frameLabel.get();
        row->AddChild(std::move(frameLabel), DuiLayout::Hint().Weight(1));

        pGif->SetFrameLabel(pFrameLabel);
        //进入页面就播放，读者不点任何按钮也能看到动起来的样子。
        pGif->Start();

        AddVariantRow(page.get(), std::move(row), kGifRowH);
    }

    //—— 段落二：拉伸与原始尺寸 ——
    AddSection(page.get(),
               Txt(_T("拉伸与原始尺寸"), _T("Stretch versus 1:1")),
               Txt(_T("SetStretch(true) 是默认值，帧被拉伸到控件矩形；")
                   _T("SetStretch(false) 则按原始像素尺寸居中画。下面两个控件")
                   _T("占的矩形一样大，装的是同一个素材，差别只在这个开关。"),
                   _T("SetStretch(true) is the default and scales each frame ")
                   _T("to the control rect; SetStretch(false) draws the frame ")
                   _T("at its native pixel size, centred. Both controls below ")
                   _T("get the same rect and the same asset - only the flag ")
                   _T("differs.")));
    if (!bHasAnimated)
    {
        AddNoticeRow(page.get(),
                     Txt(_T("素材缺失，本段无法演示。"),
                         _T("Asset missing, this section cannot run.")));
    }
    else
    {
        std::unique_ptr<GifKeepAliveRow> row(new GifKeepAliveRow());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiGifControl> gifStretch(new DuiGifControl());
        gifStretch->SetGif(LoadDemoGif(kGifAnimatedRelPath));
        gifStretch->SetStretch(true);
        DuiGifControl* pStretch = gifStretch.get();
        row->RegisterGif(pStretch);
        row->AddChild(std::move(gifStretch),
                      DuiLayout::Hint().Fixed(kGifStretchCellW));

        std::unique_ptr<DuiGifControl> gifNative(new DuiGifControl());
        gifNative->SetGif(LoadDemoGif(kGifAnimatedRelPath));
        gifNative->SetStretch(false);
        DuiGifControl* pNative = gifNative.get();
        row->RegisterGif(pNative);
        row->AddChild(std::move(gifNative),
                      DuiLayout::Hint().Fixed(kGifStretchCellW));

        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));

        pStretch->Start();
        pNative->Start();

        AddVariantRow(page.get(), std::move(row), kGifStretchRowH);

        std::unique_ptr<DuiHBox> captionRow(new DuiHBox());
        captionRow->SetGap(kRowGap);
        captionRow->AddChild(MakeCaption(Txt(_T("SetStretch(true)：填满矩形"),
                                             _T("SetStretch(true): fills the rect"))),
                             DuiLayout::Hint().Fixed(kGifStretchCellW));
        captionRow->AddChild(MakeCaption(Txt(_T("SetStretch(false)：42 × 42 居中"),
                                             _T("SetStretch(false): 42 x 42, centred"))),
                             DuiLayout::Hint().Fixed(kGifStretchCellW));
        captionRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                             DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(captionRow), kCaptionRowH);
    }

    //—— 段落三：用滑块逐帧定位 ——
    AddSection(page.get(),
               Txt(_T("用滑块逐帧定位"), _T("Stepping frame by frame")),
               Txt(_T("SetFrameIndex 可以把控件停在指定的一帧上，越界的值会被")
                   _T("收进有效范围。下面这个控件没有调用过 Start，画面完全由")
                   _T("滑块决定，适合逐帧核对每一帧的内容与延时。"),
                   _T("SetFrameIndex parks the control on a chosen frame and ")
                   _T("clamps out-of-range values. The control below never ")
                   _T("calls Start - the slider alone decides what is shown, ")
                   _T("which is handy for checking each frame and its delay.")));
    if (!bHasAnimated)
    {
        AddNoticeRow(page.get(),
                     Txt(_T("素材缺失，本段无法演示。"),
                         _T("Asset missing, this section cannot run.")));
    }
    else
    {
        std::unique_ptr<GifSliderRow> row(new GifSliderRow());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiGif> stepGifData = LoadDemoGif(kGifAnimatedRelPath);
        int nFrameCount = 0;
        if (stepGifData)
        {
            nFrameCount = stepGifData->GetFrameCount();
        }

        //本控件从不调用 Start，不会在 DuiAnimMgr 里留下心跳动画，因此不需要
        //登记保留。
        std::unique_ptr<DuiGifControl> stepGif(new DuiGifControl());
        stepGif->SetGif(std::move(stepGifData));
        stepGif->SetStretch(false);
        DuiGifControl* pStepGif = stepGif.get();
        row->AddChild(std::move(stepGif),
                      DuiLayout::Hint().Fixed(kGifCellW, kGifCellH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<DuiSlider> slider(new DuiSlider());
        slider->SetCtrlId(kIdGifFrameSlider);
        slider->SetRange(0, nFrameCount > 0 ? nFrameCount - 1 : 0);
        slider->SetPos(0, false);
        slider->SetLineSize(1);
        row->AddChild(std::move(slider),
                      DuiLayout::Hint().Weight(1)
                                       .Fixed(-1, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<DuiLabel> stepLabel = MakeCaption(_T(""));
        DuiLabel* pStepLabel = stepLabel.get();
        row->AddChild(std::move(stepLabel),
                      DuiLayout::Hint().Fixed(kGifInfoLabelW));

        row->State().m_pStepGif = pStepGif;
        row->State().m_pStepLabel = pStepLabel;
        RefreshGifStepLabel(row->State());

        //滑块的通知要靠页面钩子收，页面构建时挂上；画廊切换页面时会自行清空。
        g_pageNotifyHook = &OnGifPageNotify;

        AddVariantRow(page.get(), std::move(row), kGifRowH);
    }

    //—— 段落四：单帧动图的退化行为 ——
    AddSection(page.get(),
               Txt(_T("单帧动图的退化行为"), _T("Single-frame GIFs")),
               Txt(_T("只有一帧的 GIF 照样能加载与播放，只是画面不会变：")
                   _T("FrameAt 恒定返回第 0 帧，一个循环的总时长就等于那一帧")
                   _T("的延时。业务侧因此不必区分静态图与动图，一律按动图处理")
                   _T("即可。"),
                   _T("A one-frame GIF loads and plays like any other, it ")
                   _T("simply never changes: FrameAt always returns frame 0 ")
                   _T("and the loop duration equals that frame's delay. ")
                   _T("Callers therefore do not have to tell static images ")
                   _T("from animated ones.")));
    if (!bHasSingle)
    {
        AddNoticeRow(page.get(),
                     Txt(_T("找不到素材 Image\\DownloadFailed.gif，本段无法")
                         _T("演示。把该文件放到 DuiGallery.exe 所在目录的 ")
                         _T("Image 子目录下即可。"),
                         _T("Asset Image\\DownloadFailed.gif is missing, so ")
                         _T("this section cannot run. Copy it into the Image ")
                         _T("folder next to DuiGallery.exe.")));
    }
    else
    {
        std::unique_ptr<GifKeepAliveRow> row(new GifKeepAliveRow());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiGifControl> gif(new DuiGifControl());
        gif->SetGif(LoadDemoGif(kGifSingleRelPath));
        gif->SetStretch(false);
        DuiGifControl* pGif = gif.get();
        row->RegisterGif(pGif);
        row->AddChild(std::move(gif),
                      DuiLayout::Hint().Fixed(kGifCellW, kGifCellH)
                                       .AlignC(DuiLayout::AlignCenter));

        row->AddChild(MakeCaption(Txt(_T("已经调用过 Start，画面不动是因为")
                                      _T("素材只有一帧"),
                                      _T("Start has been called - it stands ")
                                      _T("still because the asset has one ")
                                      _T("frame"))),
                      DuiLayout::Hint().Weight(1));

        pGif->Start();

        AddVariantRow(page.get(), std::move(row), kGifRowH);
    }

    //—— 段落五：元信息 ——
    AddSection(page.get(),
               Txt(_T("元信息"), _T("Metadata")),
               Txt(_T("加载完成之后，帧数、宽高、每帧延时、一个循环的总时长")
                   _T("都可以直接读出来。延时取自 GIF 文件里记录的值，其中")
                   _T("为 0 的那些会被收敛到 100 毫秒，与浏览器的处理一致。")
                   _T("下面是两个素材文件的实测值。"),
                   _T("Once loaded, the frame count, pixel size, per-frame ")
                   _T("delays and total loop duration can all be read back. ")
                   _T("Delays come from the GIF file itself; zero delays are ")
                   _T("clamped to 100 ms, matching what browsers do. The ")
                   _T("numbers below are measured from the two assets.")));
    {
        DuiGif metaGif;
        if (metaGif.LoadFromFile(strAnimatedPath))
        {
            AddCaptionRow(page.get(),
                          FormatGifMetaLine(kGifAnimatedRelPath, metaGif));
            AddCaptionRow(page.get(), FormatGifDelayLine(metaGif));
        }
        else
        {
            AddNoticeRow(page.get(),
                         Txt(_T("Image\\DownloadImageProgress.gif 未能加载，")
                             _T("这一行没有数据。"),
                             _T("Image\\DownloadImageProgress.gif could not ")
                             _T("be loaded, so this line has no data.")));
        }
    }
    {
        DuiGif metaGif;
        if (metaGif.LoadFromFile(strSinglePath))
        {
            AddCaptionRow(page.get(),
                          FormatGifMetaLine(kGifSingleRelPath, metaGif));
            AddCaptionRow(page.get(), FormatGifDelayLine(metaGif));
        }
        else
        {
            AddNoticeRow(page.get(),
                         Txt(_T("Image\\DownloadFailed.gif 未能加载，这一行")
                             _T("没有数据。"),
                             _T("Image\\DownloadFailed.gif could not be ")
                             _T("loaded, so this line has no data.")));
        }
    }

    //—— 段落六：位图所有权 ——
    AddSection(page.get(),
               Txt(_T("位图的所有权"), _T("Who owns the bitmaps")),
               Txt(_T("GetFrameHbitmap 返回的位图句柄由 DuiGif 自己持有，")
                   _T("调用方不要对它调用 DeleteObject —— 动图销毁时会")
                   _T("统一释放全部帧。SetGif 则相反：它把 DuiGif 的所有权")
                   _T("交给控件，控件销毁或再次 SetGif 时会释放上一个动图。"),
                   _T("The HBITMAP returned by GetFrameHbitmap belongs to the ")
                   _T("DuiGif - callers must NOT call DeleteObject on it; all ")
                   _T("frames are released when the DuiGif dies. SetGif works ")
                   _T("the other way round: it takes ownership of the DuiGif, ")
                   _T("and the control frees the previous one on destruction ")
                   _T("or on the next SetGif.")));
#else
    AddSection(page.get(),
               Txt(_T("本页面未编译进来"), _T("This page was compiled out")),
               Txt(_T("动图能力被裁剪开关关掉了（定义了 BUI_DISABLE_GIF），")
                   _T("DuiGif 与 DuiGifControl 两个类都不存在，本页面因此没有")
                   _T("内容。"),
                   _T("The GIF feature was stripped (BUI_DISABLE_GIF is ")
                   _T("defined), so neither DuiGif nor DuiGifControl exists ")
                   _T("and this page has no content.")));
#endif // BUI_FEATURE_GIF

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 富文本内嵌图片（CDuiImageOle）
// =====================================================================

namespace {

#if BUI_FEATURE_RICHTEXT && BUI_FEATURE_IMAGEOLE

// 从文件插图那一段用到的表情素材，相对画廊可执行文件所在目录。
const LPCTSTR kFacePngRelPath = _T("Face\\face0.png");

// 演示用富文本控件的高度（像素）。
const int kRichEditH = 96;
// 并排对照时每个富文本控件的宽度（像素）。
const int kRichEditPairW = 200;
// 并排对照那一行的高度（像素）。
const int kRichEditPairH = 84;

// 合成的表情源图边长（像素）。刻意取 56，与客户端的表情素材同尺寸。
const int kSyntheticFaceSide = 56;
// 表情在文档里的排版边长（像素）。只有它的一半，用来说明排版尺寸与源图
// 分辨率是两回事。
const int kFaceDisplaySide = 28;
// 合成的大图尺寸（像素）。
const int kLargeImageW = 240;
const int kLargeImageH = 180;
// 大图在文档里的排版尺寸（像素）。
const int kLargeDisplayW = 120;
const int kLargeDisplayH = 90;
// 重采样对照用的条纹图边长（像素）。
const int kStripeSide = 96;
// 重采样对照的排版边长（像素）。相对源图缩小到四分之一，差别最明显。
const int kStripeDisplaySide = 24;
// 排版尺寸对照里那张源图的边长（像素）。取的是三种排版尺寸里最大的那个，
// 这样即便按最大尺寸排版也不需要把位图放大。
const int kDecoupleSourceSide = 96;
// 排版尺寸对照里三次插入各自的排版边长（像素）。
const int kDecoupleSmall = 24;
const int kDecoupleMiddle = 48;
const int kDecoupleLarge = 96;

// 插图时写进 REOBJECT.dwUser 的业务标记。本页面只用它来演示标记能原样读
// 回来，取值本身没有别的含义。
const DWORD_PTR kTagFace = 1001;
const DWORD_PTR kTagLargeImage = 1002;
const DWORD_PTR kTagAlphaImage = 1003;
const DWORD_PTR kTagFromFile = 1004;

// 32 位位图里每个像素占的字节数。
const int kBytesPerPixel = 4;
// 不透明像素的 alpha 值。
const BYTE kOpaqueAlpha = 255;

// 建一张 32 位、底朝上的 DIBSection，并把像素内存的首地址交给调用方填写。
//
// 统一建成底朝上，是因为带 alpha 的绘制路径直接在 DIBSection 的像素内存上
// 工作，而它只能按底朝上解释行序（见 CDuiImageOle::DrawWithGdiplus 里的
// 说明）；顶朝下的数据画出来会上下颠倒。注意这只影响**直接写像素**的代码，
// 用 GDI 往内存设备上下文里画的内容行序由 GDI 自己处理。
//
//   nWidth / nHeight：像素尺寸，必须大于 0。
//   ppBits：出参，像素内存首地址；不能为空。内存第 0 行对应图像的最后一行。
// 返回：新建的位图，所有权归调用方；失败返回空。
HBITMAP CreateBottomUpDib(int nWidth, int nHeight, BYTE** ppBits)
{
    if (nWidth <= 0 || nHeight <= 0 || ppBits == NULL)
    {
        return NULL;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = nWidth;
    //正数表示底朝上。
    bi.bmiHeader.biHeight = nHeight;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hbm = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hbm == NULL || pBits == NULL)
    {
        if (hbm != NULL)
        {
            ::DeleteObject(hbm);
        }
        return NULL;
    }
    *ppBits = (BYTE*)pBits;
    return hbm;
}

// 把一块像素内存里所有像素的 alpha 分量置成不透明。
//
// 用 GDI 往 32 位 DIBSection 上画东西时，画笔与文字只写颜色分量，alpha 分量
// 保持建位图时的 0。这样的位图交给 GDI+ 有可能被当作带 alpha 的图解释，于是
// 整张图都是全透明的、什么也画不出来。GDI 画完之后统一补上 alpha 就没有这
// 层不确定性。
//
//   pBits：像素内存首地址，不能为空。
//   nWidth / nHeight：像素尺寸。
void ForceOpaqueAlpha(BYTE* pBits, int nWidth, int nHeight)
{
    if (pBits == NULL)
    {
        return;
    }
    const int nPixelCount = nWidth * nHeight;
    for (int i = 0; i < nPixelCount; ++i)
    {
        pBits[i * kBytesPerPixel + 3] = kOpaqueAlpha;
    }
    ::GdiFlush();
}

// 造一张竖向一像素黑白相间的不透明位图。
//
// 这种最高频的图案是重采样质量的判据：按四比一缩小时，做了面积平均就得到
// 均匀的中灰，直接丢弃像素则得到黑白交错的杂纹。
//
//   nSide：边长（像素）。
// 返回：位图，所有权归调用方；失败返回空。
HBITMAP MakeStripeBitmap(int nSide)
{
    BYTE* pBits = NULL;
    HBITMAP hbm = CreateBottomUpDib(nSide, nSide, &pBits);
    if (hbm == NULL)
    {
        return NULL;
    }
    for (int y = 0; y < nSide; ++y)
    {
        for (int x = 0; x < nSide; ++x)
        {
            BYTE v = (x % 2 == 0) ? (BYTE)255 : (BYTE)0;
            BYTE* pPixel = pBits + ((size_t)y * nSide + x) * kBytesPerPixel;
            pPixel[0] = v;
            pPixel[1] = v;
            pPixel[2] = v;
            pPixel[3] = kOpaqueAlpha;
        }
    }
    ::GdiFlush();
    return hbm;
}

// 造一张带柔和边缘的圆形位图，格式是预乘 alpha 的 32 位 DIBSection。
//
// 圆内不透明、圆外全透明，边缘一像素内做线性过渡。预乘的含义是颜色分量
// 事先乘过 alpha，所以全透明处四个通道都是 0 —— 若把这张图按不透明位图
// 绘制，透明区域就会显示成黑色，本页正是用这一点做对照。
//
//   nSide：边长（像素）。
// 返回：位图，所有权归调用方；失败返回空。
HBITMAP MakeSoftCircleBitmap(int nSide)
{
    BYTE* pBits = NULL;
    HBITMAP hbm = CreateBottomUpDib(nSide, nSide, &pBits);
    if (hbm == NULL)
    {
        return NULL;
    }
    //圆形填充色取品牌蓝，与画廊其它演示的主色一致。
    const int kFillR = 45;
    const int kFillG = 108;
    const int kFillB = 223;
    //边缘过渡带的宽度（像素）。
    const double kEdgeWidth = 1.5;

    const double dCenter = (double)nSide / 2.0;
    const double dRadius = dCenter - 1.0;
    for (int y = 0; y < nSide; ++y)
    {
        for (int x = 0; x < nSide; ++x)
        {
            double dx = (double)x + 0.5 - dCenter;
            double dy = (double)y + 0.5 - dCenter;
            double dDist = sqrt(dx * dx + dy * dy);

            double dAlpha = 1.0;
            if (dDist > dRadius)
            {
                dAlpha = 1.0 - (dDist - dRadius) / kEdgeWidth;
            }
            if (dAlpha < 0.0)
            {
                dAlpha = 0.0;
            }
            if (dAlpha > 1.0)
            {
                dAlpha = 1.0;
            }

            BYTE alpha = (BYTE)(dAlpha * 255.0 + 0.5);
            BYTE* pPixel = pBits + ((size_t)y * nSide + x) * kBytesPerPixel;
            //预乘：颜色分量先乘上 alpha 再存。
            pPixel[0] = (BYTE)(kFillB * alpha / 255);
            pPixel[1] = (BYTE)(kFillG * alpha / 255);
            pPixel[2] = (BYTE)(kFillR * alpha / 255);
            pPixel[3] = alpha;
        }
    }
    ::GdiFlush();
    return hbm;
}

// 造一张带竖向渐变与居中文字的大图，不透明。
//
//   nWidth / nHeight：像素尺寸。
// 返回：位图，所有权归调用方；失败返回空。
HBITMAP MakeGradientLabelBitmap(int nWidth, int nHeight)
{
    BYTE* pBits = NULL;
    HBITMAP hbm = CreateBottomUpDib(nWidth, nHeight, &pBits);
    if (hbm == NULL)
    {
        return NULL;
    }
    HDC hdcMem = ::CreateCompatibleDC(NULL);
    if (hdcMem == NULL)
    {
        ::DeleteObject(hbm);
        return NULL;
    }
    HGDIOBJ hOldBitmap = ::SelectObject(hdcMem, hbm);

    //渐变的首末颜色，从品牌蓝过渡到偏青的浅蓝。
    const int kTopR = 45;
    const int kTopG = 108;
    const int kTopB = 223;
    const int kBottomR = 90;
    const int kBottomG = 160;
    const int kBottomB = 255;
    for (int y = 0; y < nHeight; ++y)
    {
        int r = kTopR + (y * (kBottomR - kTopR)) / nHeight;
        int g = kTopG + (y * (kBottomG - kTopG)) / nHeight;
        int b = kTopB + (y * (kBottomB - kTopB)) / nHeight;
        HBRUSH hBrush = ::CreateSolidBrush(RGB(r, g, b));
        RECT rcLine = { 0, y, nWidth, y + 1 };
        ::FillRect(hdcMem, &rcLine, hBrush);
        ::DeleteObject(hBrush);
    }

    ::SetBkMode(hdcMem, TRANSPARENT);
    ::SetTextColor(hdcMem, RGB(255, 255, 255));
    HFONT hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ hOldFont = ::SelectObject(hdcMem, hFont);
    RECT rcText = { 0, 0, nWidth, nHeight };
    CString strText;
    strText.Format(Txt(_T("代码合成的 %d × %d 位图"),
                       _T("%d x %d bitmap made in code")),
                   nWidth, nHeight);
    ::DrawText(hdcMem, strText, -1, &rcText,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(hdcMem, hOldFont);

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::GdiFlush();
    //GDI 画完的这张图 alpha 分量还是 0，补成不透明再交出去。
    ForceOpaqueAlpha(pBits, nWidth, nHeight);
    return hbm;
}

// 用最朴素的位块传输把一张位图缩小到指定尺寸。
//
// 这是「先把位图缩小、再按缩小后的尺寸插进文档」的做法，用来与库内的做法
// 作对照：COLORONCOLOR 模式直接丢弃多余的行列，不做任何加权平均。
//
//   hbmSrc：源位图，本函数只读取，不接管所有权。
//   nSrcW / nSrcH：源位图的像素尺寸。
//   nDstW / nDstH：目标像素尺寸。
// 返回：缩小后的位图，所有权归调用方；失败返回空。
HBITMAP MakeNaivelyShrunkBitmap(HBITMAP hbmSrc, int nSrcW, int nSrcH,
                                int nDstW, int nDstH)
{
    if (hbmSrc == NULL || nDstW <= 0 || nDstH <= 0)
    {
        return NULL;
    }
    BYTE* pBits = NULL;
    HBITMAP hbmDst = CreateBottomUpDib(nDstW, nDstH, &pBits);
    if (hbmDst == NULL)
    {
        return NULL;
    }
    HDC hdcSrc = ::CreateCompatibleDC(NULL);
    HDC hdcDst = ::CreateCompatibleDC(NULL);
    if (hdcSrc == NULL || hdcDst == NULL)
    {
        if (hdcSrc != NULL)
        {
            ::DeleteDC(hdcSrc);
        }
        if (hdcDst != NULL)
        {
            ::DeleteDC(hdcDst);
        }
        ::DeleteObject(hbmDst);
        return NULL;
    }
    HGDIOBJ hOldSrc = ::SelectObject(hdcSrc, hbmSrc);
    HGDIOBJ hOldDst = ::SelectObject(hdcDst, hbmDst);

    ::SetStretchBltMode(hdcDst, COLORONCOLOR);
    ::StretchBlt(hdcDst, 0, 0, nDstW, nDstH,
                 hdcSrc, 0, 0, nSrcW, nSrcH, SRCCOPY);

    ::SelectObject(hdcDst, hOldDst);
    ::SelectObject(hdcSrc, hOldSrc);
    ::DeleteDC(hdcDst);
    ::DeleteDC(hdcSrc);
    ::GdiFlush();
    //位块传输不保证把 alpha 分量一并搬过来，统一补成不透明。
    ForceOpaqueAlpha(pBits, nDstW, nDstH);
    return hbmDst;
}

// 把一张位图作为内嵌对象插入富文本控件的当前选区。
//
// 控件自带的 InsertTaggedImage / InsertImageFromFile 只收磁盘路径，而本页
// 的位图是代码里合成出来的，所以这里按控件文档里「直通排版引擎」的用法取出
// OLE 接口，再调 CDuiImageOle 的插入函数。
//
//   pEdit：目标富文本控件，不能为空。
//   hbm：位图；所有权转移给内嵌对象，插入失败时由被调用方负责删除。
//   tag：写进 REOBJECT.dwUser 的业务标记，日后可由 EnumContent 原样读回。
//   bPremultiplied：位图是否为预乘 alpha 的 32 位 DIBSection。
//   nDisplayW / nDisplayH：排版尺寸（逻辑像素），小于等于 0 表示按源图的
//                          像素尺寸排版。
// 返回：true 表示插入成功。
bool InsertBitmapIntoRichEdit(DuiRichEdit* pEdit, HBITMAP hbm, DWORD_PTR tag,
                              bool bPremultiplied, int nDisplayW, int nDisplayH)
{
    if (hbm == NULL)
    {
        return false;
    }
    if (pEdit == NULL)
    {
        ::DeleteObject(hbm);
        return false;
    }
    IRichEditOle* pRichEditOle = NULL;
    pEdit->SendMessageToEngine(EM_GETOLEINTERFACE, 0, (LPARAM)&pRichEditOle);
    if (pRichEditOle == NULL)
    {
        ::DeleteObject(hbm);
        return false;
    }
    bool bOk = CDuiImageOle::InsertIntoRichEditOle(pRichEditOle, hbm,
                                                   /*ownsHbm=*/true, tag,
                                                   bPremultiplied,
                                                   nDisplayW, nDisplayH);
    //接口的引用计数由引擎加好，用完必须释放。
    pRichEditOle->Release();
    pEdit->Invalidate();
    return bOk;
}

// 新建一个演示用的富文本控件。
//   szText：初始文字，允许为空串。
// 返回：控件，所有权交给调用方。
std::unique_ptr<DuiRichEdit> MakeDemoRichEdit(LPCTSTR szText)
{
    std::unique_ptr<DuiRichEdit> edit(new DuiRichEdit());
    edit->SetMultiLine(true);
    edit->SetWordWrap(true);
    //演示行的高度是固定的，关掉自动增高免得控件与版式互相较劲。
    edit->SetAutoGrow(false);
    edit->SetText(szText);
    return edit;
}

// 把光标移到文档末尾，供连续插入多张图片时使用。
//   pEdit：富文本控件，不能为空。
void MoveCaretToEnd(DuiRichEdit* pEdit)
{
    if (pEdit == NULL)
    {
        return;
    }
    //与库内 AppendText 的写法一致：两个 -1 表示把光标收到文档末尾。
    pEdit->SetSel(-1, -1);
}

// 往文档里插一张条纹图，走库内的绘制路径（源图保持全分辨率）。
//   pEdit：富文本控件。
void InsertFullResolutionStripe(DuiRichEdit* pEdit)
{
    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, MakeStripeBitmap(kStripeSide), 0,
                             /*bPremultiplied=*/false,
                             kStripeDisplaySide, kStripeDisplaySide);
}

// 往文档里插一张**先缩小再插入**的条纹图，作为对照。
//   pEdit：富文本控件。
void InsertPreShrunkStripe(DuiRichEdit* pEdit)
{
    HBITMAP hbmSrc = MakeStripeBitmap(kStripeSide);
    if (hbmSrc == NULL)
    {
        return;
    }
    HBITMAP hbmSmall = MakeNaivelyShrunkBitmap(hbmSrc, kStripeSide, kStripeSide,
                                               kStripeDisplaySide,
                                               kStripeDisplaySide);
    //源图只是中间产物，缩小完就该释放；它的所有权没有交出去过。
    ::DeleteObject(hbmSrc);

    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, hbmSmall, 0, /*bPremultiplied=*/false,
                             kStripeDisplaySide, kStripeDisplaySide);
}

// 往文档里插一张预乘 alpha 的圆形图，并按指定的标志绘制。
//   pEdit：富文本控件。
//   bDeclarePremultiplied：传给内嵌对象的「是否预乘 alpha」标志。传 false
//                          就是故意传错，用来看错在哪里。
void InsertSoftCircle(DuiRichEdit* pEdit, bool bDeclarePremultiplied)
{
    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kSyntheticFaceSide),
                             kTagAlphaImage, bDeclarePremultiplied,
                             kFaceDisplaySide, kFaceDisplaySide);
}

// 按三种排版尺寸把同一张图各插一次。
//   pEdit：富文本控件。
void InsertThreeDisplaySizes(DuiRichEdit* pEdit)
{
    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kDecoupleSourceSide), 0,
                             /*bPremultiplied=*/true,
                             kDecoupleSmall, kDecoupleSmall);
    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kDecoupleSourceSide), 0,
                             /*bPremultiplied=*/true,
                             kDecoupleMiddle, kDecoupleMiddle);
    MoveCaretToEnd(pEdit);
    InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kDecoupleSourceSide), 0,
                             /*bPremultiplied=*/true,
                             kDecoupleLarge, kDecoupleLarge);
}

// EnumContent 的逐段回调：把每一段按文档顺序追加到上下文里的字符串上。
//   bIsImage：本段是不是一张内联图片。
//   szText：文本段的内容；图片段时为空串。
//   tag：图片段的业务标记；文本段时恒为 0。
//   pCtx：上下文，实际类型是 CString*，由 EnumContent 原样透传。
void AppendContentSegment(bool bIsImage, LPCTSTR szText, DWORD_PTR tag,
                          void* pCtx)
{
    CString* pOut = (CString*)pCtx;
    if (pOut == NULL)
    {
        return;
    }
    CString strSegment;
    if (bIsImage)
    {
        strSegment.Format(Txt(_T("[图片 标记=%u]"), _T("[image tag=%u]")),
                          (unsigned)tag);
    }
    else
    {
        CString strText = (szText != NULL) ? szText : _T("");
        //说明标签只有一行的高度，文本段里的换行换成空格才不会被截断。
        strText.Replace(_T("\r"), _T(" "));
        strText.Replace(_T("\n"), _T(" "));
        strSegment.Format(Txt(_T("[文本 \"%s\"]"), _T("[text \"%s\"]")),
                          (LPCTSTR)strText);
    }
    if (!pOut->IsEmpty())
    {
        *pOut += _T("  ");
    }
    *pOut += strSegment;
}

#endif // BUI_FEATURE_RICHTEXT && BUI_FEATURE_IMAGEOLE

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_ImageOle()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

#if BUI_FEATURE_RICHTEXT && BUI_FEATURE_IMAGEOLE
    const CString strFacePath = AssetPath(kFacePngRelPath);
    const bool bHasFacePng = AssetExists(strFacePath);

    //—— 段落一：往富文本控件里插图 ——
    AddSection(page.get(),
               Txt(_T("往富文本控件里插图"), _T("Inserting images")),
               Txt(_T("CDuiImageOle 把一张位图包成 OLE 对象嵌进富文本文档。")
                   _T("它不是 DuiControl 的子类，不进控件树，业务一般不直接")
                   _T("用它，而是调 DuiRichEdit 的 InsertTaggedImage（带业务")
                   _T("标记）或 InsertImageFromFile（不带标记）。下面几个按钮")
                   _T("把图插在当前光标处，先在文本里点一下再插，就能看到")
                   _T("插入位置跟着光标走。"),
                   _T("CDuiImageOle wraps a bitmap as an OLE object embedded ")
                   _T("in a rich text document. It is not a DuiControl and ")
                   _T("never joins the control tree; callers normally go ")
                   _T("through DuiRichEdit::InsertTaggedImage (with a ")
                   _T("business tag) or InsertImageFromFile (without one). ")
                   _T("The buttons below insert at the caret - click inside ")
                   _T("the text first and the image follows the caret.")));
    {
        std::unique_ptr<DuiRichEdit> edit = MakeDemoRichEdit(
            Txt(_T("先在这里点一下把光标放好，再按下面的按钮插图。"),
                _T("Click here to place the caret, then use the buttons.")));
        DuiRichEdit* pEdit = edit.get();

        std::unique_ptr<DuiHBox> editRow(new DuiHBox());
        editRow->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(editRow), kRichEditH);

        AddGap(page.get(), kInnerGap);

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(kRowGap);

        std::unique_ptr<FnButton> buttonFace = MakeButton(Txt(_T("插入表情"),
                                                              _T("Emoji")));
        buttonFace->onClick = [pEdit](FnButton*)
        {
            InsertBitmapIntoRichEdit(pEdit,
                                     MakeSoftCircleBitmap(kSyntheticFaceSide),
                                     kTagFace, /*bPremultiplied=*/true,
                                     kFaceDisplaySide, kFaceDisplaySide);
        };
        buttonRow->AddChild(std::move(buttonFace),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonLarge = MakeButton(Txt(_T("插入大图"),
                                                               _T("Large image")));
        buttonLarge->onClick = [pEdit](FnButton*)
        {
            InsertBitmapIntoRichEdit(pEdit,
                                     MakeGradientLabelBitmap(kLargeImageW,
                                                             kLargeImageH),
                                     kTagLargeImage, /*bPremultiplied=*/false,
                                     kLargeDisplayW, kLargeDisplayH);
        };
        buttonRow->AddChild(std::move(buttonLarge),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonAlpha = MakeButton(Txt(_T("插入透明图"),
                                                               _T("Alpha image")));
        buttonAlpha->onClick = [pEdit](FnButton*)
        {
            InsertSoftCircle(pEdit, /*bDeclarePremultiplied=*/true);
        };
        buttonRow->AddChild(std::move(buttonAlpha),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonFile = MakeButton(Txt(_T("从文件插入"),
                                                              _T("From file")));
        //从磁盘插图这条路走的是控件自带的接口，素材不在就把按钮置灰。
        if (bHasFacePng)
        {
            CString strPath = strFacePath;
            buttonFile->onClick = [pEdit, strPath](FnButton*)
            {
                pEdit->InsertTaggedImage(strPath, kTagFromFile, kFaceDisplaySide);
            };
        }
        else
        {
            buttonFile->SetEnabled(false);
        }
        buttonRow->AddChild(std::move(buttonFile),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonClear = MakeButton(Txt(_T("清空"),
                                                               _T("Clear")));
        buttonClear->onClick = [pEdit](FnButton*)
        {
            pEdit->SetText(_T(""));
        };
        buttonRow->AddChild(std::move(buttonClear),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));

        buttonRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                            DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(buttonRow), kButtonRowH);

        if (!bHasFacePng)
        {
            AddNoticeRow(page.get(),
                         Txt(_T("找不到素材 Face\\face0.png，「从文件插入」")
                             _T("按钮已置灰。把 Face 目录放到 DuiGallery.exe ")
                             _T("所在目录下即可恢复。其余按钮用的是代码里")
                             _T("合成的位图，不依赖磁盘文件。"),
                             _T("Asset Face\\face0.png is missing, so the ")
                             _T("\"From file\" button is disabled. Copy the ")
                             _T("Face folder next to DuiGallery.exe to get it ")
                             _T("back. The other buttons synthesize their ")
                             _T("bitmaps in code and need no files.")));
        }
    }

    //—— 段落二：排版尺寸与源图分辨率解耦 ——
    AddSection(page.get(),
               Txt(_T("排版尺寸与源图分辨率是两回事"),
                   _T("Layout size is not source resolution")),
               Txt(_T("同一张 96 × 96 的源图按 24、48、96 三种排版尺寸各插")
                   _T("一次。位图始终按原始分辨率保留，排版尺寸只决定它在")
                   _T("文档里占多大版面；真正的缩放发生在绘制时，从全分辨率")
                   _T("源图一次性重采样到实际矩形。刻意不预先把位图缩小 —— ")
                   _T("预缩小会先丢一次信息，而排出来的矩形受屏幕缩放影响")
                   _T("未必正好等于给定值，于是绘制时又要放大回去，一缩一放")
                   _T("正是内联小图发糊的成因。"),
                   _T("The same 96 x 96 source is inserted three times, laid ")
                   _T("out at 24, 48 and 96. The bitmap always keeps its full ")
                   _T("resolution; the layout size only decides how much room ")
                   _T("it takes in the document. The actual scaling happens at ")
                   _T("draw time, resampling once from the full-resolution ")
                   _T("source into the real rect. Shrinking the bitmap up ")
                   _T("front is deliberately avoided: it throws information ")
                   _T("away, and because the final rect depends on display ")
                   _T("scaling it usually has to be scaled back up again - ")
                   _T("that round trip is what makes inline images look ")
                   _T("blurry.")));
    {
        std::unique_ptr<DuiRichEdit> edit = MakeDemoRichEdit(_T(""));
        DuiRichEdit* pEdit = edit.get();
        InsertThreeDisplaySizes(pEdit);

        std::unique_ptr<DuiHBox> editRow(new DuiHBox());
        editRow->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(editRow), kRichEditH);

        AddGap(page.get(), kInnerGap);

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        std::unique_ptr<FnButton> buttonAgain = MakeButton(
            Txt(_T("再插一组"), _T("Insert again")));
        buttonAgain->onClick = [pEdit](FnButton*)
        {
            InsertThreeDisplaySizes(pEdit);
        };
        buttonRow->AddChild(std::move(buttonAgain),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));
        buttonRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                            DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(buttonRow), kButtonRowH);
    }

    //—— 段落三：重采样质量对照 ——
    AddSection(page.get(),
               Txt(_T("重采样质量对照"), _T("Resampling quality")),
               Txt(_T("两边装的是同一张 96 × 96 的一像素竖条纹图，都按 24 × ")
                   _T("24 排版。左边把全分辨率的源图交给内嵌对象，绘制时走 ")
                   _T("GDI+ 的高质量双三次重采样，相邻的黑白被平均成均匀的")
                   _T("中灰；右边先用位块传输把位图缩到 24 × 24 再插入，")
                   _T("该模式直接丢弃多余的行列，于是留下黑白交错的杂纹。")
                   _T("条纹图是这类差别最好的判据 —— 平缓的图案两种做法看")
                   _T("起来都还行。"),
                   _T("Both sides hold the same 96 x 96 one-pixel stripe ")
                   _T("pattern laid out at 24 x 24. On the left the ")
                   _T("full-resolution source goes to the embedded object and ")
                   _T("is resampled at draw time with high quality bicubic ")
                   _T("filtering, averaging neighbouring black and white into ")
                   _T("an even grey. On the right the bitmap is first shrunk ")
                   _T("with a plain bit block transfer that simply drops rows ")
                   _T("and columns, leaving a noisy pattern. A stripe pattern ")
                   _T("is the sharpest test here - smooth images look fine ")
                   _T("either way.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiRichEdit> editGood = MakeDemoRichEdit(_T(""));
        DuiRichEdit* pEditGood = editGood.get();
        InsertFullResolutionStripe(pEditGood);
        row->AddChild(std::move(editGood),
                      DuiLayout::Hint().Fixed(kRichEditPairW));

        std::unique_ptr<DuiRichEdit> editNaive = MakeDemoRichEdit(_T(""));
        DuiRichEdit* pEditNaive = editNaive.get();
        InsertPreShrunkStripe(pEditNaive);
        row->AddChild(std::move(editNaive),
                      DuiLayout::Hint().Fixed(kRichEditPairW));

        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kRichEditPairH);

        std::unique_ptr<DuiHBox> captionRow(new DuiHBox());
        captionRow->SetGap(kRowGap);
        captionRow->AddChild(MakeCaption(Txt(_T("全分辨率源图，绘制时重采样"),
                                             _T("Full-res source, resampled at ")
                                             _T("draw time"))),
                             DuiLayout::Hint().Fixed(kRichEditPairW));
        captionRow->AddChild(MakeCaption(Txt(_T("先缩小再插入，丢像素"),
                                             _T("Pre-shrunk, pixels dropped"))),
                             DuiLayout::Hint().Fixed(kRichEditPairW));
        captionRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                             DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(captionRow), kCaptionRowH);
    }

    //—— 段落四：预乘 alpha 传对与传错 ——
    AddSection(page.get(),
               Txt(_T("预乘 alpha 传对与传错"),
                   _T("Premultiplied alpha, right and wrong")),
               Txt(_T("两边装的都是同一份预乘 alpha 的圆形位图：圆内不透明、")
                   _T("圆外全透明。左边如实告诉内嵌对象「这是预乘 alpha 的 ")
                   _T("32 位位图」，绘制时按透明通道混合，圆外保持底色；")
                   _T("右边故意传 false，绘制便走不透明路径，透明区域的四个")
                   _T("通道都是 0，于是显示成黑色方块。这个标志无法由代码")
                   _T("探测 —— 一张 32 位位图的 alpha 有没有预乘，从像素本身")
                   _T("看不出来，只能由调用方如实声明。产出这种格式的位图")
                   _T("请用 LoadPremultipliedDibFromFile。"),
                   _T("Both sides hold the same premultiplied circle: opaque ")
                   _T("inside, fully transparent outside. The left one tells ")
                   _T("the embedded object the truth - a 32 bit premultiplied ")
                   _T("bitmap - so drawing blends through the alpha channel ")
                   _T("and the background survives around the circle. The ")
                   _T("right one deliberately passes false, so drawing takes ")
                   _T("the opaque path; the transparent pixels are all zero ")
                   _T("and show up as a black square. The flag cannot be ")
                   _T("detected in code - nothing in the pixels says whether ")
                   _T("alpha was premultiplied - so the caller has to declare ")
                   _T("it honestly. Use LoadPremultipliedDibFromFile to ")
                   _T("produce bitmaps in this format.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiRichEdit> editRight = MakeDemoRichEdit(_T(""));
        InsertSoftCircle(editRight.get(), /*bDeclarePremultiplied=*/true);
        row->AddChild(std::move(editRight),
                      DuiLayout::Hint().Fixed(kRichEditPairW));

        std::unique_ptr<DuiRichEdit> editWrong = MakeDemoRichEdit(_T(""));
        InsertSoftCircle(editWrong.get(), /*bDeclarePremultiplied=*/false);
        row->AddChild(std::move(editWrong),
                      DuiLayout::Hint().Fixed(kRichEditPairW));

        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kRichEditPairH);

        std::unique_ptr<DuiHBox> captionRow(new DuiHBox());
        captionRow->SetGap(kRowGap);
        captionRow->AddChild(MakeCaption(Txt(_T("标志传 true：按透明通道混合"),
                                             _T("Flag true: blended"))),
                             DuiLayout::Hint().Fixed(kRichEditPairW));
        captionRow->AddChild(MakeCaption(Txt(_T("标志传 false：透明处成黑块"),
                                             _T("Flag false: black square"))),
                             DuiLayout::Hint().Fixed(kRichEditPairW));
        captionRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                             DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(captionRow), kCaptionRowH);
    }

    //—— 段落五：按文档顺序读回内容 ——
    AddSection(page.get(),
               Txt(_T("按文档顺序读回内容"), _T("Reading the document back")),
               Txt(_T("EnumContent 把图文混排的内容按文档顺序拆成一段一段，")
                   _T("纯文本段与图片段穿插回调，图片段带回插入时给的业务")
                   _T("标记。聊天输入框发送消息时就靠它把内容编码成正文。")
                   _T("不能改用「读全文」那条路：读回来的文本里每个内联图")
                   _T("只是一个占位字符，既认不出是哪张图，也拿不到它的标记。")
                   _T("下面这个文档里已经插好了两张带标记的图，按按钮把内容")
                   _T("读出来。"),
                   _T("EnumContent walks mixed text and images in document ")
                   _T("order, calling back once per run, with the business ")
                   _T("tag carried along for image runs. The chat input box ")
                   _T("uses it to encode a message before sending. Reading ")
                   _T("the whole text instead does not work: each inline ")
                   _T("image is only a placeholder character there, with no ")
                   _T("way to tell which image it was or what tag it carried. ")
                   _T("The document below already holds two tagged images - ")
                   _T("press the button to read it back.")));
    {
        std::unique_ptr<DuiRichEdit> edit = MakeDemoRichEdit(_T(""));
        DuiRichEdit* pEdit = edit.get();
        pEdit->AppendText(Txt(_T("今天辛苦了 "), _T("Nice work today ")));
        MoveCaretToEnd(pEdit);
        InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kSyntheticFaceSide),
                                 kTagFace, /*bPremultiplied=*/true,
                                 kFaceDisplaySide, kFaceDisplaySide);
        pEdit->AppendText(Txt(_T(" 明天见 "), _T(" see you tomorrow ")));
        MoveCaretToEnd(pEdit);
        InsertBitmapIntoRichEdit(pEdit, MakeSoftCircleBitmap(kSyntheticFaceSide),
                                 kTagAlphaImage, /*bPremultiplied=*/true,
                                 kFaceDisplaySide, kFaceDisplaySide);

        std::unique_ptr<DuiHBox> editRow(new DuiHBox());
        editRow->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(editRow), kRichEditH);

        AddGap(page.get(), kInnerGap);

        std::unique_ptr<DuiLabel> resultLabel = MakeLabel(
            Txt(_T("按下按钮后，这里显示读回来的内容。"),
                _T("The content read back appears here.")),
            kCaptionColor, DT_LEFT | DT_TOP);
        resultLabel->SetWordWrap(true);
        DuiLabel* pResultLabel = resultLabel.get();

        std::unique_ptr<DuiHBox> buttonRow(new DuiHBox());
        buttonRow->SetGap(kRowGap);
        std::unique_ptr<FnButton> buttonRead = MakeButton(
            Txt(_T("读取内容"), _T("Read back")));
        buttonRead->onClick = [pEdit, pResultLabel](FnButton*)
        {
            CString strContent;
            pEdit->EnumContent(&AppendContentSegment, &strContent);
            if (strContent.IsEmpty())
            {
                strContent = Txt(_T("文档是空的。"), _T("The document is empty."));
            }
            CString strLine;
            strLine.Format(Txt(_T("共 %d 张内联图片。%s"),
                               _T("%d inline images. %s")),
                           pEdit->GetEmbeddedImageCount(),
                           (LPCTSTR)strContent);
            pResultLabel->SetText(strLine);
        };
        buttonRow->AddChild(std::move(buttonRead),
                            DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));
        buttonRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                            DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(buttonRow), kButtonRowH);

        std::unique_ptr<DuiHBox> resultRow(new DuiHBox());
        resultRow->AddChild(std::move(resultLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(resultRow), kNoticeRowH);
    }
#else
    AddSection(page.get(),
               Txt(_T("本页面未编译进来"), _T("This page was compiled out")),
               Txt(_T("富文本或内嵌图片能力被裁剪开关关掉了（定义了 ")
                   _T("BUI_DISABLE_RICHTEXT 或 BUI_DISABLE_IMAGEOLE），")
                   _T("CDuiImageOle 与 DuiRichEdit 的插图接口都不存在，")
                   _T("本页面因此没有内容。"),
                   _T("Rich text or the embedded image feature was stripped ")
                   _T("(BUI_DISABLE_RICHTEXT or BUI_DISABLE_IMAGEOLE is ")
                   _T("defined), so neither CDuiImageOle nor the image ")
                   _T("insertion API of DuiRichEdit exists and this page has ")
                   _T("no content.")));
#endif // BUI_FEATURE_RICHTEXT && BUI_FEATURE_IMAGEOLE

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 异步图片加载（DuiAsyncImage）
// =====================================================================

namespace {

// 表情素材的文件名格式，相对画廊可执行文件所在目录。
const LPCTSTR kFacePngFormat = _T("Face\\face%d.png");

// 结果网格的行列数与单元格边长（像素）。
const int kGridRows = 5;
const int kGridCols = 8;
const int kGridCellSide = 48;
const int kGridGap = 8;
// 网格里单元格的总数，也就是一批加载多少张图。
const int kGridCellCount = kGridRows * kGridCols;
// 网格容器的固定宽高（像素）。写死是为了让单元格保持正方形 —— 网格容器
// 会把可用宽度平分给各列，容器过宽时单元格就被拉扁了。
const int kGridW = kGridCols * kGridCellSide + (kGridCols - 1) * kGridGap;
const int kGridH = kGridRows * kGridCellSide + (kGridRows - 1) * kGridGap;

// 演示行下方说明文字分成两列时每列的宽度（像素）。取网格宽度的一半，两列
// 说明正好压在网格的左右两半上。
const int kCaptionColW = kGridW / 2;
// 过期结果对照里单元格的边长（像素）。
const int kStaleCellSide = 64;
// 过期结果对照里一次连按提交多少个请求。改这个值时记得同步改该段落说明里
// 写的次数。
const int kStaleBatchCount = 8;
// 结果单元格的圆角半径（像素）。
const int kCellCornerRadius = 6;

// 请求号（Submit 的 userToken）的编码方式。
//
// 回调是一个自由函数，拿到的只有一个 DWORD_PTR，因此把三段信息编进去：
// 低 8 位是槽位序号（结果该落到哪个控件上），中间 8 位是同一槽位上的请求
// 流水号（用于丢弃过期结果），其余高位是页面代号（用于丢弃上一次页面留下
// 的结果）。
const int kTokenSlotBits = 8;
const int kTokenSerialBits = 8;
const DWORD_PTR kTokenSlotMask = 0xFF;
const DWORD_PTR kTokenSerialMask = 0xFF;

// 槽位序号：0 到 kGridCellCount - 1 是结果网格的单元格，200 之后是各段落
// 自己的单元格。
const int kSlotGuardedCell = 200;
const int kSlotNaiveCell = 201;
const int kSlotFailure = 202;
const int kSlotCancelled = 203;

// 把三段信息编成一个请求号。
//   nEpoch：页面代号。
//   nSerial：同一槽位上的请求流水号，取值 0 到 255。
//   nSlot：槽位序号，取值 0 到 255。
// 返回：编码后的请求号。
DWORD_PTR MakeAsyncToken(unsigned nEpoch, unsigned nSerial, int nSlot)
{
    DWORD_PTR token = (DWORD_PTR)nEpoch << (kTokenSlotBits + kTokenSerialBits);
    token |= ((DWORD_PTR)nSerial & kTokenSerialMask) << kTokenSlotBits;
    token |= (DWORD_PTR)nSlot & kTokenSlotMask;
    return token;
}

// 从请求号里取出页面代号。
unsigned TokenEpoch(DWORD_PTR token)
{
    return (unsigned)(token >> (kTokenSlotBits + kTokenSerialBits));
}

// 从请求号里取出请求流水号。
unsigned TokenSerial(DWORD_PTR token)
{
    return (unsigned)((token >> kTokenSlotBits) & kTokenSerialMask);
}

// 从请求号里取出槽位序号。
int TokenSlot(DWORD_PTR token)
{
    return (int)(token & kTokenSlotMask);
}

// 拼出第 n 张表情素材的绝对路径。
//   nIndex：素材序号。
// 返回：绝对路径。
CString FacePngPath(int nIndex)
{
    CString strRelative;
    strRelative.Format(kFacePngFormat, nIndex);
    return AssetPath(strRelative);
}

// 异步加载演示页面的运行期状态。
//
// 回调只能通过文件级指针找到这里，所以所有需要在回调里更新的控件都登记在
// 这个结构里。控件本身归各自的容器所有，这里只借用裸指针，页面销毁时一并
// 失效 —— 因此本结构被销毁时必须把文件级指针一起清掉。
struct AsyncPageState
{
    // 页面代号。每建一次页面加一，回调靠它丢弃上一次页面留下的结果。
    unsigned m_nEpoch;
    // 结果网格的单元格。不持有控件所有权。
    DuiAvatar* m_cells[kGridCellCount];
    // 各单元格当前显示的位图。所有权在本结构，页面销毁时统一删除。
    HBITMAP m_cellBitmaps[kGridCellCount];
    // 第一段里显示耗时与到达进度的标签。不持有所有权。
    DuiLabel* m_pStatusLabel;
    // 本批一共提交了多少个请求、已经到达多少个。
    int m_nExpected;
    int m_nArrived;
    // 本批提交的时刻（毫秒，取自系统运行时间），用于算结果到达的耗时。
    DWORD m_dwSubmitTick;

    // 比对请求号的那个单元格及其位图与计数标签。
    DuiAvatar* m_pGuardedCell;
    HBITMAP m_hGuardedBitmap;
    DuiLabel* m_pGuardedLabel;
    // 该单元格上最后一次提交的请求流水号。回调只认这一个。
    unsigned m_nGuardedLatestSerial;
    // 该单元格实际应用过多少次结果。
    int m_nGuardedApplied;

    // 不比对请求号的那个单元格及其位图与计数标签。
    DuiAvatar* m_pNaiveCell;
    HBITMAP m_hNaiveBitmap;
    DuiLabel* m_pNaiveLabel;
    // 该单元格实际应用过多少次结果。
    int m_nNaiveApplied;

    // 第四段里显示失败与取消结果的标签。不持有所有权。
    DuiLabel* m_pOutcomeLabel;
};

// 当前异步加载页面的状态。页面存在时指向 AsyncDemoRow 内的那一份，页面
// 销毁后为空。
AsyncPageState* g_pAsyncPageState = NULL;

// 页面代号的计数器。每建一次异步加载页面加一。
unsigned g_nAsyncEpochCounter = 0;

// 异步加载演示的第一行，同时充当整页运行期状态的持有者。
//
// 结果回调是一个自由函数（本仓库不鼓励使用 lambda），它必须能找到要更新的
// 控件，所以状态放在文件级指针上：本行构造时把指针指向自己内部的那一份，
// 析构时清空并释放已经加载好的位图。页面销毁之后回调只会看到空指针，把
// 结果连同位图一起丢弃。
class AsyncDemoRow : public DuiHBox
{
public:
    AsyncDemoRow();
    ~AsyncDemoRow() override;

    // 取本行持有的页面状态，供页面构建代码填写控件指针。
    // 返回：状态结构的引用，生命期与本行相同。
    AsyncPageState& State() { return m_state; }

private:
    // 本页面的状态，随本行一起存亡。
    AsyncPageState m_state;
};

AsyncDemoRow::AsyncDemoRow()
{
    ::ZeroMemory(&m_state, sizeof(m_state));
    m_state.m_nEpoch = ++g_nAsyncEpochCounter;
    g_pAsyncPageState = &m_state;
}

AsyncDemoRow::~AsyncDemoRow()
{
    //回调把位图的所有权交给了本页面，页面消失前必须逐个释放。这里只碰
    //位图句柄、不碰单元格控件，因此与各容器的销毁次序无关。
    for (int i = 0; i < kGridCellCount; ++i)
    {
        if (m_state.m_cellBitmaps[i] != NULL)
        {
            ::DeleteObject(m_state.m_cellBitmaps[i]);
            m_state.m_cellBitmaps[i] = NULL;
        }
    }
    if (m_state.m_hGuardedBitmap != NULL)
    {
        ::DeleteObject(m_state.m_hGuardedBitmap);
        m_state.m_hGuardedBitmap = NULL;
    }
    if (m_state.m_hNaiveBitmap != NULL)
    {
        ::DeleteObject(m_state.m_hNaiveBitmap);
        m_state.m_hNaiveBitmap = NULL;
    }
    // 只在全局指针仍然指向自己这一份时才清空。画廊切换页面时是**先建新
    // 页面、再销毁旧页面**，若无条件清空，旧页面的析构会把新页面刚登记
    // 上去的那一份抹掉。
    if (g_pAsyncPageState == &m_state)
    {
        g_pAsyncPageState = NULL;
    }
}

// 把一张位图交给某个单元格显示，并释放它原先显示的那一张。
//
// 头像控件不持有位图的所有权，所以旧位图必须由本页面自己删除；顺序是先让
// 控件改用新位图，再删旧的，中间不会出现控件指向已释放位图的时刻。
//
//   pCell：单元格控件，可为空（此时直接删掉传入的位图）。
//   phSlot：保存该单元格当前位图的位置，不能为空。
//   hbmNew：新位图，所有权转移给本页面。
void ApplyBitmapToCell(DuiAvatar* pCell, HBITMAP* phSlot, HBITMAP hbmNew)
{
    if (phSlot == NULL)
    {
        if (hbmNew != NULL)
        {
            ::DeleteObject(hbmNew);
        }
        return;
    }
    HBITMAP hbmOld = *phSlot;
    *phSlot = hbmNew;
    if (pCell != NULL)
    {
        //SetBitmap 内部已经触发重绘，这里不必再调一次 Invalidate。
        pCell->SetBitmap(hbmNew);
    }
    if (hbmOld != NULL)
    {
        ::DeleteObject(hbmOld);
    }
}

// 刷新过期结果对照那一段的两个计数标签。
//   state：页面状态。
void RefreshStaleCounters(const AsyncPageState& state)
{
    if (state.m_pGuardedLabel != NULL)
    {
        CString strText;
        strText.Format(Txt(_T("比对请求号：应用了 %d 次结果"),
                           _T("Checks the id: applied %d results")),
                       state.m_nGuardedApplied);
        state.m_pGuardedLabel->SetText(strText);
    }
    if (state.m_pNaiveLabel != NULL)
    {
        CString strText;
        strText.Format(Txt(_T("不比对：应用了 %d 次结果"),
                           _T("No check: applied %d results")),
                       state.m_nNaiveApplied);
        state.m_pNaiveLabel->SetText(strText);
    }
}

// 刷新第一段里那行状态文字，报告结果网格已经落位了多少张。
//   state：页面状态。
void RefreshArrivalStatus(const AsyncPageState& state)
{
    if (state.m_pStatusLabel == NULL)
    {
        return;
    }
    CString strText;
    strText.Format(Txt(_T("异步加载：已落位 %d / %d 张，自提交起 %u 毫秒"),
                       _T("Async: %d of %d placed, %u ms since submit")),
                   state.m_nArrived,
                   state.m_nExpected,
                   (unsigned)(::GetTickCount() - state.m_dwSubmitTick));
    state.m_pStatusLabel->SetText(strText);
}

// 异步加载的结果回调。
//
// 由 DuiAsyncImage 在**提交线程**上调用（靠一个只收消息的隐藏窗口投递），
// 画廊有正常的消息循环，所以不需要额外做什么结果就会自己到。位图的所有权
// 随结果转移给本函数：用得上就交给页面保管、用不上必须当场删除，否则每次
// 演示都会漏掉一批 GDI 对象。
//
//   result：加载结果。result.userToken 是提交时编好的请求号。
void OnAsyncImageLoaded(const DuiAsyncImage::Result& result)
{
    const unsigned nEpoch = TokenEpoch(result.userToken);
    const unsigned nSerial = TokenSerial(result.userToken);
    const int nSlot = TokenSlot(result.userToken);

    //页面已经换掉、或者这是上一次页面留下的结果：位图当场删除。
    if (g_pAsyncPageState == NULL || g_pAsyncPageState->m_nEpoch != nEpoch)
    {
        if (result.hbm != NULL)
        {
            ::DeleteObject(result.hbm);
        }
        return;
    }
    AsyncPageState& state = *g_pAsyncPageState;

    //结果网格：一个槽位对应一个单元格，先到先落位。
    if (nSlot >= 0 && nSlot < kGridCellCount)
    {
        ++state.m_nArrived;
        ApplyBitmapToCell(state.m_cells[nSlot], &state.m_cellBitmaps[nSlot],
                          result.hbm);
        RefreshArrivalStatus(state);
        return;
    }

    switch (nSlot)
    {
    //比对请求号的单元格：只认最后一次提交的那个流水号，其余一律丢弃。
    case kSlotGuardedCell:
        if (nSerial != state.m_nGuardedLatestSerial)
        {
            if (result.hbm != NULL)
            {
                ::DeleteObject(result.hbm);
            }
            return;
        }
        ++state.m_nGuardedApplied;
        ApplyBitmapToCell(state.m_pGuardedCell, &state.m_hGuardedBitmap,
                          result.hbm);
        RefreshStaleCounters(state);
        break;

    //不比对请求号的单元格：来一个用一个，于是每个中间结果都被画了一遍。
    case kSlotNaiveCell:
        ++state.m_nNaiveApplied;
        ApplyBitmapToCell(state.m_pNaiveCell, &state.m_hNaiveBitmap,
                          result.hbm);
        RefreshStaleCounters(state);
        break;

    //加载失败演示：这里等的就是一个失败的结果。
    case kSlotFailure:
        if (state.m_pOutcomeLabel != NULL)
        {
            CString strText;
            strText.Format(Txt(_T("失败的请求也会回调一次：ok=%s，位图为%s。"),
                               _T("A failed request still calls back: ok=%s, ")
                               _T("bitmap is %s.")),
                           result.ok ? _T("true") : _T("false"),
                           (result.hbm != NULL)
                               ? Txt(_T("非空"), _T("not null"))
                               : Txt(_T("空"), _T("null")));
            state.m_pOutcomeLabel->SetText(strText);
        }
        if (result.hbm != NULL)
        {
            ::DeleteObject(result.hbm);
        }
        break;

    //取消演示：能走到这里说明取消之后仍然回调了一次，这是文档写明的行为。
    case kSlotCancelled:
        if (state.m_pOutcomeLabel != NULL)
        {
            state.m_pOutcomeLabel->SetText(
                Txt(_T("取消之后仍然收到了一次回调 —— Cancel 只保证返回")
                    _T("之后不再回调，已经开始解码的请求照常走完。"),
                    _T("A callback arrived after Cancel - cancelling only ")
                    _T("guarantees no callback after Cancel returns; a ")
                    _T("request already being decoded runs to completion.")));
        }
        if (result.hbm != NULL)
        {
            ::DeleteObject(result.hbm);
        }
        break;

    //其余槽位序号不该出现，位图仍然要释放，避免漏掉 GDI 对象。
    default:
        if (result.hbm != NULL)
        {
            ::DeleteObject(result.hbm);
        }
        break;
    }
}

// 清空结果网格里已经落位的图片。
//   state：页面状态。
void ClearResultGrid(AsyncPageState& state)
{
    for (int i = 0; i < kGridCellCount; ++i)
    {
        ApplyBitmapToCell(state.m_cells[i], &state.m_cellBitmaps[i], NULL);
    }
    state.m_nArrived = 0;
    state.m_nExpected = 0;
}

// 同步加载一整批图片：在当前线程上逐张解码，期间界面完全不响应。
//   state：页面状态。
void RunSynchronousBatch(AsyncPageState& state)
{
    ClearResultGrid(state);
    state.m_nExpected = kGridCellCount;
    state.m_dwSubmitTick = ::GetTickCount();

    const DWORD dwBegin = ::GetTickCount();
    for (int i = 0; i < kGridCellCount; ++i)
    {
        //走的是与异步路径完全相同的解码逻辑，区别只在于它在调用线程上跑完
        //才返回。
        DuiAsyncImage::RunSyncForTests(FacePngPath(i),
                                       MakeAsyncToken(state.m_nEpoch, 0, i),
                                       &OnAsyncImageLoaded);
    }
    const DWORD dwCost = ::GetTickCount() - dwBegin;

    if (state.m_pStatusLabel != NULL)
    {
        CString strText;
        strText.Format(Txt(_T("同步加载 %d 张用了 %u 毫秒，这段时间里界面")
                           _T("完全没有响应。"),
                           _T("Loading %d images synchronously took %u ms, ")
                           _T("during which the window answered nothing.")),
                       kGridCellCount, (unsigned)dwCost);
        state.m_pStatusLabel->SetText(strText);
    }
}

// 异步加载一整批图片：只把请求排进队列，解码在后台线程上做。
//   state：页面状态。
void RunAsynchronousBatch(AsyncPageState& state)
{
    ClearResultGrid(state);
    state.m_nExpected = kGridCellCount;
    state.m_dwSubmitTick = ::GetTickCount();

    const DWORD dwBegin = ::GetTickCount();
    for (int i = 0; i < kGridCellCount; ++i)
    {
        DuiAsyncImage::Submit(FacePngPath(i), &OnAsyncImageLoaded,
                              MakeAsyncToken(state.m_nEpoch, 0, i));
    }
    const DWORD dwCost = ::GetTickCount() - dwBegin;

    if (state.m_pStatusLabel != NULL)
    {
        CString strText;
        strText.Format(Txt(_T("提交 %d 个请求只用了 %u 毫秒，结果会陆续")
                           _T("到达，其间界面照常响应。"),
                           _T("Submitting %d requests took %u ms; results ")
                           _T("arrive one by one and the window stays ")
                           _T("responsive.")),
                       kGridCellCount, (unsigned)dwCost);
        state.m_pStatusLabel->SetText(strText);
    }
}

// 连着提交一批请求给「比对请求号」与「不比对」两个单元格，制造出一串会被
// 后来者取代的中间结果。
//   state：页面状态。
void RunStaleResultBatch(AsyncPageState& state)
{
    state.m_nGuardedApplied = 0;
    state.m_nNaiveApplied = 0;
    //本批最后一个流水号就是「当前有效」的那个，比对时只认它。
    state.m_nGuardedLatestSerial = (unsigned)kStaleBatchCount;

    for (int i = 1; i <= kStaleBatchCount; ++i)
    {
        //每次换一张不同的图，才看得出中间结果确实被画了出来。
        CString strPath = FacePngPath(i * 2);
        DuiAsyncImage::Submit(strPath, &OnAsyncImageLoaded,
                              MakeAsyncToken(state.m_nEpoch, (unsigned)i,
                                             kSlotGuardedCell));
        DuiAsyncImage::Submit(strPath, &OnAsyncImageLoaded,
                              MakeAsyncToken(state.m_nEpoch, (unsigned)i,
                                             kSlotNaiveCell));
    }
    RefreshStaleCounters(state);
}

} // 匿名命名空间

std::unique_ptr<DuiControl> Build_AsyncImage()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    const CString strFirstFacePath = FacePngPath(0);
    const bool bHasFaces = AssetExists(strFirstFacePath);

    //—— 段落一：同步加载与异步加载的对照 ——
    AddSection(page.get(),
               Txt(_T("同步加载与异步加载的对照"),
                   _T("Synchronous versus asynchronous")),
               Txt(_T("界面线程上解码一张 PNG 只要几毫秒，几十上百张串起来")
                   _T("就是肉眼可见的卡顿。DuiAsyncImage 把解码挪到后台线程，")
                   _T("完成后通过一个只收消息的隐藏窗口把回调投回提交线程，")
                   _T("画廊有正常的消息循环，所以回调会自己到，不需要额外做")
                   _T("什么。下面两个按钮加载的是同一批图：按同步那个，窗口")
                   _T("在加载结束前完全不响应，拖不动滑块也滚不动页面；按")
                   _T("异步那个，提交瞬间就返回，界面照常能用。"),
                   _T("Decoding one PNG on the UI thread costs a few ")
                   _T("milliseconds; a hundred of them in a row is visible ")
                   _T("jank. DuiAsyncImage moves decoding to a worker thread ")
                   _T("and posts the callback back to the submitting thread ")
                   _T("through a hidden message-only window - the gallery ")
                   _T("runs a normal message loop, so callbacks arrive on ")
                   _T("their own. Both buttons below load the same batch: the ")
                   _T("synchronous one freezes the window until it finishes - ")
                   _T("the slider will not move and the page will not scroll - ")
                   _T("while the asynchronous one returns at once and leaves ")
                   _T("the window usable.")));

    //本行同时是整页状态的持有者，因此无论素材在不在都必须建出来。
    std::unique_ptr<AsyncDemoRow> demoRow(new AsyncDemoRow());
    demoRow->SetGap(kRowGap);
    AsyncPageState* pState = &demoRow->State();

    //按钮上的张数直接由网格的格子数算出来，改了网格文字也跟着对。
    CString strSyncText;
    strSyncText.Format(Txt(_T("同步加载 %d 张"), _T("Load %d sync")),
                       kGridCellCount);
    CString strAsyncText;
    strAsyncText.Format(Txt(_T("异步加载 %d 张"), _T("Load %d async")),
                        kGridCellCount);
    std::unique_ptr<FnButton> buttonSync = MakeButton(strSyncText);
    std::unique_ptr<FnButton> buttonAsync = MakeButton(strAsyncText);
    std::unique_ptr<FnButton> buttonClear = MakeButton(
        Txt(_T("清空"), _T("Clear")));
    if (bHasFaces)
    {
        buttonSync->onClick = [pState](FnButton*)
        {
            RunSynchronousBatch(*pState);
        };
        buttonAsync->onClick = [pState](FnButton*)
        {
            RunAsynchronousBatch(*pState);
        };
        buttonClear->onClick = [pState](FnButton*)
        {
            ClearResultGrid(*pState);
            if (pState->m_pStatusLabel != NULL)
            {
                pState->m_pStatusLabel->SetText(
                    Txt(_T("已清空。"), _T("Cleared.")));
            }
        };
    }
    else
    {
        buttonSync->SetEnabled(false);
        buttonAsync->SetEnabled(false);
        buttonClear->SetEnabled(false);
    }
    demoRow->AddChild(std::move(buttonSync),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));
    demoRow->AddChild(std::move(buttonAsync),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));
    demoRow->AddChild(std::move(buttonClear),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));

    std::unique_ptr<DuiLabel> statusLabel = MakeCaption(
        Txt(_T("点一个按钮开始。"), _T("Press a button to start.")));
    pState->m_pStatusLabel = statusLabel.get();
    demoRow->AddChild(std::move(statusLabel), DuiLayout::Hint().Weight(1));

    AddVariantRow(page.get(), std::move(demoRow), kButtonRowH);

    if (!bHasFaces)
    {
        AddNoticeRow(page.get(),
                     Txt(_T("找不到素材 Face\\face0.png，本页面的按钮已置灰。")
                         _T("把 Face 目录（face0.png 起的若干张 56 × 56 的图）")
                         _T("放到 DuiGallery.exe 所在目录下即可。"),
                         _T("Asset Face\\face0.png is missing, so the buttons ")
                         _T("on this page are disabled. Copy the Face folder ")
                         _T("(face0.png onwards, 56 x 56 each) next to ")
                         _T("DuiGallery.exe.")));
    }

    AddGap(page.get(), kInnerGap);
    {
        std::unique_ptr<DuiHBox> sliderRow(new DuiHBox());
        sliderRow->SetGap(kRowGap);
        sliderRow->AddChild(MakeCaption(Txt(_T("加载期间试着拖动这个滑块："),
                                            _T("Try dragging this while ")
                                            _T("loading:"))),
                            DuiLayout::Hint().Fixed(kCaptionColW));
        std::unique_ptr<DuiSlider> slider(new DuiSlider());
        slider->SetRange(0, 100);
        slider->SetPos(50, false);
        sliderRow->AddChild(std::move(slider),
                            DuiLayout::Hint().Weight(1)
                                             .Fixed(-1, kButtonH)
                                             .AlignC(DuiLayout::AlignCenter));
        AddVariantRow(page.get(), std::move(sliderRow), kButtonRowH);
    }

    //—— 段落二：结果逐张落位 ——
    AddSection(page.get(),
               Txt(_T("结果逐张落位"), _T("Results land one by one")),
               Txt(_T("后台只有一个解码线程，请求按提交顺序处理，因此上面那")
                   _T("一批的结果会一格一格填进下面的网格。每个请求带一个")
                   _T("自定义的请求号，回调原样带回来，本页面靠它知道这张图")
                   _T("该落到哪一格。结果里的位图所有权转移给回调，用完")
                   _T("必须自己 DeleteObject —— 本页面把它记在页面状态里，")
                   _T("换图或离开页面时统一释放。"),
                   _T("A single worker thread decodes in submission order, so ")
                   _T("the batch above fills the grid below one cell at a ")
                   _T("time. Each request carries a caller-defined token that ")
                   _T("comes back with the result, which is how this page ")
                   _T("knows where an image belongs. The bitmap in the result ")
                   _T("belongs to the callback once it arrives, so the caller ")
                   _T("has to delete it. This page keeps it in the page state ")
                   _T("and frees ")
                   _T("it when the cell is replaced or the page goes away.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        std::unique_ptr<DuiGrid> grid(new DuiGrid());
        grid->SetGrid(kGridRows, kGridCols);
        grid->SetGap(kGridGap);
        for (int i = 0; i < kGridCellCount; ++i)
        {
            std::unique_ptr<DuiAvatar> cell(new DuiAvatar());
            cell->SetShape(DuiAvatar::ShapeRoundRect);
            cell->SetCornerRadius(kCellCornerRadius);
            //没有图时画一个浅色的空位，网格的形状才看得出来。
            cell->SetFallbackBgColor(RGB(226, 229, 234));
            pState->m_cells[i] = cell.get();
            grid->AddChild(std::move(cell), DuiLayout::Hint()
                               .Fixed(kGridCellSide, kGridCellSide)
                               .AlignM(DuiLayout::AlignCenter)
                               .AlignC(DuiLayout::AlignCenter));
        }
        row->AddChild(std::move(grid),
                      DuiLayout::Hint().Fixed(kGridW).AlignM(DuiLayout::AlignNear));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kGridH);
    }

    //—— 段落三：用请求号丢弃过期结果 ——
    AddSection(page.get(),
               Txt(_T("用请求号丢弃过期结果"), _T("Dropping stale results")),
               Txt(_T("同一个控件短时间内换好几次图时，先发出去的请求可能在")
                   _T("后发的之后才回来，把新图盖成旧图。推荐的做法是自己记")
                   _T("一个「当前有效的请求号」，回调里比对，不是当前那个就")
                   _T("连同位图一起丢掉。按下面的按钮会连着提交 8 个请求：")
                   _T("左边比对请求号，只应用最后一个，中间结果全部丢弃；")
                   _T("右边不比对，来一个用一个，于是把每个中间结果都画了")
                   _T("一遍。两边的计数直观地说明差别。Cancel 不能替代这套")
                   _T("做法 —— 它只保证返回之后不再回调，已经开始解码的请求")
                   _T("照常走完。"),
                   _T("When one control swaps images several times in quick ")
                   _T("succession, an earlier request can come back after a ")
                   _T("later one and overwrite the newer image. The ")
                   _T("recommended pattern is to keep the id of the request ")
                   _T("that is still current and drop anything else, bitmap ")
                   _T("included. The button below fires eight requests: the ")
                   _T("left cell checks the id and applies only the last one, ")
                   _T("the right cell applies every result as it arrives and ")
                   _T("therefore paints every intermediate image. The two ")
                   _T("counters show the difference. Cancel is no substitute ")
                   _T("here - it only guarantees no callback after it ")
                   _T("returns, and a request already being decoded runs to ")
                   _T("completion.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiAvatar> guardedCell(new DuiAvatar());
        guardedCell->SetShape(DuiAvatar::ShapeRoundRect);
        guardedCell->SetCornerRadius(kCellCornerRadius);
        guardedCell->SetFallbackBgColor(RGB(226, 229, 234));
        pState->m_pGuardedCell = guardedCell.get();
        row->AddChild(std::move(guardedCell),
                      DuiLayout::Hint().Fixed(kStaleCellSide, kStaleCellSide)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<DuiAvatar> naiveCell(new DuiAvatar());
        naiveCell->SetShape(DuiAvatar::ShapeRoundRect);
        naiveCell->SetCornerRadius(kCellCornerRadius);
        naiveCell->SetFallbackBgColor(RGB(226, 229, 234));
        pState->m_pNaiveCell = naiveCell.get();
        row->AddChild(std::move(naiveCell),
                      DuiLayout::Hint().Fixed(kStaleCellSide, kStaleCellSide)
                                       .AlignC(DuiLayout::AlignCenter));

        CString strStaleText;
        strStaleText.Format(Txt(_T("连着换 %d 次图"), _T("Swap %d times")),
                            kStaleBatchCount);
        std::unique_ptr<FnButton> buttonStale = MakeButton(strStaleText);
        if (bHasFaces)
        {
            buttonStale->onClick = [pState](FnButton*)
            {
                RunStaleResultBatch(*pState);
            };
        }
        else
        {
            buttonStale->SetEnabled(false);
        }
        row->AddChild(std::move(buttonStale),
                      DuiLayout::Hint().Fixed(kButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kStaleCellSide + kInnerGap);

        std::unique_ptr<DuiHBox> captionRow(new DuiHBox());
        captionRow->SetGap(kRowGap);
        std::unique_ptr<DuiLabel> guardedLabel = MakeCaption(_T(""));
        pState->m_pGuardedLabel = guardedLabel.get();
        captionRow->AddChild(std::move(guardedLabel),
                             DuiLayout::Hint().Fixed(kCaptionColW));
        std::unique_ptr<DuiLabel> naiveLabel = MakeCaption(_T(""));
        pState->m_pNaiveLabel = naiveLabel.get();
        captionRow->AddChild(std::move(naiveLabel),
                             DuiLayout::Hint().Fixed(kCaptionColW));
        captionRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                             DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(captionRow), kCaptionRowH);

        RefreshStaleCounters(*pState);
    }

    //—— 段落四：失败与取消 ——
    AddSection(page.get(),
               Txt(_T("失败与取消"), _T("Failures and cancelling")),
               Txt(_T("路径不存在、格式不认识、内存不足时，回调照样会来一次，")
                   _T("只是 ok 为假、位图为空 —— 调用方不需要为失败单独设一条")
                   _T("路径。取消则只是「尽力而为」：Cancel 保证返回之后不再")
                   _T("回调，但已经被后台线程取走开始解码的请求仍会回调一次，")
                   _T("所以真正可靠的做法是上一段那种比对请求号的写法。"),
                   _T("A missing path, an unknown format or an out-of-memory ")
                   _T("condition still produces exactly one callback, with ok ")
                   _T("false and a null bitmap - callers need no separate ")
                   _T("failure path. Cancelling is best effort: Cancel ")
                   _T("guarantees no callback after it returns, but a request ")
                   _T("the worker has already picked up still calls back once, ")
                   _T("which is why checking the request id, as in the ")
                   _T("previous section, is the reliable pattern.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> buttonFail = MakeButton(
            Txt(_T("加载不存在的文件"), _T("Load a missing file")));
        buttonFail->onClick = [pState](FnButton*)
        {
            //刻意指向一个不会存在的文件名。
            CString strPath = AssetPath(_T("Face\\__no_such_face__.png"));
            DuiAsyncImage::Submit(strPath, &OnAsyncImageLoaded,
                                  MakeAsyncToken(pState->m_nEpoch, 0,
                                                 kSlotFailure));
            if (pState->m_pOutcomeLabel != NULL)
            {
                pState->m_pOutcomeLabel->SetText(
                    Txt(_T("已提交一个必然失败的请求，等回调。"),
                        _T("Submitted a request that is bound to fail; ")
                        _T("waiting for the callback.")));
            }
        };
        row->AddChild(std::move(buttonFail),
                      DuiLayout::Hint().Fixed(kWideButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));

        std::unique_ptr<FnButton> buttonCancel = MakeButton(
            Txt(_T("提交后立即取消"), _T("Submit then cancel")));
        buttonCancel->onClick = [pState](FnButton*)
        {
            CString strPath = FacePngPath(1);
            DWORD_PTR requestId = DuiAsyncImage::Submit(
                strPath, &OnAsyncImageLoaded,
                MakeAsyncToken(pState->m_nEpoch, 0, kSlotCancelled));
            DuiAsyncImage::Cancel(requestId);
            if (pState->m_pOutcomeLabel != NULL)
            {
                CString strText;
                strText.Format(Txt(_T("已提交请求 %u 并立即取消。若这行文字")
                                   _T("不再变化，说明回调确实被拦下了。"),
                                   _T("Submitted request %u and cancelled it ")
                                   _T("at once. If this line stops changing, ")
                                   _T("the callback really was suppressed.")),
                               (unsigned)requestId);
                pState->m_pOutcomeLabel->SetText(strText);
            }
        };
        row->AddChild(std::move(buttonCancel),
                      DuiLayout::Hint().Fixed(kWideButtonW, kButtonH)
                                       .AlignC(DuiLayout::AlignCenter));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kButtonRowH);

        std::unique_ptr<DuiLabel> outcomeLabel = MakeLabel(
            Txt(_T("按一个按钮看结果。"), _T("Press a button to see what ")
                _T("happens.")),
            kCaptionColor, DT_LEFT | DT_TOP);
        outcomeLabel->SetWordWrap(true);
        pState->m_pOutcomeLabel = outcomeLabel.get();

        std::unique_ptr<DuiHBox> outcomeRow(new DuiHBox());
        outcomeRow->AddChild(std::move(outcomeLabel),
                             DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(outcomeRow), kNoticeRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// =====================================================================
// 本分组的页面列表
// =====================================================================

const PageEntry* GetMediaPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("gif"),         _T("DuiGif　动图"),                _T("DuiGif"),        &Build_Gif,        true },
        { _T("image-ole"),   _T("DuiImageOle　富文本内嵌图片"), _T("DuiImageOle"),   &Build_ImageOle,   true },
        { _T("async-image"), _T("DuiAsyncImage　异步图片加载"), _T("DuiAsyncImage"), &Build_AsyncImage, true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
