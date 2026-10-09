#include "FingerSplayCoherence.h"
#include <limits>
DEFINE_LOG_CATEGORY_STATIC(LogMMVRSplayCoherenceTests,Log,All);
namespace MMVRFingerSplay
{
bool RunCoherenceSelfTests()
{
    int32 Checks=0,Failures=0;
    auto Check=[&](bool V) { ++Checks; if (!V) ++Failures; };
    constexpr int64 Hz=1000000,Epoch=100000;
    FSnapshot Raw; MMVRFinger::FSnapshot Curl;
    Raw.Sample.Valid=true; Raw.Sample.Session=Curl.Sample.Session=TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Raw.Sample.SessionStart=Curl.Sample.SessionStart=Epoch; Raw.Sample.Frequency=Curl.Sample.Frequency=Hz;
    Raw.Sample.Sequence=Curl.Sample.Sequence=10; Raw.Sample.Acquisition=Curl.Sample.Acquisition=200000;
    Raw.AgeMs=Curl.AgeMs=1;
    auto Valid=[&](const FSnapshot& R,const MMVRFinger::FSnapshot& C,int64 E=100000) { return IsAnimationSampleCoherent(R,C,Hz,250,E); };
    Check(Valid(Raw,Curl));
    Curl.Sample.Valid=false; Check(Valid(Raw,Curl)); // Curl validity never gates a separately valid family.
    auto R=Raw;R.Sample.Valid=false;Check(!Valid(R,Curl));
    for (int32 P=0;P<4;++P) for (float Bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
    {R=Raw;R.Sample.Splays[P]=Bad;Check(!Valid(R,Curl));}
    R=Raw;R.AgeMs=250;Check(!Valid(R,Curl));R.AgeMs=-1;Check(!Valid(R,Curl));
    auto C=Curl;C.AgeMs=250;Check(Valid(Raw,C)); // Fresh splays survive the independent curl timeout.
    R=Raw;R.Sample.Session=TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");Check(!Valid(R,Curl));
    R=Raw;R.Sample.SessionStart++;Check(!Valid(R,Curl));Check(!Valid(Raw,Curl,Epoch+1));
    R=Raw;R.Sample.Frequency++;Check(!Valid(R,Curl));
    R=Raw;R.Sample.Sequence=0;Check(!Valid(R,Curl));
    R=Raw;R.Sample.Acquisition++;Check(!Valid(R,Curl)); // Same sequence must be the same acquisition.
    R=Raw;R.Sample.Sequence=9;R.Sample.Acquisition--;Check(Valid(R,Curl)); // Missing/delayed RAW datagram.
    R.Sample.Acquisition=200001;Check(!Valid(R,Curl));
    R=Raw;R.Sample.Sequence=11;R.Sample.Acquisition++;Check(Valid(R,Curl)); // Lost/delayed curl datagram.
    R.Sample.Acquisition=199999;Check(!Valid(R,Curl));
    C=Curl;C.Sample.Session.Empty();Check(!Valid(Raw,C));
    R=Raw;R.Sample.Acquisition=Epoch-1;Check(!Valid(R,Curl));
    UE_LOG(LogMMVRSplayCoherenceTests,Display,TEXT("SPLAY_COHERENCE_SELFTEST checks=%d failures=%d %s"),Checks,Failures,Failures ? TEXT("FAIL") : TEXT("PASS"));
    return Failures==0;
}
}
