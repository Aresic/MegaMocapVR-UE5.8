// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#pragma once
#include "CoreMinimal.h"

// Uses the session owned by Epic LiveLinkOpenVR; never initializes or shuts down OpenVR.
// Only calls the installed SteamVR CLI to request dashboard dismissal.
class FMMVROpenVRDesktop
{
    void* Library = nullptr;
    bool EnsureLibrary();
public:
    ~FMMVROpenVRDesktop();
    bool PrepareDesktopInput();
};
