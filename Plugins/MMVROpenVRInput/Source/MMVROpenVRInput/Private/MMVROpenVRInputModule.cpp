// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#include "MMVROpenVRInputState.h"
#include "MMVROpenVRDesktop.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedPlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkSource.h"
#include "LiveLinkSourceFactory.h"
#include "Roles/LiveLinkInputDeviceRole.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMVROpenVRInput, Log, All);
static TAutoConsoleVariable<int32> Enabled(TEXT("MMVR.OpenVRInput.Enabled"), 1, TEXT("Route OpenVRInput to a locally possessed MMVR pawn outside stereo VR."));
static TAutoConsoleVariable<int32> AutoSource(TEXT("MMVR.OpenVRInput.AutoCreateSource"), 1, TEXT("Create an OpenVR Live Link source if none exists when an MMVR player starts."));
static TAutoConsoleVariable<int32> ControllerId(TEXT("MMVR.OpenVRInput.ControllerId"), 0, TEXT("Local controller receiving the single pair of Index controllers."));
static TAutoConsoleVariable<float> MaxAge(TEXT("MMVR.OpenVRInput.MaxFrameAge"), 0.25f, TEXT("Release inputs if the locally produced Live Link frame is older than this many seconds."));
static TAutoConsoleVariable<int32> AutoPrepareDesktop(TEXT("MMVR.OpenVRInput.AutoPrepareDesktop"), 1, TEXT("Dismiss SteamVR dashboard once for a new local MMVR player outside stereo VR, using installed vrcmd. Does not change standby settings."));

class FMMVROpenVRInputModule : public IModuleInterface
{
    struct FPlayerState
    {
        FMMVROpenVRInputState Input;
        TWeakObjectPtr<APawn> Pawn;
        bool bHadFrame = false;
        bool bDesktopPrepared = false;
        int32 DesktopAttempts = 0;
        double NextDesktopAttempt = 0;
    };
    TMap<TWeakObjectPtr<UPlayerInput>, FPlayerState> Players;
    FDelegateHandle TickHandle;
    FDelegateHandle CleanupHandle;
    FGuid OwnedSource;
    double NextSourceAttempt = 0;
    FMMVROpenVRDesktop Desktop;
    IConsoleObject* PrepareCommand = nullptr;

    void PrepareDesktopInput()
    {
        Desktop.PrepareDesktopInput();
    }

    static ILiveLinkClient* Client()
    {
        auto& Features = IModularFeatures::Get();
        return Features.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName)
            ? &Features.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName) : nullptr;
    }
    static bool IsMMVR(const APawn* Pawn)
    {
        for (UClass* Class = Pawn ? Pawn->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
            if (Class->GetPathName() == TEXT("/Game/MegaMocapVR/MMVR_PlayerPawn_BP.MMVR_PlayerPawn_BP_C")) return true;
        return false;
    }
    void EnsureSource(ILiveLinkClient& LiveLink)
    {
        if (!AutoSource.GetValueOnGameThread() || FPlatformTime::Seconds() < NextSourceAttempt) return;
        NextSourceAttempt = FPlatformTime::Seconds() + 5.0;
        if (OwnedSource.IsValid() && !LiveLink.IsSourceStillValid(OwnedSource))
        {
            LiveLink.RemoveSource(OwnedSource);
            OwnedSource.Invalidate();
            return;
        }
        for (FGuid Source : LiveLink.GetSources())
            if (LiveLink.GetSourceType(Source).ToString() == TEXT("OpenVR")) return;
        // Use the public factory abstraction; no private OpenVR headers or second VR_Init.
        UClass* FactoryClass = LoadClass<ULiveLinkSourceFactory>(nullptr, TEXT("/Script/LiveLinkOpenVR.LiveLinkOpenVRSourceFactory"));
        if (FactoryClass)
        {
            const auto* Factory = Cast<ULiveLinkSourceFactory>(FactoryClass->GetDefaultObject());
            TSharedPtr<ILiveLinkSource> Source = Factory->CreateSource(TEXT("(bTrackTrackers=True,bTrackControllers=True,bTrackHMDs=True,bTrackTrackingReferences=True)"));
            if (Source.IsValid())
            {
                OwnedSource = LiveLink.AddSource(Source);
                UE_LOG(LogMMVROpenVRInput, Log, TEXT("Created runtime LiveLinkOpenVR source; existing editor sources are reused."));
            }
        }
    }
    void Tick(UWorld* World, ELevelTick TickType, float DeltaTime)
    {
        if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) return;
        TSet<TWeakObjectPtr<UPlayerInput>> Active;
        if (Enabled.GetValueOnGameThread() && GEngine && !GEngine->IsStereoscopic3D())
        {
            for (auto It = World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                ULocalPlayer* Local = PC ? PC->GetLocalPlayer() : nullptr;
                if (!Local || Local->GetControllerId() != ControllerId.GetValueOnGameThread() || !IsMMVR(PC->GetPawn())) continue;
                auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
                if (!Input) continue;
                Active.Add(Input);
                FPlayerState& State = Players.FindOrAdd(Input);
                if (State.Pawn.Get() != PC->GetPawn())
                {
                    const bool bPreviousPawn = State.Pawn.IsValid();
                    if (bPreviousPawn) State.Input.Release(*Input);
                    State.Pawn = PC->GetPawn();
                    State.bHadFrame = false;
                    // Never accumulate a neutral and a live analog sample in the same tick.
                    if (bPreviousPawn) continue;
                }
                FLiveLinkSubjectFrameData Data;
                const FLiveLinkGamepadInputDeviceFrameData* Frame = nullptr;
                if (ILiveLinkClient* LiveLink = Client())
                {
                    EnsureSource(*LiveLink);
                    if (LiveLink->EvaluateFrame_AnyThread(FLiveLinkSubjectName(TEXT("OpenVRInput")), ULiveLinkInputDeviceRole::StaticClass(), Data))
                        Frame = Data.FrameData.Cast<FLiveLinkGamepadInputDeviceFrameData>();
                }
                const double Now = FPlatformTime::Seconds();
                const double Age = Frame ? Now - Frame->WorldTime.GetSourceTime() : DBL_MAX;
                // This bridge consumes the local OpenVR source, not remotely rebroadcast timestamps.
                if (Frame && FMath::IsFinite(Age) && Age >= -0.01 && Age <= FMath::Max(0.01f, MaxAge.GetValueOnGameThread()))
                {
                    if (AutoPrepareDesktop.GetValueOnGameThread() && !State.bDesktopPrepared && State.DesktopAttempts < 5 && Now >= State.NextDesktopAttempt)
                    {
                        ++State.DesktopAttempts;
                        State.NextDesktopAttempt = Now + 2.0;
                        State.bDesktopPrepared = Desktop.PrepareDesktopInput();
                    }
                    State.Input.Apply(*Input, *Frame);
                    if (!State.bHadFrame) UE_LOG(LogMMVROpenVRInput, Log, TEXT("OpenVRInput fresh frame -> %s (local controller %d); SteamVR action activity not implied"), *PC->GetPawn()->GetName(), Local->GetControllerId());
                    State.bHadFrame = true;
                }
                else if (State.bHadFrame)
                {
                    State.Input.Release(*Input);
                    State.bHadFrame = false;
                    UE_LOG(LogMMVROpenVRInput, Warning, TEXT("OpenVRInput missing/stale: released bridge keys."));
                }
            }
        }
        for (auto It = Players.CreateIterator(); It; ++It)
        {
            UPlayerInput* Input = It.Key().Get();
            if (!Input) { It.RemoveCurrent(); continue; }
            if (Input->GetWorld() == World && !Active.Contains(It.Key()))
            {
                It.Value().Input.Release(*Input);
                It.RemoveCurrent();
            }
        }
    }
    void Cleanup(UWorld* World, bool, bool)
    {
        for (auto It = Players.CreateIterator(); It; ++It)
            if (!It.Key().IsValid() || It.Key()->GetWorld() == World)
            {
                if (It.Key().IsValid()) It.Value().Input.Release(*It.Key());
                It.RemoveCurrent();
            }
        if (Players.IsEmpty() && OwnedSource.IsValid())
        {
            if (ILiveLinkClient* LiveLink = Client()) LiveLink->RemoveSource(OwnedSource);
            OwnedSource.Invalidate();
            NextSourceAttempt = 0;
        }
    }
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet()) return;
        PrepareCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("MMVR.OpenVRInput.PrepareDesktop"),
            TEXT("Dismiss SteamVR dashboard once without wearing the headset or changing power settings. Runtime must already be initialized by LiveLinkOpenVR."),
            FConsoleCommandDelegate::CreateRaw(this, &FMMVROpenVRInputModule::PrepareDesktopInput), ECVF_Default);
        TickHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FMMVROpenVRInputModule::Tick);
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FMMVROpenVRInputModule::Cleanup);
    }
    virtual void ShutdownModule() override
    {
        if (PrepareCommand) IConsoleManager::Get().UnregisterConsoleObject(PrepareCommand);
        FWorldDelegates::OnWorldPreActorTick.Remove(TickHandle);
        FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        for (auto& Pair : Players) if (Pair.Key.IsValid()) Pair.Value.Input.Release(*Pair.Key);
        Players.Empty();
        if (OwnedSource.IsValid()) if (ILiveLinkClient* LiveLink = Client()) LiveLink->RemoveSource(OwnedSource);
    }
};
IMPLEMENT_MODULE(FMMVROpenVRInputModule, MMVROpenVRInput)
