#include "FingerSplayCache.h"
#include "FingerSplayCoherence.h"
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogMMVRRawSplayTests,Log,All);
namespace MMVRFingerSplay
{
bool RunSelfTests()
{
    int32 Checks=0,Failures=0;
    auto Check=[&](bool Passed,const TCHAR* Name) { ++Checks; if (!Passed) { ++Failures; UE_LOG(LogMMVRRawSplayTests,Error,TEXT("RAW FAIL: %s"),Name); } };
    constexpr int64 Hz=1000000;
    const FString A=TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),B=TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    const FIPv4Endpoint Local(FIPv4Address(127,0,0,1),12345);
    auto Message=[&](ESide Side,int64 Seq,int64 At,bool Valid=true,const FString& Session=FString(),int64 Epoch=100000)
    {
        TArray<UE::OSC::FOSCData> Args;
        Args.Emplace(Session.IsEmpty() ? A : Session); Args.Emplace(Epoch); Args.Emplace(Seq); Args.Emplace(At); Args.Emplace(Hz);
        Args.Emplace(int32(Valid)); Args.Emplace(Valid ? int32(1) : int32(-1));
        for (int32 I=0; I<4; ++I) Args.Emplace(Valid ? float(I+1+SideIndex(Side)*4)*.11f : 0.f);
        return FOSCMessage(FOSCAddress(Side==ESide::Right ? RightAddress : Address),MoveTemp(Args));
    };
    FCache Cache(Hz); MMVRFinger::FCache Curls(Hz);
    // Exercise the unchanged legacy format in the presence of the additive format.
    auto Legacy=Message(ESide::Left,1,200000); auto Args=Legacy.GetArgumentsChecked(); Args.Emplace(.5f);
    Check(Curls.Receive(FOSCMessage(FOSCAddress(MMVRFinger::Address),Args),Local,200010,250),TEXT("V1.2 curl payload remains accepted"));
    for (ESide Side:{ESide::Left,ESide::Right})
    {
        const auto Base=Message(Side,1,200000);
        Check(Cache.Receive(Base,Local,200010,250),TEXT("RAW valid per hand"));
        const auto S=Cache.Snapshot(200010,250,Side);
        for (int32 I=0; I<4; ++I) Check(S.Sample.Splays[I]==float(I+1+SideIndex(Side)*4)*.11f,TEXT("RAW four channels exact order"));
        Check(!Cache.Receive(Base,Local,200020,250),TEXT("old/duplicate sequence independent"));
        for (int32 I=0; I<4; ++I) for (float Bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        {
            auto BadArgs=Base.GetArgumentsChecked(); BadArgs[I+7]=UE::OSC::FOSCData(Bad);
            Check(!Cache.Receive(FOSCMessage(Base.GetAddress(),BadArgs),Local,200020,250),TEXT("NaN/Inf each RAW channel and hand"));
        }
        Check(!Cache.Receive(Message(Side,2,200000),Local,450000,250),TEXT("stale RAW sample rejected"));
        Check(!Cache.Receive(Message(Side,2,200030),Local,200020,250),TEXT("future RAW rejected"));
        Check(!Cache.Receive(FOSCMessage(FOSCAddress(TEXT("/mmvr/fingers/v2/splay/left")),Base.GetArgumentsChecked()),Local,200010,250),TEXT("wrong version rejected"));
        Check(!Cache.Receive(FOSCMessage(FOSCAddress(TEXT("/mmvr/fingers/v1/splay/both")),Base.GetArgumentsChecked()),Local,200010,250),TEXT("ambiguous side rejected"));
        Check(!Cache.Receive(FOSCMessage(FOSCAddress(MMVRFinger::Address),Base.GetArgumentsChecked()),Local,200010,250),TEXT("old curls cannot be RAW"));
        Check(!Cache.Receive(FOSCMessage(Base.GetAddress(),Args),Local,200010,250),TEXT("extended curl format cannot be RAW"));
        auto BadArgs=Base.GetArgumentsChecked(); BadArgs[7]=UE::OSC::FOSCData(int32(1));
        Check(!Cache.Receive(FOSCMessage(Base.GetAddress(),BadArgs),Local,200010,250),TEXT("wrong value type"));
        Check(!Cache.Receive(Base,FIPv4Endpoint(FIPv4Address(192,168,1,1),1),200010,250),TEXT("RAW non-loopback rejected"));
    }
    Check(Curls.Snapshot(200020,250).Sample.Valid && Curls.Snapshot(200020,250).Sample.Sequence==1,TEXT("RAW errors never alter curls"));
    Check(Cache.Receive(Message(ESide::Left,2,210000,false),Local,210010,250),TEXT("LEFT explicit RAW invalidation"));
    Check(!Cache.Snapshot(210010,250,ESide::Left).Sample.Valid && Cache.Snapshot(210010,250,ESide::Right).Sample.Valid,TEXT("LEFT loss RIGHT intact"));
    Check(Cache.Receive(Message(ESide::Left,3,220000),Local,220010,250),TEXT("LEFT reconnect same session"));
    Check(Cache.Receive(Message(ESide::Right,2,230000,false),Local,230010,250),TEXT("RIGHT explicit RAW invalidation"));
    Check(Cache.Snapshot(230010,250,ESide::Left).Sample.Valid && !Cache.Snapshot(230010,250,ESide::Right).Sample.Valid,TEXT("RIGHT loss LEFT intact"));
    Check(Cache.Receive(Message(ESide::Right,3,400000),Local,400010,250),TEXT("RIGHT refresh"));
    Check(!Cache.Snapshot(480000,250,ESide::Left).Sample.Valid && Cache.Snapshot(480000,250,ESide::Right).Sample.Valid,TEXT("LEFT timeout only"));
    Check(Cache.Receive(Message(ESide::Left,4,600000),Local,600010,250),TEXT("LEFT refresh after timeout"));
    Check(Cache.Snapshot(650000,250,ESide::Left).Sample.Valid && !Cache.Snapshot(650000,250,ESide::Right).Sample.Valid,TEXT("RIGHT timeout only"));
    Check(!Cache.Snapshot(900000,250,ESide::Left).Sample.Valid && !Cache.Snapshot(900000,250,ESide::Right).Sample.Valid,TEXT("both RAW timeout"));
    Check(Cache.Snapshot(900000,250,ESide::Left).Sample.Splays[0]==0,TEXT("expired RAW values cleared"));
    Check(Cache.Receive(Message(ESide::Left,1,920000,true,B,910000),Local,920010,250),TEXT("new RAW producer session"));
    Check(!Cache.Receive(Message(ESide::Right,999,930000),Local,930010,250),TEXT("retired RIGHT epoch blocked after new LEFT"));
    Check(Cache.Receive(Message(ESide::Right,1,940000,true,B,910000),Local,940010,250),TEXT("RIGHT joins new session"));
    Check(Cache.Snapshot(940010,250,ESide::Left).Sample.Sequence==1,TEXT("RIGHT session does not reset LEFT"));
    auto Range=Message(ESide::Left,2,950000,true,B,910000); auto RArgs=Range.GetArgumentsChecked(); RArgs[7]=UE::OSC::FOSCData(-.2f);
    Check(Cache.Receive(FOSCMessage(Range.GetAddress(),RArgs),Local,950010,250),TEXT("RAW finite out-of-range preserved"));
    Check(Cache.Snapshot(950010,250).Sample.Splays[0]==-.2f && Cache.Snapshot(950010,250).Sample.OutsideRange,TEXT("no arbitrary clamp/remap"));
    UE_LOG(LogMMVRRawSplayTests,Display,TEXT("V1.3 RAW SELFTEST %s checks=%d failures=%d"),Failures ? TEXT("FAIL") : TEXT("PASS"),Checks,Failures);
    return RunCoherenceSelfTests() && Failures==0;
}
}
