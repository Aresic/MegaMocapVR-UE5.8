# UE 5.8 source contribution: validation and known limitations

Updated October 8, 2026, after recovery of the interrupted local validation.
**Ready for source publication with reservations.** This is not a certification
of the final packaged application or all hardware/runtime behavior.

## Completed checks

- The five corrected MMVR assets loaded and compiled in an independent UE 5.8.3
  project without MMVROpenVRInput or the private TESTXR diagnostic tools.
- A dependency inspection completed with exit code 0. Its content dependency
  closure covered 287 packages, with no detected dependency on the bridge or
  private TESTXR tools and no missing hard content references.
- The cleaned bridge compiled successfully for Editor Win64 Development in an
  independent host. A separate editor `-game` smoke test loaded its console
  variables and command and shut down normally. That host contained no MMVR
  content, and the bridge was disabled during the smoke test; input routing,
  source creation and dashboard preparation were not exercised.
- The recovered ten-scenario arm sweep matched the historical TESTXR result
  exactly for the compared hand/control coordinates. The sweep was not rerun
  during recovery, and its overall commandlet result was not successful: see
  the pre-existing RigVM errors below.

Historical TESTXR builds, packaging, input automation and hardware tests provide
additional development evidence, but do not certify the cleaned distribution
in every configuration. The cleaned bridge's recovered build is an Editor
Win64 Development build, not a cleaned Shipping or final packaged build.

## Known reservations inherited from earlier content or tests

Two missing **soft** references were found in the inspected content closure:

| Referencing asset | Missing target |
|---|---|
| `Assets/Animations/CalibratingCharacterPath` | `/Game/Untitled2` |
| `Blueprints/UI/WBP_MMVR_Teleprompter` | `/Game/MegaMocapVR/Components/MMVR_Teleprompter` |

These are reached from PlayerPawn through SpectatorCamera and
TeleprompterScreen, respectively. Both referencing assets are byte-identical to
the original reference content. The references are inherited, not introduced
by the five corrected assets. Their effect on the corresponding runtime uses
remains unverified. They do not establish a dependency on TESTXR or the bridge.

The recovered arm sweep logged **two occurrences** of the RigVM error
`Array Index (3) out of bounds (count 3)`. The same errors already occur in the
historical TESTXR `VerifySavedFix.log`. Although the sweep assertions completed
and the coordinates matched, the recovered commandlet reported failure.
These are known pre-existing test errors, **not confirmed regressions caused by
preparing this fork**. An earlier explanation attributing a nonzero result only
to DDC availability does not account for these errors.

The independent compilation also emitted an ImportText message about a quoted
`FootL Planted` string, a timeline curve warning and legacy OSC input mapping
warnings in the minimal host. The five selected assets still compiled; this
does not certify the affected ancillary behavior. No source or solver changes
were made to address these reservations.

## Validation still required

- Runtime and hardware tests of the **cleaned bridge**: actual desktop input
  routing, release/cleanup, automatic source creation, first-session dashboard
  preparation and manual retry.
- A **final packaged host**: build/cook/staging, portable runtime paths, loose
  action/binding JSON files, OpenVR DLL staging and executable startup/runtime.
- Hardware acceptance of Quick Select at the final **7.5 cm** threshold
  (`MenuScale = 0.75`). Previous spatial-selection tests do not certify this
  final threshold.
- Runtime assessment or explicit acceptance of the inherited soft references,
  and a fully error-free execution of the historical RigVM sweep.
- The remaining post-fix arm tracking hardware checks described in development
  evidence; compilation and deterministic coordinates do not replace them.

No claim of a fully validated packaged or hardware release is made. See the
[installation guide](UE58_INSTALLATION.md) for prerequisites and configuration.
The [integration review](MMVROpenVRInput_Integration_Review.md) records the earlier
source extraction/static review, before these recovery results.