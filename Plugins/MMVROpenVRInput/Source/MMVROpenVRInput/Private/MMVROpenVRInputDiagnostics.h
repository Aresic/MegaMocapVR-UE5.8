// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#pragma once
#include "CoreMinimal.h"

// Access to the session owned by LiveLinkOpenVR. Never initializes/shuts it down,
// polls events, updates action sets or replaces its manifest. Snapshots are passive;
// Dashboard preparation belongs to the separate desktop helper.
class FMMVROpenVRInputDiagnostics
{
    void* Library = nullptr;
    double NextDetails = 0;
    double NextStandbyCheck = 0;
    double NextStandbyWarning = 0;
    bool EnsureLibrary(bool bLogFailures = true);
public:
    ~FMMVROpenVRInputDiagnostics();
    void LogSnapshot(int32 DebugLevel);
    void CheckStandbyInput();
};
