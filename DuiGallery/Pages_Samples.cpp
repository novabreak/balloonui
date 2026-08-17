/**
 *  画廊「完整示例」分组的两个演示页面。
 *
 *  这一组不再逐个演示单个控件的用法，而是把若干控件拼成一个能看出业务形态的
 *  完整界面，用来回答「真要做一个界面，balloonui 够不够用、大致长什么样」。
 *  包含两页：
 *
 *    · xml-login    —— 同一个登录框的两种写法。左侧是交给 DuiXmlBuilder 的
 *                      XML 原文，右侧是解析出来的真实控件树。
 *    · full-layouts —— 五个完整界面（登录框、表单、三列主窗、设置页、窗口
 *                      骨架），全部用现成控件组合而成，不含自绘控件。
 *
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "DuiXmlBuilder.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Input/DuiEdit.h"
#include "Controls/Input/DuiSearchBox.h"
#include "Controls/Input/DuiComboBox.h"
#include "Controls/List/DuiListBox.h"
#include "Controls/Layout/DuiDock.h"

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 配色
// =====================================================================

// 示例界面里主标题的文字色。
const COLORREF kTitleTextColor = RGB(20, 20, 20);
// 次要说明文字的文字色，比主标题浅一档。
const COLORREF kSubTextColor = RGB(120, 120, 120);
// 表单里字段名的文字色。
const COLORREF kFieldLabelColor = RGB(60, 60, 60);
// 设置项左侧说明文字的文字色。
const COLORREF kOptionLabelColor = RGB(40, 40, 40);
// 版本号一类脚注文字的文字色，是本页面里最浅的一档。
const COLORREF kFootnoteColor = RGB(170, 170, 170);
// 品牌方块的底色。
const COLORREF kBrandTileColor = RGB(50, 160, 110);
// 反白文字的颜色，用于品牌方块与状态栏这类深色底。
const COLORREF kInverseTextColor = RGB(255, 255, 255);
// 三列主窗左侧导航条上图标占位方块的底色。
const COLORREF kRailIconColor = RGB(60, 65, 75);
// 工具栏、会话标题栏这类浅灰底。
const COLORREF kChromeTileColor = RGB(245, 245, 248);
// 主内容区的底色，接近纯白。
const COLORREF kContentTileColor = RGB(252, 252, 252);
// 输入区的底色，比主内容区略深一点以示区分。
const COLORREF kComposerTileColor = RGB(248, 248, 250);
// 状态栏的底色。
const COLORREF kStatusBarColor = RGB(40, 110, 200);
// 色块占位控件默认的文字色。
const COLORREF kTileDefaultTextColor = RGB(40, 40, 40);
// XML 演示页里左右两栏小标题的文字色。
const COLORREF kPaneHeaderColor = RGB(40, 40, 40);
// XML 解析失败时那条提示文字的颜色，用红色以便一眼看出是错误。
const COLORREF kParseErrorColor = RGB(220, 60, 60);

// =====================================================================
// 版式常量
// =====================================================================

// ---- XML 演示页 ----

// 左右两栏各自小标题的行高（像素）。
const int kXmlPaneHeaderH = 20;
// 小标题与其下方内容之间的间距（像素）。
const int kXmlPaneGap = 4;
// 左右两栏之间的间距（像素）。
const int kXmlColumnGap = 16;
// 整个「源码 + 实控件」演示行的高度（像素）。取 320 是为了让右侧建出来的
// 登录框完整可见，同时左侧的源码框还能显示十几行。
const int kXmlDemoRowH = 320;

// ---- 示例一：登录对话框 ----

// 登录卡片的四边内边距（像素）。
const int kLoginCardPadding = 20;
// 登录卡片内各元素之间的竖直间距（像素）。
const int kLoginCardGap = 10;
// 品牌方块的高度（像素）。
const int kLoginLogoH = 48;
// 产品名一行的高度（像素）。
const int kLoginTitleH = 28;
// 副标题一行的高度（像素）。
const int kLoginSubTitleH = 20;
// 账号、密码两个输入框的高度（像素），两处共用。
const int kLoginFieldH = 32;
// 「记住我 + 忘记密码」一行的高度（像素）。
const int kLoginOptionRowH = 24;
// 「忘记密码」链接的固定宽度（像素）。
const int kLoginLinkW = 80;
// 登录按钮的高度（像素）。
const int kLoginButtonH = 36;
// 底部版本号一行的高度（像素）。
const int kLoginVersionH = 18;
// 登录卡片的固定宽度（像素）。它由外层容器左右两侧的弹性空白夹住而居中。
const int kLoginCardW = 320;
// 登录对话框这一行演示的高度（像素）。
const int kLoginRowH = 360;

// ---- 示例二：表单 ----

// 表单容器的四边内边距（像素）。
const int kFormPadding = 20;
// 表单内各行之间的竖直间距（像素）。
const int kFormGap = 12;
// 表单标题一行的高度（像素）。
const int kFormTitleH = 28;
// 字段名列的固定宽度（像素）。四行共用同一个值，字段名与输入框才能对齐。
const int kFormLabelW = 80;
// 字段名与输入框之间的水平间距（像素）。
const int kFormLabelGap = 8;
// 每个「字段名 + 输入框」行的高度（像素）。
const int kFormRowH = 28;
// 底部按钮的固定宽度（像素）。
const int kFormButtonW = 80;
// 底部按钮之间的水平间距（像素）。
const int kFormButtonGap = 8;
// 底部按钮组一行的高度（像素）。
const int kFormButtonBarH = 32;
// 整个表单的固定宽度（像素）。
const int kFormW = 480;
// 表单这一行演示的高度（像素）。
const int kFormRowHeight = 280;

// ---- 示例三：三列主窗 ----

// 左侧图标导航条的固定宽度（像素）。
const int kThreePaneRailW = 60;
// 导航条的四边内边距（像素）。
const int kThreePaneRailPadding = 8;
// 导航条内各元素之间的竖直间距（像素）。
const int kThreePaneRailGap = 8;
// 导航条顶部头像的高度（像素）。
const int kThreePaneAvatarH = 40;
// 导航条上每个图标占位方块的高度（像素）。
const int kThreePaneIconH = 36;
// 导航条上图标占位方块的个数。真实业务里这里会是若干个 DuiButton。
const int kThreePaneIconCount = 4;
// 中间列表列的固定宽度（像素）。
const int kThreePaneMidW = 240;
// 中间列顶部搜索框的高度（像素）。
const int kThreePaneSearchH = 36;
// 中间列表每一项的高度（像素）。
const int kThreePaneItemH = 48;
// 右侧内容列顶部标题栏的高度（像素）。
const int kThreePaneHeaderH = 48;
// 右侧内容列底部输入区的高度（像素）。
const int kThreePaneComposerH = 80;
// 整个三列主窗的固定宽度（像素）。
const int kThreePaneW = 720;
// 三列主窗这一行演示的高度（像素）。
const int kThreePaneRowHeight = 320;

// ---- 示例四：设置页 ----

// 左侧分类导航的固定宽度（像素）。
const int kSettingsNavW = 160;
// 左侧分类导航每一项的高度（像素）。
const int kSettingsNavItemH = 32;
// 右侧内容区的四边内边距（像素）。
const int kSettingsPadding = 20;
// 右侧内容区各行之间的竖直间距（像素）。
const int kSettingsGap = 8;
// 右侧内容区标题一行的高度（像素）。
const int kSettingsTitleH = 28;
// 分组小标题一行的高度（像素）。
const int kSettingsGroupTitleH = 20;
// 每个设置项一行的高度（像素）。
const int kSettingsOptionRowH = 28;
// 设置项右侧控件列的固定宽度（像素）。三项共用同一个值，控件的左边缘才能对齐。
const int kSettingsControlW = 120;
// 整个设置页的固定宽度（像素）。
const int kSettingsW = 640;
// 设置页这一行演示的高度（像素）。
const int kSettingsRowHeight = 280;

// ---- 示例五：窗口骨架 ----

// 顶部工具栏的高度（像素）。
const int kSkeletonToolbarH = 32;
// 底部状态栏的高度（像素）。
const int kSkeletonStatusBarH = 24;
// 左侧导航栏的宽度（像素）。
const int kSkeletonNavW = 140;
// 整个窗口骨架的固定宽度（像素）。
const int kSkeletonW = 640;
// 窗口骨架这一行演示的高度（像素）。
const int kSkeletonRowHeight = 220;

// =====================================================================
// 色块占位控件
// =====================================================================

// 画一块纯色矩形并在中央写一行字的最小控件。
//
// 完整界面示例里有几个区域（工具栏、状态栏、消息内容区、输入区）在本页面
// 只需要标示出位置与范围，不需要具备真实功能。DuiLabel 没有设置底色的接口，
// 而这些区域必须靠底色互相区分，所以这里做一个最小的自绘子类。它只服务于
// 本文件的示例界面，不属于控件库的公开能力，业务代码不要复制这份实现。
class SampleColorTile : public DuiControl
{
public:
    // 构造一个色块。
    //   text：写在方块中央的文字，传空指针等同于空串。文字内容由调用方按
    //         当前语言取好，本类不做任何语言处理。
    //   bgColor：方块的底色。
    //   textColor：文字颜色，默认取一个适合浅色底的深灰。
    SampleColorTile(LPCTSTR text, COLORREF bgColor, COLORREF textColor = kTileDefaultTextColor)
        : m_text(text != NULL ? text : _T(""))
        , m_bgColor(bgColor)
        , m_textColor(textColor)
    {
    }

    // 绘制：先用底色填满自己的矩形，再在正中间写一行字。
    //   hdc：目标设备上下文。
    //   第二个参数是本次的重绘区域，本控件整块重画，因此不使用它。
    void OnPaint(HDC hdc, const RECT&) override
    {
        RECT rc = GetRect();

        HBRUSH brush = ::CreateSolidBrush(m_bgColor);
        ::FillRect(hdc, &rc, brush);
        ::DeleteObject(brush);

        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_textColor);
        ::DrawText(hdc, m_text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

private:
    // 方块中央显示的文字，构造时确定，之后不再变化。
    CString m_text;
    // 方块的底色，构造时确定。
    COLORREF m_bgColor;
    // 方块中央文字的颜色，构造时确定。
    COLORREF m_textColor;
};

// =====================================================================
// 登录框的 XML 描述
// =====================================================================
//
// 这一页要演示的是 DuiXmlBuilder::FromString 的窄字符重载 —— 它的参数类型是
// LPCSTR，内容按 UTF-8 解析（见 DuiXmlBuilder.h 的声明）。而画廊的语言切换
// 函数 Txt() 两个参数都是 LPCTSTR，在本工程的 Unicode 配置下即 const wchar_t*，
// 两者类型不同，无法把 XML 里的界面文字抽出来交给 Txt() 处理。因此这里准备了
// 中英文两份完整的 XML 常量，由 PickLoginXml() 按当前语言选出一份。
//
// 这两个常量之所以可以直接交给窄字符重载：本文件保存为不带 BOM 的 UTF-8，
// 工程又给编译器传了 /utf-8（见 DuiGallery.vcxproj 的 AdditionalOptions），
// 该选项同时指定源文件编码与执行字符集，于是下面这些窄字符串字面量在运行期
// 就是 UTF-8 字节，正好符合该重载的要求。
//
// XML 里的 id 取 3001 起的一段，只是为了让读者看到 id 属性长什么样；本页面
// 不监听这些控件的通知，数值本身没有别的含义。
//
// 与真实登录窗的差异（有意为之，属于本示例的取舍）：
//   · 头像与在线状态是占位的 label 与 button。XML 构建器目前还没有对应的
//     标签，本示例关心的是这两块地方在布局里占多大。
//   · 「注册账号」「找回密码」是普通 label 而不是超链接，同样是因为构建器
//     还没有 hyperlink 标签。

// 登录框 XML 的中文版。
const char kLoginXmlZh[] =
"<vbox padding=\"20\" gap=\"10\">"
"  <hbox gap=\"14\" fixedHeight=\"80\">"
"    <vbox fixedWidth=\"72\" gap=\"6\">"
"      <label fixedHeight=\"58\" text=\"(头像)\" textColor=\"60,60,60\"/>"
"      <button buttonType=\"icon\" fixedHeight=\"18\" text=\"在线\"/>"
"    </vbox>"
"    <vbox weight=\"1\" gap=\"6\">"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"54\" text=\"账号\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3001\" weight=\"1\" placeholder=\"账号 / 邮箱\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"54\" text=\"密码\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3002\" weight=\"1\" password=\"true\" placeholder=\"密码\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"22\" gap=\"12\">"
"        <button id=\"3003\" buttonType=\"checkbox\" fixedWidth=\"90\" text=\"记住密码\"/>"
"        <button id=\"3004\" buttonType=\"checkbox\" fixedWidth=\"90\" text=\"自动登录\"/>"
"        <label  fixedWidth=\"60\" text=\"注册账号\" textColor=\"45,108,223\"/>"
"        <label  fixedWidth=\"60\" text=\"找回密码\" textColor=\"45,108,223\"/>"
"      </hbox>"
"    </vbox>"
"  </hbox>"
"  <hbox fixedHeight=\"34\" gap=\"10\">"
"    <button id=\"3005\" text=\"设置\"  fixedWidth=\"80\"/>"
"    <label weight=\"1\" text=\"\"/>"
"    <button id=\"3006\" text=\"登录\"  fixedWidth=\"110\"/>"
"  </hbox>"
"</vbox>";

// 登录框 XML 的英文版。结构与中文版完全一致，只有文字与几处为了容纳更长的
// 英文单词而加宽的 fixedWidth 不同。
const char kLoginXmlEn[] =
"<vbox padding=\"20\" gap=\"10\">"
"  <hbox gap=\"14\" fixedHeight=\"80\">"
"    <vbox fixedWidth=\"72\" gap=\"6\">"
"      <label fixedHeight=\"58\" text=\"(avatar)\" textColor=\"60,60,60\"/>"
"      <button buttonType=\"icon\" fixedHeight=\"18\" text=\"online\"/>"
"    </vbox>"
"    <vbox weight=\"1\" gap=\"6\">"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"72\" text=\"Account\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3001\" weight=\"1\" placeholder=\"account / email\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"72\" text=\"Password\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3002\" weight=\"1\" password=\"true\" placeholder=\"password\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"22\" gap=\"12\">"
"        <button id=\"3003\" buttonType=\"checkbox\" fixedWidth=\"140\" text=\"Remember password\"/>"
"        <button id=\"3004\" buttonType=\"checkbox\" fixedWidth=\"110\" text=\"Auto sign-in\"/>"
"        <label  fixedWidth=\"64\" text=\"Sign up\" textColor=\"45,108,223\"/>"
"        <label  fixedWidth=\"96\" text=\"Reset password\" textColor=\"45,108,223\"/>"
"      </hbox>"
"    </vbox>"
"  </hbox>"
"  <hbox fixedHeight=\"34\" gap=\"10\">"
"    <button id=\"3005\" text=\"Settings\"  fixedWidth=\"80\"/>"
"    <label weight=\"1\" text=\"\"/>"
"    <button id=\"3006\" text=\"Sign in\"  fixedWidth=\"110\"/>"
"  </hbox>"
"</vbox>";

// 按当前语言选出登录框 XML 的一份。
// 返回：静态存储期的 UTF-8 字节串，调用方只读、不得释放。
const char* PickLoginXml()
{
    if (CurrentLanguage() == LangEnglish)
    {
        return kLoginXmlEn;
    }
    return kLoginXmlZh;
}

// 把 UTF-8 字节串转成本工程使用的宽字符 CString。
//   utf8：以 0 结尾的 UTF-8 字节串，允许为空指针。
// 返回：转换结果；入参为空指针或转换失败时返回空串。
//
// 不能直接写 CString(kLoginXmlZh)：本工程是 Unicode 配置，CString 的 LPCSTR
// 构造函数按系统 ANSI 代码页解码，而这里的字节是 UTF-8，中文会被解成乱码。
// 所以显式指定 CP_UTF8 做一次转换。
CString Utf8ToCString(const char* utf8)
{
    if (utf8 == NULL)
    {
        return CString();
    }

    int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wideLen <= 0)
    {
        return CString();
    }

    std::vector<wchar_t> buffer((size_t)wideLen);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &buffer[0], wideLen);
    return CString(&buffer[0]);
}

// =====================================================================
// 五个完整界面各自的构建函数
// =====================================================================

// 构建示例一：登录对话框的卡片本体。
// 返回：卡片根控件，所有权交给调用方。
std::unique_ptr<DuiControl> BuildLayoutLogin()
{
    std::unique_ptr<DuiVBox> card(new DuiVBox());
    card->SetPadding(kLoginCardPadding);
    card->SetGap(kLoginCardGap);

    // 品牌方块。真实产品这里会放一张图标位图，本示例用色块代替。
    std::unique_ptr<SampleColorTile> logo(
        new SampleColorTile(_T("F"), kBrandTileColor, kInverseTextColor));
    card->AddChild(std::move(logo), DuiLayout::Hint().Fixed(kLoginLogoH));

    // 产品名。它是示例产品的名字，两种语言下是同一个词，因此不经由 Txt 取文案。
    std::unique_ptr<DuiLabel> title(new DuiLabel());
    title->SetText(_T("FlamingoNewUI"));
    title->SetTextColor(kTitleTextColor);
    title->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(title), DuiLayout::Hint().Fixed(kLoginTitleH));

    std::unique_ptr<DuiLabel> subTitle(new DuiLabel());
    subTitle->SetText(Txt(_T("登录账号"), _T("Sign in to your account")));
    subTitle->SetTextColor(kSubTextColor);
    subTitle->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(subTitle), DuiLayout::Hint().Fixed(kLoginSubTitleH));

    std::unique_ptr<DuiEdit> userEdit(new DuiEdit());
    userEdit->SetPlaceholder(Txt(_T("用户名 / 邮箱"), _T("Username / email")));
    card->AddChild(std::move(userEdit), DuiLayout::Hint().Fixed(kLoginFieldH));

    std::unique_ptr<DuiEdit> passwordEdit(new DuiEdit());
    passwordEdit->SetPlaceholder(Txt(_T("密码"), _T("Password")));
    passwordEdit->SetPassword(true);
    card->AddChild(std::move(passwordEdit), DuiLayout::Hint().Fixed(kLoginFieldH));

    // 「记住我」占弹性宽度、「忘记密码」占固定宽度，于是复选框靠左、链接靠右。
    std::unique_ptr<DuiHBox> options(new DuiHBox());

    std::unique_ptr<DuiButton> remember(new DuiButton());
    remember->SetButtonType(DuiButton::StyleCheckbox);
    remember->SetText(Txt(_T("记住我"), _T("Remember me")));

    std::unique_ptr<DuiLabel> forgot(new DuiLabel());
    forgot->SetText(Txt(_T("忘记密码？"), _T("Forgot password?")));
    forgot->SetMode(DuiLabel::ModeLink);
    forgot->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    options->AddChild(std::move(remember), DuiLayout::Hint().Weight(1));
    options->AddChild(std::move(forgot), DuiLayout::Hint().Fixed(kLoginLinkW));
    card->AddChild(std::move(options), DuiLayout::Hint().Fixed(kLoginOptionRowH));

    std::unique_ptr<DuiButton> loginButton(new DuiButton());
    loginButton->SetText(Txt(_T("登录"), _T("Sign in")));
    card->AddChild(std::move(loginButton), DuiLayout::Hint().Fixed(kLoginButtonH));

    // 版本号。数字与产品名两种语言下相同，无需翻译。
    std::unique_ptr<DuiLabel> version(new DuiLabel());
    version->SetText(_T("v1.0   © 2026 FlamingoNewUI"));
    version->SetTextColor(kFootnoteColor);
    version->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(version), DuiLayout::Hint().Fixed(kLoginVersionH));

    return std::unique_ptr<DuiControl>(card.release());
}

// 构建表单里的一行：右对齐的字段名加一个占据剩余宽度的输入框。
//   labelText：字段名，调用方按当前语言取好。
//   placeholder：输入框的占位文字；传空指针表示这一行不需要占位文字。
// 返回：这一行的水平容器，所有权交给调用方。
std::unique_ptr<DuiHBox> MakeFormRow(LPCTSTR labelText, LPCTSTR placeholder)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());
    row->SetGap(kFormLabelGap);

    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(labelText);
    label->SetTextColor(kFieldLabelColor);
    label->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    std::unique_ptr<DuiEdit> edit(new DuiEdit());
    if (placeholder != NULL)
    {
        edit->SetPlaceholder(placeholder);
    }

    row->AddChild(std::move(label), DuiLayout::Hint().Fixed(kFormLabelW));
    row->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
    return row;
}

// 构建示例二：资料表单。
// 返回：表单根控件，所有权交给调用方。
std::unique_ptr<DuiControl> BuildLayoutForm()
{
    std::unique_ptr<DuiVBox> col(new DuiVBox());
    col->SetPadding(kFormPadding);
    col->SetGap(kFormGap);

    std::unique_ptr<DuiLabel> title(new DuiLabel());
    title->SetText(Txt(_T("个人资料"), _T("Profile")));
    title->SetTextColor(kTitleTextColor);
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(kFormTitleH));

    col->AddChild(MakeFormRow(Txt(_T("姓名"), _T("Name")),
                              Txt(_T("请输入姓名"), _T("Enter your name"))),
                  DuiLayout::Hint().Fixed(kFormRowH));
    col->AddChild(MakeFormRow(Txt(_T("昵称"), _T("Display name")),
                              Txt(_T("可选"), _T("Optional"))),
                  DuiLayout::Hint().Fixed(kFormRowH));
    // 邮箱示例值本身与语言无关，占位文字直接写字面量。
    col->AddChild(MakeFormRow(Txt(_T("邮箱"), _T("Email")),
                              _T("name@example.com")),
                  DuiLayout::Hint().Fixed(kFormRowH));
    // 最后一行刻意不设占位文字，用来对照有占位文字与没有占位文字时输入框的外观。
    col->AddChild(MakeFormRow(Txt(_T("电话"), _T("Phone")), NULL),
                  DuiLayout::Hint().Fixed(kFormRowH));

    // 一个只占空间、不绘制任何内容的弹性子控件，把下面的按钮组推到容器底部。
    col->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                  DuiLayout::Hint().Weight(1));

    std::unique_ptr<DuiHBox> buttons(new DuiHBox());
    buttons->SetGap(kFormButtonGap);
    // 按钮组内部同样先放一个弹性空白，两个按钮因此靠右排列。
    buttons->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));

    // 取消按钮取图标样式，效果是浅底无边框，与主操作按钮形成主次区别。
    std::unique_ptr<DuiButton> cancelButton(new DuiButton());
    cancelButton->SetButtonType(DuiButton::StyleIcon);
    cancelButton->SetText(Txt(_T("取消"), _T("Cancel")));

    std::unique_ptr<DuiButton> saveButton(new DuiButton());
    saveButton->SetText(Txt(_T("保存"), _T("Save")));

    buttons->AddChild(std::move(cancelButton), DuiLayout::Hint().Fixed(kFormButtonW));
    buttons->AddChild(std::move(saveButton), DuiLayout::Hint().Fixed(kFormButtonW));
    col->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(kFormButtonBarH));

    return std::unique_ptr<DuiControl>(col.release());
}

// 构建示例三：三列主窗。
// 返回：主窗根控件，所有权交给调用方。
//
// 这里用 DuiHBox 而不是 DuiSplitter，因为本示例只说明三列的宽度关系；需要让
// 用户拖动分栏时把外层换成 DuiSplitter 即可，子控件不必改动。
std::unique_ptr<DuiControl> BuildLayoutThreePane()
{
    std::unique_ptr<DuiHBox> root(new DuiHBox());

    // 第一列：窄条形的图标导航。
    std::unique_ptr<DuiVBox> rail(new DuiVBox());
    rail->SetPadding(kThreePaneRailPadding);
    rail->SetGap(kThreePaneRailGap);

    std::unique_ptr<DuiAvatar> avatar(new DuiAvatar());
    avatar->SetName(Txt(_T("陆"), _T("Lucas")));
    avatar->SetFallbackBgColor(kBrandTileColor);
    rail->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(kThreePaneAvatarH));

    for (int i = 0; i < kThreePaneIconCount; ++i)
    {
        // 图标占位方块。真实业务这里会是 DuiButton(StyleIcon) 或自绘图标按钮。
        std::unique_ptr<SampleColorTile> tile(
            new SampleColorTile(_T(""), kRailIconColor));
        rail->AddChild(std::move(tile), DuiLayout::Hint().Fixed(kThreePaneIconH));
    }
    root->AddChild(std::move(rail), DuiLayout::Hint().Fixed(kThreePaneRailW));

    // 第二列：搜索框加会话列表。
    std::unique_ptr<DuiVBox> mid(new DuiVBox());

    std::unique_ptr<DuiSearchBox> search(new DuiSearchBox());
    search->SetPlaceholder(Txt(_T("搜索"), _T("Search")));
    mid->AddChild(std::move(search), DuiLayout::Hint().Fixed(kThreePaneSearchH));

    std::unique_ptr<DuiListBox> list(new DuiListBox());
    list->SetItemHeight(kThreePaneItemH);
    list->AddItem(Txt(_T("苏文嘉"), _T("Alice")));
    list->AddItem(Txt(_T("# 设计周会"), _T("# Design sync")));
    list->AddItem(Txt(_T("李静"), _T("Grace")));
    list->AddItem(Txt(_T("F 前端基础设施"), _T("F Frontend platform")));
    list->AddItem(Txt(_T("陈聪"), _T("Ethan")));
    list->SetCurSel(0, false);
    mid->AddChild(std::move(list), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(mid), DuiLayout::Hint().Fixed(kThreePaneMidW));

    // 第三列：标题栏、内容区、输入区自上而下排列，中间那块占据剩余高度。
    std::unique_ptr<DuiVBox> right(new DuiVBox());

    std::unique_ptr<SampleColorTile> header(
        new SampleColorTile(Txt(_T(" 苏文嘉   ·   在线"), _T(" Alice   ·   Online")),
                            kChromeTileColor));
    right->AddChild(std::move(header), DuiLayout::Hint().Fixed(kThreePaneHeaderH));

    std::unique_ptr<SampleColorTile> messageArea(
        new SampleColorTile(Txt(_T("(消息内容区)"), _T("(message area)")),
                            kContentTileColor));
    right->AddChild(std::move(messageArea), DuiLayout::Hint().Weight(1));

    std::unique_ptr<SampleColorTile> composer(
        new SampleColorTile(Txt(_T("(输入区)"), _T("(composer)")),
                            kComposerTileColor));
    right->AddChild(std::move(composer), DuiLayout::Hint().Fixed(kThreePaneComposerH));

    root->AddChild(std::move(right), DuiLayout::Hint().Weight(1));

    return std::unique_ptr<DuiControl>(root.release());
}

// 构建设置页里的一行：左侧说明文字占弹性宽度，右侧控件占固定宽度。
//   caption：设置项的说明文字，调用方按当前语言取好。
//   control：右侧的控件（复选框、下拉框等），所有权转移给本行。
// 返回：这一行的水平容器，所有权交给调用方。
std::unique_ptr<DuiHBox> MakeSettingOption(LPCTSTR caption,
                                           std::unique_ptr<DuiControl> control)
{
    std::unique_ptr<DuiHBox> row(new DuiHBox());

    std::unique_ptr<DuiLabel> label(new DuiLabel());
    label->SetText(caption);
    label->SetTextColor(kOptionLabelColor);

    row->AddChild(std::move(label), DuiLayout::Hint().Weight(1));
    row->AddChild(std::move(control), DuiLayout::Hint().Fixed(kSettingsControlW));
    return row;
}

// 构建示例四：设置页。
// 返回：设置页根控件，所有权交给调用方。
std::unique_ptr<DuiControl> BuildLayoutSettings()
{
    std::unique_ptr<DuiHBox> root(new DuiHBox());

    // 左侧分类导航。这里直接用 DuiListBox 充当竖排的分类栏，不需要另外实现控件。
    std::unique_ptr<DuiListBox> nav(new DuiListBox());
    nav->SetItemHeight(kSettingsNavItemH);
    nav->AddItem(Txt(_T("通用"), _T("General")));
    nav->AddItem(Txt(_T("账号与安全"), _T("Account & security")));
    nav->AddItem(Txt(_T("消息与通知"), _T("Messages & notifications")));
    nav->AddItem(Txt(_T("隐私"), _T("Privacy")));
    nav->AddItem(Txt(_T("聊天偏好"), _T("Chat preferences")));
    nav->AddItem(Txt(_T("音频与视频"), _T("Audio & video")));
    nav->AddItem(Txt(_T("文件与存储"), _T("Files & storage")));
    nav->AddItem(Txt(_T("外观"), _T("Appearance")));
    nav->SetCurSel(0, false);
    root->AddChild(std::move(nav), DuiLayout::Hint().Fixed(kSettingsNavW));

    // 右侧内容区：标题、分组小标题、若干设置项。
    std::unique_ptr<DuiVBox> content(new DuiVBox());
    content->SetPadding(kSettingsPadding);
    content->SetGap(kSettingsGap);

    std::unique_ptr<DuiLabel> title(new DuiLabel());
    title->SetText(Txt(_T("通用"), _T("General")));
    title->SetTextColor(kTitleTextColor);
    content->AddChild(std::move(title), DuiLayout::Hint().Fixed(kSettingsTitleH));

    std::unique_ptr<DuiLabel> groupTitle(new DuiLabel());
    groupTitle->SetText(Txt(_T("启动行为"), _T("Startup behavior")));
    groupTitle->SetTextColor(kSubTextColor);
    content->AddChild(std::move(groupTitle), DuiLayout::Hint().Fixed(kSettingsGroupTitleH));

    // 第一项默认为选中状态，第二项默认为未选中状态，用来对照复选框的两种外观。
    std::unique_ptr<DuiButton> autoStart(new DuiButton());
    autoStart->SetButtonType(DuiButton::StyleCheckbox);
    autoStart->SetText(Txt(_T("启用"), _T("Enabled")));
    autoStart->SetCheck(true, false);
    content->AddChild(MakeSettingOption(Txt(_T("开机自动启动"), _T("Start with the system")),
                                        std::move(autoStart)),
                      DuiLayout::Hint().Fixed(kSettingsOptionRowH));

    std::unique_ptr<DuiButton> silentStart(new DuiButton());
    silentStart->SetButtonType(DuiButton::StyleCheckbox);
    silentStart->SetText(Txt(_T("启用"), _T("Enabled")));
    content->AddChild(MakeSettingOption(Txt(_T("启动时静默运行"), _T("Start minimized")),
                                        std::move(silentStart)),
                      DuiLayout::Hint().Fixed(kSettingsOptionRowH));

    // 第三项换成下拉框，用来说明同一套行结构可以放不同类型的控件。
    std::unique_ptr<DuiComboBox> closeAction(new DuiComboBox());
    closeAction->AddString(Txt(_T("最小化到托盘"), _T("Minimize to tray")));
    closeAction->AddString(Txt(_T("退出程序"), _T("Quit the application")));
    closeAction->SetCurSel(0, false);
    content->AddChild(MakeSettingOption(Txt(_T("关闭主窗口时"), _T("When the main window is closed")),
                                        std::move(closeAction)),
                      DuiLayout::Hint().Fixed(kSettingsOptionRowH));

    // 弹性空白，让上面的设置项靠顶部排列，而不是被平均分配到整个高度。
    content->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));

    root->AddChild(std::move(content), DuiLayout::Hint().Weight(1));

    return std::unique_ptr<DuiControl>(root.release());
}

// 构建示例五：用 DuiDock 描述的窗口骨架。
// 返回：骨架根控件，所有权交给调用方。
//
// 停靠的先后顺序决定四个拐角由哪一条边占用：这里先停靠上下两条，再停靠左侧
// 一条，所以左侧导航栏不会延伸到工具栏与状态栏所在的行。
std::unique_ptr<DuiControl> BuildLayoutSkeleton()
{
    std::unique_ptr<DuiDock> dock(new DuiDock());

    std::unique_ptr<SampleColorTile> toolbar(
        new SampleColorTile(Txt(_T(" 工具栏"), _T(" Toolbar")), kChromeTileColor));
    dock->AddDocked(std::move(toolbar), DuiDock::DockTop, kSkeletonToolbarH);

    std::unique_ptr<SampleColorTile> statusBar(
        new SampleColorTile(Txt(_T(" 状态栏 — 就绪"), _T(" Status bar — Ready")),
                            kStatusBarColor, kInverseTextColor));
    dock->AddDocked(std::move(statusBar), DuiDock::DockBottom, kSkeletonStatusBarH);

    std::unique_ptr<SampleColorTile> nav(
        new SampleColorTile(Txt(_T("导航栏"), _T("Nav")), kChromeTileColor));
    dock->AddDocked(std::move(nav), DuiDock::DockLeft, kSkeletonNavW);

    std::unique_ptr<SampleColorTile> center(
        new SampleColorTile(Txt(_T("(主内容区)"), _T("(content area)")), kContentTileColor));
    dock->AddDocked(std::move(center), DuiDock::DockFill);

    return std::unique_ptr<DuiControl>(dock.release());
}

} // 匿名命名空间

// ===== XML 布局：登录框 ==============================================

std::unique_ptr<DuiControl> Build_LoginXml()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("用 XML 描述出来的登录框"), _T("Login dialog built from XML")),
               Txt(_T("右侧的登录框不是用 C++ 一个控件一个控件拼出来的，而是 ")
                   _T("DuiXmlBuilder::FromString 在运行期解析左侧那段 XML 之后生成的。")
                   _T("左侧只读输入框里显示的就是交给解析器的原文，包括缩进与引号转义，")
                   _T("便于逐条对照解析结果。"),
                   _T("The login form on the right is not assembled control by control in ")
                   _T("C++ — DuiXmlBuilder::FromString parses the XML on the left at runtime ")
                   _T("and produces it. The read-only box on the left shows exactly the bytes ")
                   _T("handed to the parser, indentation and quote escaping included.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kXmlColumnGap);

        // 左栏：XML 原文。用多行只读输入框显示，内容与解析器拿到的字节完全一致。
        std::unique_ptr<DuiVBox> sourceBox(new DuiVBox());
        sourceBox->SetGap(kXmlPaneGap);

        std::unique_ptr<DuiLabel> sourceHeader(new DuiLabel());
        sourceHeader->SetText(Txt(_T("XML 原文（传给 DuiXmlBuilder::FromString 的内容）："),
                                  _T("XML source (passed to DuiXmlBuilder::FromString):")));
        sourceHeader->SetTextColor(kPaneHeaderColor);
        sourceBox->AddChild(std::move(sourceHeader), DuiLayout::Hint().Fixed(kXmlPaneHeaderH));

        std::unique_ptr<DuiEdit> source(new DuiEdit());
        source->SetMultiLine(true);
        source->SetReadOnly(true);
        source->SetText(Utf8ToCString(PickLoginXml()));
        sourceBox->AddChild(std::move(source), DuiLayout::Hint().Weight(1));

        // 右栏：由左栏那段 XML 解析出来的真实控件树。
        std::unique_ptr<DuiVBox> resultBox(new DuiVBox());
        resultBox->SetGap(kXmlPaneGap);

        std::unique_ptr<DuiLabel> resultHeader(new DuiLabel());
        resultHeader->SetText(Txt(_T("由上面这段 XML 建出来的真实控件树："),
                                  _T("Live tree built from the XML above:")));
        resultHeader->SetTextColor(kPaneHeaderColor);
        resultBox->AddChild(std::move(resultHeader), DuiLayout::Hint().Fixed(kXmlPaneHeaderH));

        // 解析失败时改为显示一条红色提示文字，保证这一栏总有可见的结果，
        // 避免整栏空白、让人误以为是布局出了问题。
        std::unique_ptr<DuiControl> built = DuiXmlBuilder::FromString(PickLoginXml());
        if (built)
        {
            resultBox->AddChild(std::move(built), DuiLayout::Hint().Weight(1));
        }
        else
        {
            std::unique_ptr<DuiLabel> error(new DuiLabel());
            error->SetText(Txt(_T("DuiXmlBuilder::FromString 返回了空指针"),
                               _T("DuiXmlBuilder::FromString returned null")));
            error->SetTextColor(kParseErrorColor);
            resultBox->AddChild(std::move(error), DuiLayout::Hint().Weight(1));
        }

        row->AddChild(std::move(sourceBox), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(resultBox), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kXmlDemoRowH);
    }

    AddSection(page.get(),
               Txt(_T("已识别的标签与属性"), _T("Recognized tags and attributes")),
               Txt(_T("标签：vbox、hbox、grid、label、button、edit。")
                   _T("通用属性：id、fixedWidth、fixedHeight、weight、margin；")
                   _T("布局容器另有 padding 与 gap。控件各自的属性：text、")
                   _T("textColor=\"r,g,b\"、placeholder、password、multiline、")
                   _T("buttonType=push|icon|checkbox|radio。")
                   _T("不认识的标签与属性会被直接忽略，既不报错也不中断解析；")
                   _T("需要更多标签时扩展 DuiXmlBuilder 即可。"),
                   _T("Tags: vbox, hbox, grid, label, button, edit. ")
                   _T("Common attributes: id, fixedWidth, fixedHeight, weight, margin. ")
                   _T("Layout containers also take padding and gap. Per-control: text, ")
                   _T("textColor=\"r,g,b\", placeholder, password, multiline, ")
                   _T("buttonType=push|icon|checkbox|radio. ")
                   _T("Unrecognized tags and attributes are silently ignored — extend the ")
                   _T("builder to add more.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 完整界面：五个示例 ============================================

std::unique_ptr<DuiControl> Build_Layouts()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    // 五个界面都用 AddVariantRowCapture 登记截图标记，命令行的 --capture-all
    // 模式会按标记名生成 ctl-layout-<名字>.png 供文档使用。标记名是文档里
    // 引用的文件名，不随语言变化，因此不经由 Txt 取文案。

    AddSection(page.get(),
               Txt(_T("登录对话框"), _T("Login dialog")),
               Txt(_T("整张卡片是一个 DuiVBox：品牌方块、产品名、副标题、两个输入框、")
                   _T("一行「记住我 + 忘记密码」、登录按钮、版本号自上而下排列。")
                   _T("卡片本身再由外层 DuiHBox 用「左弹性空白 + 固定宽度卡片 + 右弹性空白」")
                   _T("的办法居中。"),
                   _T("The whole card is one DuiVBox stacking a brand tile, the product name, ")
                   _T("a subtitle, two input fields, a \"remember me + forgot password\" row, ")
                   _T("the sign-in button and the version line. An outer DuiHBox centers the ")
                   _T("card by putting a flexible spacer on each side of it.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        row->AddChild(BuildLayoutLogin(), DuiLayout::Hint().Fixed(kLoginCardW));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRowCapture(page.get(), _T("layout-login"),
                             std::move(row), kLoginRowH);
    }

    AddSection(page.get(),
               Txt(_T("表单"), _T("Form")),
               Txt(_T("四行「右对齐字段名 + 弹性输入框」。字段名列宽度固定，输入框占据")
                   _T("剩下的宽度，因此各行的字段名与输入框自动对齐。底部先放一个弹性")
                   _T("空白把按钮组推到容器底部，按钮组内部再放一个弹性空白让两个按钮靠右。"),
                   _T("Four rows of \"right-aligned field name + flexible input\". The label ")
                   _T("column has a fixed width and the input takes the rest, so labels and ")
                   _T("inputs line up across rows. A flexible spacer pushes the button bar to ")
                   _T("the bottom, and another one inside the bar right-aligns the buttons.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(BuildLayoutForm(), DuiLayout::Hint().Fixed(kFormW));
        AddVariantRowCapture(page.get(), _T("layout-form"),
                             std::move(row), kFormRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("三列主窗"), _T("Three-pane main window")),
               Txt(_T("外层是一个 DuiHBox：60 像素的图标导航条、240 像素的列表列、")
                   _T("以及占据剩余宽度的内容列。三列全部用固定宽度与弹性宽度描述，")
                   _T("不含任何自绘代码。这里用 DuiHBox 而不是 DuiSplitter，是因为本例")
                   _T("只说明分栏比例；需要让用户拖动分栏时把外层换成 DuiSplitter 即可，")
                   _T("子控件不必改动。"),
                   _T("An outer DuiHBox holds a 60px icon rail, a 240px list column and a ")
                   _T("content column that takes the remaining width. All three are described ")
                   _T("with fixed and flexible widths alone — no custom painting. DuiHBox is ")
                   _T("used rather than DuiSplitter because this sample only shows the ")
                   _T("proportions; swap the outer container for a DuiSplitter to let the user ")
                   _T("drag the dividers, and the children stay as they are.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(BuildLayoutThreePane(), DuiLayout::Hint().Fixed(kThreePaneW));
        AddVariantRowCapture(page.get(), _T("layout-three-pane"),
                             std::move(row), kThreePaneRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("设置页"), _T("Settings page")),
               Txt(_T("左侧用 DuiListBox 充当竖排的分类导航，右侧是一个带内边距的 DuiVBox。")
                   _T("每个设置项都是一行「说明文字占弹性宽度 + 控件占固定宽度」，")
                   _T("因此复选框与下拉框的左边缘在各行之间自动对齐。末尾的弹性空白让")
                   _T("设置项靠顶部排列。"),
                   _T("A DuiListBox on the left serves as the vertical category navigation; ")
                   _T("the right side is a padded DuiVBox. Every option is a row of ")
                   _T("\"flexible caption + fixed-width control\", so the checkboxes and the ")
                   _T("combo box line up on their left edge. A trailing flexible spacer keeps ")
                   _T("the options packed to the top.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(BuildLayoutSettings(), DuiLayout::Hint().Fixed(kSettingsW));
        AddVariantRowCapture(page.get(), _T("layout-settings"),
                             std::move(row), kSettingsRowHeight);
    }

    AddSection(page.get(),
               Txt(_T("窗口骨架"), _T("Window skeleton")),
               Txt(_T("DuiDock 按停靠边依次划分空间：先划出顶部的工具栏，再划出底部的")
                   _T("状态栏，然后划出左侧的导航栏，最后一个 DockFill 的子控件占满剩下")
                   _T("的矩形。停靠的先后顺序决定四个拐角由哪一条边占用 —— 这里先上下")
                   _T("后左右，所以左侧导航栏不会延伸到工具栏与状态栏所在的行。"),
                   _T("DuiDock carves space off one edge at a time: the toolbar takes the top, ")
                   _T("the status bar the bottom, the navigation pane the left, and the ")
                   _T("DockFill child occupies whatever rectangle is left. Docking order ")
                   _T("decides who owns the corners — top and bottom are docked first here, ")
                   _T("so the left pane does not extend into the toolbar or status bar rows.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->AddChild(BuildLayoutSkeleton(), DuiLayout::Hint().Fixed(kSkeletonW));
        AddVariantRowCapture(page.get(), _T("layout-skeleton"),
                             std::move(row), kSkeletonRowHeight);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 =============================================

const PageEntry* GetSamplePages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("xml-login"),    _T("XML 布局　登录框"),   _T("XML Layout: Login"), &Build_LoginXml, true },
        { _T("full-layouts"), _T("完整界面　五个示例"), _T("Full Screens"),      &Build_Layouts,  true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
