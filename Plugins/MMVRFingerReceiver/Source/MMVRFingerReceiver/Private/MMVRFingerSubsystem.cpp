#include "MMVRFingerSubsystem.h"
#include "FingerCache.h"
#include "FingerSplayCache.h"
#include "FingerSplayCoherence.h"
#include "OSCServer.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMVRFingers,Log,All);
namespace
{
TAutoConsoleVariable<int32> Enabled(TEXT("MMVR.Fingers.Enabled"),1,TEXT("OSC receiver/cache enable. No direct animation writes."));
TAutoConsoleVariable<int32> PortSetting(TEXT("MMVR.Fingers.Port"),19761,TEXT("Exclusive UDP receive port; applied at next receiver start."));
TAutoConsoleVariable<float> Timeout(TEXT("MMVR.Fingers.TimeoutMs"),250.f,TEXT("Maximum acquisition age; same-machine QPC only."));
TAutoConsoleVariable<float> Interval(TEXT("MMVR.Fingers.LogInterval"),1.f,TEXT("Periodic summary seconds; 0 disables summaries, transitions remain."));
TAutoConsoleVariable<int32> HUD(TEXT("MMVR.Fingers.HUD"),1,TEXT("Show diagnostic text in PIE/game viewport."));
TAutoConsoleVariable<float> HUDHz(TEXT("MMVR.Fingers.HUDHz"),8.f,TEXT("Visual diagnostic refresh only, 1..30 Hz. Capture, cache and expiry remain full rate."));
TAutoConsoleVariable<int32> Pause(TEXT("MMVR.Fingers.PauseDiagnostics"),0,TEXT("Pause diagnostic consumption, not network capture; resume rechecks acquisition age."));
TSet<int32> OwnedPorts; // GameThread only; OS socket also refuses reuse for other processes
float TimeoutMs() { return FMath::Clamp(Timeout.GetValueOnAnyThread(),1.f,10000.f); }
}
void UMMVRFingerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IsRunningCommandlet()) return;
    WasEnabled = Enabled.GetValueOnGameThread()!=0;
    if (WasEnabled) StartReceiver();
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UMMVRFingerSubsystem::Tick));
}
void UMMVRFingerSubsystem::StartReceiver()
{
    if (Server.IsValid()) return;
    Port = PortSetting.GetValueOnGameThread();
    if (Port < 1 || Port > 65535 || OwnedPorts.Contains(Port))
    { UE_LOG(LogMMVRFingers,Error,TEXT("Receiver not started: port %d invalid/already owned by another game instance. Use a distinct port or one PIE consumer."),Port); return; }
    const int64 Frequency = MMVRFinger::QpcFrequency();
    if (Frequency <= 0) { UE_LOG(LogMMVRFingers,Error,TEXT("Receiver not started: QPC unavailable")); return; }
    Cache = MakeShared<MMVRFinger::FCache,ESPMode::ThreadSafe>(Frequency);
    SplayCache = MakeShared<MMVRFingerSplay::FCache,ESPMode::ThreadSafe>(Frequency);
    Server = UE::OSC::IServerProxy::Create();
    if (!Server.IsValid()) { SplayCache.Reset(); Cache.Reset(); UE_LOG(LogMMVRFingers,Error,TEXT("OSC proxy unavailable")); return; }
    // Epic rewrites loopback binds to local-host NIC. Wildcard bind plus strict loopback-only input filtering.
    Server->SetIPEndpoint(FIPv4Endpoint(FIPv4Address::Any,static_cast<uint16>(Port)));
    Server->ClearClientEndpointAllowList();
    Server->AddClientEndpointToAllowList(FIPv4Endpoint(FIPv4Address(127,0,0,1),0));
    Server->SetFilterClientsByAllowList(true);
    const auto State = Cache;
    const auto RawState = SplayCache;
    auto Dispatch = MakeShared<UE::OSC::IServerProxy::FOnDispatchPacket>([State,RawState](TSharedRef<UE::OSC::IPacket> Packet)
    {
        const int64 Before = MMVRFinger::QpcNow();
        if (!Packet->IsMessage()) State->Reject(TEXT("bundles/non-message unsupported"));
        else
        {
            const FOSCMessage Message(Packet);
            const FString Path=Message.GetAddress().GetFullPath();
            if (Path.Contains(TEXT("/splay/"),ESearchCase::CaseSensitive))
                RawState->Receive(Message,Packet->GetIPEndpoint(),Before,TimeoutMs());
            else State->Receive(Message,Packet->GetIPEndpoint(),Before,TimeoutMs());
        }
        State->RecordCost(1000000.0 * static_cast<double>(MMVRFinger::QpcNow()-Before)/static_cast<double>(MMVRFinger::QpcFrequency()));
    });
    Server->SetOnDispatchPacket(Dispatch);
    const FString Name = FString::Printf(TEXT("MMVRFingers_%u_%d"),GetUniqueID(),Port);
    Server->Listen(Name);
    // Epic proxy IsActive alone also returns true after failed socket creation. Description requires a real socket.
    if (!Server->IsActive() || Server->GetDescription() != Name)
    {
        UE_LOG(LogMMVRFingers,Error,TEXT("Receiver not started: real OSC socket bind failed on port %d (another consumer?)."),Port);
        StopReceiver(); return;
    }
    OwnedPorts.Add(Port); OwnsPort = true;
    for (int32 I=0; I<2; ++I) { LastSession[I].Empty(); LastReason[I].Empty(); LastValid[I]=false; }
    LastReport=0; LastHUD=0;
    UE_LOG(LogMMVRFingers,Log,TEXT("Dual-hand receiver started port=%d allowed source=127.0.0.1 QPC=%lld native worker/latest per hand; optional fusion reader."),Port,Frequency);
}
void UMMVRFingerSubsystem::StopReceiver()
{
    if (Server.IsValid()) { Server->SetOnDispatchPacket(nullptr); Server->Stop(); Server.Reset(); }
    SplayCache.Reset();
    Cache.Reset();
    if (OwnsPort) { OwnedPorts.Remove(Port); OwnsPort=false; UE_LOG(LogMMVRFingers,Log,TEXT("Receiver stopped, port %d released"),Port); }
    if (GEngine) GEngine->RemoveOnScreenDebugMessage(static_cast<uint64>(GetUniqueID())+0x4D4D560000000000ULL);
}
void UMMVRFingerSubsystem::Deinitialize()
{
    if (TickHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); TickHandle.Reset(); }
    StopReceiver(); Super::Deinitialize();
}
bool UMMVRFingerSubsystem::IsReceiverActiveForDiagnostics() const { return OwnsPort && Server.IsValid(); }
void UMMVRFingerSubsystem::ReadLatestCurls(FMMVRFingerPair& Out) const
{
    Out={};
    if (!IsInGameThread() || Enabled.GetValueOnGameThread()==0 || !OwnsPort || !Cache.IsValid()) return;
    const int64 Now=MMVRFinger::QpcNow();
    // Independent of HUD, log cadence and PauseDiagnostics. No old cache becomes a new acquisition.
    for (int32 H=0; H<2; ++H)
    {
        const auto S=Cache->Snapshot(Now,TimeoutMs(),H==0 ? MMVRFinger::ESide::Left : MMVRFinger::ESide::Right);
        const auto& V=S.Sample;
        bool Valid=V.Valid && !V.Session.IsEmpty() && V.SessionStart>0 && V.Sequence>0 &&
            V.Acquisition>=V.SessionStart && V.Frequency==MMVRFinger::QpcFrequency() && S.AgeMs>=0 && S.AgeMs<TimeoutMs();
        for (float Curl:V.Curls) Valid &= FMath::IsFinite(Curl);
        Out.Hands[H].Valid=Valid;
        if (Valid) for (int32 F=0; F<5; ++F) Out.Hands[H].Values[F]=V.Curls[F];
    }
}
FMMVRRawSplayDiagnostics UMMVRFingerSubsystem::GetRawSplayDiagnostics(bool RightHand) const
{
    FMMVRRawSplayDiagnostics Out;
    if (!IsInGameThread() || Enabled.GetValueOnGameThread()==0 || !OwnsPort || !SplayCache.IsValid()) return Out;
    const auto S=SplayCache->Snapshot(MMVRFinger::QpcNow(),TimeoutMs(),RightHand ? MMVRFinger::ESide::Right : MMVRFinger::ESide::Left);
    Out.Valid=S.Sample.Valid; Out.AgeMs=static_cast<float>(S.AgeMs); Out.Sequence=S.Sample.Sequence; Out.Session=S.Sample.Session;
    Out.ThumbIndex=S.Sample.Splays[0]; Out.IndexMiddle=S.Sample.Splays[1]; Out.MiddleRing=S.Sample.Splays[2]; Out.RingPinky=S.Sample.Splays[3];
    return Out;
}
void UMMVRFingerSubsystem::ReadLatestSplays(FMMVRSplayPair& Out) const
{
    Out={};
    if (!IsInGameThread() || Enabled.GetValueOnGameThread()==0 || !OwnsPort || !Cache.IsValid() || !SplayCache.IsValid()) return;
    const int64 Now=MMVRFinger::QpcNow(), Frequency=MMVRFinger::QpcFrequency();
    const double Limit=TimeoutMs();
    MMVRFinger::FSnapshot Curls[2]; MMVRFingerSplay::FSnapshot Splays[2];
    int64 NewestEpoch=0;
    for (int32 H=0; H<2; ++H)
    {
        const auto Side=H==0 ? MMVRFinger::ESide::Left : MMVRFinger::ESide::Right;
        Curls[H]=Cache->Snapshot(Now,Limit,Side); Splays[H]=SplayCache->Snapshot(Now,Limit,Side);
        NewestEpoch=FMath::Max(NewestEpoch,FMath::Max(Curls[H].Sample.SessionStart,Splays[H].Sample.SessionStart));
    }
    for (int32 H=0; H<2; ++H)
        if (MMVRFingerSplay::IsAnimationSampleCoherent(Splays[H],Curls[H],Frequency,Limit,NewestEpoch))
        {
            Out.Hands[H].Valid=true;
            for (int32 P=0; P<4; ++P) Out.Hands[H].Values[P]=Splays[H].Sample.Splays[P];
        }
}
bool UMMVRFingerSubsystem::Tick(float DeltaTime)
{
    const bool IsEnabled = Enabled.GetValueOnGameThread()!=0;
    if (IsEnabled != WasEnabled) { WasEnabled=IsEnabled; if (IsEnabled) StartReceiver(); else StopReceiver(); }
    if (!IsEnabled) return true;
    // No per-frame retry if initialization failed. Toggle Enabled or restart PIE after resolving port/error.
    if (!Cache.IsValid()) return true;
    const uint64 Key = static_cast<uint64>(GetUniqueID())+0x4D4D560000000000ULL;
    const int64 Before = MMVRFinger::QpcNow();
    const double NowSec = static_cast<double>(Before)/static_cast<double>(MMVRFinger::QpcFrequency());
    const double VisualPeriod = 1.0/FMath::Clamp(HUDHz.GetValueOnGameThread(),1.f,30.f);
    const bool ShowHUD = HUD.GetValueOnGameThread()!=0 && GEngine;
    const bool Draw = ShowHUD && NowSec-LastHUD >= VisualPeriod;
    if (!ShowHUD && GEngine) { GEngine->RemoveOnScreenDebugMessage(Key); LastHUD=0; }
    if (Pause.GetValueOnGameThread())
    {
        if (Draw) { GEngine->AddOnScreenDebugMessage(Key,static_cast<float>(VisualPeriod+0.2),FColor::Yellow,
            TEXT("LEFT FINGERS / RIGHT FINGERS: DIAGNOSTICS PAUSED / VALID=false (network worker still active)")); LastHUD=NowSec; }
        return true;
    }
    const float Seconds = Interval.GetValueOnGameThread();
    const bool Report = Seconds > 0 && NowSec-LastReport >= FMath::Max(0.1f,Seconds);
    FString Text;
    bool BothValid = true;
    for (int32 I=0; I<2; ++I)
    {
        const auto Side = I==0 ? MMVRFinger::ESide::Left : MMVRFinger::ESide::Right;
        const TCHAR* Name = MMVRFinger::SideName(Side);
        // Every game tick checks expiry; HUD throttle never gates data/cache/validity.
        const auto S = Cache->Snapshot(Before,TimeoutMs(),Side);
        const auto Raw = SplayCache->Snapshot(Before,TimeoutMs(),Side);
        BothValid &= S.Sample.Valid;
        if (S.Sample.Session != LastSession[I])
        {
            UE_LOG(LogMMVRFingers,Log,TEXT("%s producer session changed: %s -> %s epoch=%lld"),Name,*LastSession[I],*S.Sample.Session,S.Sample.SessionStart);
            LastSession[I]=S.Sample.Session;
        }
        if (S.Sample.Valid != LastValid[I] || S.Reason != LastReason[I])
        {
            UE_LOG(LogMMVRFingers,Log,TEXT("%s VALID=%d reason=%s age=%.2fms seq=%lld"),Name,S.Sample.Valid,*S.Reason,S.AgeMs,S.Sample.Sequence);
            LastValid[I]=S.Sample.Valid; LastReason[I]=S.Reason;
        }
        if (Draw || Report)
        {
            const TCHAR* Tracking = S.Sample.Tracking==1 ? TEXT("Partial") : S.Sample.Tracking==2 ? TEXT("Full") : S.Sample.Tracking==0 ? TEXT("Estimated") : TEXT("Unknown");
            const FString Line = FString::Printf(TEXT("%s FINGERS  VALID=%s  AGE=%.1f ms  RECV=%.1f ms  SEQ=%lld  TRACKING=%s\nT=%.2f I=%.2f M=%.2f R=%.2f P=%.2f  %s%s"),
                Name,S.Sample.Valid ? TEXT("true") : TEXT("false"),S.AgeMs,S.ReceptionAgeMs,S.Sample.Sequence,Tracking,
                S.Sample.Curls[0],S.Sample.Curls[1],S.Sample.Curls[2],S.Sample.Curls[3],S.Sample.Curls[4],*S.Reason,
                S.Sample.OutsideRange ? TEXT(" / RANGE WARNING (unclamped)") : TEXT(""));
            const FString RawLine=FString::Printf(TEXT("SPLAY RAW  VALID=%s AGE=%.1f ms SEQ=%lld  TI=%.3f IM=%.3f MR=%.3f RP=%.3f  %s%s"),
                Raw.Sample.Valid ? TEXT("true") : TEXT("false"),Raw.AgeMs,Raw.Sample.Sequence,Raw.Sample.Splays[0],Raw.Sample.Splays[1],Raw.Sample.Splays[2],Raw.Sample.Splays[3],
                *Raw.Reason,Raw.Sample.OutsideRange ? TEXT(" / RANGE WARNING (unclamped)") : TEXT(""));
            if (Report) UE_LOG(LogMMVRFingers,Log,TEXT("%s %s rejected=%llu lastReject=%s (RAW ONLY / animation disconnected)"),Name,*RawLine,Raw.Rejected,*Raw.LastReject);
            if (Draw) { if (I) Text+=TEXT("\n\n"); Text+=Line; Text+=TEXT("\n"); Text+=RawLine; }
            if (Report) UE_LOG(LogMMVRFingers,Log,TEXT("%s session=%s accepted=%llu rejected=%llu invalidations=%llu sessions=%llu lastReject=%s maxCallbackUs=%.2f maxDisplayTickUs=%.2f"),
                *Line.Replace(TEXT("\n"),TEXT(" | ")),*S.Sample.Session,S.Accepted,S.Rejected,S.Invalidations,S.Sessions,*S.LastReject,S.MaxCallbackUs,MaxTickUs);
        }
    }
    if (Draw) { GEngine->AddOnScreenDebugMessage(Key,static_cast<float>(VisualPeriod+0.2),BothValid ? FColor::Green : FColor::Yellow,Text); LastHUD=NowSec; }
    if (Report) LastReport=NowSec;
    MaxTickUs = FMath::Max(MaxTickUs,1000000.0*static_cast<double>(MMVRFinger::QpcNow()-Before)/static_cast<double>(MMVRFinger::QpcFrequency()));
    return true;
}
