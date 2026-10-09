# MegaMocapVR UE5.8 + Valve Index Finger Tracking

Community fork of [MegaMocapVR by Megasteakman](https://github.com/Megasteakman/MegaMocapVR),
targeting Unreal Engine 5.8 with compatibility/runtime fixes, desktop Valve Index
input improvements and optional dual-hand finger tracking. This is a
content-and-plugin distribution, **not a standalone project**.

See [installation, prerequisites and validation limits](Docs/UE58_INSTALLATION.md).
If controllers track but buttons do not respond, see
[OpenVR input troubleshooting](Docs/OpenVR_Input_Troubleshooting.md).
Finger tracking uses a lightweight external **OpenVR Background sidecar** with
FromDevice skeletal summaries: five curls and four splays per hand, transported
over loopback OSC into MMVR's existing animation path. The implementation was
hardware validated in **PIE, Standalone Game and Windows Development Package**.
See [finger sidecar setup and runtime integration](Docs/Finger_Tracking_OpenVR.md).
Source publication is ready with reservations; see the
[current validation status and known limitations](Docs/UE58_VALIDATION.md).
The MMVR content retains upstream GPL-3.0; the separately authored
[MMVROpenVRInput plugin](Plugins/MMVROpenVRInput/THIRD_PARTY_NOTICES.md) is MIT.

## Repository and branches

Canonical repository: [Aresic/MegaMocapVR-UE5.8-FingerTracking](https://github.com/Aresic/MegaMocapVR-UE5.8-FingerTracking).
Use **main**, the default branch. It contains the validated `bb56e00` implementation
and subsequent documentation updates. `ue5.8-fixes` is temporarily retained at
`bb56e00` as a redundant implementation branch; it is not the recommended branch.
The historical main baseline is preserved by both branch and annotated tag
`baseline-pre-ue5.8-fixes`, targeting `b0805089e0bf0d5192893f93843cb7550ae21ed0`.
The baseline tag is a fixed historical checkpoint, not a product release.

```shell
git clone --branch main https://github.com/Aresic/MegaMocapVR-UE5.8-FingerTracking.git
```

## Credits

MegaMocapVR was created by **Megasteakman**; the original project information and
links below are preserved. UE 5.8 fixes, input improvements and this finger
tracking integration are contributions of the Aresic community fork.

The Valve Index finger tracking work was informed in part by
[toyxyz/Openvr_toolkit](https://github.com/toyxyz/Openvr_toolkit) and
[toyxyz/toyxyz-vr-mocap](https://github.com/toyxyz/toyxyz-vr-mocap). Special thanks
to **toyxyz** for the OpenVR skeletal tracking reference implementation, which
helped explain skeletal actions, background acquisition, hand skeletons,
skeletal summaries and capture architecture, and for explicitly allowing reuse,
modification and adaptation of the code and ideas.

The implementation here uses a lightweight background sidecar tailored for
MegaMocapVR and Unreal Engine. Its minimal skeletal JSON layout was adapted from
Openvr_toolkit; its C++ summary probe is newly written, and toyxyz-vr-mocap served
as an architectural reference. Direct author permission is recorded in the
[probe NOTICE](Tools/MMVR_FingerProbe/NOTICE.md); it is not a claim of a standard
MIT, BSD or GPL license for those reference repositories. No official affiliation
or endorsement is implied. Valve OpenVR and Unreal Engine retain their own terms.

---

![](Examples.gif)

MegaMocapVR is a project for Unreal Engine used to drive humanoid character animation live in editor using SteamVR hardware.  This motion can be used in take recorder for the creation of cinematics/animations or be used in streaming for VTuber applications.

Mega Mocap VR is the proud recipient of an Epic MegaGrant!

[![EpicMegaGrantsBadge](https://i.imgur.com/7O3F2Eh.png)]()

**Check out our 2022 sizzle reel!**
[![YouTubeVideo](https://i.imgur.com/mn51yVm.jpg)](https://youtu.be/mf0ioDCkrsU)



**Quick Start Video**
Complete set-up guide using Unreal Engine 5 done using early access: the process still applies to UE 5.0.  Just replace the words 'VRMocap' with 'MMVR' and you should be good to go!
[![YouTubeVideo](https://i.imgur.com/OdwBANp.png)](https://youtu.be/crv6AIbadSo)



**Documentation**
https://github.com/Megasteakman/MegaMocapVR/wiki





**Watch our most recent short film made with MMVR that was made in 48 hours!:** 
[![YouTubeVideo](https://i.imgur.com/lgWaydv.jpg)](https://youtu.be/N3mjv7ar4SA?si=21OlRA-_a0hjCKbo)
https://www.youtube.com/watch?v=N3mjv7ar4SA

Thanks so much, and I hope this project helps you create awesome streams and animations.  I want this tool to be free for everyone to use, so if you add any features that might benefit others or fix any workflow painpoints I would love to incorporate those changes into the core of VRMocap! 

https://ko-fi.com/megasteakman

[![donate to the project](https://i.imgur.com/MFZdDlK.png)](https://ko-fi.com/megasteakman)

