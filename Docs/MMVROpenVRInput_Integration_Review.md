# MMVROpenVRInput local integration review

2026-10-08. Prepared on `ue5.8-fixes` after asset commit `21c69b8`. No push,
upstream synchronization, Unreal compilation, packaging or hardware test.

## Provenance and licensing decision

The selected bridge is the local adapter developed for TESTXR, using public UE
Live Link/Enhanced Input and OpenVR APIs. Existing development reports describe
its implementation and validations. It is not a copy of Epic LiveLinkOpenVR;
no Toyxyz implementation was integrated in those reports or selected here.
Original adapter sources are explicitly MIT (Aresic, 2026). MMVR's original GPL-3.0
content/license is unchanged. The plugin is optional and has no compile-time
link to MMVR Blueprint implementations; it identifies a possessed pawn by path.
This separate license does not relicense MMVR or third-party code.

Epic engine code/JSON/binaries and Valve SDK files are dependencies, not copied
into the contribution. The installed OpenVR 1.5.17 LICENSE permits source/binary
redistribution under BSD-3-Clause with notices; no SDK redistribution is performed
here. SteamVR/vrcmd is used from the user's installation. No blocking provenance
issue was identified in the included adapter sources.

References checked: [Epic EULA, sections 5 and 6(c)](https://www.unrealengine.com/eula/unreal),
[Valve license](https://github.com/ValveSoftware/openvr/blob/master/LICENSE),
and the LICENSE bundled with Epic's OpenVR 1.5.17. Epic's restriction on
non-compatible licenses is why no Epic implementation is copied or labelled
under the content GPL. These notices grant no new rights in Epic/Valve software.

## Files and deliberate differences from TESTXR

- `Plugins/MMVROpenVRInput/`: descriptor, Build.cs, input module/state and desktop
  helper; plugin LICENSE and THIRD_PARTY_NOTICES. No Content, Binaries,
  Intermediate, local manifests, tests or captures.
- `README.md` links English `Docs/UE58_INSTALLATION.md`.
- Desktop helper extracted from the runtime portion of the former diagnostic
  class. Its OpenVR loading, dashboard check, CLI arguments and bounded retry
  policy are preserved. Renaming only; no new runtime ownership.
- Input state implementation preserved except license header. Tick/factory/
  freshness/release/player/source lifetime paths preserved; optional input
  snapshots/debug CVar and all hand-chain instrumentation removed.
- Removed ControlRig/RigVM/AnimGraphRuntime dependencies used only by excluded
  diagnostics. Remaining modules: Core, CoreUObject, Engine, InputCore,
  LiveLinkInterface, EnhancedInput, Projects and OpenVR. Plugin dependencies:
  LiveLink, LiveLinkOpenVR and EnhancedInput.
- Kept original NonUFS staging of Epic's two installed JSON files. Kept the SDK
  layout used by UE 5.8/OpenVR 1.5.17. Restricted platform to Windows x64;
  native arm64 is unsupported by the stock dependency.
- Five assets and original upstream license untouched. TESTXR and the original
  reference were read-only; 1,134 protected source/reference files were checked
  unchanged during extraction.

## Static verification and limits

Descriptor JSON, runtime-only module/dependency declarations, local includes,
source/header names, exact source transformations and expected Git file list
are checked before commit. The input state and desktop preparation bodies are
compared to the TESTXR versions; removed sections are diagnostic-only. The
engine's installed OpenVR Build.cs declares its library, delay-load DLL and
runtime staging. Required stock engine JSON/SDK files exist locally. No local
project paths, Toyxyz code, copied headers/DLLs/JSON or five-asset changes are
included. No build result is claimed for this distribution copy.

Historical tests belong to the TESTXR implementation: compilation in three
configurations, routing/release automation, isolated packaging and hardware PIE
inputs. The reports distinguish the tested CLI from first-session automatic
integration acceptance; this cleaned source distribution has not yet been run.

## Validation backlog retained

1. Separate UE 5.8 host: build/load the cleaned plugin, exercise desktop input,
   release/cleanup, auto source, first-session dashboard preparation and manual retry.
2. Confirm final packaged runtime paths/staging before a packaged-validation claim.
3. Five corrected assets in a separate UE 5.8 project without the bridge.
4. Hardware Quick Select final threshold 7.5 cm.

No heavy tests requested or run at this stage. Integration is ready for local
source review; future publication must retain the validation limits.

## Final static result

**PASS**: 11 expected contribution files, zero changes to `MegaMocapVR/`.
Input state matches TESTXR except SPDX; desktop preparation matches its original
function bodies after extraction/rename; source factory, identification and
cleanup functions match exactly. Descriptor, local includes, dependency files
and documentation links checked. `git diff --check` passed. No Unreal build or
runtime result is claimed. This review is included in the separate bridge commit
following `21c69b8`; the existing asset commit is preserved.

## Source snapshot (TESTXR, read only)

Paths below are relative to `Plugins/MMVROpenVRInput` in TESTXR. SHA-256 identifies
the versions used; distribution helper/module changes are explained above.

| Source | SHA-256 |
|---|---|
| `MMVROpenVRInput.uplugin` | `5b3dc42d033e1db6452f980201131abd64c3672e70d64cf314e05e9506eecf87` |
| `Source/MMVROpenVRInput/MMVROpenVRInput.Build.cs` | `ab92262c38255d132077f5d6bb74562e2103edd7b8f0c6f132e37a360edb71ce` |
| `Source/MMVROpenVRInput/Private/MMVROpenVRInputModule.cpp` | `c3e5b52674e4c7000f36f2d1d7138f59f188bf8345f0271cebaef784fdcd24c5` |
| `Source/MMVROpenVRInput/Private/MMVROpenVRInputState.h` | `5066bd52df29331aa6ade9691161c48ca14f3b3fae9a4990200f72209845c901` |
| `Source/MMVROpenVRInput/Private/MMVROpenVRInputDiagnostics.cpp` | `0facef32457df021537bb32d14548d892222091fe0e18a4a2453d85661ac01f9` |
| `Source/MMVROpenVRInput/Private/MMVROpenVRInputDiagnostics.h` | `5189168fcc60fb867428175193d33663f7dbd3224b3c57e6d903077cfcbfd149` |
