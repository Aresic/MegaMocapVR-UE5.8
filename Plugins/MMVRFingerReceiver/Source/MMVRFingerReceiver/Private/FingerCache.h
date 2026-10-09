#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "OSCMessage.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

namespace MMVRFinger
{
constexpr const TCHAR* Address = TEXT("/mmvr/fingers/v1/left");
constexpr const TCHAR* RightAddress = TEXT("/mmvr/fingers/v1/right");
enum class ESide : uint8 { Left, Right };
inline int32 SideIndex(ESide Side) { return Side == ESide::Right ? 1 : 0; }
inline const TCHAR* SideName(ESide Side) { return Side == ESide::Right ? TEXT("RIGHT") : TEXT("LEFT"); }
int64 QpcNow();
int64 QpcFrequency();
struct FSample
{
    ESide Side = ESide::Left; // OSC address is the authoritative side; V1 LEFT payload unchanged.
    FString Session;
    int64 SessionStart = 0, Sequence = 0, Acquisition = 0, Frequency = 0, Reception = 0;
    bool Valid = false, OutsideRange = false;
    int32 Tracking = -1;
    float Curls[5]{};
};
struct FSnapshot
{
    FSample Sample;
    FString Reason = TEXT("waiting for producer"), LastReject;
    uint64 Accepted = 0, Rejected = 0, Invalidations = 0, Sessions = 0;
    double AgeMs = -1, ReceptionAgeMs = -1, MaxCallbackUs = 0;
};
class FCache
{
public:
    explicit FCache(int64 InFrequency) : Frequency(InFrequency) {}
    bool Receive(const FOSCMessage& Message, const FIPv4Endpoint& Source, int64 Now, double TimeoutMs);
    FSnapshot Snapshot(int64 Now, double TimeoutMs, ESide Side = ESide::Left);
    void Reject(const FString& Reason);
    void RecordCost(double Microseconds);
private:
    FCriticalSection Lock;
    int64 Frequency;
    FSnapshot Hands[2];
    FString ProducerSession, LastReject;
    int64 ProducerEpoch = 0;
    uint64 Rejected = 0;
    double MaxCallbackUs = 0;
};
bool Decode(const FOSCMessage& Message, FSample& Out, FString& Error);
bool RunSelfTests();
}
