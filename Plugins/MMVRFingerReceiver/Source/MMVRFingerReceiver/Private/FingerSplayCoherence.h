#pragma once
#include "FingerSplayCache.h"
namespace MMVRFingerSplay
{
// Independently latest datagrams can differ after a loss or during dispatch. Do not
// demand exact sequence equality or add a pairing queue. Order must agree with QPC.
inline bool IsAnimationSampleCoherent(const FSnapshot& Raw,const MMVRFinger::FSnapshot& Curl,int64 Frequency,double TimeoutMs,int64 NewestEpoch)
{
    const auto& R=Raw.Sample; const auto& C=Curl.Sample;
    if (!R.Valid || R.Session.IsEmpty() || R.Session!=C.Session || R.SessionStart<=0 ||
        R.SessionStart!=C.SessionStart || R.SessionStart!=NewestEpoch || R.Sequence<=0 || C.Sequence<=0 ||
        R.Acquisition<R.SessionStart || C.Acquisition<C.SessionStart || R.Frequency!=Frequency || C.Frequency!=Frequency ||
        Raw.AgeMs<0 || Raw.AgeMs>=TimeoutMs || Curl.AgeMs<0) return false;
    if ((R.Sequence==C.Sequence && R.Acquisition!=C.Acquisition) ||
        (R.Sequence<C.Sequence && R.Acquisition>C.Acquisition) ||
        (R.Sequence>C.Sequence && R.Acquisition<C.Acquisition)) return false;
    for (float V:R.Splays) if (!FMath::IsFinite(V)) return false;
    return true;
}
bool RunCoherenceSelfTests();
}
