#pragma once
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

// Transport validates session, sequence and acquisition age before publishing.
// This interface owns no animation target, queue, smoothing or synthetic input.
struct FMMVRHandCurls
{
    bool Valid = false;
    float Values[5]{}; // Thumb, Index, Middle, Ring, Pinky
};
struct FMMVRFingerPair
{
    FMMVRHandCurls Hands[2]; // LEFT, RIGHT
};
class IMMVRFingerSource : public IModularFeature
{
public:
    static FName FeatureName() { return TEXT("MMVRFingerSource"); }
    virtual void ReadLatest(const UObject* WorldContextObject, FMMVRFingerPair& Out) = 0;
};

// Optional additive source. The V1.2 curl contract remains unchanged.
struct FMMVRHandSplays
{
    bool Valid = false;
    float Values[4]{}; // ThumbIndex, IndexMiddle, MiddleRing, RingPinky
};
struct FMMVRSplayPair { FMMVRHandSplays Hands[2]; };
class IMMVRFingerSplaySource : public IModularFeature
{
public:
    static FName FeatureName() { return TEXT("MMVRFingerSplaySource"); }
    virtual void ReadLatestSplays(const UObject* WorldContextObject, FMMVRSplayPair& Out) = 0;
};
