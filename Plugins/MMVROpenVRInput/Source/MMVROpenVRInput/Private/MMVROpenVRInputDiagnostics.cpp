// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#include "MMVROpenVRInputDiagnostics.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include <openvr.h>

DEFINE_LOG_CATEGORY_STATIC(LogMMVROpenVRInputDiagnostics, Log, All);

FMMVROpenVRInputDiagnostics::~FMMVROpenVRInputDiagnostics()
{
    if (Library) FPlatformProcess::FreeDllHandle(Library);
}

bool FMMVROpenVRInputDiagnostics::EnsureLibrary(bool bLogFailures)
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("LiveLinkOpenVR"));
    if (!Plugin || !FModuleManager::Get().IsModuleLoaded(TEXT("LiveLinkOpenVR")))
    {
        if (bLogFailures) UE_LOG(LogMMVROpenVRInputDiagnostics, Warning, TEXT("[1 runtime] LiveLinkOpenVR not loaded; observer will not initialize it."));
        return false;
    }
    if (!Library)
    {
        FString SDK = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/ThirdParty/OpenVR/OpenVRv1_5_17"));
        const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("VR_OVERRIDE"));
        if (!Override.IsEmpty()) SDK = Override;
        Library = FPlatformProcess::GetDllHandle(*FPaths::Combine(SDK, TEXT("bin/win64/openvr_api.dll")));
        if (!Library)
        {
            if (bLogFailures) UE_LOG(LogMMVROpenVRInputDiagnostics, Warning, TEXT("[1 runtime] OpenVR DLL unavailable at %s"), *SDK);
            return false;
        }
    }
    return true;
}

void FMMVROpenVRInputDiagnostics::CheckStandbyInput()
{
    // Module-wide timers bound polling and warnings across players/worlds, even with Debug=0.
    const double Now = FPlatformTime::Seconds();
    if (Now < NextStandbyCheck) return;
    NextStandbyCheck = Now + 1.0;
    if (Now < NextStandbyWarning || !EnsureLibrary(false)) return;

    // Passive observation of LiveLinkOpenVR's session; never initialize or update actions.
    vr::EVRInitError Error = vr::VRInitError_None;
    auto* System = static_cast<vr::IVRSystem*>(vr::VR_GetGenericInterface(vr::IVRSystem_Version, &Error));
    auto* Input = static_cast<vr::IVRInput*>(vr::VR_GetGenericInterface(vr::IVRInput_Version, &Error));
    auto* Overlay = static_cast<vr::IVROverlay*>(vr::VR_GetGenericInterface(vr::IVROverlay_Version, &Error));
    if (!System || !Input || !Overlay || Overlay->IsDashboardVisible() || System->IsInputAvailable()
        || !System->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)
        || System->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd) != vr::k_EDeviceActivityLevel_Standby) return;

    bool bControllerConnected = false;
    for (vr::TrackedDeviceIndex_t Device = 0; Device < vr::k_unMaxTrackedDeviceCount; ++Device)
        if (System->IsTrackedDeviceConnected(Device) && System->GetTrackedDeviceClass(Device) == vr::TrackedDeviceClass_Controller)
        {
            bControllerConnected = true;
            break;
        }
    if (!bControllerConnected) return;

    // Use the same analog actions as LogSnapshot. Unknown handles/read errors are not
    // evidence of inactive actions and must not trigger this specific standby advice.
    const char* Names[] = {"FaceButtonLeft", "FaceButtonTop", "FaceButtonBottom", "FaceButtonRight", "LeftTriggerAnalog", "RightTriggerAnalog", "LeftStick_2D", "RightStick_2D", "LeftThumb", "RightThumb", "LeftAnalog_2D", "RightAnalog_2D", "LeftShoulder", "RightShoulder", "SpecialLeft", "SpecialRight"};
    for (const char* Name : Names)
    {
        vr::VRActionHandle_t Handle = 0;
        const FString Path = FString::Printf(TEXT("/actions/LiveLinkGamepadInputDevice/in/%s"), UTF8_TO_TCHAR(Name));
        vr::InputAnalogActionData_t Data{};
        if (Input->GetActionHandle(TCHAR_TO_UTF8(*Path), &Handle) != vr::VRInputError_None || !Handle
            || Input->GetAnalogActionData(Handle, &Data, sizeof(Data), vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None
            || Data.bActive) return;
    }

    NextStandbyWarning = Now + 30.0;
    UE_LOG(LogMMVROpenVRInputDiagnostics, Warning, TEXT("OpenVR controller actions are inactive while the HMD is currently in standby. Waking the HMD once may reinitialize SteamVR input actions. If the issue persists, check bindings, focus and runtime state."));
}

void FMMVROpenVRInputDiagnostics::LogSnapshot(int32 DebugLevel)
{
    const double Now = FPlatformTime::Seconds();
    const bool Details = DebugLevel > 1 || Now >= NextDetails;
    if (Details) NextDetails = Now + 5.0;
    if (!EnsureLibrary()) return;
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("LiveLinkOpenVR"));
    // Resolve interfaces every snapshot: do not retain pointers across runtime reinitialization.
    vr::EVRInitError SystemError = vr::VRInitError_None, InputError = vr::VRInitError_None;
    auto* System = static_cast<vr::IVRSystem*>(vr::VR_GetGenericInterface(vr::IVRSystem_Version, &SystemError));
    auto* Input = static_cast<vr::IVRInput*>(vr::VR_GetGenericInterface(vr::IVRInput_Version, &InputError));
    if (!System || !Input)
    {
        UE_LOG(LogMMVROpenVRInputDiagnostics, Warning, TEXT("[1 runtime] system=%d error=%d input=%d error=%d (no VR_Init attempted)"), System != nullptr, int32(SystemError), Input != nullptr, int32(InputError));
        return;
    }
    vr::EVRInitError OptionalError = vr::VRInitError_None;
    auto* Overlay = static_cast<vr::IVROverlay*>(vr::VR_GetGenericInterface(vr::IVROverlay_Version, &OptionalError));
    auto* Apps = static_cast<vr::IVRApplications*>(vr::VR_GetGenericInterface(vr::IVRApplications_Version, &OptionalError));
    auto* Settings = static_cast<vr::IVRSettings*>(vr::VR_GetGenericInterface(vr::IVRSettings_Version, &OptionalError));
    vr::EVRSettingsError SettingError = vr::VRSettingsError_None;
    const int32 PauseOnStandby = Settings ? int32(Settings->GetBool(vr::k_pch_Power_Section, vr::k_pch_Power_PauseCompositorOnStandby_Bool, &SettingError)) : -1;
    if (Details) UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[1 power] pauseCompositorOnStandby=%d readError=%d (read only; standby alone is not proof of blocked OpenVR actions)"), PauseOnStandby, int32(SettingError));
    UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[1 runtime] system=1 input=1 available=%d dashboard=%d hmdActivity=%d scenePid=%u observerPid=%u"),
        System->IsInputAvailable(), Overlay ? int32(Overlay->IsDashboardVisible()) : -1,
        int32(System->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd)),
        Apps ? Apps->GetCurrentSceneProcessId() : 0, FPlatformProcess::GetCurrentProcessId());
    if (Details)
    {
        const FString Manifest = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/livelinkopenvr_action_manifest.json")));
        char Key[512]{}, URL[4096]{};
        vr::EVRApplicationError KeyError = vr::VRApplicationError_NoApplication;
        vr::EVRApplicationError URLError = vr::VRApplicationError_NoApplication;
        if (Apps)
        {
            KeyError = Apps->GetApplicationKeyByProcessId(FPlatformProcess::GetCurrentProcessId(), Key, sizeof(Key));
            if (KeyError == vr::VRApplicationError_None)
                Apps->GetApplicationPropertyString(Key, vr::VRApplicationProperty_ActionManifestURL_String, URL, sizeof(URL), &URLError);
        }
        UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[2 manifest] expected=%s exists=%d app=%s appError=%d runtimeURL=%s urlError=%d (no SetActionManifestPath attempted)"),
            *Manifest, IFileManager::Get().FileExists(*Manifest), UTF8_TO_TCHAR(Key), int32(KeyError), UTF8_TO_TCHAR(URL), int32(URLError));
    }
    vr::VRActionSetHandle_t Set = 0;
    const auto SetError = Input->GetActionSetHandle("/actions/LiveLinkGamepadInputDevice", &Set);
    if (Details) UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[3 handles] set=%llu error=%d; UpdateActionState owned by LiveLinkOpenVR (result unavailable to passive observer)"), Set, int32(SetError));
    if (Details) for (int32 Hand = 0; Hand < 2; ++Hand)
    {
        const auto Device = System->GetTrackedDeviceIndexForControllerRole(Hand ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand);
        vr::VRInputValueHandle_t Source = 0;
        const auto SourceError = Input->GetInputSourceHandle(Hand ? "/user/hand/right" : "/user/hand/left", &Source);
        char Type[128]{};
        const bool Connected = Device != vr::k_unTrackedDeviceIndexInvalid && System->IsTrackedDeviceConnected(Device);
        if (Connected) System->GetStringTrackedDeviceProperty(Device, vr::Prop_ControllerType_String, Type, sizeof(Type));
        UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[4 controller] hand=%s device=%u connected=%d type=%s source=%llu sourceError=%d"),
            Hand ? TEXT("R") : TEXT("L"), Device, Connected, UTF8_TO_TCHAR(Type), Source, int32(SourceError));
    }
    // Stock Epic manifest declares even button clicks as vector1: analog reads are intentional.
    const char* Names[] = {"FaceButtonLeft", "FaceButtonTop", "FaceButtonBottom", "FaceButtonRight", "LeftTriggerAnalog", "RightTriggerAnalog", "LeftStick_2D", "RightStick_2D", "LeftThumb", "RightThumb", "LeftAnalog_2D", "RightAnalog_2D", "LeftShoulder", "RightShoulder", "SpecialLeft", "SpecialRight"};
    uint32 ActiveMask = 0, ErrorMask = 0;
    FString Values;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        vr::VRActionHandle_t Handle = 0;
        const FString Path = FString::Printf(TEXT("/actions/LiveLinkGamepadInputDevice/in/%s"), UTF8_TO_TCHAR(Names[Index]));
        const auto HandleError = Input->GetActionHandle(TCHAR_TO_UTF8(*Path), &Handle);
        vr::InputAnalogActionData_t Data{};
        const auto ReadError = HandleError == vr::VRInputError_None && Handle
            ? Input->GetAnalogActionData(Handle, &Data, sizeof(Data), vr::k_ulInvalidInputValueHandle) : vr::VRInputError_InvalidHandle;
        if (Data.bActive) ActiveMask |= 1u << Index;
        if (ReadError != vr::VRInputError_None) ErrorMask |= 1u << Index;
        if (Index < 8) Values += FString::Printf(TEXT(" %s=%.3f,%.3f"), UTF8_TO_TCHAR(Names[Index]), Data.x, Data.y);
        if (Details)
        {
            vr::InputBindingInfo_t Bindings[8]{};
            uint32 Count = 0;
            const auto BindingError = Handle ? Input->GetActionBindingInfo(Handle, Bindings, sizeof(Bindings[0]), UE_ARRAY_COUNT(Bindings), &Count) : vr::VRInputError_InvalidHandle;
            UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[3/5 action] %s handle=%llu handleError=%d readError=%d active=%d origin=%llu xy=%.3f,%.3f bindingError=%d bindings=%u first=%s%s mode=%s slot=%s"),
                UTF8_TO_TCHAR(Names[Index]), Handle, int32(HandleError), int32(ReadError), Data.bActive, Data.activeOrigin, Data.x, Data.y,
                int32(BindingError), Count, UTF8_TO_TCHAR(Bindings[0].rchDevicePathName), UTF8_TO_TCHAR(Bindings[0].rchInputPathName), UTF8_TO_TCHAR(Bindings[0].rchModeName), UTF8_TO_TCHAR(Bindings[0].rchSlotName));
        }
    }
    UE_LOG(LogMMVROpenVRInputDiagnostics, Log, TEXT("[5 SteamVR] activeMask=0x%04x errorMask=0x%04x raw:%s"), ActiveMask, ErrorMask, *Values);
    if (!ActiveMask) UE_LOG(LogMMVROpenVRInputDiagnostics, Warning, TEXT("Actions inactive: fresh OpenVRInput frames do NOT prove usable controller input. Check dashboard/focus, connections and binding details above."));
}
