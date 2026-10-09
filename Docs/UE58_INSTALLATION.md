# Installing the UE 5.8 community fork

This repository contains Unreal content and an optional source plugin. It is
**not a standalone Unreal project**. The plugins and the content are installed
separately; the five corrected MMVR assets do not require the bridge directly.

## Install

Use the canonical [main branch](https://github.com/Aresic/MegaMocapVR-UE5.8-FingerTracking/tree/main).
`ue5.8-fixes` is temporarily retained for history and is no longer recommended.

1. Use a Windows x64 Unreal Engine 5.8 project. Create/use a C++ project target
   and the compiler toolchain required by your engine version: this distribution
   has no prebuilt plugin DLLs. A Blueprint-only project may need an empty C++
   class added before building the plugin.
2. Close the editor. Copy the repository's `MegaMocapVR` folder to
   `<YourProject>/Content/MegaMocapVR`. Keep that exact folder name and path.
3. For desktop Index input, copy `Plugins/MMVROpenVRInput` to
   `<YourProject>/Plugins/MMVROpenVRInput` (not into Content).
   Copy `Plugins/MMVRFingerFusion` too: the distributed finger component requires
   its runtime Blueprint node even when external tracking is OFF. For real Index
   finger data, also install MMVRFingerReceiver and follow the
   [sidecar setup](Finger_Tracking_OpenVR.md).
4. Build your project's Editor target. Enable **MMVR OpenVR Input Bridge**,
   **MMVR Finger Fusion**, **Live Link**, **LiveLinkOpenVR** and **Enhanced Input**,
   then restart the editor.
   Dependencies are declared by the plugin. For MMVR itself, keep its normal
   Control Rig, XRBase and other upstream setup requirements; this plugin does not
   replace them. Configure your own host project; do not copy another host's private settings.
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
| `MMVR.OpenVRInput.Debug 1` / `0` | Enable/disable passive OpenVR and bridge diagnostics (default 0) |
| `MMVR.OpenVRInput.ControllerId 0` | Local player controller ID |
| `MMVR.OpenVRInput.MaxFrameAge 0.25` | Maximum accepted local frame age, in seconds |

The passive HMD standby warning is included and remains active with detailed
logging off. See [OpenVR input troubleshooting](OpenVR_Input_Troubleshooting.md)
for the distinct Dashboard and action-initialization cases. Temporary arm-chain
probes and editor repair tools are not distributed.

## What this fork adds

- UE 5.8 cook fixes for the obsolete finger-curl function, an unused editor-widget
  dependency and the FootIK chain traversal.
- Explicit arm Soft IK inputs to prevent identity effectors after calibration.
- Quick Select spatial selection, neutral release and one-time consumption;
  the current threshold is 7.5 cm (`10 * MenuScale`, MenuScale 0.75).
  Hold A on the corresponding controller, move that hand and release to confirm.
  Left Up is unassigned: it consumes the selection and closes the wheel without
  a business action. Its label/icon are blank. Other directions and the right
  wheel are unchanged; there is no stick selection or Recovery feature.
- Optional desktop OpenVR input adapter: A/B, stick clicks, trigger pull/click,
  stick/trackpad axes, grip pull and trackpad force. Trigger click is synthesized
  with 0.90/0.80 hysteresis. Stale/missing frames and player cleanup release input.

No grip-force/capacitive-touch or skeletal finger data is invented by the input bridge.
Optional real Index curls/splays use the separate
[OpenVR finger sidecar and two runtime plugins](Finger_Tracking_OpenVR.md), not a
Toyxyz animation plugin. The bridge is inactive during stereo VR and does not change
tracking, calibration, the solver or the existing Input Actions/Mapping Contexts.

## Packaging and limitations

The bridge's build rules stage the installed Epic action manifest and knuckles
binding as loose NonUFS files; SteamVR reads native files outside pak/IoStore.
The engine's OpenVR module supplies its SDK linkage/DLL staging. This source fork
contains none of those third-party files. Use the stock UE 5.8 engine plugin layout;
custom LiveLinkOpenVR overrides and different SDK layouts are not validated.
A packaged host must include its MMVR map and enable the same dependencies.

Recent development hardware checks found no blocking issue in the baseline,
including spatial Quick Select at 7.5 cm and arm tracking after calibration.
The extracted diagnostics compile in an independent Editor Win64 Development
host; the final Pawn/wheel pass save/reload and compiled spatial tests.
These results do **not** certify hardware operation of the extracted distribution,
Standalone or a final packaged executable. No package was produced for this
checkpoint.

Read the [current validation status and known reservations](UE58_VALIDATION.md)
before using or distributing this source contribution. It documents two
inherited missing soft references and two pre-existing RigVM sweep errors;
neither is presented as a confirmed regression introduced by this fork.

Pending checks:

- Runtime/hardware validation of the cleaned bridge: desktop source creation,
  actual input routing and release/cleanup, first-session automatic dashboard
  preparation and manual retry.
- Final packaging, portable runtime paths, manifest/DLL staging and executable
  runtime validation.
- Independent host/hardware checks of the extracted final Quick Select.
  Development hardware acceptance at **7.5 cm** is recorded in the validation guide.
- Assessment or acceptance of the inherited soft references and an error-free
  RigVM sweep. Recent development hardware checks found no blocking arm issue,
  but do not certify every retargeting or packaged configuration.

## Licenses

MMVR content retains [upstream GPL-3.0](../LICENSE.md). The independently authored
native adapters and sidecar use MIT in their own directories. The input bridge is
[MIT](../Plugins/MMVROpenVRInput/LICENSE); see its
[dependency/provenance notices](../Plugins/MMVROpenVRInput/THIRD_PARTY_NOTICES.md).
Unreal/Epic and Valve dependencies remain under their own terms. Engine and SDK
binaries are not bundled; the sidecar includes the official OpenVR header and
its BSD-3-Clause license.
