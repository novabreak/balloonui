#include "stdafx.h"
#include "DuiAnimationTests.h"
#include <math.h>

namespace balloonwjui {

namespace DuiAnimationTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

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
#define EXPECT_NEAR(a, b, eps, name) \
    do { double _a = (a); double _b = (b); double _e = (eps); \
         double diff = _a - _b; if (diff < 0) diff = -diff; \
         if (diff > _e) { CString _d; _d.Format(_T("expected=%.4f got=%.4f"), _b, _a); return Fail(name, _d); } \
    } while (0)

// ----- easing endpoints ----------------------------------------------

static Result Test_EasingEndpoints()
{
    // All curves must hit (0,0) and (1,1) exactly.
    EXPECT_NEAR(DuiEase::Linear(0.0),       0.0, 1e-9, _T("End/lin0"));
    EXPECT_NEAR(DuiEase::Linear(1.0),       1.0, 1e-9, _T("End/lin1"));
    EXPECT_NEAR(DuiEase::EaseInQuad(0.0),   0.0, 1e-9, _T("End/inQ0"));
    EXPECT_NEAR(DuiEase::EaseInQuad(1.0),   1.0, 1e-9, _T("End/inQ1"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(0.0),  0.0, 1e-9, _T("End/outQ0"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(1.0),  1.0, 1e-9, _T("End/outQ1"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(0.0), 0.0, 1e-9, _T("End/ioQ0"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(1.0), 1.0, 1e-9, _T("End/ioQ1"));
    EXPECT_NEAR(DuiEase::EaseInCubic(0.0),  0.0, 1e-9, _T("End/inC0"));
    EXPECT_NEAR(DuiEase::EaseInCubic(1.0),  1.0, 1e-9, _T("End/inC1"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(0.0), 0.0, 1e-9, _T("End/outC0"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(1.0), 1.0, 1e-9, _T("End/outC1"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(0.0), 0.0, 1e-9, _T("End/ioC0"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(1.0), 1.0, 1e-9, _T("End/ioC1"));
    return OK(_T("EasingEndpoints"));
}

// Specific midpoint values verify the curve shape.
static Result Test_EasingMidpoints()
{
    EXPECT_NEAR(DuiEase::Linear(0.5),        0.5,    1e-9, _T("Mid/lin"));
    EXPECT_NEAR(DuiEase::EaseInQuad(0.5),    0.25,   1e-9, _T("Mid/inQ"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(0.5),   0.75,   1e-9, _T("Mid/outQ"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(0.5), 0.5,    1e-9, _T("Mid/ioQ"));
    EXPECT_NEAR(DuiEase::EaseInCubic(0.5),   0.125,  1e-9, _T("Mid/inC"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(0.5),  0.875,  1e-9, _T("Mid/outC"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(0.5), 0.5,   1e-9, _T("Mid/ioC"));
    return OK(_T("EasingMidpoints"));
}

// ----- DuiDoubleAnim Tick semantics -----------------------------------

static Result Test_TickProgress()
{
    double v = 0;
    auto setter = [&](double x)
    {
        v = x;
    };
    DuiDoubleAnim a(1000, 0.0, 100.0, setter);
    EXPECT_TRUE(!a.IsRunning(), _T("Tick/preRun"));

    a.TickWithElapsed(0);     // t=0 -> v=0
    EXPECT_NEAR(v, 0.0, 1e-9, _T("Tick/0"));
    EXPECT_TRUE(a.IsRunning(), _T("Tick/runningAt0"));

    a.TickWithElapsed(250);   // t=0.25 -> v=25 (linear default)
    EXPECT_NEAR(v, 25.0, 1e-9, _T("Tick/.25"));

    a.TickWithElapsed(500);   // t=0.5 -> v=50
    EXPECT_NEAR(v, 50.0, 1e-9, _T("Tick/.5"));

    bool more = a.TickWithElapsed(1000);  // t=1 -> v=100, returns false
    EXPECT_TRUE(!more, _T("Tick/1retFalse"));
    EXPECT_TRUE(a.IsDone(), _T("Tick/done"));
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Tick/atEnd"));

    // Extra ticks after done are ignored.
    bool more2 = a.TickWithElapsed(2000);
    EXPECT_TRUE(!more2, _T("Tick/postDone"));
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Tick/postDoneVal"));
    return OK(_T("TickProgress"));
}

static Result Test_FromToAccessors()
{
    double v = 0;
    DuiDoubleAnim a(500, 30.0, 70.0, [&](double x)
    {
        v = x;
    });
    EXPECT_NEAR(a.From(), 30.0, 1e-9, _T("FT/from"));
    EXPECT_NEAR(a.To(),   70.0, 1e-9, _T("FT/to"));
    a.TickWithElapsed(250);
    EXPECT_NEAR(a.Current(), 50.0, 1e-9, _T("FT/cur"));
    EXPECT_NEAR(v, 50.0, 1e-9, _T("FT/setter"));
    return OK(_T("FromToAccessors"));
}

// EaseInQuad applied to a tween clamps endpoints exactly.
static Result Test_EasingApplied()
{
    double v = 0;
    DuiDoubleAnim a(100, 0.0, 10.0, [&](double x)
    {
        v = x;
    });
    a.SetEasing(&DuiEase::EaseInQuad);
    a.TickWithElapsed(50);    // t=0.5 -> ease=0.25 -> v=2.5
    EXPECT_NEAR(v, 2.5, 1e-9, _T("Eased/mid"));
    a.TickWithElapsed(100);   // t=1 -> v=10
    EXPECT_NEAR(v, 10.0, 1e-9, _T("Eased/end"));
    return OK(_T("EasingApplied"));
}

// OnComplete fires exactly once on the tick that crossed t=1.
static Result Test_OnCompleteFires()
{
    int hits = 0;
    DuiDoubleAnim a(100, 0.0, 1.0, [](double){});
    a.SetOnComplete([&]()
    {
        ++hits;
    });
    a.TickWithElapsed(50);
    EXPECT_INT(hits, 0, _T("Cmp/midNotYet"));
    a.TickWithElapsed(100);
    EXPECT_INT(hits, 1, _T("Cmp/atEnd"));
    a.TickWithElapsed(200);
    EXPECT_INT(hits, 1, _T("Cmp/onceOnly"));
    return OK(_T("OnCompleteFires"));
}

// Finish() jumps to t=1 + fires OnComplete + can be called only once.
static Result Test_FinishJumpsToEnd()
{
    double v = 0;
    int hits = 0;
    DuiDoubleAnim a(1000, 0.0, 100.0, [&](double x)
    {
        v = x;
    });
    a.SetOnComplete([&]()
    {
        ++hits;
    });
    a.Finish();
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Fin/atEnd"));
    EXPECT_INT(hits, 1, _T("Fin/cb"));
    a.Finish();   // idempotent
    EXPECT_INT(hits, 1, _T("Fin/idem"));
    return OK(_T("FinishJumpsToEnd"));
}

// Duration <= 0 clamps to 1ms (avoid divide-by-zero).
static Result Test_DurationClamp()
{
    double v = -1;
    DuiDoubleAnim a(0, 0.0, 5.0, [&](double x)
    {
        v = x;
    });
    EXPECT_INT(a.GetDurationMs(), 1, _T("Dur/clamp"));
    a.TickWithElapsed(1);
    EXPECT_TRUE(a.IsDone(), _T("Dur/instantDone"));
    EXPECT_NEAR(v, 5.0, 1e-9, _T("Dur/atEnd"));
    return OK(_T("DurationClamp"));
}

// ----- DuiAnimMgr -----------------------------------------------------

static Result Test_MgrAddAndTick()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    int finished = 0;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    a->SetOnComplete([&]()
    {
        ++finished;
    });
    DuiAnim* raw = a.get();
    m.Add(std::move(a));

    EXPECT_INT(m.GetActiveCount(), 1, _T("Mgr/added"));

    // First tick records start time at nowMs=1000. Anim still running.
    m.TickAll(1000);
    EXPECT_INT(m.GetActiveCount(), 1, _T("Mgr/midRun"));

    // Tick at 1100: elapsed=100 -> done -> mgr removes it.
    m.TickAll(1100);
    EXPECT_INT(m.GetActiveCount(), 0, _T("Mgr/cleanedUp"));
    EXPECT_INT(finished, 1, _T("Mgr/cb"));
    (void)raw;
    return OK(_T("MgrAddAndTick"));
}

// Mgr::Clear cancels without firing OnComplete.
static Result Test_MgrClearCancels()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    int finished = 0;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    a->SetOnComplete([&]()
    {
        ++finished;
    });
    m.Add(std::move(a));

    m.Clear();
    EXPECT_INT(m.GetActiveCount(), 0, _T("MgrClr/empty"));
    EXPECT_INT(finished, 0, _T("MgrClr/noFire"));
    return OK(_T("MgrClearCancels"));
}

// ----- DuiAnimMgr self-driving pulse timer -----------------------------

// Adding the first anim installs the shared pulse timer; Clear drops it.
static Result Test_MgrSelfDriveOnAddOffOnClear()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    EXPECT_TRUE(!m.IsSelfDriving(), _T("Drive/idleAfterClear"));
    EXPECT_TRUE(m.GetPulseTimerId() == 0, _T("Drive/noIdWhenIdle"));

    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    m.Add(std::move(a));
    EXPECT_TRUE(m.IsSelfDriving(), _T("Drive/onAfterAdd"));
    EXPECT_TRUE(m.GetPulseTimerId() != 0, _T("Drive/idAfterAdd"));

    m.Clear();
    EXPECT_TRUE(!m.IsSelfDriving(), _T("Drive/offAfterClear"));
    EXPECT_TRUE(m.GetPulseTimerId() == 0, _T("Drive/idClearedAfterClear"));
    return OK(_T("MgrSelfDriveOnAddOffOnClear"));
}

// The pulse timer is dropped as soon as the last anim finishes, so an
// idle process never carries a timer around.
static Result Test_MgrSelfDriveStopsWhenAllDone()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    m.Add(std::move(a));
    EXPECT_TRUE(m.IsSelfDriving(), _T("DriveStop/onWhileRunning"));

    m.TickAll(5000);          // records start time, still running
    EXPECT_INT(m.GetActiveCount(), 1, _T("DriveStop/stillActive"));
    EXPECT_TRUE(m.IsSelfDriving(), _T("DriveStop/stillDriving"));

    m.TickAll(5100);          // elapsed=100 -> done -> list empties
    EXPECT_INT(m.GetActiveCount(), 0, _T("DriveStop/emptied"));
    EXPECT_TRUE(!m.IsSelfDriving(), _T("DriveStop/off"));
    EXPECT_TRUE(m.GetPulseTimerId() == 0, _T("DriveStop/idCleared"));
    return OK(_T("MgrSelfDriveStopsWhenAllDone"));
}

// Several anims share one timer: the id must not change as more are added.
// ::SetTimer hands out a fresh id every call, so an unchanged id proves no
// second timer was installed.
static Result Test_MgrOneTimerForManyAnims()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    m.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){})));
    UINT_PTR firstId = m.GetPulseTimerId();
    EXPECT_TRUE(firstId != 0, _T("OneTimer/firstId"));

    m.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        200, 0.0, 1.0, [](double){})));
    m.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        300, 0.0, 1.0, [](double){})));
    EXPECT_INT(m.GetActiveCount(), 3, _T("OneTimer/count"));
    EXPECT_TRUE(m.GetPulseTimerId() == firstId, _T("OneTimer/sameId"));

    // A tick that leaves work behind must not restart the timer either.
    m.TickAll(6000);
    EXPECT_TRUE(m.GetPulseTimerId() == firstId, _T("OneTimer/sameIdAfterTick"));

    m.Clear();
    return OK(_T("MgrOneTimerForManyAnims"));
}

// Hosts that already run their own pulse keep calling TickAll while the
// manager's timer ticks too, so the same frame gets ticked twice. Progress
// is computed from absolute time, so a repeated nowMs must be a no-op.
static Result Test_MgrTickAllRepeatIsHarmless()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    double v = -1.0;
    int finished = 0;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        1000, 0.0, 100.0, [&](double x)
        {
            v = x;
        }));
    a->SetOnComplete([&]()
    {
        ++finished;
    });
    m.Add(std::move(a));

    m.TickAll(2000);
    m.TickAll(2000);          // same frame again
    EXPECT_NEAR(v, 0.0, 1e-9, _T("Repeat/atStart"));
    EXPECT_INT(m.GetActiveCount(), 1, _T("Repeat/stillOne"));

    m.TickAll(2500);
    EXPECT_NEAR(v, 50.0, 1e-9, _T("Repeat/half"));
    m.TickAll(2500);
    EXPECT_NEAR(v, 50.0, 1e-9, _T("Repeat/halfAgain"));

    m.TickAll(3000);
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Repeat/end"));
    EXPECT_INT(finished, 1, _T("Repeat/cbOnce"));
    EXPECT_INT(m.GetActiveCount(), 0, _T("Repeat/removed"));

    // Ticking an empty manager is legal and fires nothing.
    m.TickAll(3000);
    m.TickAll(4000);
    EXPECT_INT(finished, 1, _T("Repeat/cbStillOnce"));
    EXPECT_TRUE(!m.IsSelfDriving(), _T("Repeat/idle"));
    return OK(_T("MgrTickAllRepeatIsHarmless"));
}

// A completion callback that queues a follow-up anim (DuiToast chains
// fade-in -> hold -> fade-out exactly this way) must not corrupt the walk,
// and the queued anim must be advanced by the following ticks.
static Result Test_MgrAddFromCompleteCallback()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    double chainedValue = -1.0;
    int chainStarts = 0;
    auto first = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    first->SetOnComplete([&]()
    {
        ++chainStarts;
        auto second = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
            100, 0.0, 10.0, [&](double x)
            {
                chainedValue = x;
            }));
        DuiAnimMgr::Inst().Add(std::move(second));
    });
    m.Add(std::move(first));

    m.TickAll(7000);          // first records its start time
    EXPECT_INT(m.GetActiveCount(), 1, _T("Chain/onlyFirst"));

    m.TickAll(7100);          // first completes and queues the second
    EXPECT_INT(chainStarts, 1, _T("Chain/cbFired"));
    EXPECT_INT(m.GetActiveCount(), 1, _T("Chain/secondMerged"));
    EXPECT_TRUE(m.IsSelfDriving(), _T("Chain/stillDriving"));

    m.TickAll(7150);          // second records its start time -> t=0
    EXPECT_NEAR(chainedValue, 0.0, 1e-9, _T("Chain/secondStart"));
    m.TickAll(7200);          // t=0.5
    EXPECT_NEAR(chainedValue, 5.0, 1e-9, _T("Chain/secondHalf"));
    m.TickAll(7250);          // t=1 -> done
    EXPECT_NEAR(chainedValue, 10.0, 1e-9, _T("Chain/secondEnd"));

    EXPECT_INT(m.GetActiveCount(), 0, _T("Chain/allDone"));
    EXPECT_TRUE(!m.IsSelfDriving(), _T("Chain/idleAtEnd"));
    return OK(_T("MgrAddFromCompleteCallback"));
}

// Clear() called from inside a completion callback is deferred to the end
// of the walk: everything is dropped, including anims the same callback
// queued, and the pulse timer goes away.
static Result Test_MgrClearFromCompleteCallback()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    // A long-lived companion so the deferred Clear has something to drop.
    m.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        10000, 0.0, 1.0, [](double){})));

    auto trigger = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    trigger->SetOnComplete([]()
    {
        DuiAnimMgr& mgr = DuiAnimMgr::Inst();
        mgr.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
            100, 0.0, 1.0, [](double){})));
        mgr.Clear();
    });
    m.Add(std::move(trigger));

    m.TickAll(8000);
    EXPECT_INT(m.GetActiveCount(), 2, _T("ClrCb/bothRunning"));

    m.TickAll(8100);          // trigger completes -> deferred Clear
    EXPECT_INT(m.GetActiveCount(), 0, _T("ClrCb/allDropped"));
    EXPECT_TRUE(!m.IsSelfDriving(), _T("ClrCb/idle"));
    EXPECT_TRUE(m.GetPulseTimerId() == 0, _T("ClrCb/idCleared"));
    return OK(_T("MgrClearFromCompleteCallback"));
}

// The other ordering: a callback that clears first and then queues a
// replacement. The replacement belongs to a fresh round and must survive
// the deferred cancel - that is how "hard cancel, then restart" reads.
static Result Test_MgrClearThenAddFromCallback()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    double restarted = -1.0;
    auto trigger = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    trigger->SetOnComplete([&]()
    {
        DuiAnimMgr& mgr = DuiAnimMgr::Inst();
        mgr.Clear();
        mgr.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
            100, 0.0, 20.0, [&](double x)
            {
                restarted = x;
            })));
    });
    m.Add(std::move(trigger));

    m.TickAll(8500);
    m.TickAll(8600);          // trigger completes -> Clear then Add
    EXPECT_INT(m.GetActiveCount(), 1, _T("ClrAdd/replacementKept"));
    EXPECT_TRUE(m.IsSelfDriving(), _T("ClrAdd/stillDriving"));

    m.TickAll(8650);          // replacement records its start time
    EXPECT_NEAR(restarted, 0.0, 1e-9, _T("ClrAdd/start"));
    m.TickAll(8750);          // t=1 -> done
    EXPECT_NEAR(restarted, 20.0, 1e-9, _T("ClrAdd/end"));
    EXPECT_INT(m.GetActiveCount(), 0, _T("ClrAdd/allDone"));
    EXPECT_TRUE(!m.IsSelfDriving(), _T("ClrAdd/idleAtEnd"));
    return OK(_T("MgrClearThenAddFromCallback"));
}

// A nested TickAll (from a setter or a completion callback) is ignored.
// Without the guard the setter below would recurse until the stack blows.
static Result Test_MgrNestedTickAllIgnored()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    int setterCalls = 0;
    m.Add(std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        1000, 0.0, 100.0, [&](double)
        {
            ++setterCalls;
            DuiAnimMgr::Inst().TickAll(4000);   // re-entrant, must no-op
        })));

    m.TickAll(3000);
    EXPECT_INT(setterCalls, 1, _T("Nested/onceOnly"));
    EXPECT_INT(m.GetActiveCount(), 1, _T("Nested/stillActive"));

    m.Clear();
    EXPECT_INT(m.GetActiveCount(), 0, _T("Nested/cleanedUp"));
    return OK(_T("MgrNestedTickAllIgnored"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_NEAR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("EasingEndpoints"), &Test_EasingEndpoints },
        { _T("EasingMidpoints"), &Test_EasingMidpoints },
        { _T("TickProgress"),    &Test_TickProgress    },
        { _T("FromToAccessors"), &Test_FromToAccessors },
        { _T("EasingApplied"),   &Test_EasingApplied   },
        { _T("OnCompleteFires"), &Test_OnCompleteFires },
        { _T("FinishJumpsToEnd"),&Test_FinishJumpsToEnd},
        { _T("DurationClamp"),   &Test_DurationClamp   },
        { _T("MgrAddAndTick"),   &Test_MgrAddAndTick   },
        { _T("MgrClearCancels"), &Test_MgrClearCancels },
        { _T("MgrSelfDriveOnAddOffOnClear"),   &Test_MgrSelfDriveOnAddOffOnClear   },
        { _T("MgrSelfDriveStopsWhenAllDone"),  &Test_MgrSelfDriveStopsWhenAllDone  },
        { _T("MgrOneTimerForManyAnims"),       &Test_MgrOneTimerForManyAnims       },
        { _T("MgrTickAllRepeatIsHarmless"),    &Test_MgrTickAllRepeatIsHarmless    },
        { _T("MgrAddFromCompleteCallback"),    &Test_MgrAddFromCompleteCallback    },
        { _T("MgrClearFromCompleteCallback"),  &Test_MgrClearFromCompleteCallback  },
        { _T("MgrClearThenAddFromCallback"),   &Test_MgrClearThenAddFromCallback   },
        { _T("MgrNestedTickAllIgnored"),       &Test_MgrNestedTickAllIgnored       },
    };

    CString out;
    int passed = 0;
    int failed = 0;
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
    summary.Format(_T("[summary] DuiAnimationTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiAnimationTests

} // namespace balloonwjui
