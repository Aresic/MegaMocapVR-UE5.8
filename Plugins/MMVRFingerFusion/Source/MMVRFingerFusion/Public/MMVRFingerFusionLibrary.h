#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MMVRFingerSource.h"
#include "MMVRFingerFusionLibrary.generated.h"

namespace MMVRFingerFusion
{
    MMVRFINGERFUSION_API void ReadLatest(const UObject* Context, FMMVRFingerPair& Out);
    MMVRFINGERFUSION_API bool Merge(UScriptStruct* Type, const void* Simulation, void* Result,
        const FMMVRFingerPair& External, bool Enabled);
    MMVRFINGERFUSION_API FProperty* CurlProperty(UScriptStruct* Type, int32 Hand, int32 Finger);
    MMVRFINGERFUSION_API void ReadLatestSplays(const UObject* Context, FMMVRSplayPair& Out);
    MMVRFINGERFUSION_API FProperty* SplayProperty(UScriptStruct* Type, int32 Hand, int32 Pair);
    MMVRFINGERFUSION_API bool MergeSplays(UScriptStruct* Type, void* Result, const FMMVRSplayPair& External, bool Enabled);
}
UCLASS()
class MMVRFINGERFUSION_API UMMVRFingerFusionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Wildcard pins retain the existing user-defined MMVR struct and its defaults.
    UFUNCTION(BlueprintPure, CustomThunk, Category="MMVR|Fingers",
        meta=(WorldContext="WorldContextObject", CustomStructureParam="Simulation,Merged", DisplayName="Merge External Finger Curls"))
    static void MergeExternalFingerCurls(const UObject* WorldContextObject, const int32& Simulation, int32& Merged);
    DECLARE_FUNCTION(execMergeExternalFingerCurls);
};
