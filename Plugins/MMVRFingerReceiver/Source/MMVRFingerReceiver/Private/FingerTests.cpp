#include "FingerCache.h"
#include "FingerSplayCache.h"
#include "MMVRFingerSubsystem.h"
#include "OSCServer.h"
#include "OSCStream.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogMMVRFingerTests,Log,All);
namespace MMVRFinger
{
static FOSCMessage Message(const FString& Session,int64 Epoch,int64 Seq,int64 Acquisition,int64 Hz,bool Valid=true,ESide Side=ESide::Left)
{
    TArray<UE::OSC::FOSCData> A;
    A.Emplace(Session); A.Emplace(Epoch); A.Emplace(Seq); A.Emplace(Acquisition); A.Emplace(Hz);
    A.Emplace(static_cast<int32>(Valid)); A.Emplace(Valid ? 1 : -1);
    for (int32 I=0; I<5; ++I) A.Emplace(Valid ? (I+1+(Side==ESide::Right ? 5 : 0))*0.1f : 0.f);
    return FOSCMessage(FOSCAddress(Side==ESide::Right ? RightAddress : Address),MoveTemp(A));
}
static FOSCMessage Change(const FOSCMessage& M,int32 Index,UE::OSC::FOSCData Value)
{
    auto A=M.GetArgumentsChecked(); A[Index]=MoveTemp(Value); return FOSCMessage(M.GetAddress(),MoveTemp(A));
}
bool RunSelfTests()
{
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.World() && Context.World()->IsGameWorld())
        { UE_LOG(LogMMVRFingerTests,Error,TEXT("Run self-tests without PIE/game instances; existing consumers are preserved.")); return false; }
    int32 Checks=0,Failures=0;
    auto Check=[&](bool Passed,const TCHAR* Name)
    { ++Checks; if (!Passed) { ++Failures; UE_LOG(LogMMVRFingerTests,Error,TEXT("FAIL: %s"),Name); } };
    const FIPv4Endpoint Local(FIPv4Address(127,0,0,1),12345);
    const FString A=TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),B=TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    constexpr int64 Hz=1000000;
    FCache Cache(Hz);
    const auto Base=Message(A,100000,1,200000,Hz);
    Check(Cache.Receive(Base,Local,200010,250),TEXT("valid atomic left sample"));
    Check(Cache.Snapshot(200010,250).Sample.Curls[4]==0.5f,TEXT("five channels preserved"));
    Check(!Cache.Receive(Base,Local,200020,250),TEXT("duplicate sequence"));
    Check(!Cache.Receive(Message(A,100000,0,200000,Hz),Local,200030,250),TEXT("non-positive sequence"));
    Check(!Cache.Receive(Message(A,100000,2,199999,Hz),Local,200040,250),TEXT("acquisition reversal"));
    Check(!Cache.Receive(Base,FIPv4Endpoint(FIPv4Address(192,168,1,2),123),200010,250),TEXT("non-loopback source"));
    auto Arguments=Base.GetArgumentsChecked(); Arguments.Pop();
    Check(!Cache.Receive(FOSCMessage(FOSCAddress(Address),Arguments),Local,200010,250),TEXT("wrong count"));
    Check(!Cache.Receive(FOSCMessage(FOSCAddress(TEXT("/mmvr/fingers/v2/left")),Base.GetArgumentsChecked()),Local,200010,250),TEXT("wrong version/address"));
    Check(!Cache.Receive(Change(Base,7,UE::OSC::FOSCData(int32(1))),Local,200010,250),TEXT("wrong type"));
    Check(!Cache.Receive(Change(Base,7,UE::OSC::FOSCData(std::numeric_limits<float>::quiet_NaN())),Local,200010,250),TEXT("NaN"));
    Check(!Cache.Receive(Change(Base,7,UE::OSC::FOSCData(std::numeric_limits<float>::infinity())),Local,200010,250),TEXT("Inf"));
    Check(!Cache.Receive(Change(Base,5,UE::OSC::FOSCData(int32(2))),Local,200010,250),TEXT("invalid validity flag"));
    Check(!Cache.Receive(Change(Base,6,UE::OSC::FOSCData(int32(-1))),Local,200010,250),TEXT("valid sample requires tracking level"));
    Check(!Cache.Receive(Change(Base,0,UE::OSC::FOSCData(FString(TEXT("bad")))),Local,200010,250),TEXT("bad session format"));
    Check(!Cache.Receive(Change(Base,4,UE::OSC::FOSCData(int64(123))),Local,200010,250),TEXT("clock frequency mismatch"));
    Check(!Cache.Receive(Message(A,100000,2,200012,Hz),Local,200010,250),TEXT("future timestamp"));
    Check(!Cache.Receive(Message(A,100000,2,200000,Hz),Local,500000,250),TEXT("old timestamp on newly delivered message"));
    Check(Cache.Snapshot(500000,250).Sample.Valid==false,TEXT("250 ms timeout"));
    Check(Cache.Snapshot(600000,250).Invalidations==1,TEXT("timeout transition counted once"));
    auto Range=Message(A,100000,2,600000,Hz); Range=Change(Range,7,UE::OSC::FOSCData(1.2f));
    Check(Cache.Receive(Range,Local,600010,250),TEXT("range warning does not clamp/reject finite value"));
    Check(Cache.Snapshot(600010,250).Sample.Curls[0]==1.2f && Cache.Snapshot(600010,250).Sample.OutsideRange,TEXT("raw range preserved"));
    Check(Cache.Receive(Message(A,100000,3,610000,Hz,false),Local,610010,250),TEXT("explicit invalidation"));
    Check(!Cache.Snapshot(610010,250).Sample.Valid,TEXT("no cached valid curls after invalid"));
    Check(Cache.Receive(Message(A,100000,4,620000,Hz),Local,620010,250),TEXT("same session recovery"));
    Check(!Cache.Receive(Message(A,110000,5,630000,Hz),Local,630010,250),TEXT("epoch changed inside same UUID"));
    Check(Cache.Receive(Message(B,700000,1,710000,Hz),Local,710010,250),TEXT("new session starts at seq 1"));
    Check(!Cache.Receive(Message(A,100000,999,720000,Hz),Local,720010,250),TEXT("retired session cannot return"));
    Check(!Cache.Receive(Message(TEXT("cccccccccccccccccccccccccccccccc"),700000,1,730000,Hz),Local,730010,250),TEXT("ambiguous equal session epoch"));
    Check(Cache.Receive(Message(B,700000,MAX_int64,740000,Hz),Local,740010,250),TEXT("sequence maximum accepted"));
    Check(!Cache.Receive(Message(B,700000,1,750000,Hz),Local,750010,250),TEXT("sequence never wraps in same session"));
    Check(Cache.Receive(Message(TEXT("cccccccccccccccccccccccccccccccc"),760000,1,770000,Hz),Local,770010,250),TEXT("new epoch can reset sequence"));

    // Dual-hand loss/recovery and order: deliberately distinct values and sequence progress.
    FCache Dual(Hz);
    auto L=[&](int64 Seq,int64 At,bool Valid=true) { return Message(A,100000,Seq,At,Hz,Valid,ESide::Left); };
    auto R=[&](int64 Seq,int64 At,bool Valid=true) { return Message(A,100000,Seq,At,Hz,Valid,ESide::Right); };
    auto Left=[&](int64 Now) { return Dual.Snapshot(Now,250,ESide::Left); };
    auto Right=[&](int64 Now) { return Dual.Snapshot(Now,250,ESide::Right); };
    Check(Dual.Receive(L(1,200000),Local,200010,250),TEXT("dual LEFT initial"));
    Check(!Right(200010).Sample.Valid && Right(200010).Sample.Sequence==0,TEXT("LEFT cannot populate RIGHT"));
    Check(Dual.Receive(R(1,200000),Local,200010,250),TEXT("dual RIGHT same seq is independent"));
    Check(Left(200010).Sample.Side==ESide::Left && Right(200010).Sample.Side==ESide::Right,TEXT("side derived unambiguously from exact address"));
    for (int32 I=0; I<5; ++I)
        Check(FMath::IsNearlyEqual(Left(200010).Sample.Curls[I],(I+1)*0.1f) && FMath::IsNearlyEqual(Right(200010).Sample.Curls[I],(I+6)*0.1f),TEXT("each channel stays on its own hand"));
    Check(Dual.Receive(R(7,210000),Local,210010,250) && Dual.Receive(L(2,220000),Local,220010,250),TEXT("alternation with unrelated sequence progress"));
    Check(!Dual.Receive(L(1,230000),Local,230010,250),TEXT("old LEFT sequence rejected"));
    Check(!Dual.Receive(R(6,230000),Local,230010,250),TEXT("old RIGHT sequence rejected"));
    Check(Left(230010).Sample.Sequence==2 && Right(230010).Sample.Sequence==7,TEXT("old sequences do not contaminate either hand"));
    for (ESide Side : {ESide::Left,ESide::Right})
    {
        auto M=Message(A,100000,99,230000,Hz,true,Side);
        Check(!Dual.Receive(Change(M,7,UE::OSC::FOSCData(std::numeric_limits<float>::quiet_NaN())),Local,230010,250),TEXT("NaN rejected per hand"));
        Check(!Dual.Receive(Change(M,11,UE::OSC::FOSCData(std::numeric_limits<float>::infinity())),Local,230010,250),TEXT("Inf rejected per hand"));
        Check(!Dual.Receive(Message(A,100000,99,230012,Hz,true,Side),Local,230010,250),TEXT("future acquisition rejected per hand"));
        Check(!Dual.Receive(Change(Message(A,100000,99,230000,Hz,false,Side),7,UE::OSC::FOSCData(0.2f)),Local,230010,250),TEXT("invalid sample with nonzero curls rejected per hand"));
    }
    for (const TCHAR* Bad : {TEXT("/wrong"),TEXT("/mmvr/fingers/v2/right"),TEXT("/mmvr/fingers/v1/both"),TEXT("/mmvr/fingers/v1/RIGHT")})
        Check(!Dual.Receive(FOSCMessage(FOSCAddress(Bad),R(99,230000).GetArgumentsChecked()),Local,230010,250),TEXT("wrong address/version/side rejected"));
    Check(Left(230010).Sample.Sequence==2 && Right(230010).Sample.Sequence==7 && Left(230010).Sample.Valid && Right(230010).Sample.Valid,TEXT("malformed messages do not refresh or invalidate either slot"));
    Check(Dual.Receive(L(3,240000,false),Local,240010,250),TEXT("LEFT OFF accepted"));
    Check(!Left(240010).Sample.Valid && Left(240010).Sample.Curls[4]==0 && Right(240010).Sample.Valid && Right(240010).Sample.Sequence==7,TEXT("LEFT OFF leaves RIGHT intact"));
    Check(Dual.Receive(L(4,250000),Local,250010,250),TEXT("LEFT ON accepted"));
    Check(Left(250010).Sample.Valid && Right(250010).Sample.Sequence==7 && Right(250010).Sample.Acquisition==210000,TEXT("LEFT reconnect does not reset RIGHT"));
    Check(Dual.Receive(R(8,260000,false),Local,260010,250),TEXT("RIGHT OFF accepted"));
    Check(!Right(260010).Sample.Valid && Right(260010).Sample.Curls[4]==0 && Left(260010).Sample.Valid && Left(260010).Sample.Sequence==4,TEXT("RIGHT OFF leaves LEFT intact"));
    Check(Dual.Receive(R(9,270000),Local,270010,250),TEXT("RIGHT ON accepted"));
    Check(Right(270010).Sample.Valid && Left(270010).Sample.Sequence==4 && Left(270010).Sample.Acquisition==250000,TEXT("RIGHT reconnect does not reset LEFT"));
    Check(Dual.Receive(R(10,490000),Local,490010,250),TEXT("RIGHT continues while LEFT silent"));
    Check(!Left(500000).Sample.Valid && Left(500000).Sample.Curls[0]==0 && Right(500000).Sample.Valid && FMath::IsNearlyEqual(Right(500000).AgeMs,10.0),TEXT("LEFT-only timeout at boundary; RIGHT acquisition age independent"));
    Check(Dual.Receive(L(5,730000),Local,730010,250),TEXT("LEFT continues while RIGHT silent"));
    Check(Left(740000).Sample.Valid && !Right(740000).Sample.Valid && Right(740000).Sample.Curls[4]==0,TEXT("RIGHT-only timeout at boundary"));
    Check(!Left(1000000).Sample.Valid && !Right(1000000).Sample.Valid,TEXT("both timeout when both silent"));
    Check(Dual.Receive(L(6,1010000),Local,1010010,250) && Dual.Receive(R(11,1010000),Local,1010010,250),TEXT("both reconnect independently"));
    Check(Dual.Receive(L(7,1020000,false),Local,1020010,250) && Dual.Receive(R(12,1020000,false),Local,1020010,250),TEXT("both explicit OFF"));
    Check(!Left(1020010).Sample.Valid && !Right(1020010).Sample.Valid,TEXT("both OFF carry zero invalid data"));
    Check(Dual.Receive(L(8,1030000),Local,1030010,250) && !Right(1030010).Sample.Valid,TEXT("only LEFT ON cannot revive RIGHT"));
    Check(Dual.Receive(R(13,1030000),Local,1030010,250),TEXT("RIGHT separately ON"));
    Check(Dual.Receive(Message(B,1040000,1,1050000,Hz,true,ESide::Right),Local,1050010,250),TEXT("new producer may arrive RIGHT first"));
    Check(Left(1050010).Sample.Sequence==8 && Left(1050010).Sample.Acquisition==1030000,TEXT("session change on RIGHT does not rewrite LEFT acquisition"));
    Check(!Dual.Receive(L(99,1060000),Local,1060010,250),TEXT("retired LEFT rejected even before its new-session message"));
    Check(!Dual.Receive(Message(B,1040001,1,1060000,Hz,true,ESide::Left),Local,1060010,250),TEXT("common UUID must keep same epoch on both hands"));
    Check(Dual.Receive(Message(B,1040000,1,1070000,Hz,true,ESide::Left),Local,1070010,250),TEXT("new producer LEFT restarts sequence independently"));
    Check(Left(1070010).Sample.Session==B && Right(1070010).Sample.Session==B && Right(1070010).Sample.Sequence==1,TEXT("both hands converge on common producer session"));
    Check(Dual.Receive(Message(A,1080000,1,1090000,Hz,true,ESide::Left),Local,1090010,250),TEXT("later session may arrive LEFT first"));
    Check(!Dual.Receive(Message(B,1040000,99,1100000,Hz,true,ESide::Right),Local,1100010,250),TEXT("retired RIGHT fenced after LEFT session change"));
    Check(Dual.Receive(Message(A,1080000,1,1110000,Hz,true,ESide::Right),Local,1110010,250),TEXT("RIGHT accepts later common session"));

    // Real Epic OSC socket/decoder + latest-only worker. No scene, actor or animation writer.
    const int32 TestPort=24891;
    auto State=MakeShared<FCache,ESPMode::ThreadSafe>(QpcFrequency());
    auto Server=UE::OSC::IServerProxy::Create();
    Server->SetIPEndpoint(FIPv4Endpoint(FIPv4Address::Any,TestPort));
    Server->SetOnDispatchPacket(MakeShared<UE::OSC::IServerProxy::FOnDispatchPacket>([State](TSharedRef<UE::OSC::IPacket> Packet)
    { const int64 Before=QpcNow(); if (Packet->IsMessage()) State->Receive(FOSCMessage(Packet),Packet->GetIPEndpoint(),Before,250); else State->Reject(TEXT("bundle"));
      State->RecordCost(1000000.0*static_cast<double>(QpcNow()-Before)/static_cast<double>(QpcFrequency())); }));
    Server->Listen(TEXT("MMVRFingerSelfTest"));
    const bool Listening=Server->IsActive() && Server->GetDescription()==TEXT("MMVRFingerSelfTest");
    Check(Listening,TEXT("Epic OSC wildcard socket actually bound"));
    auto* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    FSocket* Sender=Sockets->CreateSocket(NAME_DGram,TEXT("MMVRFingerSyntheticSender"),NAME_None);
    if (Sender && Listening)
    {
        Sender->SetNonBlocking(true);
        Sender->Bind(*FIPv4Endpoint(FIPv4Address(127,0,0,1),0).ToInternetAddr());
        const int64 NativeHz=QpcFrequency(),Epoch=QpcNow();
        auto Send=[&](const FOSCMessage& M)
        { UE::OSC::FStream Stream; M.GetPacketRef()->WriteData(Stream); int32 Sent=0;
          return Sender->SendTo(Stream.GetData(),Stream.GetLength(),Sent,*FIPv4Endpoint(FIPv4Address(127,0,0,1),TestPort).ToInternetAddr()) && Sent==Stream.GetLength(); };
        Check(Send(Message(A,Epoch,1,QpcNow(),NativeHz)),TEXT("synthetic UDP sent"));
        FPlatformProcess::Sleep(0.05f);
        Check(State->Snapshot(QpcNow(),250).Sample.Valid,TEXT("real Epic decoded int64/float loopback sample"));
        const int64 Old=QpcNow();
        FPlatformProcess::Sleep(0.30f); // test-only pause; not used anywhere in production capture/display
        Check(!State->Snapshot(QpcNow(),250).Sample.Valid,TEXT("real clock timeout"));
        Send(Message(A,Epoch,2,Old,NativeHz)); FPlatformProcess::Sleep(0.05f);
        const auto Stale=State->Snapshot(QpcNow(),250);
        Check(!Stale.Sample.Valid && Stale.LastReject==TEXT("stale acquisition"),TEXT("delayed datagram cannot revive stale cache"));
        Send(Message(A,Epoch,3,QpcNow(),NativeHz)); FPlatformProcess::Sleep(0.05f);
        Check(State->Snapshot(QpcNow(),250).Sample.Valid,TEXT("fresh recovery after stall"));
        Check(Send(Message(A,Epoch,1,QpcNow(),NativeHz,true,ESide::Right)),TEXT("RIGHT UDP sent at independent seq 1"));
        FPlatformProcess::Sleep(0.05f);
        Check(State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid && FMath::IsNearlyEqual(State->Snapshot(QpcNow(),250,ESide::Right).Sample.Curls[4],1.f),TEXT("real Epic RIGHT decoded with distinct channels"));
        Send(Message(A,Epoch,4,QpcNow(),NativeHz,false)); FPlatformProcess::Sleep(0.02f);
        Check(!State->Snapshot(QpcNow(),250).Sample.Valid && State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid,TEXT("real UDP LEFT OFF preserves RIGHT"));
        Send(Message(A,Epoch,5,QpcNow(),NativeHz)); Send(Message(A,Epoch,2,QpcNow(),NativeHz,false,ESide::Right)); FPlatformProcess::Sleep(0.02f);
        Check(State->Snapshot(QpcNow(),250).Sample.Valid && !State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid,TEXT("real UDP RIGHT OFF preserves LEFT"));
        Send(Message(A,Epoch,3,QpcNow(),NativeHz,true,ESide::Right)); FPlatformProcess::Sleep(0.30f);
        Send(Message(A,Epoch,4,QpcNow(),NativeHz,true,ESide::Right)); FPlatformProcess::Sleep(0.02f);
        Check(!State->Snapshot(QpcNow(),250).Sample.Valid && State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid,TEXT("real UDP LEFT-only timeout"));
        FPlatformProcess::Sleep(0.30f); Send(Message(A,Epoch,6,QpcNow(),NativeHz)); FPlatformProcess::Sleep(0.02f);
        Check(State->Snapshot(QpcNow(),250).Sample.Valid && !State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid,TEXT("real UDP RIGHT-only timeout"));
        // Validate actual native encoder if explicitly supplied; never launch hardware capture here.
        FString ProbeExe;
        if (FParse::Value(FCommandLine::Get(),TEXT("MMVRFingerProbeExe="),ProbeExe))
        {
            const FString Args=FString::Printf(TEXT("--osc-test --osc-port %d --duration 1 --hz 90"),TestPort);
            auto Process=FPlatformProcess::CreateProc(*ProbeExe,*Args,false,true,true,nullptr,0,nullptr,nullptr);
            Check(Process.IsValid(),TEXT("native synthetic OSC process launched"));
            const double Until=FPlatformTime::Seconds()+4;
            while (Process.IsValid() && FPlatformProcess::IsProcRunning(Process) && FPlatformTime::Seconds()<Until)
                FPlatformProcess::Sleep(0.01f);
            const auto FromProbe=State->Snapshot(QpcNow(),250);
            Check(FromProbe.Sessions>=2 && FromProbe.Accepted>=50 && FMath::IsNearlyEqual(FromProbe.Sample.Curls[4],0.5f),TEXT("native OSC bytes decoded in UE with new session"));
            const auto ProbeRight=State->Snapshot(QpcNow(),250,ESide::Right);
            Check(FromProbe.Sample.Valid && ProbeRight.Sample.Valid && ProbeRight.Accepted>=50 && FMath::IsNearlyEqual(ProbeRight.Sample.Curls[0],0.6f) && FMath::IsNearlyEqual(ProbeRight.Sample.Curls[4],1.f),TEXT("native RIGHT OSC bytes decoded independently"));
            Check(FromProbe.Sample.Session==ProbeRight.Sample.Session && FromProbe.Sample.SessionStart==ProbeRight.Sample.SessionStart && FromProbe.Sample.Sequence==ProbeRight.Sample.Sequence,TEXT("native common session and two sequence counters"));
            int32 Exit=-1; if (Process.IsValid()) { FPlatformProcess::GetProcReturnCode(Process,&Exit); FPlatformProcess::CloseProc(Process); }
            Check(Exit==0,TEXT("native synthetic producer clean exit"));
            FPlatformProcess::Sleep(0.30f);
            Check(!State->Snapshot(QpcNow(),250).Sample.Valid,TEXT("native producer stop -> timeout"));
            Check(!State->Snapshot(QpcNow(),250,ESide::Right).Sample.Valid,TEXT("native producer stop -> RIGHT timeout"));
        }
        Sockets->DestroySocket(Sender);
    }
    else { Check(false,TEXT("UDP sender/listener available")); if (Sender) Sockets->DestroySocket(Sender); }
    const auto Metrics=State->Snapshot(QpcNow(),250);
    UE_LOG(LogMMVRFingerTests,Display,TEXT("Network test LEFT accepted=%llu RIGHT accepted=%llu rejected=%llu sessions=%llu maxCallbackUs=%.2f cache slots=2"),Metrics.Accepted,State->Snapshot(QpcNow(),250,ESide::Right).Accepted,Metrics.Rejected,Metrics.Sessions,Metrics.MaxCallbackUs);
    Server->SetOnDispatchPacket(nullptr); Server->Stop(); Server.Reset();
    auto Rebind=UE::OSC::IServerProxy::Create(); Rebind->SetIPEndpoint(FIPv4Endpoint(FIPv4Address::Any,TestPort)); Rebind->Listen(TEXT("MMVRFingerRebind"));
    Check(Rebind->GetDescription()==TEXT("MMVRFingerRebind"),TEXT("stopped OSC socket port released")); Rebind->Stop(); Rebind.Reset();

    // Exercise runtime game-instance initialize/shutdown three times, without loading a game map or starting actors.
    for (int32 I=0; I<3; ++I)
    {
        UGameInstance* GI=NewObject<UGameInstance>(GEngine);
        GI->InitializeStandalone();
        auto* Sub=GI->GetSubsystem<UMMVRFingerSubsystem>();
        Check(Sub && Sub->IsReceiverActiveForDiagnostics(),TEXT("game-instance receiver initialized"));
        if (I==0)
        {
            UGameInstance* Other=NewObject<UGameInstance>(GEngine); Other->InitializeStandalone();
            auto* OtherSub=Other->GetSubsystem<UMMVRFingerSubsystem>();
            Check(OtherSub && !OtherSub->IsReceiverActiveForDiagnostics(),TEXT("second game instance denied same port explicitly"));
            UWorld* OtherWorld=Other->GetWorld(); Other->Shutdown();
            if (OtherWorld) { OtherWorld->DestroyWorld(false); GEngine->DestroyWorldContext(OtherWorld); }
        }
        UWorld* World=GI->GetWorld();
        GI->Shutdown();
        Check(Sub && !Sub->IsReceiverActiveForDiagnostics(),TEXT("game-instance receiver deinitialized"));
        if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
    }
    UE_LOG(LogMMVRFingerTests,Display,TEXT("V1.1 SELFTEST %s checks=%d failures=%d (synthetic dual-hand transport/cache/lifecycle, not hardware/PIE controls)"),Failures ? TEXT("FAIL") : TEXT("PASS"),Checks,Failures);
    return MMVRFingerSplay::RunSelfTests() && Failures==0;
}
}
