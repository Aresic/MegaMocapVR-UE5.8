// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Roles/LiveLinkInputDeviceTypes.h"

// An adapter only: poses, calibration and the MMVR solver never pass through here.
struct FMMVROpenVRInputState
{
    TSet<FKey> Pressed;

    void Apply(UPlayerInput& PlayerInput, const FLiveLinkGamepadInputDeviceFrameData& Frame)
    {
        auto Axis = [&PlayerInput](FKey Key, float Value)
        {
            PlayerInput.InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Axis,
                FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f, 1));
        };
        auto Button = [this, &PlayerInput](FKey Key, float Value, float Press = 0.5f, float Release = 0.5f)
        {
            const bool WasDown = Pressed.Contains(Key);
            const bool Down = FMath::IsFinite(Value) && Value >= (WasDown ? Release : Press);
            if (Down != WasDown)
            {
                PlayerInput.InputKey(FInputKeyEventArgs::CreateSimulated(Key, Down ? IE_Pressed : IE_Released, Down ? 1.0f : 0.0f));
                if (Down) Pressed.Add(Key); else Pressed.Remove(Key);
            }
            else if (Down)
            {
                // UE 5.8 reconciles a held key after FlushPressedKeys on its next repeat.
                PlayerInput.InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Repeat, 1.0f));
            }
        };
        Button(EKeys::ValveIndex_Left_A_Click, Frame.FaceButtonLeft);
        Button(EKeys::ValveIndex_Left_B_Click, Frame.FaceButtonTop);
        Button(EKeys::ValveIndex_Right_A_Click, Frame.FaceButtonBottom);
        Button(EKeys::ValveIndex_Right_B_Click, Frame.FaceButtonRight);
        Button(EKeys::ValveIndex_Left_Thumbstick_Click, Frame.LeftThumb);
        Button(EKeys::ValveIndex_Right_Thumbstick_Click, Frame.RightThumb);
        // Epic's stock manifest only supplies trigger pull, not its hardware click.
        Button(EKeys::ValveIndex_Left_Trigger_Click, Frame.LeftTriggerAnalog, 0.9f, 0.8f);
        Button(EKeys::ValveIndex_Right_Trigger_Click, Frame.RightTriggerAnalog, 0.9f, 0.8f);
        Axis(EKeys::ValveIndex_Left_Trigger_Axis, Frame.LeftTriggerAnalog);
        Axis(EKeys::ValveIndex_Right_Trigger_Axis, Frame.RightTriggerAnalog);
        Axis(EKeys::ValveIndex_Left_Grip_Axis, Frame.LeftShoulder);
        Axis(EKeys::ValveIndex_Right_Grip_Axis, Frame.RightShoulder);
        // InputCore pairs X/Y into the 2D keys already mapped by MMVR.
        Axis(EKeys::ValveIndex_Left_Thumbstick_X, Frame.LeftStickRight);
        Axis(EKeys::ValveIndex_Left_Thumbstick_Y, Frame.LeftStickUp);
        Axis(EKeys::ValveIndex_Right_Thumbstick_X, Frame.RightStickRight);
        Axis(EKeys::ValveIndex_Right_Thumbstick_Y, Frame.RightStickUp);
        Axis(EKeys::ValveIndex_Left_Trackpad_X, Frame.LeftAnalogX);
        Axis(EKeys::ValveIndex_Left_Trackpad_Y, Frame.LeftAnalogY);
        Axis(EKeys::ValveIndex_Right_Trackpad_X, Frame.RightAnalogX);
        Axis(EKeys::ValveIndex_Right_Trackpad_Y, Frame.RightAnalogY);
        Axis(EKeys::ValveIndex_Left_Trackpad_Force, Frame.SpecialLeft);
        Axis(EKeys::ValveIndex_Right_Trackpad_Force, Frame.SpecialRight);
        // Grip force and capacitive touch are absent from Epic's stock binding.
        // Do not manufacture either from grip pull or a nonzero stick position.
    }

    void Release(UPlayerInput& PlayerInput)
    {
        Apply(PlayerInput, FLiveLinkGamepadInputDeviceFrameData{});
    }
};
