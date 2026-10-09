#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "MMVRFingerSource.h"
#include "MMVRFingerSubsystem.generated.h"
namespace UE::OSC { class IServerProxy; }
namespace MMVRFinger { class FCache; }
namespace MMVRFingerSplay { class FCache; }
USTRUCT(BlueprintType)
struct MMVRFINGERRECEIVER_API FMMVRRawSplayDiagnostics
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Valid = false;
    UPROPERTY(BlueprintReadOnly) float AgeMs = -1;
    UPROPERTY(BlueprintReadOnly) int64 Sequence = 0;
    UPROPERTY(BlueprintReadOnly) FString Session;
    UPROPERTY(BlueprintReadOnly) float ThumbIndex = 0;
    UPROPERTY(BlueprintReadOnly) float IndexMiddle = 0;
    UPROPERTY(BlueprintReadOnly) float MiddleRing = 0;
    UPROPERTY(BlueprintReadOnly) float RingPinky = 0;
};
UCLASS()
class MMVRFINGERRECEIVER_API UMMVRFingerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool IsReceiverActiveForDiagnostics() const;
    void ReadLatestCurls(FMMVRFingerPair& Out) const;
    void ReadLatestSplays(FMMVRSplayPair& Out) const;
    // RAW inspection only. Not registered as an MMVR animation source.
    UFUNCTION(BlueprintPure, Category="MMVR|Finger Diagnostics")
    FMMVRRawSplayDiagnostics GetRawSplayDiagnostics(bool RightHand) const;
private:
    void StartReceiver();
    void StopReceiver();
    bool Tick(float DeltaTime);
    TSharedPtr<UE::OSC::IServerProxy> Server;
    TSharedPtr<MMVRFinger::FCache, ESPMode::ThreadSafe> Cache;
    TSharedPtr<MMVRFingerSplay::FCache, ESPMode::ThreadSafe> SplayCache;
    FTSTicker::FDelegateHandle TickHandle;
    FString LastSession[2], LastReason[2];
    bool LastValid[2]{}, OwnsPort = false, WasEnabled = false;
    int32 Port = 0;
    double LastReport = 0, LastHUD = 0, MaxTickUs = 0;
};
