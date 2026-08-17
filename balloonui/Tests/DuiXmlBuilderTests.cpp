#include "stdafx.h"
#include <string>
#include <map>
#include <vector>
#include "DuiXmlBuilderTests.h"

#if BUI_FEATURE_XMLBUILDER && BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL

#include "../Controls/Layout/DuiLayout.h"
#include "../Controls/Basic/DuiButton.h"
#include "../Controls/Basic/DuiLabel.h"
#if BUI_FEATURE_RICHTEXT
#include "../Controls/Input/DuiRichEdit.h"
#endif

namespace balloonwjui {

namespace DuiXmlBuilderTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)

// ----- pure parser ----------------------------------------------------

static Result Test_ParseEmpty()
{
    DuiXmlBuilder::Node root;
    EXPECT_TRUE(!DuiXmlBuilder::Parse(nullptr, root), _T("Pe/null"));
    EXPECT_TRUE(!DuiXmlBuilder::Parse("", root), _T("Pe/empty"));
    return OK(_T("ParseEmpty"));
}

static Result Test_ParseSelfClosing()
{
    DuiXmlBuilder::Node n;
    EXPECT_TRUE(DuiXmlBuilder::Parse("<button id=\"7\" text=\"OK\"/>", n), _T("PSC/ok"));
    EXPECT_TRUE(n.tag == "button", _T("PSC/tag"));
    EXPECT_TRUE(n.attrs["id"] == "7", _T("PSC/id"));
    EXPECT_TRUE(n.attrs["text"] == "OK", _T("PSC/text"));
    EXPECT_INT((int)n.children.size(), 0, _T("PSC/noChildren"));
    return OK(_T("ParseSelfClosing"));
}

static Result Test_ParseNested()
{
    const char* xml =
        "<vbox padding=\"10\">"
            "<label text=\"hi\"/>"
            "<hbox gap=\"4\">"
                "<button text=\"a\"/>"
                "<button text=\"b\"/>"
            "</hbox>"
        "</vbox>";
    DuiXmlBuilder::Node n;
    EXPECT_TRUE(DuiXmlBuilder::Parse(xml, n), _T("PN/ok"));
    EXPECT_TRUE(n.tag == "vbox", _T("PN/root"));
    EXPECT_INT((int)n.children.size(), 2, _T("PN/rootChildren"));
    EXPECT_TRUE(n.children[0].tag == "label", _T("PN/c0"));
    EXPECT_TRUE(n.children[1].tag == "hbox",  _T("PN/c1"));
    EXPECT_INT((int)n.children[1].children.size(), 2, _T("PN/c1Children"));
    EXPECT_TRUE(n.children[1].children[0].attrs["text"] == "a", _T("PN/grandA"));
    return OK(_T("ParseNested"));
}

static Result Test_ParseEntityDecode()
{
    DuiXmlBuilder::Node n;
    EXPECT_TRUE(DuiXmlBuilder::Parse(
        "<label text=\"a&amp;b&lt;c&gt;d&quot;e&apos;f\"/>", n), _T("PE/ok"));
    EXPECT_TRUE(n.attrs["text"] == "a&b<c>d\"e'f", _T("PE/decoded"));
    return OK(_T("ParseEntityDecode"));
}

static Result Test_ParseSkipsProlog()
{
    DuiXmlBuilder::Node n;
    EXPECT_TRUE(DuiXmlBuilder::Parse(
        "<?xml version=\"1.0\"?><!--note--><vbox/>", n), _T("PP/ok"));
    EXPECT_TRUE(n.tag == "vbox", _T("PP/tag"));
    return OK(_T("ParseSkipsProlog"));
}

static Result Test_ParseMalformedReturnsFalse()
{
    DuiXmlBuilder::Node n;
    EXPECT_TRUE(!DuiXmlBuilder::Parse("<vbox><button></vbox>", n), _T("PM/wrongCloser"));
    n = DuiXmlBuilder::Node();
    EXPECT_TRUE(!DuiXmlBuilder::Parse("<vbox", n), _T("PM/unterminated"));
    n = DuiXmlBuilder::Node();
    EXPECT_TRUE(!DuiXmlBuilder::Parse("<vbox attr=value/>", n), _T("PM/unquotedAttr"));
    return OK(_T("ParseMalformedReturnsFalse"));
}

// ----- builder dispatch -----------------------------------------------

static Result Test_BuildVBoxWithButton()
{
    auto root = DuiXmlBuilder::FromString(
        "<vbox padding=\"5\" gap=\"4\">"
        "  <button id=\"42\" text=\"Send\"/>"
        "</vbox>");
    EXPECT_TRUE(root.get() != nullptr, _T("BVB/built"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("BVB/isVBox"));
    DuiButton* b = nullptr;
    if (box)
    {
        // Layout to expose child rect; verify the child is a DuiButton
        // by walking parent chain via a temp Layout.
        box->Layout(RECT{ 0, 0, 200, 100 });
        // We can't introspect DuiLayout's m_children from outside without
        // a helper, so instead exercise the path: hit-test a point inside
        // the only child's expected rect and inspect what we get.
        DuiControl* hit = box->HitTest(POINT{ 100, 50 });
        b = dynamic_cast<DuiButton*>(hit);
    }
    EXPECT_TRUE(b != nullptr, _T("BVB/isButton"));
    if (b)
    {
        EXPECT_INT((int)b->GetCtrlId(), 42, _T("BVB/id"));
        EXPECT_STR(b->GetText(), _T("Send"), _T("BVB/text"));
    }
    return OK(_T("BuildVBoxWithButton"));
}

static Result Test_BuildLabelTextColor()
{
    auto root = DuiXmlBuilder::FromString(
        "<label id=\"3\" text=\"X\" textColor=\"45,108,223\"/>");
    DuiLabel* l = dynamic_cast<DuiLabel*>(root.get());
    EXPECT_TRUE(l != nullptr, _T("BL/isLabel"));
    if (l)
    {
        EXPECT_INT((int)l->GetCtrlId(), 3, _T("BL/id"));
        EXPECT_STR(l->GetText(), _T("X"), _T("BL/text"));
        EXPECT_INT((int)l->GetTextColor(), (int)RGB(45, 108, 223), _T("BL/clr"));
    }
    return OK(_T("BuildLabelTextColor"));
}

// Unrecognized tag returns null root.
static Result Test_BuildUnknownTag()
{
    auto root = DuiXmlBuilder::FromString("<frobnicate/>");
    EXPECT_TRUE(root.get() == nullptr, _T("BU/null"));
    return OK(_T("BuildUnknownTag"));
}

// Wide string overload re-encodes correctly for non-ASCII text. The
// label's payload is constructed at runtime from explicit codepoints
// so the test cpp file's source encoding is irrelevant (MSVC defaults
// to system code page for non-BOM files; runtime concatenation dodges
// that trap entirely).
static Result Test_FromStringWide()
{
    std::wstring xml = L"<label text=\"";
    xml += (wchar_t)0x4F60;     // U+4F60 (Chinese ni3)
    xml += (wchar_t)0x597D;     // U+597D (Chinese hao3)
    xml += L"\"/>";

    auto root = DuiXmlBuilder::FromString(xml.c_str());
    if (!root.get())
    {
        CString d;
        d.Format(_T("FromString returned null, xml.size()=%d"), (int)xml.size());
        return Fail(_T("FW/parse"), d);
    }
    DuiLabel* l = dynamic_cast<DuiLabel*>(root.get());
    if (!l)
    {
        return Fail(_T("FW/isLabel"), _T("root is not a DuiLabel"));
    }
    if (l)
    {
        CString want;
        want += (TCHAR)0x4F60;
        want += (TCHAR)0x597D;
        EXPECT_STR(l->GetText(), want, _T("FW/text"));
    }
    return OK(_T("FromStringWide"));
}

// Layout hints attach to children. Use a vbox and verify children take
// the right vertical extent given fixedHeight + weight.
static Result Test_LayoutHintsApply()
{
    auto root = DuiXmlBuilder::FromString(
        "<vbox padding=\"0\" gap=\"0\">"
        "  <label text=\"hdr\" fixedHeight=\"30\"/>"
        "  <label text=\"body\" weight=\"1\"/>"
        "</vbox>");
    EXPECT_TRUE(root.get() != nullptr, _T("LH/built"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("LH/isVBox"));
    if (box)
    {
        box->Layout(RECT{ 0, 0, 100, 100 });
    }

    // Hit (10,10) -> header (top 30 px).
    DuiControl* hHdr = box->HitTest(POINT{ 10, 10 });
    DuiLabel* hdr = dynamic_cast<DuiLabel*>(hHdr);
    EXPECT_TRUE(hdr != nullptr, _T("LH/hdrAt10"));
    if (hdr)
    {
        EXPECT_STR(hdr->GetText(), _T("hdr"), _T("LH/hdrText"));
    }

    // Hit (10,50) -> body label.
    DuiControl* hBody = box->HitTest(POINT{ 10, 50 });
    DuiLabel* body = dynamic_cast<DuiLabel*>(hBody);
    EXPECT_TRUE(body != nullptr, _T("LH/bodyAt50"));
    if (body)
    {
        EXPECT_STR(body->GetText(), _T("body"), _T("LH/bodyText"));
    }
    return OK(_T("LayoutHintsApply"));
}

// ----- Custom factory hook --------------------------------------------

// A trivial paint-only DuiControl subclass used as the "custom" type.
// HitTest of the base class returns `this` when the hit point is in
// m_rcItem and the control is visible+enabled, so we use HitTest after
// Layout to introspect the built tree without touching protected
// member m_children.
class CFLeaf : public DuiControl
{
public:
    int  marker = 0;
    void OnPaint(HDC, const RECT&) override { /* no-op */ }
};

static Result Test_CustomFactoryLeaf()
{
    DuiXmlBuilder::CustomFactory fac =
        [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
        {
            if (n.tag == "rail-item")
            {
                auto c = std::unique_ptr<CFLeaf>(new CFLeaf());
                auto it = n.attrs.find("hue");
                c->marker = (it != n.attrs.end()) ? std::atoi(it->second.c_str()) : 0;
                return c;
            }
            return nullptr;
        };
    auto root = DuiXmlBuilder::FromString(
        "<vbox><rail-item id=\"42\" hue=\"165\" fixedHeight=\"40\"/></vbox>", fac);
    EXPECT_TRUE(root != nullptr, _T("CFL/root"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("CFL/box"));
    if (box)
    {
        box->Layout(RECT{ 0, 0, 100, 100 });
        DuiControl* hit = box->HitTest(POINT{ 50, 20 });
        CFLeaf* leaf = dynamic_cast<CFLeaf*>(hit);
        EXPECT_TRUE(leaf != nullptr, _T("CFL/leafType"));
        if (leaf)
        {
            EXPECT_INT((int)leaf->GetCtrlId(), 42, _T("CFL/idApplied"));
            EXPECT_INT(leaf->marker,           165, _T("CFL/hue"));
        }
    }
    return OK(_T("CustomFactoryLeaf"));
}

static Result Test_CustomFactoryWithChildren()
{
    // Custom node ("card") has built-in <label> children. The builder
    // recurses and AddChild()s them onto the custom control, so a
    // HitTest inside the card hits the right Label.
    class CardCtrl : public DuiControl
    {
    public:
        void OnPaint(HDC, const RECT&) override {}
        void Layout(const RECT& rcAvail) override
        {
            m_rcItem = rcAvail;
            // Stack two children top-to-bottom, 20px each.
            int n = (int)m_children.size();
            for (int i = 0; i < n; ++i)
            {
                RECT r = { rcAvail.left,
                           rcAvail.top + i * 20,
                           rcAvail.right,
                           rcAvail.top + (i + 1) * 20 };
                m_children[i]->SetRect(r);
                m_children[i]->Layout(r);
            }
        }
    };
    DuiXmlBuilder::CustomFactory fac =
        [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
        {
            if (n.tag == "card")
            {
                return std::unique_ptr<CardCtrl>(new CardCtrl());
            }
            return nullptr;
        };
    auto root = DuiXmlBuilder::FromString(
        "<card><label text=\"hi\" fixedHeight=\"20\"/><label text=\"there\" fixedHeight=\"20\"/></card>",
        fac);
    EXPECT_TRUE(root != nullptr, _T("CFC/root"));
    if (root)
    {
        root->Layout(RECT{ 0, 0, 100, 40 });
        DuiControl* hit0 = root->HitTest(POINT{ 50,  5 });
        DuiControl* hit1 = root->HitTest(POINT{ 50, 25 });
        DuiLabel* l0 = dynamic_cast<DuiLabel*>(hit0);
        DuiLabel* l1 = dynamic_cast<DuiLabel*>(hit1);
        EXPECT_TRUE(l0 && l1, _T("CFC/childType"));
        if (l0 && l1)
        {
            EXPECT_STR(l0->GetText(), _T("hi"),    _T("CFC/c0text"));
            EXPECT_STR(l1->GetText(), _T("there"), _T("CFC/c1text"));
        }
    }
    return OK(_T("CustomFactoryWithChildren"));
}

// Regression: a prolog comment whose body contains a `>` character (for
// example `<!-- mentions <foo/> -->`) must not terminate at the inner
// `>`. Earlier SkipProlog implementation did exactly that and the
// surrounding parse silently failed.
static Result Test_PrologCommentWithGtChar()
{
    const char* xml =
        "<!--\n"
        "  mentions <foo/> in the body and a > stray char\n"
        "  multi line ok too\n"
        "-->\n"
        "<vbox>\n"
        "  <label text=\"after\" fixedHeight=\"20\"/>\n"
        "</vbox>";
    auto root = DuiXmlBuilder::FromString(xml);
    EXPECT_TRUE(root != nullptr, _T("PCG/built"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("PCG/isVBox"));
    if (box)
    {
        box->Layout(RECT{ 0, 0, 100, 40 });
        DuiLabel* l = dynamic_cast<DuiLabel*>(box->HitTest(POINT{ 50, 10 }));
        EXPECT_TRUE(l != nullptr, _T("PCG/labelType"));
        if (l)
        {
            EXPECT_STR(l->GetText(), _T("after"), _T("PCG/labelText"));
        }
    }
    return OK(_T("PrologCommentWithGtChar"));
}

// Regression: an inline comment between sibling child elements (e.g.
// inside a vbox) must be skipped so authors can annotate layouts. Same
// fix as above but on the children-loop path.
static Result Test_InlineCommentBetweenChildren()
{
    const char* xml =
        "<vbox>"
        "  <label text=\"a\" fixedHeight=\"20\"/>"
        "  <!-- comment between siblings, has <inline/> bracket -->"
        "  <label text=\"b\" fixedHeight=\"20\"/>"
        "</vbox>";
    auto root = DuiXmlBuilder::FromString(xml);
    EXPECT_TRUE(root != nullptr, _T("ICB/built"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("ICB/isVBox"));
    if (box)
    {
        box->Layout(RECT{ 0, 0, 100, 40 });
        DuiLabel* a = dynamic_cast<DuiLabel*>(box->HitTest(POINT{ 50, 10 }));
        DuiLabel* b = dynamic_cast<DuiLabel*>(box->HitTest(POINT{ 50, 30 }));
        EXPECT_TRUE(a && b, _T("ICB/twoLabels"));
        if (a && b)
        {
            EXPECT_STR(a->GetText(), _T("a"), _T("ICB/aText"));
            EXPECT_STR(b->GetText(), _T("b"), _T("ICB/bText"));
        }
    }
    return OK(_T("InlineCommentBetweenChildren"));
}

static Result Test_CustomFactoryNullSkips()
{
    // Factory returning null for every tag must NOT break v1 behavior:
    // unknown tags inside a container are skipped, surrounding known
    // ones still build.
    DuiXmlBuilder::CustomFactory fac =
        [](const DuiXmlBuilder::Node&) -> std::unique_ptr<DuiControl>
        {
            return nullptr;
        };
    auto root = DuiXmlBuilder::FromString(
        "<vbox>"
        "  <label text=\"a\" fixedHeight=\"20\"/>"
        "  <weird/>"
        "  <label text=\"b\" fixedHeight=\"20\"/>"
        "</vbox>", fac);
    EXPECT_TRUE(root != nullptr, _T("CFN/root"));
    DuiVBox* box = dynamic_cast<DuiVBox*>(root.get());
    EXPECT_TRUE(box != nullptr, _T("CFN/box"));
    if (box)
    {
        box->Layout(RECT{ 0, 0, 100, 40 });
        DuiLabel* a = dynamic_cast<DuiLabel*>(box->HitTest(POINT{ 50, 10 }));
        DuiLabel* b = dynamic_cast<DuiLabel*>(box->HitTest(POINT{ 50, 30 }));
        EXPECT_TRUE(a && b, _T("CFN/twoLabelsRemain"));
        if (a && b)
        {
            EXPECT_STR(a->GetText(), _T("a"), _T("CFN/aText"));
            EXPECT_STR(b->GetText(), _T("b"), _T("CFN/bText"));
        }
    }
    return OK(_T("CustomFactoryNullSkips"));
}

#if BUI_FEATURE_RICHTEXT

// ----- <richtext>（无窗口富文本控件）---------------------------------
//
// 这一组验的是「XML 属性有没有真的落到控件上」。标签名与 <richedit> 只差
// 两个字母，属性名又大半同名，写混了不会报错、只会建出另一个控件，所以
// 第一条用例先钉住建出来的确实是 DuiRichEdit。

// 与 <richedit> 同名的那批基本属性。
static Result Test_BuildRichTextBasicAttrs()
{
    auto root = DuiXmlBuilder::FromString(
        "<richtext id=\"77\" text=\"hello\" placeholder=\"type here\" "
        "read-only=\"true\" multi-line=\"true\" word-wrap=\"false\" "
        //颜色属性的格式是逗号分隔的三个十进制分量，不是 #RRGGBB ——
        //库内 ParseColor 只认前者，认不出时静默退回默认值。
        "max-length=\"140\" text-color=\"255,0,0\" bg-color=\"0,255,0\"/>");
    EXPECT_TRUE(root.get() != nullptr, _T("BRT/built"));

    DuiRichEdit* rt = dynamic_cast<DuiRichEdit*>(root.get());
    EXPECT_TRUE(rt != nullptr, _T("BRT/isRichEdit"));
    if (rt == nullptr)
    {
        return Fail(_T("BRT/isRichEdit"), _T("tag did not build a DuiRichEdit"));
    }

    EXPECT_INT((int)rt->GetCtrlId(), 77, _T("BRT/id"));
    EXPECT_STR(rt->GetText(), _T("hello"), _T("BRT/text"));
    EXPECT_STR(rt->GetPlaceholder(), _T("type here"), _T("BRT/placeholder"));
    EXPECT_TRUE(rt->IsReadOnly(), _T("BRT/readOnly"));
    EXPECT_TRUE(rt->IsMultiLine(), _T("BRT/multiLine"));
    EXPECT_TRUE(!rt->IsWordWrap(), _T("BRT/wordWrapOff"));
    EXPECT_INT(rt->GetMaxLength(), 140, _T("BRT/maxLength"));
    EXPECT_INT((int)rt->GetTextColor(), (int)RGB(255, 0, 0), _T("BRT/textColor"));
    EXPECT_INT((int)rt->GetBackgroundColor(), (int)RGB(0, 255, 0), _T("BRT/bgColor"));
    return OK(_T("BuildRichTextBasicAttrs"));
}

// 本控件独有的那批属性：边框、可聚焦、拖放、右键菜单、两个方向的滚动条策略。
static Result Test_BuildRichTextOwnAttrs()
{
    auto root = DuiXmlBuilder::FromString(
        "<richtext show-border=\"false\" focusable=\"false\" "
        "drag-drop=\"false\" context-menu=\"false\" "
        "v-scroll=\"always\" h-scroll=\"never\"/>");
    DuiRichEdit* rt = dynamic_cast<DuiRichEdit*>(root.get());
    EXPECT_TRUE(rt != nullptr, _T("BRTOwn/isRichEdit"));
    if (rt == nullptr)
    {
        return Fail(_T("BRTOwn/isRichEdit"), _T("tag did not build a DuiRichEdit"));
    }

    EXPECT_TRUE(!rt->IsShowBorder(), _T("BRTOwn/noBorder"));
    EXPECT_TRUE(!rt->IsFocusable(), _T("BRTOwn/notFocusable"));
    EXPECT_TRUE(!rt->IsDragDropEnabled(), _T("BRTOwn/noDragDrop"));
    EXPECT_TRUE(!rt->IsContextMenuEnabled(), _T("BRTOwn/noContextMenu"));
    EXPECT_INT((int)rt->GetVScrollPolicy(), (int)DuiRichEdit::kScrollBarAlways,
               _T("BRTOwn/vScrollAlways"));
    EXPECT_INT((int)rt->GetHScrollPolicy(), (int)DuiRichEdit::kScrollBarNever,
               _T("BRTOwn/hScrollNever"));
    return OK(_T("BuildRichTextOwnAttrs"));
}

// 自动增高：像素版上下限。
static Result Test_BuildRichTextAutoGrowPixels()
{
    auto root = DuiXmlBuilder::FromString(
        "<richtext auto-grow=\"true\" auto-grow-min=\"30\" auto-grow-max=\"120\"/>");
    DuiRichEdit* rt = dynamic_cast<DuiRichEdit*>(root.get());
    EXPECT_TRUE(rt != nullptr, _T("BRTGrow/isRichEdit"));
    if (rt == nullptr)
    {
        return Fail(_T("BRTGrow/isRichEdit"), _T("tag did not build a DuiRichEdit"));
    }

    EXPECT_TRUE(rt->IsAutoGrow(), _T("BRTGrow/on"));
    EXPECT_INT(rt->GetAutoGrowMin(), 30,  _T("BRTGrow/min"));
    EXPECT_INT(rt->GetAutoGrowMax(), 120, _T("BRTGrow/max"));

    // 关掉的写法也要生效。
    auto rootOff = DuiXmlBuilder::FromString("<richtext auto-grow=\"false\"/>");
    DuiRichEdit* rtOff = dynamic_cast<DuiRichEdit*>(rootOff.get());
    EXPECT_TRUE(rtOff != nullptr, _T("BRTGrow/offBuilt"));
    if (rtOff != nullptr)
    {
        EXPECT_TRUE(!rtOff->IsAutoGrow(), _T("BRTGrow/off"));
    }
    return OK(_T("BuildRichTextAutoGrowPixels"));
}

// 自动增高：行数版换算成像素，且写在像素版之后时以行数版为准。
//
// 两种写法改的是同一对成员，处理顺序决定谁生效。这条用例钉住的就是那个
// 顺序 —— 顺序写反的话，同时写了两种属性时会以像素版为准，与文档不符。
static Result Test_BuildRichTextAutoGrowLinesWins()
{
    auto root = DuiXmlBuilder::FromString(
        "<richtext auto-grow-min=\"30\" auto-grow-max=\"120\" "
        "auto-grow-lines=\"1,5\"/>");
    DuiRichEdit* rt = dynamic_cast<DuiRichEdit*>(root.get());
    EXPECT_TRUE(rt != nullptr, _T("BRTLines/isRichEdit"));
    if (rt == nullptr)
    {
        return Fail(_T("BRTLines/isRichEdit"), _T("tag did not build a DuiRichEdit"));
    }

    // 行数换算用的是当前默认字体的行高，具体像素值与字体有关，所以不断言
    // 绝对值，只断言「确实是按行高换算出来的」以及「覆盖掉了像素版的值」。
    const int nLineH = rt->GetLineHeight();
    EXPECT_TRUE(nLineH > 0, _T("BRTLines/lineHeightPositive"));
    EXPECT_INT(rt->GetAutoGrowMin(), nLineH,     _T("BRTLines/minIsOneLine"));
    EXPECT_INT(rt->GetAutoGrowMax(), nLineH * 5, _T("BRTLines/maxIsFiveLines"));
    return OK(_T("BuildRichTextAutoGrowLinesWins"));
}

// 不写任何属性时，控件保持自己的默认值 —— 解析过程不能顺手改掉什么。
//
// 这条防的是「属性缺省时误把空串当成 false / 0 设进去」这类错误：那种错误
// 在写了属性的用例里看不出来，只有不写属性时才暴露。
static Result Test_BuildRichTextDefaultsUntouched()
{
    auto root = DuiXmlBuilder::FromString("<richtext/>");
    DuiRichEdit* rt = dynamic_cast<DuiRichEdit*>(root.get());
    EXPECT_TRUE(rt != nullptr, _T("BRTDef/isRichEdit"));
    if (rt == nullptr)
    {
        return Fail(_T("BRTDef/isRichEdit"), _T("tag did not build a DuiRichEdit"));
    }

    // 与控件自身的默认值逐项对照。
    DuiRichEdit reference;
    EXPECT_INT(rt->IsReadOnly() ? 1 : 0, reference.IsReadOnly() ? 1 : 0,
               _T("BRTDef/readOnly"));
    EXPECT_INT(rt->IsMultiLine() ? 1 : 0, reference.IsMultiLine() ? 1 : 0,
               _T("BRTDef/multiLine"));
    EXPECT_INT(rt->IsWordWrap() ? 1 : 0, reference.IsWordWrap() ? 1 : 0,
               _T("BRTDef/wordWrap"));
    EXPECT_INT(rt->IsShowBorder() ? 1 : 0, reference.IsShowBorder() ? 1 : 0,
               _T("BRTDef/showBorder"));
    EXPECT_INT(rt->IsFocusable() ? 1 : 0, reference.IsFocusable() ? 1 : 0,
               _T("BRTDef/focusable"));
    EXPECT_INT(rt->IsAutoGrow() ? 1 : 0, reference.IsAutoGrow() ? 1 : 0,
               _T("BRTDef/autoGrow"));
    EXPECT_INT(rt->IsContextMenuEnabled() ? 1 : 0,
               reference.IsContextMenuEnabled() ? 1 : 0, _T("BRTDef/contextMenu"));
    EXPECT_INT(rt->GetMaxLength(), reference.GetMaxLength(), _T("BRTDef/maxLength"));
    EXPECT_INT((int)rt->GetVScrollPolicy(), (int)reference.GetVScrollPolicy(),
               _T("BRTDef/vScroll"));
    return OK(_T("BuildRichTextDefaultsUntouched"));
}

#endif // BUI_FEATURE_RICHTEXT

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("ParseEmpty"),                &Test_ParseEmpty                },
        { _T("ParseSelfClosing"),          &Test_ParseSelfClosing          },
        { _T("ParseNested"),               &Test_ParseNested               },
        { _T("ParseEntityDecode"),         &Test_ParseEntityDecode         },
        { _T("ParseSkipsProlog"),          &Test_ParseSkipsProlog          },
        { _T("ParseMalformedReturnsFalse"),&Test_ParseMalformedReturnsFalse},
        { _T("BuildVBoxWithButton"),       &Test_BuildVBoxWithButton       },
        { _T("BuildLabelTextColor"),       &Test_BuildLabelTextColor       },
        { _T("BuildUnknownTag"),           &Test_BuildUnknownTag           },
        { _T("FromStringWide"),            &Test_FromStringWide            },
        { _T("LayoutHintsApply"),          &Test_LayoutHintsApply          },
        { _T("CustomFactoryLeaf"),           &Test_CustomFactoryLeaf           },
        { _T("CustomFactoryWithChildren"),   &Test_CustomFactoryWithChildren   },
        { _T("CustomFactoryNullSkips"),      &Test_CustomFactoryNullSkips      },
        { _T("PrologCommentWithGtChar"),     &Test_PrologCommentWithGtChar     },
        { _T("InlineCommentBetweenChildren"),&Test_InlineCommentBetweenChildren},
#if BUI_FEATURE_RICHTEXT
        { _T("BuildRichTextBasicAttrs"),     &Test_BuildRichTextBasicAttrs     },
        { _T("BuildRichTextOwnAttrs"),       &Test_BuildRichTextOwnAttrs       },
        { _T("BuildRichTextAutoGrowPixels"), &Test_BuildRichTextAutoGrowPixels },
        { _T("BuildRichTextAutoGrowLinesWins"), &Test_BuildRichTextAutoGrowLinesWins },
        { _T("BuildRichTextDefaultsUntouched"), &Test_BuildRichTextDefaultsUntouched },
#endif
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
    summary.Format(_T("[summary] DuiXmlBuilderTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiXmlBuilderTests

} // namespace balloonwjui

#endif // BUI_FEATURE_XMLBUILDER && BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL
