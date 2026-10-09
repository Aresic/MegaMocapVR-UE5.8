#include "MMVRFingerFusionLibrary.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "Modules/ModuleManager.h"

namespace
{
TAutoConsoleVariable<int32> ExternalEnabled(TEXT("MMVR.Fingers.ExternalEnabled"),1,
    TEXT("1: merge fresh receiver curls per hand; 0: original MMVR simulation only."));
TAutoConsoleVariable<int32> ExternalSplayEnabled(TEXT("MMVR.Fingers.ExternalSplayEnabled"),0,
    TEXT("0: V1.2 curls plus historical splays (default); 1: also merge fresh coherent splays. ExternalEnabled is the master switch."));
const TCHAR* Fields[2][5] = {
    {TEXT("L_Thumb_Curl_"),TEXT("L_Index_Curl_"),TEXT("L_Middle_Curl_"),TEXT("L_Ring_Curl_"),TEXT("L_Pinky_Curl_")},
    {TEXT("R_Thumb_Curl_"),TEXT("R_Index_Curl_"),TEXT("R_Middle_Curl_"),TEXT("R_Ring_Curl_"),TEXT("R_Pinky_Curl_")}};
}
FProperty* MMVRFingerFusion::CurlProperty(UScriptStruct* Type, int32 Hand, int32 Finger)
{
    if (!Type || Hand<0 || Hand>1 || Finger<0 || Finger>4) return nullptr;
    FProperty* Found=nullptr;
    for (TFieldIterator<FProperty> It(Type); It; ++It)
        if (It->GetName().StartsWith(Fields[Hand][Finger],ESearchCase::CaseSensitive))
        { if (Found || !CastField<FDoubleProperty>(*It)) return nullptr; Found=*It; }
    return Found;
}
void MMVRFingerFusion::ReadLatest(const UObject* Context, FMMVRFingerPair& Out)
{
    Out={};
    if (!IsInGameThread() || ExternalEnabled.GetValueOnGameThread()==0) return;
    // Optional feature: no receiver class/package reference, so absence is a normal fallback.
    const auto Sources=IModularFeatures::Get().GetModularFeatureImplementations<IMMVRFingerSource>(IMMVRFingerSource::FeatureName());
    if (Sources.Num()==1) Sources[0]->ReadLatest(Context,Out);
}
bool MMVRFingerFusion::Merge(UScriptStruct* Type, const void* Simulation, void* Result,
    const FMMVRFingerPair& External, bool Enabled)
{
    if (!Type || !Simulation || !Result) return false;
    Type->CopyScriptStruct(Result,Simulation);
    if (!Enabled) return true;
    // Fail closed on a changed schema. Never write splays, perpendicular or another struct.
    if (Type->GetPathName()!=TEXT("/Game/MegaMocapVR/Blueprints/GameSystems/Structs/SteamVR_fingerCurls_Struct.SteamVR_fingerCurls_Struct")) return false;
    FDoubleProperty* Props[2][5]{};
    for (int32 H=0; H<2; ++H) for (int32 F=0; F<5; ++F)
    { Props[H][F]=CastField<FDoubleProperty>(CurlProperty(Type,H,F)); if (!Props[H][F]) return false; }
    for (int32 H=0; H<2; ++H)
    {
        bool Valid=External.Hands[H].Valid;
        for (float V:External.Hands[H].Values) Valid &= FMath::IsFinite(V);
        if (!Valid) continue;
        for (int32 F=0; F<5; ++F) Props[H][F]->SetPropertyValue_InContainer(Result,External.Hands[H].Values[F]);
    }
    return true;
}
FProperty* MMVRFingerFusion::SplayProperty(UScriptStruct* Type,int32 Hand,int32 Pair)
{
    static const TCHAR* Names[2][4]={{TEXT("L_ThumbIndex_Splay_"),TEXT("L_IndexMiddle_Splay_"),TEXT("L_MiddleRing_Splay_"),TEXT("L_RingPinky_Splay_")},
        {TEXT("R_ThumbIndex_Splay_"),TEXT("R_IndexMiddle_Splay_"),TEXT("R_MiddleRing_Splay_"),TEXT("R_RingPinky_Splay_")}};
    if (!Type || Hand<0 || Hand>1 || Pair<0 || Pair>3) return nullptr;
    FProperty* Found=nullptr;
    for (TFieldIterator<FProperty> It(Type); It; ++It)
        if (It->GetName().StartsWith(Names[Hand][Pair],ESearchCase::CaseSensitive))
        { if (Found || !CastField<FDoubleProperty>(*It)) return nullptr; Found=*It; }
    return Found;
}
void MMVRFingerFusion::ReadLatestSplays(const UObject* Context,FMMVRSplayPair& Out)
{
    Out={};
    if (!IsInGameThread() || ExternalEnabled.GetValueOnGameThread()==0 || ExternalSplayEnabled.GetValueOnGameThread()==0) return;
    const auto Sources=IModularFeatures::Get().GetModularFeatureImplementations<IMMVRFingerSplaySource>(IMMVRFingerSplaySource::FeatureName());
    if (Sources.Num()==1) Sources[0]->ReadLatestSplays(Context,Out);
}
bool MMVRFingerFusion::MergeSplays(UScriptStruct* Type,void* Result,const FMMVRSplayPair& External,bool Enabled)
{
    if (!Type || !Result) return false;
    if (!Enabled) return true;
    if (Type->GetPathName()!=TEXT("/Game/MegaMocapVR/Blueprints/GameSystems/Structs/SteamVR_fingerCurls_Struct.SteamVR_fingerCurls_Struct")) return false;
    FDoubleProperty* Props[2][4]{};
    // Preflight all fields before any write. An incompatible splay schema cannot undo curls.
    for (int32 H=0; H<2; ++H) for (int32 P=0; P<4; ++P)
    { Props[H][P]=CastField<FDoubleProperty>(SplayProperty(Type,H,P)); if (!Props[H][P]) return false; }
    for (int32 H=0; H<2; ++H)
    {
        bool Valid=External.Hands[H].Valid;
        for (float V:External.Hands[H].Values) Valid &= FMath::IsFinite(V);
        if (Valid) for (int32 P=0; P<4; ++P) Props[H][P]->SetPropertyValue_InContainer(Result,External.Hands[H].Values[P]);
    }
    return true;
}
void UMMVRFingerFusionLibrary::MergeExternalFingerCurls(const UObject*,const int32&,int32&)
{ checkNoEntry(); }
DEFINE_FUNCTION(UMMVRFingerFusionLibrary::execMergeExternalFingerCurls)
{
    P_GET_OBJECT(UObject,WorldContextObject);
    Stack.MostRecentProperty=nullptr;
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const FStructProperty* Input=CastField<FStructProperty>(Stack.MostRecentProperty);
    const void* InputData=Stack.MostRecentPropertyAddress;
    Stack.MostRecentProperty=nullptr;
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const FStructProperty* Output=CastField<FStructProperty>(Stack.MostRecentProperty);
    void* OutputData=Stack.MostRecentPropertyAddress;
    P_FINISH;
    P_NATIVE_BEGIN;
    if (Input && Output && Input->Struct==Output->Struct && InputData && OutputData)
    {
        FMMVRFingerPair Latest;
        MMVRFingerFusion::ReadLatest(WorldContextObject,Latest);
        MMVRFingerFusion::Merge(Input->Struct,InputData,OutputData,Latest,ExternalEnabled.GetValueOnGameThread()!=0);
        FMMVRSplayPair Splays;
        MMVRFingerFusion::ReadLatestSplays(WorldContextObject,Splays);
        MMVRFingerFusion::MergeSplays(Input->Struct,OutputData,Splays,
            ExternalEnabled.GetValueOnGameThread()!=0 && ExternalSplayEnabled.GetValueOnGameThread()!=0);
    }
    P_NATIVE_END;
}
IMPLEMENT_MODULE(FDefaultModuleImpl,MMVRFingerFusion)
