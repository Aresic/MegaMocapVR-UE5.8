# MegaMocapVR UE5.8 + Valve Index Finger Tracking

Community fork of [MegaMocapVR by Megasteakman](https://github.com/Megasteakman/MegaMocapVR)
for **Unreal Engine 5.8**: runtime fixes, desktop OpenVR input improvements and
Valve Index / Knuckles finger tracking with **five curls + four splays per hand**.
Finger Tracking V1 was hardware validated in **PIE, Standalone Game and Windows
Development Package** on the tested Index setup.

This is a content-and-source-plugin distribution, **not a standalone Unreal
project**. Use the [main branch](https://github.com/Aresic/MegaMocapVR-UE5.8-FingerTracking/tree/main).

## Quick Start

You need Windows x64, UE 5.8, SteamVR and Index controllers. Install Visual Studio
2022 with the C++ workloads/Windows SDK required by UE 5.8 and CMake 3.21+ for the
probe (Visual Studio's bundled CMake also works).

**1. Install the content.** Close Unreal. Copy `MegaMocapVR/` to
`<YourProject>/Content/MegaMocapVR/`. Keep the asset path exactly
`/Game/MegaMocapVR`.

**2. Install and compile the plugins.** Copy these folders into
`<YourProject>/Plugins/`:

- `MMVROpenVRInput` — desktop controller input bridge.
- `MMVRFingerFusion` — integration with the existing MMVR finger animation.
- `MMVRFingerReceiver` — local OSC finger data receiver.

In **Edit > Plugins**, enable **MMVR OpenVR Input Bridge**, **MMVR Finger Fusion**,
**MMVR Finger Receiver**, **OSC**, **Live Link**, **LiveLinkOpenVR** and
**Enhanced Input**. Retain MMVR's normal **Control Rig**,
**IK Rig** and **XRBase** dependencies.

Compile your UE 5.8 project after installing and enabling the plugins: they contain
C++ source and must be built for your project before Unreal can use them. If
Unreal asks to rebuild modules when opening the `.uproject`, accept the rebuild.
A Blueprint-only project may first need an empty C++ class to create the build
target. For a manual Visual Studio build, close Unreal, choose **Development
Editor / Win64** and build your project's **Editor** target, then reopen Unreal.
See the [installation guide](Docs/UE58_INSTALLATION.md).

**3. Start SteamVR and MMVR.** Turn on the Knuckles and your trackers. Use the
stock Epic **LiveLinkOpenVR** plugin. Set up MMVR in your host project using its
example level, then run desktop PIE, Standalone Game or your Windows Development
package and follow **Tracker assignment → Calibration → Actor Mode**. Use desktop
PIE, rather than VR Preview, for the desktop input bridge. For the MMVR workflow,
see the [original project wiki](https://github.com/Megasteakman/MegaMocapVR/wiki).

**4. Build and launch the finger probe.** `Tools/MMVR_FingerProbe` is a small
external **OpenVR Background sidecar**: it reads Valve Index skeletal summaries
and sends curls/splays locally to Unreal. In PowerShell, from that directory:

```powershell
.\fetch_openvr.ps1
.\build.ps1
.\bin\MMVR_FingerProbe.exe --self-test
.\bin\MMVR_FingerProbe.exe --osc-splays --debug
```

The first three commands are setup/checks; the last starts live capture.
Keep that terminal running; **Ctrl+C** stops the probe. It stays separate from
Unreal, including packaged games. Use one probe and one game receiver on
**127.0.0.1:19761**. Either launch order works.

**5. Enable finger tracking.** Enter these commands in **Unreal's console**,
not PowerShell (in the editor you can also use the Output Log command field):

```text
MMVR.Fingers.ExternalEnabled 1
MMVR.Fingers.ExternalSplayEnabled 1
MMVR.Fingers.HUD 1
```

Curls are enabled by default; external splays default **OFF**. In Actor Mode,
move each finger and spread/close the fingers on both hands: Manny should follow,
with LEFT/RIGHT values visible in the diagnostic HUD.

## Diagnostics and comparison

<details>
<summary>Useful console commands and fallback behavior</summary>

Run these individually in Unreal's console:

```text
MMVR.Fingers.HUDHz 8
MMVR.Fingers.ExternalSplayEnabled 0
MMVR.Fingers.ExternalEnabled 0
MMVR.Fingers.HUD 0
MMVR.OpenVRInput.Debug 1
```

- `HUDHz 8` changes only the visual refresh; capture remains about 90 Hz per hand.
- `ExternalSplayEnabled 0` keeps external curls and uses historical splays.
- `ExternalEnabled 0` restores historical MMVR finger simulation; set it back to
  `1` to resume eligible external samples.
- `HUD 0` hides the diagnostic without stopping tracking.
- `MMVR.OpenVRInput.Debug 1` logs controller-input diagnostics; use `0` to stop.

An absent/disconnected/stale hand falls back independently. Missing splays never
cancel valid curls. A stopped probe expires after 250 ms; no stale external pose
is retained as valid. Receiver is optional, but **Fusion must remain installed**
for the integrated Blueprint node even when external tracking is OFF.

</details>

<details>
<summary>Quick troubleshooting</summary>

- **Missing modules / unknown console command:** compile the installed C++ plugins,
  enable them and restart Unreal. See [installation](Docs/UE58_INSTALLATION.md).
- **INVALID fingers:** check SteamVR, connected Knuckles and the probe terminal;
  close any other PIE/Standalone/package instance using port 19761.
- **Curls work, splays do not:** launch with `--osc-splays` and enable
  `MMVR.Fingers.ExternalSplayEnabled 1`.
- **HUD changes, Manny does not:** complete MMVR's normal Calibration → Actor Mode
  workflow and check the selected actor/AnimInstance.
- **Controller poses work but buttons do not:** see
  [OpenVR input troubleshooting](Docs/OpenVR_Input_Troubleshooting.md).
- **Known startup caveat:** an initial physical HMD wake may be needed when actions
  remain inactive. Standby itself is compatible; no fake presence or automated
  SteamVR restart is needed.

See the [finger guide](Docs/Finger_Tracking_OpenVR.md) for diagnostics and known
limitations. The self-test is synthetic; it does not certify hardware operation.

</details>

## Documentation

- [Installation and prerequisites](Docs/UE58_INSTALLATION.md)
- [Finger tracking: architecture, OSC, CVars and packaging](Docs/Finger_Tracking_OpenVR.md)
- [Sidecar build, options and diagnostic modes](Tools/MMVR_FingerProbe/README.md)
- [OpenVR controller input troubleshooting](Docs/OpenVR_Input_Troubleshooting.md)
- [Input bridge integration review](Docs/MMVROpenVRInput_Integration_Review.md)
- [Validation scope and known limitations](Docs/UE58_VALIDATION.md)

<details>
<summary>Repository history and baseline branches</summary>

`main` is canonical and contains the validated `bb56e00` implementation plus
documentation updates. `ue5.8-fixes` is temporarily retained at `bb56e00`.
Both branch and annotated tag `baseline-pre-ue5.8-fixes` preserve the old main
at `b0805089e0bf0d5192893f93843cb7550ae21ed0`; the tag is a historical checkpoint,
not a product release.

```shell
git clone --branch main https://github.com/Aresic/MegaMocapVR-UE5.8-FingerTracking.git
```

</details>

## Credits

**Megasteakman** created MegaMocapVR. This community fork contributes the UE 5.8
fixes, desktop input improvements and finger integration.

Special thanks to **toyxyz** for
[Openvr_toolkit](https://github.com/toyxyz/Openvr_toolkit) and
[toyxyz-vr-mocap](https://github.com/toyxyz/toyxyz-vr-mocap), the OpenVR skeletal
tracking references and explicit permission to reuse, modify and adapt code/ideas.

<details>
<summary>Attribution and licenses</summary>

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

MMVR content retains [upstream GPL-3.0](LICENSE.md). Newly authored native plugins
and probe/tooling have MIT notices in their own folders; Valve OpenVR and
Unreal/Epic dependencies retain their separate terms. See the
[sidecar NOTICE](Tools/MMVR_FingerProbe/NOTICE.md) and plugin third-party notices.

</details>

<details>
<summary>Original MegaMocapVR videos and project information</summary>

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

</details>
