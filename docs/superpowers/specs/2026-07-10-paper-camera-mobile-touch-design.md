# Mobile Touch Camera Controls for 3D Paper Battle View

## Context

The 3D paper battle camera currently has desktop-only free-camera controls in `BattleSceneHades::dealEvent()`:

- `W/A/S/D` pans the paper camera center.
- `Z/X` changes camera distance.
- Arrow left/right rotates the camera.
- Arrow up/down changes camera height.
- `O` and the top-right `跟隨`/`自由` button toggle auto-center.

The Android build does not use the C++ `VirtualStick` path. `KysActivity` is a thin `SDLActivity` wrapper with asset extraction and one Java overlay button that injects a right click through JNI. Android already sets `SDL_HINT_TOUCH_MOUSE_EVENTS`, so taps can appear as mouse input, but multi-touch gestures need real SDL finger events in C++.

`Engine.h` defines `EVENT_FINGER_DOWN`, `EVENT_FINGER_MOTION`, and `EVENT_FINGER_UP`, and some menus already know how to interpret finger coordinates. However, `RunNode::isSpecialEvent()` currently does not pass finger events to scenes, so battle scenes cannot consume multi-touch reliably.

## Goals

- Give mobile players access to the full 3D paper camera adjustment set without virtual keys.
- Keep Java battle-state agnostic; camera behavior belongs in C++.
- Preserve existing mouse/tap top-right battle controls.
- Avoid duplicating camera math between desktop keyboard and mobile touch paths.
- Keep auto-center easy to return to with the existing `跟隨`/`自由` button.

## Non-Goals

- Do not add a mobile virtual keyboard or virtual gamepad.
- Do not move battle camera logic into Android Java.
- Do not change classic 2D battle camera behavior.
- Do not require Java to know whether the game is in 2D battle, 3D paper battle, menus, or other scenes.

## Approach

Use SDL finger events as the mobile input boundary.

1. Add `EVENT_FINGER_DOWN`, `EVENT_FINGER_MOTION`, and `EVENT_FINGER_UP` to `RunNode::isSpecialEvent()`.
2. Add a small C++ paper-camera touch controller used by `BattleSceneHades`.
3. Feed finger events to that controller only while `active_paper_battle_view_` is true.
4. Leave `KysActivity` unchanged unless device testing proves SDL finger events are not usable.

This keeps Android as a host and keeps game-specific gestures in the same layer as `paper_camera_angle_`, `paper_camera_distance_`, `paper_camera_height_`, `pos_`, and `paper_camera_auto_center_`.

## Gesture Design

One-finger drag pans the battlefield. This is the primary mobile replacement for `W/A/S/D`. The drag should feel like the ground follows the finger. Internally it moves `pos_` along the paper camera's ground-plane right/forward vectors and clamps with `clampPaperCameraCenter()`.

Any accepted one-finger pan switches `paper_camera_auto_center_` to `false`. The existing camera mode button remains the way to return to follow mode.

Two-finger pinch changes camera distance:

- Pinch out zooms in by reducing `paper_camera_distance_`.
- Pinch in zooms out by increasing `paper_camera_distance_`.
- The value remains clamped by `PAPER_CAMERA_MIN_DISTANCE` and `PAPER_CAMERA_MAX_DISTANCE`.

Two-finger centroid movement adjusts camera orientation and height:

- Horizontal centroid movement rotates `paper_camera_angle_`.
- Vertical centroid movement changes `paper_camera_height_`.
- Height remains clamped by `PAPER_CAMERA_MIN_HEIGHT` and `PAPER_CAMERA_MAX_HEIGHT`.

Do not use twist rotation for the first version. It is harder to discover, easier to trigger accidentally during pinch, and unnecessary because two-finger horizontal drag covers arrow left/right.

## Event Flow

`BattleSceneHades::dealEvent()` keeps the existing battle-control handling first. If a touch starts on a top-right battle control, the touch controller ignores that finger sequence so the existing synthetic mouse path can activate the button.

For finger events in paper view:

1. Convert `e.tfinger.x/y` from normalized window coordinates into window or present pixels.
2. Track active fingers by SDL finger id.
3. Ignore gestures that start outside the rendered present rect or over UI controls.
4. For one active tracked finger, apply pan deltas.
5. For two active tracked fingers, compare the current pair to the previous pair and apply pinch, horizontal centroid, and vertical centroid deltas.
6. Return whether the touch event was consumed for camera input, but do not stop battle simulation advancement for that frame.

Mouse events continue to handle the top-right battle controls. In active paper view, scene mouse drags do not use `BattleSceneCamera::handleManualInput()`, so synthetic touch mouse events should not double-apply camera movement.

The Java right-click overlay remains independent. Touches on its button are captured by Android and produce only the existing right-click JNI event.

## C++ Structure

Prefer a small helper rather than expanding `BattleSceneHades::dealEvent()` further.

Suggested shape:

- `BattleScenePaperCameraTouch.h/.cpp`
- A stateful class that tracks active fingers and previous gesture geometry.
- A result type with optional pan, distance, angle, height deltas, plus a consumed flag.
- Helper methods for UI hit testing should reuse the existing battle-control layout logic instead of duplicating rect math.

`BattleSceneHades` remains responsible for applying deltas because it owns the camera state, clamps, auto-center flag, and view vectors.

Desktop keyboard input remains as a separate input source but should share small camera-vector helpers with touch input where practical.

## Fallback

If Android device testing shows SDL finger events are missing or unusable, add a narrow JNI fallback:

- Override `KysActivity.dispatchTouchEvent()`.
- Always call `super.dispatchTouchEvent(event)` so SDL keeps normal input.
- Send normalized touch snapshots to native C++ through a new JNI method.
- Reuse the same C++ paper-camera touch controller with an alternate input adapter.

This fallback should only be used after SDL finger delivery fails, because it couples Java more tightly to native input.

## Testing

Unit-test the touch helper with synthetic finger events:

- One-finger drag emits pan and switches out of auto-center when applied.
- One-finger drag beginning over battle controls is ignored.
- Pinch changes distance in the expected direction and respects clamps.
- Two-finger horizontal movement emits rotation.
- Two-finger vertical movement emits height change and respects clamps.
- Finger-up resets the relevant gesture baseline.

Manual Android smoke test:

- Top-right speed, 2D/3D, `跟隨`/`自由`, pause, and log controls still tap correctly.
- One-finger pan works in 3D paper mode.
- Pinch zoom works.
- Two-finger horizontal rotation and vertical height adjustment work.
- Right-click overlay still injects cancel/right-click behavior.
- Classic 2D battle behavior is unchanged.
