# MMVR OpenVR finger sidecar

Windows x64 source tool for Valve Index / Knuckles. It reads five curls and four
splays per hand from OpenVR skeletal summaries and optionally sends OSC to MMVR.
It is a separate process, not an Unreal plugin or an embedded packaged executable.
See the [complete setup guide](../../Docs/Finger_Tracking_OpenVR.md).

## Build

Install Visual Studio 2022 with Desktop development with C++ and a Windows SDK.
CMake 3.21+ is required; build.ps1 also finds Visual Studio's bundled CMake.
From this directory, in PowerShell:

```powershell
.\fetch_openvr.ps1
.\build.ps1
.\bin\MMVR_FingerProbe.exe --self-test
.\bin\MMVR_FingerProbe.exe --timing-test --duration 4 --hz 90
```

fetch_openvr.ps1 downloads only the official OpenVR 1.5.17 x64 import library and
DLL, verifying their pinned SHA-256 hashes. The header and Valve LICENSE are
included as source. No precompiled sidecar, SDK binary or generated build output
is committed. Release uses C++17, /W4 /WX and the static MSVC CRT.
No Unreal installation is needed to build the tool.

## Run

Start SteamVR manually, turn on both Knuckles, then run:

```powershell
.\bin\MMVR_FingerProbe.exe --osc-splays --debug
```

Ctrl+C stops it. Default: FromDevice, 90 Hz per hand, UDP 127.0.0.1:19761,
console about once per second. --osc sends curls only; --osc-splays sends both
families on the same socket. With neither option it is console-only.
--duration 30 bounds a run; --hz 60 changes capture cadence; --osc-port 19762
changes the port (configure the consumer to match). --summary animation selects
FromAnimation explicitly; it is not a silent fallback.

Keep the locally generated bin directory together: executable, openvr_api.dll,
config JSON, OPENVR_LICENSE.txt and NOTICE.md. Paths resolve relative to the
executable, independently of the working directory. Do not copy this directory
into an Unreal package or start multiple producers on the same port.

LEFT/RIGHT VALID requires successful API calls, active action, correct connected
Knuckles origin/source/role and finite values. Invalid acquisitions do not reuse
old values. Partial tracking is expected for Index and is not per-finger confidence.
SDK nominal curl: 0 straight, 1 curled; splay: 0 touching, 1 separated. Values are
not clamped or smoothed; anatomy may not reach both endpoints.

Only skeletal actions are declared. No button/axis/haptic actions, full bones,
poses, Scene application, compositor submission, focus overrides, SteamVR restart
or fake HMD presence/wake. Controller reconnect keeps polling; runtime shutdown
ends this process and requires manual relaunch.

## Diagnostic modes

--self-test validates numbers/origins; --timing-test measures the scheduler only.
--osc-test --osc-splays --duration 3 emits fixed distinct L/R curls and splays
without OpenVR. These modes are explicitly synthetic and never certify hardware.
No synthetic injection is active by default.

Exit codes: 0 clean exit; 1 arguments/internal error; 2 Background initialization
unavailable; 3 manifest/handle setup failure. Clean exit does not imply tracking valid.

## Credits and terms

Newly authored probe/tooling: [MIT](LICENSE). Toyxyz reference/adapted skeletal
manifest layout and direct author permission: [NOTICE](NOTICE.md); no standard
license is claimed for the Toyxyz repositories. Valve OpenVR is separately
BSD-3-Clause; retain [its LICENSE](third_party/openvr-1.5.17/LICENSE) and notices
when redistributing generated binaries. [SDK provenance](third_party/openvr-1.5.17/PROVENANCE.md).
