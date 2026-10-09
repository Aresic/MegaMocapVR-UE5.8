#include "Modules/ModuleManager.h"
#include "FingerCache.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "MMVRFingerSubsystem.h"
#include "Features/IModularFeatures.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
class FMMVRReceiverSource : public IMMVRFingerSource
{
public:
    virtual void ReadLatest(const UObject* Context,FMMVRFingerPair& Out) override
    {
        Out={};
        UWorld* World=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
        UGameInstance* GI=World ? World->GetGameInstance() : nullptr;
        if (GI) if (auto* Receiver=GI->GetSubsystem<UMMVRFingerSubsystem>()) Receiver->ReadLatestCurls(Out);
    }
};
class FMMVRFingerReceiverModule : public IModuleInterface
{
    FMMVRReceiverSource Source;
    class FSplaySource : public IMMVRFingerSplaySource
    {
        virtual void ReadLatestSplays(const UObject* Context,FMMVRSplayPair& Out) override
        {
            Out={};
            UWorld* World=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull) : nullptr;
            UGameInstance* GI=World ? World->GetGameInstance() : nullptr;
            if (GI) if (auto* Receiver=GI->GetSubsystem<UMMVRFingerSubsystem>()) Receiver->ReadLatestSplays(Out);
        }
    } SplaySource;
    IConsoleObject* TestCommand=nullptr;
    FTSTicker::FDelegateHandle TestTicker;
public:
    virtual void StartupModule() override
    {
        IModularFeatures::Get().RegisterModularFeature(IMMVRFingerSource::FeatureName(),&Source);
        IModularFeatures::Get().RegisterModularFeature(IMMVRFingerSplaySource::FeatureName(),&SplaySource);
        TestCommand=IConsoleManager::Get().RegisterConsoleCommand(TEXT("MMVR.Fingers.SelfTest"),TEXT("Synthetic parser/cache/UDP/lifecycle tests; no OpenVR or animation."),
            FConsoleCommandDelegate::CreateLambda([] { MMVRFinger::RunSelfTests(); }),ECVF_Default);
        if (FParse::Param(FCommandLine::Get(),TEXT("MMVRFingerSelfTest")))
            TestTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
            { const bool Passed=MMVRFinger::RunSelfTests(); FPlatformMisc::RequestExitWithStatus(false,Passed ? 0 : 1); return false; }),1.f);
    }
    virtual void ShutdownModule() override
    {
        IModularFeatures::Get().UnregisterModularFeature(IMMVRFingerSource::FeatureName(),&Source);
        IModularFeatures::Get().UnregisterModularFeature(IMMVRFingerSplaySource::FeatureName(),&SplaySource);
        if (TestTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TestTicker);
        if (TestCommand) IConsoleManager::Get().UnregisterConsoleObject(TestCommand);
    }
};
IMPLEMENT_MODULE(FMMVRFingerReceiverModule,MMVRFingerReceiver)
