# Provenance and dependency notices

The original adapter sources and documentation in this plugin directory are
licensed under MIT; see LICENSE. The original adapter is contributed by Aresic.
This is not an Epic plugin fork
and does not contain a copied LiveLinkOpenVR implementation or MMVR Blueprint
code. The distribution includes passive OpenVR input diagnostics; temporary
arm-chain probes and editor repair tools are excluded.

The separate MegaMocapVR content and its modifications retain the upstream
GPL-3.0 license in the repository root. The plugin's MIT license does not
relicense that content or any third-party dependency.

## External dependencies (not bundled)

- Unreal Engine and Epic LiveLinkOpenVR: supplied by the user's licensed UE 5.8
  installation and governed by Epic's terms. Engine headers, source,
  DLLs and action/binding JSON files are not included in this Git distribution.
  The build stages the installed engine's two JSON files for local packaged
  applications; this is not a grant to redistribute Epic files under MIT/GPL.
  See https://www.unrealengine.com/eula/unreal (sections 5 and 6(c)).
- Valve OpenVR SDK: the installed Epic plugin supplies OpenVR 1.5.17. Its local
  LICENSE is BSD-3-Clause, copyright (c) 2015 Valve Corporation. No SDK headers,
  libraries or DLLs are bundled here. Future binary releases must preserve the
  required Valve notices. See https://github.com/ValveSoftware/openvr/blob/master/LICENSE
  and the LICENSE shipped with the installed SDK.
- SteamVR and vrcmd.exe: supplied by the user's SteamVR installation, not copied
  or relicensed. Desktop preparation invokes the installed tool.
- Toyxyz: no source, manifest, skeletal finger tracking implementation or asset
  is included. Any future incorporation requires a separate rights review.

Only the original adapter is licensed by this plugin. Unreal Engine remains
subject to Epic's terms; do not apply the repository's content GPL license to
Epic's engine code or treat this source package as a license to distribute it.
