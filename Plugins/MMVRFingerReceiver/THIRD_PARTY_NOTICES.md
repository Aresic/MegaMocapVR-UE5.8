# Source and dependency notices

The newly authored native receiver is MIT, Copyright (c) 2026 Aresic; see LICENSE.
It uses the public Epic OSC parser/server interface and Windows QPC, with a
bounded latest-sample cache. No Epic OSC implementation or Toyxyz source is copied.
Reusable, explicitly invoked parser/cache/UDP self-tests are included because
they are linked by the runtime diagnostic command; private audit scripts and
fixtures are excluded.

Unreal Engine and OSC are supplied by the user's licensed UE 5.8 installation
under Epic's terms. MMVRFingerFusion is a separate MIT adapter. The MMVR content
and Blueprint modifications retain upstream GPL-3.0 in LICENSE.md at repository
root. No engine headers, libraries or binaries are bundled with this plugin.
