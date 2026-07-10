# Mobile Touch Camera Controls for 3D Paper Battle View

## Status

This design has been reviewed against the current Android Java bridge, SDL 3.4.2, the native and Emscripten RunNode event loops, ImGui integration, VirtualStick, generic mouse consumers, and BattleSceneHades camera code. It is ready for implementation planning after user review.

The original proposal to add finger events directly to RunNode::isSpecialEvent() is rejected. Native RunNode dispatches one legacy event per render, while BattleSceneHades advances simulation from dealEvent(). Sending every finger event through that method would create input backlog or make simulation advance according to input rate.

The approved design makes raw SDL finger events authoritative and introduces streaming pointer-only and direct-touch paths that can consume every eligible sample in a render without advancing simulation.

## Context

The 3D paper battle camera currently has desktop keyboard controls in BattleSceneHades::dealEvent():

- W/A/S/D pans the paper camera center.
- Z/X changes camera distance.
- Arrow left/right rotates the camera.
- Arrow up/down changes camera height.
- O and the top-right 跟隨/自由 button toggle auto-center.

Android already has the complete Java-to-SDL raw touch path:

1. SDLSurface.onTouch() receives Android MotionEvent input.
2. For finger and unknown tools, it sends every pointer on move/cancel and only the changed pointer on pointer-down/pointer-up through SDLActivity.onNativeTouch().
3. SDL converts the calls into SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_MOTION, SDL_EVENT_FINGER_UP, and SDL_EVENT_FINGER_CANCELED.
4. Finger coordinates arrive normalized to the SDL window.

KysActivity does not need battle-specific touch code. Its bottom-right Java overlay remains an independent cancel command, but it no longer fabricates SDL mouse-button events.

The current Android setup also enables SDL_HINT_TOUCH_MOUSE_EVENTS. SDL therefore produces a synthetic mouse stream from its first tracked finger as well as the raw finger stream for the same physical touch. Additional fingers remain raw-only, and SDL does not promote a remaining secondary finger when the tracked finger lifts. The game must not depend on the relative ordering of the raw and synthetic streams. Enabling both game paths would cause duplicate clicks, duplicate ImGui input, ambiguous multi-touch ownership, and avoidable queue traffic.

## Verified Repository Constraints

- src/Engine.cpp currently forces SDL_HINT_TOUCH_MOUSE_EVENTS to "1" before SDL_Init().
- the locally copied SDL 3.4.2 SDLSurface.java forwards raw finger input through onNativeTouch(); move/cancel sends all pointers, while pointer-down/up sends only the changed pointer;
- native src/RunNode.cpp currently selects one special event per render, while its Emscripten branch sends every special event through scene logic;
- src/BattleSceneHades.cpp advances battle simulation from dealEvent(), so pointer rate must not determine update rate;
- the installed Dear ImGui SDL3 backend handles mouse input and identifies touchscreen origin through SDL_TOUCH_MOUSEID, but does not translate raw SDL finger events;
- src/VirtualStick.cpp independently polls SDL touch snapshots and mutates EngineEvent, so it must migrate to the central stream;
- src/ChessSystemSettingsMenu.cpp already accepts both mouse and finger input, which would double-activate once raw fingers and SDL synthesis are both game-visible;
- SDL_PushEvent() does not update device state, so neither replacement touch-mouse events nor the current Java nativeInjectRightClick() pair are valid device-state adapters.

## Goals

- Give mobile players the full 3D paper-camera adjustment set without virtual keys.
- Use raw SDL finger input as the single authoritative touchscreen source.
- Ingest every queued pointer edge and motion sample and process the complete pointer prefix belonging to the current owner without advancing battle simulation more than once per render.
- Preserve short taps, cancellation, and multi-touch transitions even when several events arrive within one render.
- Preserve physical mouse behavior.
- Preserve existing single-pointer touch behavior in menus and classic 2D scenes through a unified logical pointer path.
- Keep Java battle-state agnostic.
- Avoid duplicate camera, control-layout, coordinate-conversion, and pointer-state logic.
- Keep the existing 跟隨/自由 control as the explicit way to return to auto-center.

## Non-Goals

- Do not add a mobile virtual keyboard or virtual gamepad.
- Do not move battle-camera behavior into Java.
- Do not add twist rotation, gesture inertia, or three-finger commands.
- Do not make generic menus multi-touch aware; they receive one logical primary pointer.
- Do not preserve the old dual mouse-plus-finger input paths. Existing consumers are migrated to the new pointer path.
- Do not change classic 2D battle behavior, although its input plumbing is migrated.
- Do not refactor keyboard, gamepad, text, or lifecycle events beyond what is required to keep one simulation update per render.

## Input Source Policy

Each physical input has one authoritative source:

- Physical mouse: native SDL mouse events.
- Physical touchscreen: native SDL finger events.
- The first finger in a physical touch sequence: the sole global primary-finger candidate for ImGui and, on a single-pointer screen, an in-process logical left pointer derived from the raw finger.
- Additional fingers: raw touch only; they never become generic clicks.
- Java cancel overlay: one explicit application Cancel action queued through SDL's thread-safe user-event path.
- VirtualStick fingers, when that optional feature is enabled: virtual controls only after explicit capture.

SDL must not manufacture parallel game-input streams.

Before SDL_Init(), every build that enables raw-finger routing, including Android and Emscripten, sets:

    SDL_HINT_TOUCH_MOUSE_EVENTS = "0"
    SDL_HINT_MOUSE_TOUCH_EVENTS = "0"

After SDL_Init(), every raw-finger build disables unused SDL pinch event queueing:

    SDL_SetEventEnabled(SDL_EVENT_PINCH_BEGIN, false)
    SDL_SetEventEnabled(SDL_EVENT_PINCH_UPDATE, false)
    SDL_SetEventEnabled(SDL_EVENT_PINCH_END, false)

On Android, the unchanged Java ScaleGestureDetector still runs, but SDL_SendPinch() does not enqueue disabled events. Pinch distance, centroid, rotation, and height are all derived from the raw finger geometry. SDL pinch events do not contain finger IDs or centroid information and must not be combined with the raw calculation.

The input router also rejects SDL_MOUSE_TOUCHID and SDL_PEN_TOUCHID from the game TouchFrame. Residual mouse events marked SDL_TOUCH_MOUSEID are dropped centrally as a platform-defect safeguard.

The temporary ImGui translation described below is local and non-queued, so it is exempt from the residual-event filter. Raw game finger routing must not be enabled on a platform unless synthesis is disabled or its residual events are centrally identifiable and removable.

Do not toggle either synthesis hint during an active gesture.

## Input Types

### PointerEvent

PointerEvent replaces direct mouse-event handling in RunNode UI and other single-pointer consumers.

It contains:

- phase: motion, button down, button up, cancel, or wheel;
- source: mouse or touch;
- pointer ID;
- button and button state;
- window position and delta;
- present-local UI position;
- whether the position lies inside the present rect;
- wheel delta when applicable;
- SDL timestamp and window ID.

Native mouse events become PointerEvent values directly.

A physical touch sequence begins when the accepted physical-contact count changes from zero to one and ends when it returns to zero. PrimaryPointerTracker selects the first finger down as the sole global primary-finger candidate until that sequence ends. If ImGui, VirtualStick, or DirectTouch captures that finger, no secondary finger is promoted to generic pointer or ImGui input during the same sequence.

In PrimaryPointer mode, the global primary finger becomes logical left-pointer down, motion, up, and cancel input only if a higher-priority owner did not capture it. Secondary fingers never produce generic pointer input.

The logical pointer owns its own position and button state. Code migrated to PointerEvent must not query SDL_GetMouseState() to recover touch coordinates and must not warp the OS mouse.

### TouchFrame

TouchFrame is a streaming, interruptible cursor over one consecutive DirectTouch segment assigned to the current run owner during a render. It is not a vector pre-popped from the global queue. One render may dispatch multiple TouchFrame segments when DirectTouch is interleaved with physical mouse input or another ordering boundary.

Each cursor carries:

- a render/input-frame serial;
- an ownership epoch;
- a reset flag.

Calling next() detaches and yields at most the single raw touch event currently at the front of the global pending queue. Before yielding another sample, the cursor verifies that the active run owner, ownership epoch, touch policy, ImGui ownership, VirtualStick capture, control-layout revision, and owner exit state are unchanged. If any changed, the cursor ends and leaves every later event queued for fresh classification.

Each yielded TouchSample contains:

- phase: down, motion, up, or canceled;
- the pair of SDL touch ID and finger ID;
- normalized source position and delta;
- floating-point window position and delta;
- floating-point present-local UI position and delta;
- whether the position lies inside the present rect;
- pressure;
- window ID and timestamp.

PointerInput snapshots window size, present rect, and UI size for the current ownership epoch. Geometry changes increment the epoch before another sample is routed. BattleScenePaperCameraTouch therefore never calls Engine or repeats coordinate conversion.

PointerInput separately maintains:

- physicalContacts: every accepted native finger currently down, regardless of owner;
- activeDirectContacts: only contacts owned by the current DirectTouch owner and epoch;
- rejectedPhysicalContacts: contacts still physically down after an epoch/reset boundary that cannot be reclaimed until physical up or cancel.

The TouchFrame cursor exposes the activeDirectContacts snapshot after each yielded sample. An empty reset cursor is still delivered when required to clear recognizer state.

Every ordered edge and, initially, every motion sample is preserved. A down and up, or a 1-to-2-to-1 transition, can occur within one render; a final snapshot alone would lose short taps, path-dependent threshold crossings, and transition boundaries.

TouchFrame is a render-frame consumption abstraction, not an Android MotionEvent snapshot. SDL sends the pointers from one Android MotionEvent as separate JNI calls, so C++ must not assume they share one timestamp or arrive atomically.

## Event Pump and Simulation Cadence

The input pump owns a global ordered pending-event queue shared by nested RunNode::run() loops. The queue cannot belong to a scene because a legacy event may synchronously open another run loop, and later input must belong to the new active owner.

At each interactive render:

1. Poll SDL until its queue is empty and append the raw events to the global pending queue in order.
2. Start an input frame for the current RunNode::run() owner and its current ownership epoch.
3. Process the global pending queue from its front. Polling only copies events; ImGui, routing, and game callbacks never preprocess a future queued event.
4. For one queue-front pointer event at a time:
   - classify its event family without preprocessing or removing a later event;
   - detach that single event into an in-flight slot so a synchronously entered nested run cannot see it again; TouchFrame::next() performs this operation for DirectTouch;
   - reject residual synthesized streams centrally;
   - update physical touch state and select the global primary candidate;
   - process the native mouse event or prescribed primary-finger translation through ImGui exactly once;
   - make the fixed game-ownership decision;
   - dispatch the resulting PointerEvent immediately, or expose the DirectTouch sample through a TouchFrame cursor;
   - return control to PointerInput after the callback.
5. After every PointerEvent callback, yielded TouchSample, ImGui ownership transition, or VirtualStick capture/action, compare the active run owner, ownership epoch, touch policy, control-layout revision, and exit state with the frame-start values. If any changed, stop immediately. Every event not yet detached remains queued.
6. Stop at the first keyboard, gamepad, text, window, lifecycle, VirtualControlAction, ApplicationCancelAction, or other legacy ordering boundary. A legacy event is not converted, passed to ImGui, or marked consumed until it is actually selected for dispatch.
7. If owner identity, epoch, and exit state still match after pointer-only dispatch, detach at most one legacy event and commit it to this render. Process its global/ImGui handling once. The normal RunNode tree update receives that event when unconsumed, or EVENT_FIRST when no legacy event exists or ImGui consumes it.
8. If the frame-start owner exited, was removed, or ceased to be the active synchronous run owner before the normal update call, skip that update. An epoch change caused by the already-detached legacy event does not by itself cancel its committed update.
9. Draw and present once if the run loop remains active.

Detaching creates an RAII in-flight event/sample owned by the current C++ call frame. In-flight storage is stack-reentrant: a nested run loop may detach its own event without overwriting the suspended caller's event.

PointerEvent and TouchFrame callbacks never substitute for the normal update and never call dealEvent(), dealEvent2(), or the once-per-render battle simulation code. BattleSceneHades continues to advance simulation only from its normal dealEvent() call.

Each render consumes the complete consecutive pointer prefix belonging to the current owner before the next legacy or ownership boundary. During touch-only input this removes the one-touch-event-per-render bottleneck: a 120 Hz touch stream can be fully consumed by a 60 Hz render without advancing simulation per sample.

The initial implementation preserves every DirectTouch motion sample in order. Batching is not required to prevent backlog because samples use the non-simulation path. Future coalescing is allowed only after proving semantic equivalence: it must preserve maximum baseline excursion, threshold crossing, active-contact and finger-count transitions, final position, accumulated delta, and pair geometry. It may never cross down/up/cancel, capture, policy, owner, epoch, PointerEvent, control-layout, or legacy-event boundaries.

High-rate gamepad-axis coalescing must not flush unrelated SDL events.

All PointerInput state changes, ImGui routing, RunNode callbacks, and camera/control mutation occur on the SDL video/main thread while consuming the pending queue. Android JNI callbacks, SDL event watches, and the optional Java fallback may only enqueue/copy input; they never mutate tracker or scene state directly.

### Nested run loops and ownership epochs

Input after an event that opens a nested menu belongs to the nested run owner, not the still-drawn parent. Detaching only the current raw event leaves the suffix available to the nested loop. When that loop returns, the epoch changes again, so the suspended outer input frame cannot resume with stale capture or geometry.

Increment the ownership epoch and reset direct-touch gesture state when:

- a nested RunNode::run() owner is entered or exited;
- paper view is enabled or disabled;
- an ImGui overlay opens, closes, or invalidates existing game capture;
- the app backgrounds or loses window focus;
- the window/present geometry changes;
- a scene exits;
- noninteractive drawAndPresent() processing discards input.

Contacts still physically down after an epoch change remain rejected until their physical up or cancel. They must not become a new camera or UI gesture halfway through a sequence.

A queued legacy boundary is eligible only when both owner identity and ownership epoch still match the frame-start values after pointer-only dispatch. If an earlier pointer callback changes either value or the owner exit state, the suspended frame performs no normal update; its next run-loop iteration begins a fresh input frame and the legacy event remains raw in the queue.

Once an eligible legacy event is detached, its dispatch is committed. Geometry, lifecycle, ImGui, or normal-handler side effects may then change the epoch, but the detached event is still handled exactly once unless the owner exits. The new epoch applies to the queued suffix.

### Noninteractive and Emscripten processing

During dealEventSelfChilds(false), poll SDL into the shared queue, consume and discard every pointer/finger event while updating physical state, cancel captures, release any temporary ImGui touch button, and increment the epoch. Do not deliver PointerEvent or TouchFrame callbacks, and never replay those pointer events afterward. Discard at most one eligible legacy event per noninteractive render to retain the existing approximate legacy cadence.

The new ordered queue, pointer-only dispatch, and at-most-one normal update replace both the native and Emscripten event-loop branches. Emscripten still drains all eligible browser pointer input per browser render, but no longer calls normal scene logic once per pointer event.

## Routing and Capture Order

Touch ownership is keyed by the SDL touch/finger ID pair from down through up/cancel. Physical mouse capture is keyed by mouse ID and button from button-down through button-up/cancel. Unpressed mouse motion and wheel have no persistent sequence owner and are routed against the current owner for each event.

For each touch sequence, ownership is decided on down and remains fixed until up or cancel:

1. ImGui overlay;
2. VirtualStick, if enabled and the contact begins in one of its controls;
3. current RunNode input policy;
4. ignored/outside input.

The global primary candidate is still the first physical finger even when a higher-priority component captures it. Secondary contacts are never promoted to ImGui or generic pointer input within that physical sequence.

### RunNode pointer dispatch and capture

RunNode::dispatchPointerEvent() is a pointer-only traversal. It visits visible children in reverse draw order and then the current node. A pointer handler returns Ignored, Handled, Captured, or Released, and traversal stops at the first handled or captured target.

On button down, the first Captured result becomes the weak capture target for that pointer ID and button. Subsequent motion, up, and cancel for that capture go only to the target. Up/cancel releases capture after dispatch. Uncaptured hover motion is hit-tested each event. A hidden, removed, exited, or epoch-invalidated capture target is canceled if still reachable; its central capture state is cleared in all cases and it cannot activate.

Pointer activation, including migrated mouse/touch OK and cancel behavior, occurs in this pointer-only path. It may call the same semantic action methods as keyboard/gamepad handling, but it never invokes dealEvent() or creates an additional normal update. Legacy isPressOK()/isPressCancel() retain keyboard and gamepad semantics only.

### ImGui

The installed Dear ImGui SDL3 backend consumes SDL mouse events, not raw SDL finger events. The original raw finger event is not passed to the backend. ImGuiLayer translates the complete global-primary sequence exactly once as follows:

- down: temporary SDL_EVENT_MOUSE_MOTION at the touch position, then SDL_EVENT_MOUSE_BUTTON_DOWN;
- motion: temporary SDL_EVENT_MOUSE_MOTION;
- up: final motion when the position changed, then SDL_EVENT_MOUSE_BUTTON_UP;
- cancel or any central reset: SDL_EVENT_MOUSE_BUTTON_UP if ImGui previously received the translated down.

Every temporary event is zero-initialized and carries the actual SDL timestamp, window ID, window coordinates, relative motion/button state, and which = SDL_TOUCH_MOUSEID. Button events use SDL_BUTTON_LEFT and set down/clicks consistently. SDL has no mouse-cancel phase, so a translated cancel is a balanced left-button release.

ImGuiLayer owns construction and backend delivery of those temporary events; PointerInput owns global-primary selection and the game-routing decision. The temporary events are passed through a dedicated ImGuiLayer entry point, are never pushed back into SDL, bypass the queued residual-event filter, and are never delivered to game code. ImGui_ImplSDL3_ProcessEvent() returning true means that the backend recognized an event, not that ImGui captured it; ownership comes from ImGuiLayer's overlay/capture policy.

ImGui receives the balanced primary down/motion/up sequence even when it did not claim game ownership. Whether the game sequence is ImGui-owned is latched on primary down. If an overlay opens/closes or otherwise invalidates capture mid-sequence, PointerInput cancels the game capture, increments the epoch, and rejects the still-down contact rather than transferring it to another game owner.

When ImGui owns the sequence, the game receives neither its logical pointer view nor DirectTouch samples. SDL_EVENT_FINGER_CANCELED is included in ImGuiLayer's guarded and capture event classifications. Up/cancel still retires physical state even when the game never saw the sequence.

If ImGui captures the global primary sequence, every contact in that physical touch sequence is excluded from VirtualStick, logical-pointer, and DirectTouch game delivery until the physical contact count returns to zero.

### VirtualStick

VirtualStick is retained and fully migrated. The old SDL touch polling, time-based synthetic-mouse suppression, and EngineEvent mutation paths are removed. It consumes the central raw stream and latches capture by touch/finger ID only when a contact begins in one of its controls.

Captured samples update virtual controller button/axis state. Edge-triggered A/B/Menu/View behavior emits an explicit VirtualControlAction. PointerInput inserts that action as the next ordered legacy boundary immediately after its source sample. When selected, the normal dispatcher converts it once to the existing semantic key/gamepad action for dealEvent(); it never mutates or replaces the raw finger event and never causes an extra normal update.

Routing order is: update central physical-contact state; feed the global-primary translation to ImGui; if ImGui did not capture, allow VirtualStick to latch ownership on down; exclude captured IDs from logical PointerEvent and DirectTouch delivery.

Up, cancel, background/focus loss, owner/epoch invalidation, or any central reset immediately releases every virtual button and axis associated with captured contacts. Reset never emits A/B/Menu/View activation. Those still-down contacts become rejected tombstones until physical up/cancel and cannot drive a newly active run owner.

The Android camera feature does not enable VirtualStick.

### RunNode policies

RunNode exposes two touch policies:

- PrimaryPointer: ordinary screens receive the uncaptured global primary finger as logical single-pointer input.
- DirectTouch: the owner receives raw TouchFrame input and no game-side primary-pointer projection for those fingers.

BattleSceneHades uses PrimaryPointer in classic 2D mode and DirectTouch in active 3D paper mode.

Physical mouse PointerEvent values remain available in both modes.

## Pointer Migration

The old design must not remain alongside PointerEvent. Direct mouse branches are migrated and then removed.

Required migration includes:

- RunNode hover/press state and mouseIn(); pointer activation is removed from isPressOK()/isPressCancel() and routed through dispatchPointerEvent();
- Button activation;
- Menu and UI hover/click handling inherited from RunNode;
- BattleSceneCamera manual mouse dragging;
- BattleSceneHades top-right controls and position-swap selection;
- BattleCursor mouse targeting;
- BattleStatsView pre/post-battle taps;
- ChessSystemSettingsMenu row activation and slider dragging;
- MainScene and SubScene pathfinding/click actions;
- SuperMenuText paging;
- UIItem dragging;
- Scene and other code that currently queries SDL_GetMouseState() after receiving an event.

ChessSystemSettingsMenu's separate mouse and finger branches are replaced by one PointerEvent branch. A physical touch must activate a setting exactly once.

Mouse wheel and physical right-click remain mouse-specific PointerEvent operations. The Java overlay is not a mouse: its old nativeInjectRightClick() down/up pair is replaced by one ApplicationCancelAction. ImGui gets the first chance to close an open overlay; otherwise the normal dispatcher invokes the same semantic cancel action used by Escape, gamepad East, and physical right-button release.

Migration is complete only when no scene, menu, control, or camera handler branches directly on SDL mouse event types or queries SDL_GetMouseState() for interaction coordinates. Native SDL mouse ingestion is confined to PointerInput/Engine. This rule includes Menu.cpp, UI.cpp, Scene pointer-position helpers, SubScene hover, PositionSwapNode, and inherited RunNode UI paths.

## Battle Controls

The battle-control layout and hit testing currently live in an anonymous BattleSceneHades.cpp block. Extract them into a shared pure module used by:

- rendering;
- physical mouse PointerEvent handling;
- primary-pointer handling in 2D;
- direct finger classification in paper mode.

Do not duplicate button rectangles or coordinate transforms.

A control is armed on down and activates only when the same pointer/finger releases over the same control. Releasing over a different control, or starting on the battlefield and ending over a control, does not activate anything.

At most one touch contact may arm a battle control at a time. Once any touch control is active, only the first eligible control down may arm; no later contact may arm until the control-owned contact count returns to zero, even if the originally armed finger releases first. Additional contacts beginning over visible controls are control-owned and continue to suspend camera output, but they do not activate another control. Up/cancel of the armed contact clears its arm.

In DirectTouch paper mode:

- a finger beginning over a visible control is control-owned;
- a finger beginning outside the present rect is ignored for the whole sequence;
- a finger beginning on the battlefield is camera-owned;
- any active control-owned finger suspends and rebaselines the camera gesture;
- ownership does not change when a finger crosses a boundary.

When the last control-owned contact terminates and the owner, policy, overlay state, and ownership epoch remain unchanged, current camera-owned contacts are rebaselined. With one remaining camera contact, the 10-pixel pan threshold restarts. With two, a new pair baseline is established. No camera delta is emitted on resume. If the control action changed view policy or overlay ownership, the epoch reset takes precedence and still-down contacts are rejected instead of resumed.

Pause/unpause and any other change to which controls are visible increments controlLayoutRevision, stops the current TouchFrame cursor, and makes a later queued down use the new layout. It does not by itself increment the ownership epoch or reject already-owned camera contacts.

Top-right controls remain usable while the battle is paused. Camera gestures remain usable while paused, but are disabled and reset while the battle-log overlay is open.

## Coordinate System

Finger coordinates are normalized to the SDL window and are not guaranteed to be clamped.

For every sample:

    windowX = normalizedX * windowWidth
    windowY = normalizedY * windowHeight
    uiX = (windowX - presentX) * uiWidth / presentWidth
    uiY = (windowY - presentY) * uiHeight / presentHeight

Use floating-point coordinates for gesture math.

A new sequence must begin inside the current present rect. Do not clamp a letterbox or outside touch into the battlefield. Once a camera-owned sequence begins, movement may leave the rect without changing ownership; terminal up/cancel always clears the state.

No Android landscape-rotation transform is added in C++. SDLSurface and SDL already report surface-relative coordinates in the active orientation.

## Gesture State Machine

The paper-camera recognizer consumes DirectTouch samples in order, classifies them as control-owned, camera-owned, or ignored, and performs camera geometry only for camera-owned contacts. For each sample it returns an ordered list of semantic actions such as control arm/release/activation, panActivated, pan delta, or a two-finger camera delta. BattleSceneHades applies those actions in order. If an action changes the run owner, input policy, overlay state, control layout, or ownership epoch, the TouchFrame cursor refuses to yield another sample and the queue suffix is rerouted later.

### One finger

- Down establishes a baseline and emits no camera delta.
- Accumulate movement from the baseline.
- Movement below 10 virtual UI pixels is tap jitter and does nothing.
- Crossing the threshold emits panActivated exactly once followed by the accumulated pan delta.
- Further ordered motion emits pan deltas.

BattleSceneHades applies panActivated by setting paper_camera_auto_center_ to false. Any nonzero accepted keyboard W/A/S/D manual-pan command does the same. Zoom, rotation, and height adjustment preserve the current follow/free mode.

### Two fingers

- The 1-to-2 transition establishes a pair baseline and emits no delta.
- Use pair-order-independent centroid and span geometry.
- Span is the Euclidean distance between the two contacts in present-local virtual UI pixels.
- Pinch changes distance.
- Horizontal centroid movement rotates the camera.
- Vertical centroid movement changes camera height.
- Apply all three components from the same previous pair geometry, then clamp once.

### Returning to one finger

- The 2-to-1 transition rebaselines the remaining finger.
- It emits no pan on the transition.
- A new 10-pixel pan threshold begins from the remaining finger's current location.

### More than two fingers

- Suspend camera output.
- Preserve ownership.
- Rebaseline when the eligible contact count returns to one or two.

### Cancel

SDL_EVENT_FINGER_CANCELED is handled as a terminal phase.

Android cancellation is stream-wide even though SDL reports one canceled event per finger. Cancellation:

- releases any armed control;
- delivers logical pointer cancel and releases its left-button state if the game received a logical down;
- delivers a temporary ImGui left-button-up if ImGui received a translated down;
- clears camera baselines;
- discards pending tap activation;
- increments or resets the gesture ownership state as appropriate.

The first accepted Android canceled sample aborts the entire game touch gesture. Later per-finger canceled events still retire or reject their physical contact IDs, but cannot reactivate any game owner.

## Camera Mapping

BattleSceneHades owns and clamps the camera state. The recognizer emits gesture-space values, not camera-world deltas.

Use one shared paper-camera ground basis for keyboard and touch:

    forward = normalize({ -cos(angle), -sin(angle), 0 })
    screenRight = normalize({ forward.y, -forward.x, 0 })

The basis matches the Camera projection's screen-right direction.

To preserve the current keyboard directions while using the same basis:

    W = +forward
    S = -forward
    A = +screenRight
    D = -screenRight

For a one-finger UI delta of dx, dy:

    fovRadians = PAPER_CAMERA_FOV * pi / 180
    slantDistance = hypot(cameraDistance, cameraHeight)
    worldPerUiPixel =
        2 * slantDistance * tan(fovRadians / 2) / uiHeight

    centerDelta =
        -screenRight * dx * worldPerUiPixel
        +forward * dy * worldPerUiPixel

    centerDelta *= PAPER_TOUCH_PAN_SCALE

Dragging the battlefield right therefore moves the camera center left so the ground follows the finger. Clamp the result with clampPaperCameraCenter().

Pinch is multiplicative and resolution-independent:

    newDistance =
        oldDistance
        * pow(previousSpan / currentSpan, PAPER_TOUCH_PINCH_SCALE)

Clamp with PAPER_CAMERA_MIN_DISTANCE and PAPER_CAMERA_MAX_DISTANCE. A zero or near-zero span is a real possible condition and establishes a new baseline instead of dividing.

Two-finger horizontal movement follows the existing K_RIGHT sign:

    angleDelta =
        centroidDx / uiWidth * pi * PAPER_TOUCH_ROTATE_SCALE

Two-finger upward movement raises the camera:

    heightDelta =
        -centroidDy / uiHeight
        * (PAPER_CAMERA_MAX_HEIGHT - PAPER_CAMERA_MIN_HEIGHT)
        * PAPER_TOUCH_HEIGHT_SCALE

Clamp height once after applying the delta.

PAPER_TOUCH_PAN_SCALE, PAPER_TOUCH_PINCH_SCALE, PAPER_TOUCH_ROTATE_SCALE, and PAPER_TOUCH_HEIGHT_SCALE are named constants with initial value 1.0. Angle remains unbounded like the existing keyboard control. Device testing may tune only those named constants; direction, coordinate system, and base formulas are fixed by this design.

## C++ Structure

### PointerInput.h/.cpp

Owns:

- the global ordered pending SDL-event queue;
- PointerEvent;
- TouchFingerKey, TouchSample, and TouchFrame;
- physical mouse and touch state;
- PrimaryPointerTracker;
- input ownership epochs, capture records, and rejected-contact tombstones;
- queue-front source conversion, routing, and cancellation;
- selection of the global primary finger and calls into ImGuiLayer's touch entry point;
- VirtualControlAction insertion;
- ApplicationCancelAction ingestion and ordering;
- residual synthetic-event filtering.

It has no battle-camera knowledge and never calls scene simulation. It exposes only the queue-front classification/dispatch operations needed by RunNode. After every RunNode callback, control returns to PointerInput to validate owner, epoch, policy, capture target, layout revision, and exit state before another event is detached.

### ImGuiLayer

Adds a dedicated primary-touch entry point that constructs and delivers the temporary SDL_TOUCH_MOUSEID mouse-motion/button sequence and reports the overlay/capture policy. Raw finger events never go directly to ImGui_ImplSDL3_ProcessEvent().

### RunNode

Adds:

- a pointer-only dispatch path that may process multiple ordered pointer events without advancing simulation;
- a touch policy query;
- an interruptible direct TouchFrame cursor callback for the current run owner;
- at most one normal dealEvent() update for each still-active run-owner render.

RunNode owns tree traversal and callback invocation; PointerInput owns the persistent capture record and validates its weak target.

### BattleSceneControls.h/.cpp

Owns the pure battle-control layout, visible-control hit testing, control identity, and same-control down/up capture rules.

### BattleScenePaperCameraTouch.h/.cpp

Pure stateful recognizer over yielded TouchFrame samples. Owns finger eligibility, control/camera ownership, threshold, pair baselines, transitions, and ordered semantic output. It uses the converted coordinates in TouchSample, does not access Engine, and does not mutate BattleSceneHades.

### PaperCameraControlMath

Pure shared helpers for the paper-camera basis, pan mapping, pinch scaling, rotation scaling, and height scaling. Keyboard and touch reuse the same basis and camera-state application functions.

BattleSceneHades remains responsible for:

- choosing PrimaryPointer versus DirectTouch;
- applying recognizer output;
- camera clamps;
- paper_camera_auto_center_;
- view and overlay resets;
- battle-control actions.

## Java Boundary and Fallback

The primary touch implementation adds no battle-state handling to KysActivity and leaves the locally copied SDL 3.4.2 Java bridge unchanged. The bridge is supplied from the matching vcpkg SDL build as documented in android/README.md and is ignored by git, so the build prerequisite must keep that version match reproducible.

KysActivity's overlay JNI boundary is intentionally changed. Rename nativeInjectRightClick() to a cancel-command API that pushes one registered SDL user event carrying ApplicationCancelAction. SDL_PushEvent() is appropriate for this non-device event and preserves queue ordering without pretending to update mouse state. The event is converted only when it reaches the legacy queue front. If an ImGui battle-log/changelog overlay is open, ImGuiLayer consumes it and closes the overlay; otherwise normal semantic cancel handling receives it once.

Do not override KysActivity.dispatchTouchEvent(). It sees touches intended for the full-screen Java overlay and uses decor coordinates rather than guaranteed SDLSurface-local coordinates.

If device testing proves the SDL raw finger path unusable, override SDLActivity.createSDLSurface() and return a small SDLSurface subclass. In explicit fallback mode it forwards surface-local pointer snapshots through JNI into the same C++ PointerInput queue.

Fallback requirements:

- a startup source-mode gate makes it mutually exclusive with SDL raw-finger ingestion;
- when SDL raw events are merely missing or semantically unsuitable, the subclass still calls super.onTouch() and PointerInput ignores every polled SDL finger/pinch event in Java-frame fallback mode;
- if device evidence shows that super.onTouch() itself is unusable for finger tools, the subclass bypasses it only for finger/unknown tools while preserving SDL mouse/stylus handling separately;
- it preserves action, action index, device ID, tool type, every pointer ID, surface-local coordinates, pressure, event time, and ACTION_CANCEL;
- it sends one complete JNI snapshot per Android MotionEvent rather than separate JNI calls that pretend to be atomic;
- it never mutates scenes or camera state on the Android UI thread;
- it excludes the Java overlay naturally by operating at the SDLSurface boundary;
- it queues data for consumption on the SDL main thread.

## Testing

### PointerInput unit tests

- Native mouse produces one logical pointer sequence.
- Primary finger produces exactly one logical left-pointer down/motion/up.
- Additional fingers produce no generic pointer actions.
- First finger captured by VirtualStick or DirectTouch prevents a second finger from becoming the generic primary pointer.
- DirectTouch mode produces TouchFrame input and no game-side primary-pointer duplicate.
- Residual SDL_TOUCH_MOUSEID mouse events are rejected.
- SDL_MOUSE_TOUCHID and SDL_PEN_TOUCHID finger streams are rejected.
- Down and up in one render are both preserved.
- Every initial DirectTouch motion sample is preserved, including a threshold-crossing excursion that returns near its baseline within one render.
- A 120 Hz touch stream consumed by a 60 Hz render loop does not build a motion backlog during an uninterrupted pointer prefix.
- Cancel releases pointer state and direct-touch state.
- Ownership and epoch changes reject already-down contacts.
- An event opening a nested run leaves later pending input for the nested owner.
- Pointer/legacy/pointer ordering leaves the suffix queued behind the legacy boundary.
- A pointer or control action that changes policy in the middle of an SDL batch leaves its suffix queued for the new policy.
- Pause activation updates the visible control layout before a later queued down is classified.
- ImGui receives one balanced translated touchscreen sequence and can capture it.
- ImGui receives up/cancel after observing but not capturing down, leaving no backend mouse button stuck.
- Primary down reaches ImGui as motion then button-down, and the raw finger event never reaches the SDL backend.
- ImGui capture prevents game pointer and TouchFrame delivery.
- A consumed ImGui event still allows one normal EVENT_FIRST game update.
- Capture invalidation sends cancel without activation and rejects the still-down contact.
- VirtualControlAction becomes the next ordered legacy boundary and does not add a normal update.
- The Java overlay produces one ApplicationCancelAction, no mouse down/up events, and closes ImGui or game UI exactly once.

### Pointer migration tests

- Generic Button activates once per physical touch.
- Button/control down outside and up inside does not activate when capture is required.
- Removing, hiding, exiting, or epoch-invalidating a captured target cancels it without activation.
- ChessSystemSettingsMenu toggles once and sliders receive one motion stream.
- MainScene and SubScene use logical pointer coordinates for pathfinding.
- UIItem dragging no longer depends on SDL_GetMouseState().
- BattleStatsView and SuperMenuText taps remain single actions.
- Classic 2D battle camera and position swap retain their existing behavior.
- VirtualStick capture prevents pointer and paper-camera leakage; A/B/Menu/View actions remain ordered.

### Paper-camera recognizer tests

- Down establishes a baseline without motion.
- Below-threshold jitter does not disable auto-center.
- Threshold crossing emits panActivated once and then the accumulated pan.
- UI/outside-start contacts never become camera contacts.
- Control capture requires matching down/up on the same control.
- Only one touch control can be armed; other control-owned contacts cannot activate controls.
- Ending the final control-owned contact rebaselines remaining camera contacts without a jump.
- 1-to-2 and 2-to-1 transitions never jump.
- Pair results are independent of finger ordering.
- More than two fingers suspend and rebaseline.
- Canceled contacts clear all state.
- Paper-view and overlay changes reset the recognizer.

### Camera math tests

- Pan direction and magnitude at several camera angles.
- Shared keyboard/touch ground basis.
- Pinch direction and min/max distance clamps.
- Rotation sign and scaling.
- Height sign and min/max clamps.
- Default 1.0 touch sensitivity scales reproduce the base formulas.
- Paper-camera center bounds.
- Applying panActivated or a nonzero keyboard manual-pan command exits auto-center; two-finger zoom/orientation preserves it.

### Manual Android smoke test

- Log input types and confirm SDL no longer emits touch-generated mouse events.
- Ordinary menus, settings, sliders, item drag, pathfinding, and text paging work by touch.
- ImGui battle log and changelog work by touch.
- Top-right speed, 2D/3D, 跟隨/自由, pause, and log controls activate once.
- A battlefield drag ending over every top-right control activates none of them.
- One-finger paper pan is immediate and does not lag after a sustained fast drag.
- Pinch zoom, two-finger rotation, and two-finger height work together.
- Both landscape orientations, letterboxing, and display cutouts are checked.
- Cancel/background/foreground transitions leave no stuck contacts.
- The Java cancel overlay closes ImGui or game UI once, produces no SDL mouse event, and never moves the camera or logical pointer.
- Real mouse input remains unchanged.

### Manual Emscripten smoke test

- Emscripten drains browser pointer input without simulation-per-event.
- Pointer/legacy ordering is preserved.
- Ordinary touch UI, dragging, ImGui input, and physical mouse behavior remain correct.

## Feasibility Conclusion

This design is cleanly implementable through the existing Android Java to SDL to C++ boundary.

The required input refactor is broader than adding a camera helper, but it removes the duplicate source at its origin, gives pointer input a non-simulation dispatch path, preserves render-frame latency, and creates one reusable touch foundation instead of adding paper-camera-specific exceptions.

Do not implement an intermediate state that both enables raw finger game handling and leaves SDL touch-to-mouse synthesis active.
