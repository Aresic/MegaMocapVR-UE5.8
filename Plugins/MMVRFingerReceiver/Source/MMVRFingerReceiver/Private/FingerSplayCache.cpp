// V1.3 RAW only. Its epoch fence and failures cannot alter the V1.2 curl cache.
#include "FingerSplayCache.h"
#include "Misc/ScopeLock.h"
#include "Windows/WindowsHWrapper.h"

namespace MMVRFingerSplay
{
bool Decode(const FOSCMessage& Message, FSample& Out, FString& Error)
{
    using UE::OSC::EDataType;
    const FString Path = Message.GetAddress().GetFullPath();
    if (Path.Equals(Address,ESearchCase::CaseSensitive)) Out.Side = ESide::Left;
    else if (Path.Equals(RightAddress,ESearchCase::CaseSensitive)) Out.Side = ESide::Right;
    else { Error = TEXT("address/version/side"); return false; }
    const auto& A = Message.GetArgumentsChecked();
    if (A.Num() != 11) { Error = TEXT("argument count"); return false; }
    const EDataType Types[]{EDataType::String,EDataType::Int64,EDataType::Int64,EDataType::Int64,EDataType::Int64,
        EDataType::Int32,EDataType::Int32,EDataType::Float,EDataType::Float,EDataType::Float,EDataType::Float};
    for (int32 I=0; I<11; ++I) if (A[I].GetDataType() != Types[I]) { Error = TEXT("argument type"); return false; }
    Out.Session = A[0].GetString();
    if (Out.Session.Len() != 32) { Error = TEXT("session format"); return false; }
    bool NonZero = false;
    for (TCHAR C : Out.Session)
    {
        if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) { Error = TEXT("session format"); return false; }
        NonZero |= C != '0';
    }
    Out.SessionStart = A[1].GetInt64(); Out.Sequence = A[2].GetInt64();
    Out.Acquisition = A[3].GetInt64(); Out.Frequency = A[4].GetInt64();
    const int32 Valid = A[5].GetInt32(); Out.Tracking = A[6].GetInt32();
    if (!NonZero || Out.SessionStart <= 0 || Out.Sequence <= 0 || Out.Acquisition < Out.SessionStart || Out.Frequency <= 0
        || (Valid != 0 && Valid != 1) || Out.Tracking < -1 || Out.Tracking > 2 || (Valid == 1 && Out.Tracking < 0))
    { Error = TEXT("field constraints"); return false; }
    Out.Valid = Valid == 1;
    for (int32 I=0; I<4; ++I)
    {
        Out.Splays[I] = A[I+7].GetFloat();
        if (!FMath::IsFinite(Out.Splays[I])) { Error = TEXT("NaN/Inf"); return false; }
        Out.OutsideRange |= Out.Splays[I] < 0.f || Out.Splays[I] > 1.f;
        if (!Out.Valid && Out.Splays[I] != 0.f) { Error = TEXT("invalid sample must carry zero splays"); return false; }
    }
    return true;
}
void FCache::Reject(const FString& Reason) { FScopeLock Guard(&Lock); ++Rejected; LastReject = Reason; }
void FCache::RecordCost(double Us) { FScopeLock Guard(&Lock); MaxCallbackUs = FMath::Max(MaxCallbackUs,Us); }
bool FCache::Receive(const FOSCMessage& Message, const FIPv4Endpoint& Source, int64 Now, double TimeoutMs)
{
    // Loopback restriction is explicit, independent of the underlying socket's wildcard bind.
    if (Source.Address != FIPv4Address(127,0,0,1)) { Reject(TEXT("unexpected source")); return false; }
    FSample S; FString Error;
    if (!Decode(Message,S,Error)) { Reject(Error); return false; }
    if (Frequency <= 0 || S.Frequency != Frequency || Now <= 0) { Reject(TEXT("QPC frequency/clock")); return false; }
    // Same-machine QPC uncertainty is at most one tick, not an arbitrary clock-offset estimate.
    if (S.Acquisition > Now && S.Acquisition - Now > 1) { Reject(TEXT("future acquisition")); return false; }
    const double Age = 1000.0 * static_cast<double>(Now - S.Acquisition) / static_cast<double>(Frequency);
    if (Age >= TimeoutMs) { Reject(TEXT("stale acquisition")); return false; }
    FScopeLock Guard(&Lock);
    // One producer epoch fence for both addresses: a delayed old RIGHT must not be
    // accepted just because the new session has so far sent only LEFT (and vice versa).
    const bool NewProducer = S.Session != ProducerSession;
    if (!ProducerSession.IsEmpty() &&
        ((NewProducer && S.SessionStart <= ProducerEpoch) || (!NewProducer && S.SessionStart != ProducerEpoch)))
    { ++Rejected; LastReject = TEXT("retired/ambiguous session epoch"); return false; }
    auto& Latest = Hands[SideIndex(S.Side)];
    const auto& Previous = Latest.Sample;
    const bool NewSession = S.Session != Previous.Session;
    if (!Previous.Session.IsEmpty())
    {
        if (NewSession && S.SessionStart <= Previous.SessionStart) { ++Rejected; LastReject = TEXT("retired/ambiguous session"); return false; }
        if (!NewSession && (S.SessionStart != Previous.SessionStart || S.Sequence <= Previous.Sequence || S.Acquisition < Previous.Acquisition))
        { ++Rejected; LastReject = TEXT("sequence/epoch/order"); return false; }
    }
    if (NewProducer) { ProducerSession = S.Session; ProducerEpoch = S.SessionStart; }
    if (Previous.Valid && !S.Valid) ++Latest.Invalidations;
    if (NewSession) ++Latest.Sessions;
    S.Reception = Now;
    Latest.Sample = MoveTemp(S); ++Latest.Accepted;
    Latest.Reason = Latest.Sample.Valid ? TEXT("fresh") : TEXT("producer invalid");
    return true;
}
FSnapshot FCache::Snapshot(int64 Now, double TimeoutMs, ESide Side)
{
    FScopeLock Guard(&Lock);
    auto& Latest = Hands[SideIndex(Side)];
    Latest.Sample.Side = Side;
    if (Latest.Sample.Acquisition > 0)
    {
        Latest.AgeMs = FMath::Max(0.0,1000.0 * static_cast<double>(Now - Latest.Sample.Acquisition) / static_cast<double>(Frequency));
        Latest.ReceptionAgeMs = FMath::Max(0.0,1000.0 * static_cast<double>(Now - Latest.Sample.Reception) / static_cast<double>(Frequency));
        if (Latest.Sample.Valid && Latest.AgeMs >= TimeoutMs)
        {
            Latest.Sample.Valid = false; ++Latest.Invalidations; Latest.Reason = TEXT("acquisition timeout");
            for (float& Splay : Latest.Sample.Splays) Splay = 0.f;
        }
    }
    FSnapshot Result = Latest;
    Result.Rejected = Rejected; Result.LastReject = LastReject; Result.MaxCallbackUs = MaxCallbackUs;
    return Result;
}
}
