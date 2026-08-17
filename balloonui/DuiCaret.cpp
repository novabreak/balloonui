/**
 *  DuiCaret 的实现。设计意图与使用约束见 DuiCaret.h 的文件头注释。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "DuiCaret.h"

namespace balloonwjui {

DuiCaret::DuiCaret()
    : m_hwndOwner(nullptr)
    , m_bCreated(false)
    , m_bVisible(false)
{
    m_ptPos.x = 0;
    m_ptPos.y = 0;
    m_szCaret.cx = 0;
    m_szCaret.cy = 0;
}

DuiCaret::~DuiCaret()
{
    // 兜底销毁。正常流程里调用方应当在失去焦点时就主动 Destroy，
    // 这里只是防止对象被销毁时还占着线程唯一的那份系统光标。
    Destroy();
}

bool DuiCaret::Create(HWND hwndOwner, HBITMAP hbmp, int width, int height)
{
    if (hwndOwner == nullptr || !::IsWindow(hwndOwner))
    {
        return false;
    }

    // 已经占有光标时先让出来再重建。直接叠着调 ::CreateCaret 虽然系统
    // 允许，但本类的 m_bVisible 计数会跟系统的显示计数脱节，后续 Show
    // 的折叠逻辑就不准了。
    if (m_bCreated)
    {
        Destroy();
    }

    if (!::CreateCaret(hwndOwner, hbmp, width, height))
    {
        return false;
    }

    m_hwndOwner  = hwndOwner;
    m_bCreated   = true;
    // 系统新建出来的光标默认不可见，必须显式 Show(true) 才会出现。
    m_bVisible   = false;
    m_szCaret.cx = width;
    m_szCaret.cy = height;

    // 位置尚未确定，先归零；调用方随后会调 SetPos 给出真实插入点。
    m_ptPos.x = 0;
    m_ptPos.y = 0;
    return true;
}

void DuiCaret::Destroy()
{
    if (!m_bCreated)
    {
        return;
    }

    // 系统的 ::DestroyCaret 作用于"当前线程拥有的光标"，不接受句柄参数。
    // 因此只有在确实由本对象抢占着的时候才调用，否则会把别的控件刚建好的
    // 光标误销毁掉。m_bCreated 这个标志位守的就是这件事。
    ::DestroyCaret();

    m_hwndOwner  = nullptr;
    m_bCreated   = false;
    m_bVisible   = false;
    m_szCaret.cx = 0;
    m_szCaret.cy = 0;
    m_ptPos.x    = 0;
    m_ptPos.y    = 0;
}

bool DuiCaret::Show(bool bShow)
{
    if (!m_bCreated)
    {
        return false;
    }

    // 系统的显示状态是一个可叠加的计数器：隐藏两次就要显示两次才能恢复。
    // 调用方（排版引擎的回调）很可能重复要求同一个状态，若原样透传下去，
    // 计数会越叠越深，最终表现为"光标怎么也不出来"。这里用 m_bVisible
    // 记住当前状态，把重复调用折叠掉，保证系统计数始终只在 0 和 -1 之间。
    if (m_bVisible == bShow)
    {
        return true;
    }

    if (bShow)
    {
        ::ShowCaret(m_hwndOwner);
    }
    else
    {
        ::HideCaret(m_hwndOwner);
    }

    m_bVisible = bShow;
    return true;
}

bool DuiCaret::SetPos(int x, int y)
{
    if (!m_bCreated)
    {
        return false;
    }

    // 即使当前隐藏也照样设置：输入法要靠系统光标的位置决定候选条弹在哪里，
    // 详见 DuiCaret.h 的文件头说明。
    ::SetCaretPos(x, y);

    m_ptPos.x = x;
    m_ptPos.y = y;
    return true;
}

} // namespace balloonwjui
