# Desktop OpenVR input bridge architecture

The optional MMVROpenVRInput runtime plugin connects Epic LiveLinkOpenVR's
`OpenVRInput` subject to MegaMocapVR's existing Valve Index controller mappings
in a Windows x64 desktop workflow. See the [installation guide](UE58_INSTALLATION.md)
and [validation limits](UE58_VALIDATION.md).

## Input ownership and routing

Epic LiveLinkOpenVR owns the OpenVR session, action manifest and action-state
updates. The bridge reuses or creates its public Live Link source, evaluates the
input subject and injects controller keys into an eligible local MMVR player's
EnhancedPlayerInput outside stereo VR. It does not modify the Input Actions,
Mapping Contexts, body tracking, calibration or Control Rigs.

Missing or stale frames and player/world cleanup release injected keys. A fresh
Live Link frame alone does not establish that SteamVR actions are available.
The adapter does not replace Epic's plugin or use a second calibration system.

## Desktop preparation and diagnostics

`MMVR.OpenVRInput.PrepareDesktop` requests Dashboard dismissal through the
installed SteamVR utility. Automatic preparation is bounded and does not keep
closing a Dashboard deliberately reopened later. It does not reset actions or
change headset presence/power settings.

`MMVR.OpenVRInput.Debug 1` enables passive runtime/action and bridge snapshots;
`0` disables detailed logging. A separately throttled standby warning describes
an observed inactive-action condition. Diagnostics do not initialize/shut down
OpenVR, update action sets, poll events, replace bindings or reset tracking.
See [troubleshooting](OpenVR_Input_Troubleshooting.md) for the two distinct
SteamVR conditions and the one-time physical wake workaround.

The runtime distribution excludes temporary arm-chain diagnostics, tests,
editor repair tools, private host configuration and generated binaries. It has
no ControlRig/RigVM diagnostic dependency or local project-path dependency.

## Dependencies and distribution

The plugin requires Live Link, Epic LiveLinkOpenVR, Enhanced Input and the
installed OpenVR SDK. Build rules stage Epic's installed action manifest and
knuckles binding as loose NonUFS files for local packaged hosts; those files,
engine source and SDK DLLs are not bundled in this Git distribution.
Windows native arm64 is excluded for the stock dependency layout.

MMVR content retains upstream GPL-3.0. The original adapter is MIT; this does
not relicense Epic/Valve dependencies. See the plugin's
[provenance and dependency notices](../Plugins/MMVROpenVRInput/THIRD_PARTY_NOTICES.md).
No Toyxyz skeletal finger tracking implementation is included.
