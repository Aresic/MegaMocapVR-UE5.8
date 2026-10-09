# Valve Index finger tracking (UE 5.8 / Windows x64)

Optional live **five curls + four splays per hand** for Valve Index / Knuckles.
This extends MMVR's existing finger animation path; it does not replace body
tracking, controller inputs, calibration or the HandRig.

## Architecture

```text
SteamVR / Index LEFT + RIGHT
  -> external MMVR_FingerProbe (OpenVR Background, FromDevice summaries)
  -> OSC UDP loopback -> MMVRFingerReceiver -> MMVRFingerFusion
simulation -> one final SteamVR_fingerCurls_Struct -> existing SendFingerData
  -> MMVR Pawn / selected AnimInstance -> existing Manny HandRig
```

The Background sidecar has its own skeletal-only action manifest and process.
Unreal keeps stock Epic LiveLinkOpenVR and the OpenVR input bridge. This avoids
adding another OpenVR owner or changing Epic's manifest in Unreal. The sidecar
can keep running across game restarts. No OpenXR migration is required: this
workflow intentionally uses the existing LiveLinkOpenVR/OpenVR setup and Valve's
skeletal summaries. No full-bone capture, fake trigger/grip input or second writer.

## Install and build

1. Follow [UE 5.8 installation](UE58_INSTALLATION.md). Keep content under
   `/Game/MegaMocapVR`; the fusion checks the existing struct's path/schema.
2. Copy **both** `Plugins/MMVRFingerFusion` and `Plugins/MMVRFingerReceiver`
   into your project's Plugins folder. Enable them and Epic OSC. Receiver declares
   the Fusion/OSC dependencies; Fusion must remain installed for the integrated
   Blueprint node, even if the optional receiver is disabled.
3. Build your project's Editor target with UE 5.8's Windows C++ toolchain.
   This source distribution is not a .uproject and supplies no prebuilt DLLs.
4. Build the sidecar separately; from `Tools/MMVR_FingerProbe`:

   ```powershell
   .\fetch_openvr.ps1
   .\build.ps1
   .\bin\MMVR_FingerProbe.exe --self-test
   ```

The fetch script downloads the official OpenVR **1.5.17** x64 library/DLL with
pinned SHA-256 checks. Header/license are included; SDK binaries and build outputs
remain ignored. No Unreal installation path is required by the sidecar.

## Launch

Start SteamVR manually, turn on both Knuckles, launch normal desktop PIE,
Standalone or the Development package, and follow MMVR's usual tracker assignment,
Calibration -> Actor Mode workflow. In a separate PowerShell terminal:

```powershell
.\bin\MMVR_FingerProbe.exe --osc-splays --debug
```

Run that command from the sidecar directory. Ctrl+C stops it. Game-before-probe
and probe-before-game are supported. Keep only one producer and one game consumer
per port. **Do not embed/copy the probe into the Unreal package** or start it from
a second finger-animation system. Keep its generated bin directory complete.

Enable splays explicitly in the Unreal console (they default OFF):

```text
MMVR.Fingers.ExternalEnabled 1
MMVR.Fingers.ExternalSplayEnabled 1
```

## Runtime controls

| CVar | Default | Purpose |
|---|---:|---|
| MMVR.Fingers.ExternalEnabled | 1 | 0: all external fingers OFF, original simulation |
| MMVR.Fingers.ExternalSplayEnabled | 0 | 0: external curls only; 1: also eligible splays |
| MMVR.Fingers.Enabled | 1 | Receiver OFF/ON; closes/recreates its socket |
| MMVR.Fingers.Port | 19761 | Receiver port at next creation; producer must match |
| MMVR.Fingers.TimeoutMs | 250 | Acquisition-age expiration per hand/data family |
| MMVR.Fingers.HUD | 1 | Diagnostic display only |
| MMVR.Fingers.HUDHz | 8 | Display refresh only, adjustable 1..30 Hz |
| MMVR.Fingers.LogInterval | 1 | Summary interval in seconds; 0 disables summaries |

Capture targets **90 Hz per hand**; caches update on the network worker and
animation consumes the latest eligible sample at MMVR's normal tick. HUD OFF
does not stop transport/fusion. No additional smoothing, deadzone or artistic
curve is applied. FingerBreathing and Thumb_Perpendicular remain historical.

## Channels and protocol

Endpoint **127.0.0.1:19761**, one UDP socket, separate atomic OSC messages:

| Family | LEFT / RIGHT addresses | Tags | Values |
|---|---|---|---|
| Curls | `/mmvr/fingers/v1/left`, `/mmvr/fingers/v1/right` | `,shhhhiifffff` | Thumb, Index, Middle, Ring, Pinky |
| Splays | `/mmvr/fingers/v1/splay/left`, `/mmvr/fingers/v1/splay/right` | `,shhhhiiffff` | ThumbIndex, IndexMiddle, MiddleRing, RingPinky |

Metadata, in order: session (32 lowercase hex GUID), session-start QPC int64,
per-hand sequence int64, acquisition QPC int64, QPC frequency int64, validity int32,
tracking int32 (-1 unknown, 0 Estimated, 1 Partial, 2 Full). Values follow as
float32. Curls and splays from one acquisition share metadata. Sessions are common
to the producer, sequences independent per hand. The legacy curl payload remains
unchanged; `--osc` is curl-only, `--osc-splays` is additive.

FromDevice nominal ranges: curl 0 straight -> 1 curled; splay 0 touching -> 1
separated. Mapping is direct to corresponding L/R struct fields. Values are finite
and unclamped; anatomy need not reach exact endpoints. Partial tracking describes
hardware capability, not dynamic confidence. No summary field provides a justified
Thumb_Perpendicular mapping, so that field is not derived from curls/splays.

The receiver filters loopback sources (its socket binds wildcard due to the Epic
OSC implementation). It rejects malformed, out-of-order, retired-session and
already-stale acquisitions. QPC is comparable only on the **same Windows machine**
and dates the API acquisition, not the sensor's internal clock. This protocol does
not authenticate multiple local producers or synchronize machines.

## Fallback and lifecycle

The fusion copies the original simulated struct, then overrides only eligible
fields. LEFT invalid/absent/stale -> LEFT historical fallback; RIGHT continues,
and vice versa. Invalid splays never cancel valid curls. A stopped producer causes
expiration at 250 ms; no last external pose is retained as valid. Receiver absent
or disabled gives historical simulation with Fusion still installed.

New producer sessions are recognized; game restart recreates the receiver and
reacquires a still-running producer. One GameInstance owns the socket and releases
it on teardown. Simultaneous PIE/Standalone/package consumers on 19761 are rejected;
close the previous consumer or deliberately configure different ports.

## Standalone, packaging and validation scope

The source-identical finger implementation and integrated component were hardware
validated through PIE, Standalone Game and a **Windows Development Package** on the
tested UE 5.8.3 / Index setup: ten curls, eight splays, combined gestures, toggles,
independent L/R/BOTH OFF/ON, producer/game restarts, both launch orders and normal
Calibration/Actor Mode/tracking/input/Quick Select. Full Rebuild remained **OFF**.

Runtime Fusion and Receiver require no FusionEditor audit module; that module is
not distributed. Cook the MMVR example map in your own host, enable the runtime
dependencies, and retain the existing OpenVR manifest/DLL staging. The external
probe remains separately built/launched. No Shipping or universal host/rig guarantee
is implied; the public host configuration and historical content exclusions remain
the user's responsibility. See [validation scope](UE58_VALIDATION.md).

## Troubleshooting and limitations

- INVALID: check SteamVR, connected Knuckles, sidecar API/origin/role diagnostics
  and whether another game already owns the port. Do not change Epic's bindings
  or invent controller inputs to force a pass.
- Curls work but splays do not: use --osc-splays and ExternalSplayEnabled 1.
  Splay failure preserves curls. HUD RAW labels are historical diagnostics;
  animation is governed by the toggles and MMVR's existing mode guards.
- Values arrive but Manny is in Calibration Mode: perform the normal MMVR workflow;
  the fusion does not bypass target/AnimInstance selection or force Actor Mode.
- HMD standby itself is compatible. If the known inactive-action startup occurs,
  one physical HMD wake may be needed; no fake presence, power cycle or automated
  SteamVR restart. See [OpenVR troubleshooting](OpenVR_Input_Troubleshooting.md).
- One apparent loss of both hands after a single-controller OFF was observed once
  in Standalone and not reproduced in targeted repeats or the Development package.
  No fix is claimed. If repeated, stop and capture game/probe logs and SteamVR state
  together before changing the pipeline.

Windows x64, local loopback, one Index pair; no full bones, thumb opposition,
multiplayer, recording integration or artistic tuning. Historical project warnings
are not finger fixes. Existing body/calibration/arms behavior is unchanged.

## Attribution and licenses

MegaMocapVR is the original project by
[Megasteakman](https://github.com/Megasteakman/MegaMocapVR). The UE 5.8 fixes and
finger integration are contributions of this community fork.

Newly authored native plugins and probe: MIT notices in their directories.
MMVR content and Blueprint modifications retain upstream GPL-3.0. Unreal/Epic
dependencies are supplied by the user's licensed engine and are not bundled.
The probe's official Valve OpenVR header/license retains Valve's BSD-3-Clause terms;
downloaded libraries/DLL and generated binaries must preserve those notices.

Special thanks to **toyxyz** and the reference projects
[Openvr_toolkit](https://github.com/toyxyz/Openvr_toolkit) and
[toyxyz-vr-mocap](https://github.com/toyxyz/toyxyz-vr-mocap) for helping explain
OpenVR skeletal actions, background acquisition, hand skeletons/summaries and
capture architecture. Openvr_toolkit informed the Background initialization and
minimal skeletal manifest layout (the JSON layout was adapted); the probe C++
is newly written. toyxyz-vr-mocap served as an architectural reference, not an
imported animation plugin. Direct author permission was granted: “Feel free to use it. You are welcome
to copy, modify, and reuse the code without restriction.” This is not a claim of
MIT/BSD/GPL licensing for either Toyxyz repository. See the
[probe NOTICE](../Tools/MMVR_FingerProbe/NOTICE.md) for exact provenance and credits.
No official affiliation or endorsement is implied.
