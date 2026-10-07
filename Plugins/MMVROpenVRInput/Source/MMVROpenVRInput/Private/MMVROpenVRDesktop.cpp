// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#include "MMVROpenVRDesktop.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include <openvr.h>

DEFINE_LOG_CATEGORY_STATIC(LogMMVROpenVRDiagnostics, Log, All);

FMMVROpenVRDesktop::~FMMVROpenVRDesktop()
{
    if (Library) FPlatformProcess::FreeDllHandle(Library);
}

bool FMMVROpenVRDesktop::EnsureLibrary()
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("LiveLinkOpenVR"));
    if (!Plugin || !FModuleManager::Get().IsModuleLoaded(TEXT("LiveLinkOpenVR")))
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[1 runtime] LiveLinkOpenVR not loaded; observer will not initialize it."));
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
            UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[1 runtime] OpenVR DLL unavailable at %s"), *SDK);
            return false;
        }
    }
    return true;
}

bool FMMVROpenVRDesktop::PrepareDesktopInput()
{
    if (!EnsureLibrary()) return false;
    vr::EVRInitError Error = vr::VRInitError_None;
    auto* Overlay = static_cast<vr::IVROverlay*>(vr::VR_GetGenericInterface(vr::IVROverlay_Version, &Error));
    if (!Overlay)
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[desktop] Runtime not ready, overlay interface error=%d; no VR_Init attempted."), int32(Error));
        return false;
    }
    if (!Overlay->IsDashboardVisible())
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Log, TEXT("[desktop] Dashboard already closed. Headset may remain unworn; power settings unchanged."));
        return true;
    }
    char RuntimePath[4096]{};
    uint32 RequiredSize = 0;
    if (!vr::VR_GetRuntimePath(RuntimePath, sizeof(RuntimePath), &RequiredSize) || RequiredSize > sizeof(RuntimePath))
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[desktop] Could not resolve installed SteamVR runtime path."));
        return false;
    }
    const FString Command = FPaths::Combine(UTF8_TO_TCHAR(RuntimePath), TEXT("bin/win64/vrcmd.exe"));
    if (!IFileManager::Get().FileExists(*Command))
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[desktop] SteamVR utility missing: %s. Close the dashboard manually."), *Command);
        return false;
    }
    // No shell, no scene application, no synchronous wait on the game thread.
    // Do not repeatedly hide a dashboard the user deliberately opens during mocap.
    FProcHandle Process = FPlatformProcess::CreateProc(*Command, TEXT("--background --hidedashboard"), true, true, true, nullptr, 0, nullptr, nullptr);
    if (!Process.IsValid())
    {
        UE_LOG(LogMMVROpenVRDiagnostics, Warning, TEXT("[desktop] Failed to start SteamVR dashboard dismissal."));
        return false;
    }
    FPlatformProcess::CloseProc(Process);
    UE_LOG(LogMMVROpenVRDiagnostics, Log, TEXT("[desktop] Requested dashboard dismissal once via %s --background --hidedashboard. Confirm dashboard=0 / action activity in Debug logs; power settings unchanged."), *Command);
    return true;
}
