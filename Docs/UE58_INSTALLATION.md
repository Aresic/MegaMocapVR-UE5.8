# Installing the UE 5.8 community fork

This repository contains Unreal content and an optional source plugin. It is
**not a standalone Unreal project**. The plugin and the content are installed
separately; the five corrected MMVR assets do not require the bridge directly.

## Install

1. Use a Windows x64 Unreal Engine 5.8 project. Create/use a C++ project target
   and the compiler toolchain required by your engine version: this distribution
   has no prebuilt plugin DLLs. A Blueprint-only project may need an empty C++
   class added before building the plugin.
2. Close the editor. Copy the repository's `MegaMocapVR` folder to
   `<YourProject>/Content/MegaMocapVR`. Keep that exact folder name and path.
3. For desktop Index input, copy `Plugins/MMVROpenVRInput` to
   `<YourProject>/Plugins/MMVROpenVRInput` (not into Content).
4. Build your project's Editor target. Enable **MMVR OpenVR Input Bridge**,
   **Live Link**, **LiveLinkOpenVR** and **Enhanced Input**, then restart the editor.
   Dependencies are declared by the plugin. For MMVR itself, keep its normal
   Control Rig and other upstream setup requirements; this plugin does not
   replace them. Do not copy the TESTXR project's configuration or plugins list.
5. Install/start SteamVR and connect the Index controllers and trackers. Use the
   stock Epic UE 5.8 LiveLinkOpenVR implementation: no engine patch, replacement
   manifest or Toyxyz plugin is provided or required.
6. Use MMVR's example/setup in your host project. The locally possessed pawn
   must be `MMVR_PlayerPawn_BP` or a subclass, with `EnhancedPlayerInput` and the
   existing MMVR mapping context active. For desktop use, launch normal PIE,
   not stereo VR Preview. Focus the game window. Follow MMVR's usual tracker
   assignment/calibration steps for body tracking.

The bridge automatically creates an OpenVR Live Link source when needed, or
reuses one with type `OpenVR`. Its local `OpenVRInput` subject must be publishing.
Existing MMVR ValveIndex mappings are used unchanged. Use one controller pair
and one local player (controller ID 0 by default); multiplayer/rebroadcast input
is not covered. Keep the content under `/Game/MegaMocapVR`.

## Desktop and dashboard

The headset may remain unworn. Do not spoof its presence sensor or change power
settings for this plugin. SteamVR dashboard/focus can suppress action input even
when tracking and fresh Live Link frames continue.

By default, after the first fresh frame the bridge requests dashboard dismissal
once through the installed `bin/win64/vrcmd.exe --background --hidedashboard`.
It does not repeatedly close a dashboard deliberately reopened later. Runtime
preparation failures are retried at most five times, two seconds apart. A launch
log confirms a request, not that the dashboard actually closed. If the utility
is absent or the request fails, close the dashboard manually.

Console commands/options:

| Command | Purpose |
|---|---|
| `MMVR.OpenVRInput.Enabled 0` / `1` | Disable/enable the bridge (default 1); cleanup releases bridge input |
| `MMVR.OpenVRInput.AutoCreateSource 0` / `1` | Disable/enable automatic Live Link source creation (default 1) |
| `MMVR.OpenVRInput.AutoPrepareDesktop 0` / `1` | Manual/automatic dashboard preparation (default 1) |
| `MMVR.OpenVRInput.PrepareDesktop` | Request dashboard dismissal again after SteamVR/LiveLinkOpenVR is running |
| `MMVR.OpenVRInput.ControllerId 0` | Local player controller ID |
| `MMVR.OpenVRInput.MaxFrameAge 0.25` | Maximum accepted local frame age, in seconds |

The temporary TESTXR diagnostic commands are intentionally not shipped.

## What this fork adds

- UE 5.8 cook fixes for the obsolete finger-curl function, an unused editor-widget
  dependency and the FootIK chain traversal.
- Explicit arm Soft IK inputs to prevent identity effectors after calibration.
- Quick Select spatial selection, neutral release and one-time consumption;
  the current threshold is 7.5 cm (`10 * MenuScale`, MenuScale 0.75).
- Optional desktop OpenVR input adapter: A/B, stick clicks, trigger pull/click,
  stick/trackpad axes, grip pull and trackpad force. Trigger click is synthesized
  with 0.90/0.80 hysteresis. Stale/missing frames and player cleanup release input.

No grip-force/capacitive-touch or skeletal finger data is invented. Toyxyz finger
tracking is excluded. The bridge is inactive during stereo VR and does not change
tracking, calibration, the solver or the existing Input Actions/Mapping Contexts.

## Packaging and limitations

The bridge's build rules stage the installed Epic action manifest and knuckles
binding as loose NonUFS files; SteamVR reads native files outside pak/IoStore.
The engine's OpenVR module supplies its SDK linkage/DLL staging. This source fork
contains none of those third-party files. Use the stock UE 5.8 engine plugin layout;
custom LiveLinkOpenVR overrides and different SDK layouts are not validated.
A packaged host must include its MMVR map and enable the same dependencies.

TESTXR previously passed Editor/Development/Shipping bridge compilation, a
synthetic routing/release test, an isolated Development package/launch and MMVR
Development cooks. Hardware PIE input and dashboard handling worked with the
headset unworn. These results do **not** certify this cleaned source distribution
or every SteamVR standby configuration. No new build or hardware test was run
for this integration.

Pending checks:

- Build/load this source distribution in a separate UE 5.8 host and confirm the
  desktop source, input/release, first-session auto preparation and manual retry.
- Reconfirm portable packaged runtime paths and manifest/DLL staging before
  claiming the final distribution validated in a packaged executable.
- Preserve the independent checks: five assets in a UE 5.8 project **without**
  the bridge; hardware Quick Select at the final **7.5 cm** threshold.

## Licenses

MMVR content retains [upstream GPL-3.0](../LICENSE.md). The independently authored
adapter is [MIT](../Plugins/MMVROpenVRInput/LICENSE); see its
[dependency/provenance notices](../Plugins/MMVROpenVRInput/THIRD_PARTY_NOTICES.md).
Unreal/Epic and Valve dependencies remain under their own terms and are not
bundled as part of this source release.
