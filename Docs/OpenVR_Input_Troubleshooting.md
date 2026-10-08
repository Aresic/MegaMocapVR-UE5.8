# OpenVR controller input troubleshooting

For UE 5.8, SteamVR, LiveLinkOpenVR and Valve Index Controllers in a Windows
desktop workflow: controllers track correctly, but buttons, triggers or sticks
do not respond.

LiveLinkOpenVR supplies tracking and the `OpenVRInput` subject. This fork's
optional **MMVROpenVRInput desktop bridge** routes that subject to MMVR's existing
controller mappings outside stereo VR. The commands below belong to this bridge,
not upstream MegaMocapVR or Epic LiveLinkOpenVR. See the
[installation guide](UE58_INSTALLATION.md) for prerequisites.

Two different SteamVR conditions can produce this symptom. Successful tracking
or a fresh Live Link frame does **not** establish that controller actions are
available.

## Commands and diagnostic availability

Enter commands in Unreal's console while SteamVR and LiveLinkOpenVR are running.

| Command | Purpose | Availability |
|---|---|---|
| `MMVR.OpenVRInput.PrepareDesktop` | Prepare desktop input by requesting SteamVR Dashboard dismissal when it is open. | Included in this fork. |
| `MMVR.OpenVRInput.AutoPrepareDesktop 1` | Enable automatic desktop preparation (default). | Included in this fork. |
| `MMVR.OpenVRInput.AutoPrepareDesktop 0` | Disable automatic preparation; use the manual command when needed. | Included in this fork. |
| `MMVR.OpenVRInput.Debug 1` | Enable detailed OpenVR bridge diagnostics. | Included in this fork; detailed logging is off by default. |
| `MMVR.OpenVRInput.Debug 0` | Disable detailed diagnostics. | Included in this fork; detailed logging is off by default. |

If `Debug` is reported as an unknown command, check that the bridge plugin is
enabled and built from the current source version.

Check the Output Log:

- `available` reports SteamVR input availability for the application.
- `dashboard=1` means the SteamVR Dashboard is visible; `0` means it is closed.
- `hmdActivity=3` means HMD Standby.
- `active=1` means an action is available, **not** that a button is pressed.
  Zero values are normal when controls are released.
- `activeMask=0x0000` means all 16 sampled actions are inactive; `0xffff` means
  all 16 are active. This mask does not cover every entry in the action manifest.

## Quick troubleshooting tree

```text
Controllers track, but buttons/triggers/sticks do not respond
  |
  +-- Enable MMVR.OpenVRInput.Debug 1
       |
       +-- Actions active?
            |
            +-- Yes -> Press a control and check its values.
            |          If values change, check game-window focus,
            |          bridge prerequisites and MMVR input routing.
            |
            +-- No -> SteamVR Dashboard open/capturing input?
                       |
                       +-- Yes -> MMVR.OpenVRInput.PrepareDesktop
                       |          Confirm dismissal, then re-test.
                       |
                       +-- No -> Controllers connected, bindings valid,
                                 HMD in standby, actions still inactive?
                                   |
                                   +-- Yes -> Wake the HMD once; wait
                                   |          a few seconds; re-test.
                                   |
                                   +-- No / still failing -> Check bindings,
                                              focus and SteamVR runtime state.
```

Close the Dashboard first. If input still fails
with connected controllers and valid bindings, try the one-time physical wake
when the HMD is in standby. After successful initialization, it can return to
standby without losing controller input in the observed setup.

## Case 1: SteamVR Dashboard captures input

Tracking can remain available while the Dashboard captures controller actions.
Run:

```text
MMVR.OpenVRInput.PrepareDesktop
```

The bridge uses the installed SteamVR utility to request Dashboard dismissal;
it does not change headset presence or power settings. If the utility is missing
or fails, close the Dashboard manually. A logged request is not confirmation
that dismissal succeeded: re-test the controls. The small desktop SteamVR status
window is distinct from the Dashboard.

When `AutoPrepareDesktop` is enabled, preparation is attempted after a fresh
Live Link frame reaches an eligible local MMVR player. It stops after a successful
preparation request or confirmation that the Dashboard is already closed.
Failures are retried at most five times, two seconds apart. It does not keep
closing a Dashboard deliberately reopened later; use the manual command again.

**PrepareDesktop is not a universal input reset.** If the Dashboard is already
closed, it does not reinitialize controller actions.

## Case 2: known SteamVR action initialization issue

An intermittent startup condition was reproduced with the Valve Index HMD
already in standby, connected controllers, working tracking, valid OpenVR
bindings and the Dashboard closed:

```text
available=0  dashboard=0  hmdActivity=3
activeMask=0x0000  actions active=0
```

PrepareDesktop did not resolve this condition. The validated workaround was:

1. Physically wake the HMD once by briefly covering its presence sensor.
2. Wait a few seconds, then re-test buttons, triggers and sticks.
3. In the reproduced session, availability changed to `available=1`,
   `activeMask=0xffff` and actions `active=1`, without restarting anything.
4. Uncover the sensor and allow the HMD to return to standby normally.
5. Inputs continued working with `hmdActivity=3`, `available=1` and active actions.

**Standby itself does not disable controller input.** This issue concerns some
startup sessions where actions remain inactive until a first physical wake.
The wake appears to prompt a SteamVR availability/focus reevaluation; the exact
internal cause has not been demonstrated. No safe software recovery API has
been identified for this observed issue. Keeping the sensor covered or changing
power settings is not required by this workaround.

The behavior was confirmed in standard PIE with UE 5.8.3, SteamVR 2.17.10 and
Valve Index hardware. It is not a universal guarantee across hardware or runtime
versions. Standalone and packaged behavior, and hardware operation of the
cleaned distribution, still require separate validation; see the
[validation limits](UE58_VALIDATION.md). This observed SteamVR condition is not
established as an upstream MegaMocapVR defect.

## Standby diagnostic warning

The bridge includes a passive standby check that can display:

> OpenVR controller actions are inactive while the HMD is currently in standby. Waking the HMD once may reinitialize SteamVR input actions. If the issue persists, check bindings, focus and runtime state.

This warning deliberately describes an observed combination of states. It does
not claim that standby caused the failure or that waking will always resolve it.
The check runs at most once per second for an eligible local desktop MMVR
player. It requires a closed Dashboard, unavailable input, a connected standby
HMD and controller, and all 16 sampled actions readable but inactive. Warnings
are limited to one every 30 seconds. Disabling detailed logging with `Debug 0`
does not disable this passive warning. Neither diagnostics nor the warning
initialize OpenVR, update action sets, change bindings, wake the HMD or reset
tracking. Run `MMVR.OpenVRInput.Debug 0` when detailed logs are no longer needed.
