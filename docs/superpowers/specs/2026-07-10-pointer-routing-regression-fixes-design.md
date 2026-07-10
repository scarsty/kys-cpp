# Pointer Routing Regression Fixes Design

## Goal

Restore three desktop input behaviors regressed by the pointer-input migration:

- Mouse-wheel scrolling must continue to work when the pointer is over a menu item or `SuperMenuText` item.
- Popup dialogue text must advance or dismiss on a left click anywhere in the game window, matching the previous raw-event behavior. Right click must continue to invoke Cancel.
- The 2D battle view must allow left-button camera dragging when Manual Camera is enabled.

## Root Causes

The pointer dispatcher currently selects the deepest node under the pointer and sends uncaptured events only to that node. When the node returns `Ignored`, the event is discarded instead of being offered to its ancestors. Consequently, a hovered child can prevent its menu ancestor from receiving Wheel, and a child in the battle scene can prevent `BattleSceneHades` from claiming the initial camera-drag press.

The raw event path previously treated every left mouse-button release as OK. That global behavior was removed during pointer migration. `Talk` has no pointer-specific behavior or hit area, so it no longer establishes a left-button sequence; the dispatcher's global right-button Cancel path is the only mouse dismissal that remains.

## Pointer Dispatch Design

Uncaptured pointer dispatch will walk from the deepest hit target toward the run owner:

- Wheel starts at the deepest target and continues through ancestors until a node returns a result other than `Ignored`.
- ButtonDown follows the same rule. If a node returns `Captured`, that node becomes the exclusive capture target for subsequent Motion, ButtonUp, and Cancel events in the sequence.
- A child that handles or captures the event prevents its ancestors from independently handling the same initial event.
- Existing captured-event propagation remains unchanged so ancestors can observe a child's captured sequence without stealing ownership.
- Uncaptured Motion remains targeted at the deepest hovered node; this change does not broaden hover handling.

The traversal belongs in `RunNode` so menus, scrolling text, battle scenes, and future composite controls share one routing rule rather than forwarding events manually.

## Dialogue Design

`Talk` will explicitly implement an anywhere-click pointer sequence:

- Any left ButtonDown received by the active `Talk` node captures that pointer, including clicks outside the rendered dialogue panel or presentation rectangle.
- The matching left ButtonUp calls `onPressedOK()` exactly once and releases capture.
- Cancel releases the sequence without advancing the dialogue.
- The existing global right-mouse ButtonUp path continues to call `onPressedCancel()`.

This preserves the old desktop left-click behavior while avoiding a global rule that would activate unrelated menus when blank space is clicked. The same pointer sequence also gives touch dialogue taps consistent press/release semantics.

## Camera Design

`BattleSceneHades` remains responsible for deciding whether a 2D camera drag is allowed. When Manual Camera is enabled, an ignored child ButtonDown can reach the battle-scene ancestor, whose existing `BattleSceneCamera::handleManualInput()` logic begins capture. Motion then remains routed exclusively to the captured battle scene until ButtonUp or Cancel.

Camera movement math and the Manual Camera setting are unchanged unless a focused regression test demonstrates an independent fault.

## Tests

Automated regression coverage will verify:

- A child that ignores Wheel allows its scrolling ancestor to handle the event exactly once.
- A child that ignores ButtonDown allows an ancestor to capture it, while a capturing child still wins.
- `Talk` advances once for a matching anywhere-left down/up sequence and does not advance on Cancel.
- `BattleSceneCamera` starts on an enabled left press, changes the camera center on Motion, and stops on ButtonUp.
- The pointer-routing ancestor-capture test covers the integration condition that previously prevented the battle scene from receiving the drag start.

Focused tests will run after each red-green cycle. Completion verification will include the full test suite and `.github/build-command.ps1`; because `RunNode` is shared across platforms, Android and WebAssembly builds will also be rerun.

## Non-Goals

- Changing menu selection or activation semantics.
- Allowing parents to pre-empt child controls.
- Changing right-click Cancel behavior.
- Changing 3D paper-camera gestures or 2D camera movement sensitivity.
