# Attribution and SDK provenance

The OpenVR initialization sequence and skeletal manifest/binding layout are inspired by Toyxyz Openvr_toolkit, revision `9aa9a32bbf74ef0f0ae2fd025c3db3dcc0915042`, especially `src/vr/OpenVRProvider.cpp`, `src/vr/OpenVRProviderSkeletal.cpp` and the skeletal JSON files in `assets/`.

The minimal manifest/binding layout is adapted from that reference with a separate namespace, initially left hand only in V0/V1 and extended to independent left/right actions in V1.1. The probe C++ is newly written for summary acquisition; no toolkit application, full-bone capture, UI, solvers or streaming code is included. The older toyxyz-vr-mocap architecture is not incorporated.

Toyxyz provided this direct permission to the project owner: "Feel free to use it. You are welcome to copy, modify, and reuse the code without restriction." This is author permission, not a claim that the repositories have a standard MIT/BSD/GPL license.

Special thanks to **toyxyz** for the OpenVR skeletal tracking reference implementation and explicit permission to reuse, modify and adapt code and ideas. These references helped explain skeletal actions, background acquisition, hand skeletons/summaries and the general OpenVR capture architecture. This lightweight sidecar is tailored for MegaMocapVR/Unreal Engine; only the minimal JSON layout is adapted as described above. No affiliation or endorsement is implied.

References: [toyxyz/Openvr_toolkit](https://github.com/toyxyz/Openvr_toolkit) and [toyxyz/toyxyz-vr-mocap](https://github.com/toyxyz/toyxyz-vr-mocap).

This source distribution includes the official Valve OpenVR 1.5.17 header and LICENSE. The matching x64 import library and DLL are not committed; fetch_openvr.ps1 retrieves the official files from tag `v1.5.17` and verifies their pinned SHA-256 hashes before build. This is the same SDK revision selected by Epic LiveLinkOpenVR UE 5.8, but uses the unmodified Valve release instead of Epic's annotated header and different DLL. No Epic header, plugin or runtime code is retained. Building and running require no Unreal installation path. File provenance and SHA-256 values are recorded in `third_party/openvr-1.5.17/PROVENANCE.md`.

The Valve copyright notice and redistribution terms are preserved in `third_party/openvr-1.5.17/LICENSE` and the locally built `bin/OPENVR_LICENSE.txt`.

The newly authored probe code and tooling are licensed under MIT (LICENSE). This does not relicense Toyxyz reference projects, the Valve SDK, Unreal Engine or the separate GPL-3.0 MMVR content.
