# UE 5.8 validation and known limitations

Updated October 8, 2026: **pre-finger-tracking checkpoint**.
The development hardware baseline has no currently observed blocking issue.
This is a source contribution, not certification of every host, runtime or
packaged application. No skeletal finger tracking is included.

## Development hardware acceptance

Recent desktop hardware checks found no blocking issue with Vive/Index tracking,
calibration, Actor Mode, hand/arm tracking, the OpenVR bridge, Enhanced Input or
Quick Select. Spatial selection at **7.5 cm** (`MenuScale = 0.75`) is working.
These are qualitative development results, not exhaustive measurements or
proof that the final extracted distribution has been exercised on hardware.

Two distinct SteamVR cases were reproduced with Valve Index in standard PIE:
Dashboard input capture can be cleared with PrepareDesktop; an intermittent
inactive-action startup state with the Dashboard closed was cleared by a single
physical HMD wake. Actions then remained active after return to standby.
The exact internal cause is unproven. See [troubleshooting](OpenVR_Input_Troubleshooting.md).

## Checks of the current source distribution

- The bridge with passive diagnostics and the standby warning compiled for
  **Editor Win64 Development** in an independent UE 5.8.3 host, including a unity
  build. It has no arm-chain diagnostic or private-host dependency.
- A separate editor `-game` smoke test loaded the runtime module and accepted
  `MMVR.OpenVRInput.Debug 1` and `0`, then exited normally. The bridge was disabled;
  this checks loading/registration, not controller routing or Dashboard dismissal.
- The final Pawn and Quick Select wheel compiled with **zero compiler errors
  and warnings**, were saved, reloaded in another process and compiled again.
- **25 compiled spatial/lifecycle assertions passed** before and after reload:
  directions, 7.5 cm threshold, neutral release, unique consumption, invisible
  wheel cancellation, animation stability and two-A cancellation.
- Source-graph comparison restricted Pawn changes to left-Up dispatch and wheel
  changes to the neutral visual hook. Other directions, right-wheel routing,
  stick behavior and calibration/physics graphs were unchanged.
- Four visual assignment checks covered left/right UV rows in body and VTuber
  modes: left Up uses the neutral material; the right row and other buttons keep
  their original materials. These are editor checks, not a new headset render test.
- Left Up is unassigned. No Actor/Director/Recovery action is attached to it;
  no experimental Recovery function or texture is included.

## Earlier validation retained

The earlier five corrected assets compiled independently without the bridge.
Their inspected dependency closure contained no missing hard content reference;
two inherited missing soft references remain below. The Soft IK arm input fix
is unchanged. A recovered ten-scenario sweep matched its historical control/hand
coordinates, but reported pre-existing RigVM errors; it was not an error-free run.

Historical development build/cook/package successes do **not** establish that
the current complete host or cleaned distribution has been repackaged. This
publication did not create a packaged build.

## Known limitations

| Referencing asset | Inherited missing soft target |
|---|---|
| `Assets/Animations/CalibratingCharacterPath` | `/Game/Untitled2` |
| `Blueprints/UI/WBP_MMVR_Teleprompter` | `/Game/MegaMocapVR/Components/MMVR_Teleprompter` |

The referencing assets are unchanged; their affected runtime uses remain
unverified. The historical arm sweep logged two occurrences of
`Array Index (3) out of bounds (count 3)`. These are pre-existing reservations,
not confirmed regressions introduced by this publication. A timeline curve
warning, legacy OSC mapping warnings and an ImportText message about
`FootL Planted` were also recorded in earlier independent validation.

A separate unresolved ragdoll/physics incident involving B/B+B predates the
Recovery experiment. No ragdoll fix is claimed. Soft Recovery remains paused
and is not distributed; restarting PIE is the current manual workaround for
a severe spatial problem during testing.

## Checks still required

- Hardware operation of the **extracted distribution**: source creation,
  controller routing, release/cleanup, first-session automatic Dashboard
  preparation, manual retry and standby-warning guards.
- Targeted hardware confirmation of the blank, unassigned left-Up slot in body
  and VTuber modes, with other directions and the right wheel preserved.
- Standalone desktop and a final packaged host: cook/staging, portable native
  paths, loose action/binding JSON files, OpenVR DLL and executable runtime.
- Assessment/acceptance of inherited soft references and an error-free RigVM
  sweep; broader arm/retargeting configurations remain outside current acceptance.

Compilation and editor tests do not replace these checks. Follow the
[installation guide](UE58_INSTALLATION.md) for host prerequisites and setup.
