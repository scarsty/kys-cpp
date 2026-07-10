# Mobile Touch Camera Controls for 3D Paper Battle View

## Status

This design has been reviewed against the Android Java bridge, SDL 3.4.2 Android and Emscripten raw-finger backends, the native and browser RunNode event loops, ImGui integration, generic mouse consumers, and BattleSceneHades camera code. Android and Emscripten mobile web use the same C++ input architecture. It is ready for implementation planning after user review.

The original proposal to add finger events directly to RunNode::isSpecialEvent() is rejected. Native RunNode dispatches one legacy event per render, while BattleSceneHades advances simulation from dealEvent(). Sending every finger event through that method would create input backlog or make simulation advance according to input rate.

The design makes raw SDL finger events authoritative and introduces streaming pointer-only and direct-touch paths that can consume every eligible sample in a render without advancing simulation.

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

Emscripten has an equivalent raw path in SDL 3.4.2:

1. SDL installs Pointer Events handlers on the game canvas and sets touch-action to none.
2. Browser pointers with pointerType "touch" become SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_MOTION, SDL_EVENT_FINGER_UP, and SDL_EVENT_FINGER_CANCELED.
3. SDL uses touchID 1 and fingerID = browser pointerId + 1 for the contact, and normalizes CSS-canvas coordinates before C++ receives them.
4. Browser Pointer Events update one contact at a time; contacts are not delivered as an atomic multi-contact snapshot. Pointer down produces finger-down. Pointer move produces finger-motion when normalized x or y changes. Pointer up calls the final touch-motion update before finger-up, but SDL queues the preceding finger-motion only when x or y changed. Emscripten touch pressure is always 1.0. Pointer cancel/leave emits one contact-specific SDL_EVENT_FINGER_CANCELED with no final motion update.

The web build does not need a separate JavaScript touch, compatibility-mouse, or camera path.

KysActivity does not need battle-specific touch code. Its bottom-right Java overlay remains an independent cancel command, but it no longer fabricates SDL mouse-button events.

The current Android setup explicitly enables SDL_HINT_TOUCH_MOUSE_EVENTS, and Emscripten inherits SDL's enabled default unless the game overrides it. SDL therefore produces a synthetic mouse stream from its first tracked finger as well as the raw finger stream for the same physical touch. Additional fingers remain raw-only, and SDL does not promote a remaining secondary finger when the tracked finger lifts. The game must not depend on the relative ordering of the raw and synthetic streams. Enabling both game paths would cause duplicate clicks, duplicate ImGui input, ambiguous multi-touch ownership, and avoidable queue traffic.

## Verified Repository Constraints

- src/Engine.cpp currently forces SDL_HINT_TOUCH_MOUSE_EVENTS to "1" before SDL_Init().
- the locally copied SDL 3.4.2 SDLSurface.java forwards raw finger input through onNativeTouch(); move/cancel sends all pointers, while pointer-down/up sends only the changed pointer;
- native src/RunNode.cpp currently selects one special event per render, while its Emscripten branch sends every special event through scene logic;
- src/BattleSceneHades.cpp advances battle simulation from dealEvent(), so pointer rate must not determine update rate;
- the installed Dear ImGui SDL3 backend handles mouse input and identifies touchscreen origin through SDL_TOUCH_MOUSEID, but does not translate raw SDL finger events;
- SDL 3.4.2's Emscripten backend produces direct SDL finger events from browser Pointer Events, uses normalized CSS-canvas coordinates, and reports pointer cancel/leave as finger canceled;
- src/VirtualStick.cpp/.h and all of their integration points are obsolete and are removed by this change; no VirtualStick behavior is migrated into PointerInput;
- no tracked top-level config/chess_*.yaml currently defines game.use_virtual_stick;
- src/ChessSystemSettingsMenu.cpp already accepts both mouse and finger input, which would double-activate once raw fingers and SDL synthesis are both game-visible;
- SDL_PushEvent() does not update device state, so replacement touch-mouse events and the current Android/web fake-right-click pairs are not valid device-state adapters;
- wasm/shell.html contains unused last-touch tracking and a cancel button that currently calls inject_right_click(); both are cleanup targets, not input adapters;
- SuperMenuText's mobile double-tap timing guard explicitly compensates for a raw-touch-plus-synthetic-mouse burst and must no longer serve that purpose once one physical touch has one authoritative stream.
- SDL 3.4.2 does not queue SDL_EVENT_TERMINATING, low-memory, or application background/foreground events; it delivers that family synchronously to SDL_AddEventWatch() callbacks.
- SDL 3.4.2 removes superseded WINDOW_RESIZED and WINDOW_PIXEL_SIZE_CHANGED events from its queue even when pointer events were queued between the old and new geometry events; wasm/resize_to_viewport() also mutates the live SDL window and Engine present rect synchronously. Raw window events plus live Engine queries are therefore not a lossless input-geometry timeline.

## Goals

- Give mobile players the full 3D paper-camera adjustment set without virtual keys.
- Use raw SDL finger input as the single authoritative touchscreen source.
- Ingest every queued pointer edge and motion sample and process the complete pointer prefix belonging to the current owner without advancing battle simulation more than once per render.
- Preserve short taps, cancellation, and multi-touch transitions even when several events arrive within one render.
- Preserve physical mouse behavior.
- Preserve existing single-pointer touch behavior in menus and classic 2D scenes through a unified logical pointer path.
- Make Android and Emscripten mobile web share PointerInput, PrimaryPointer, ImGui translation, DirectTouch, and paper-camera behavior.
- Remove the existing VirtualStick feature completely, including runtime code, injected controller state, configuration, RunNode hooks, and project entries.
- Keep Java battle-state agnostic.
- Avoid duplicate camera, control-layout, coordinate-conversion, and pointer-state logic.
- Keep the existing 跟隨/自由 control as the explicit way to return to auto-center.

## Non-Goals

- Do not add a mobile virtual keyboard or virtual gamepad.
- Do not retain VirtualStick as a disabled compatibility path and do not recreate its directional, button, Menu/Escape, View/Tab, or virtual-axis behavior inside PointerInput.
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
- Physical pen/stylus: SDL pen and existing pen-to-mouse behavior; it is never promoted into the finger stream.
- The first finger in a physical touch sequence: the sole global primary-finger candidate for ImGui and, on a single-pointer screen, an in-process logical left pointer derived from the raw finger.
- Additional fingers: raw touch only; they never become generic clicks.
- Java cancel overlay: one explicit application Cancel action queued through SDL's thread-safe user-event path.
- Web cancel overlay: the same explicit application Cancel action, not injected SDL mouse buttons.

SDL must not manufacture parallel game-input streams.

Before SDL_Init(), every touch-capable build, including Android and Emscripten, enables raw-finger routing and sets:

    SDL_HINT_TOUCH_MOUSE_EVENTS = "0"
    SDL_HINT_MOUSE_TOUCH_EVENTS = "0"
    SDL_HINT_PEN_TOUCH_EVENTS = "0"

After SDL_Init(), every raw-finger build disables unused SDL pinch event queueing:

    SDL_SetEventEnabled(SDL_EVENT_PINCH_BEGIN, false)
    SDL_SetEventEnabled(SDL_EVENT_PINCH_UPDATE, false)
    SDL_SetEventEnabled(SDL_EVENT_PINCH_END, false)

On Android, the unchanged Java ScaleGestureDetector still runs, but SDL_SendPinch() does not enqueue disabled events. Pinch distance, centroid, rotation, and height are all derived from the raw finger geometry. SDL pinch events do not contain finger IDs or centroid information and must not be combined with the raw calculation.

Disabling SDL_HINT_PEN_TOUCH_EVENTS removes pen-generated finger input at its source while leaving SDL's physical pen and pen-to-mouse behavior unchanged. The input router also rejects residual SDL_MOUSE_TOUCHID and SDL_PEN_TOUCHID streams from the game TouchFrame. Residual mouse events marked SDL_TOUCH_MOUSEID are dropped centrally as a platform-defect safeguard.

The temporary ImGui translation described below is local and non-queued, so it is exempt from the residual-event filter. Raw game finger routing must not be enabled on a platform unless synthesis is disabled or its residual events are centrally identifiable and removable.

There is no Emscripten exception that falls back to game-visible compatibility mouse events. Do not toggle either synthesis hint during an active gesture.

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

A physical touch sequence begins when the accepted physical-contact count changes from zero to one and ends when it returns to zero. PrimaryPointerTracker selects the first finger down as the sole global primary-finger candidate until that sequence ends. If ImGui or DirectTouch owns that finger, no secondary finger is promoted to generic pointer or ImGui input during the same sequence.

In PrimaryPointer mode, the global primary finger becomes logical left-pointer down, motion, up, and cancel input only if a higher-priority owner did not capture it. Secondary fingers never produce generic pointer input.

The logical pointer owns its own position and button state. Code migrated to PointerEvent must not query SDL_GetMouseState() to recover touch coordinates and must not warp the OS mouse.

### TouchFrame

TouchFrame is a streaming, interruptible cursor over one consecutive DirectTouch segment assigned to the current run owner during a render. It is not a vector pre-popped from the global queue. One render may dispatch multiple TouchFrame segments when DirectTouch is interleaved with physical mouse input or another ordering boundary.

Each cursor carries:

- a render/input-frame serial;
- an ownership epoch;
- a reset flag.

Calling next() detaches and yields at most the single raw touch event currently at the front of the global pending queue. Before yielding another sample, the cursor verifies that the active run owner, ownership epoch, touch policy, ImGui ownership, control-layout revision, and owner exit state are unchanged. If any changed, the cursor ends and leaves every later event queued for fresh classification.

Each yielded TouchSample contains:

- phase: down, motion, up, or canceled;
- the pair of SDL touch ID and finger ID;
- normalized source position and delta;
- floating-point window position and delta;
- floating-point present-local UI position and delta;
- whether the position lies inside the present rect;
- pressure;
- window ID and timestamp.

PointerInput owns a committed PresentGeometrySnapshot containing a monotonic revision, window size, present rect, and UI size. It retains that immutable snapshot across renders and never rebuilds it by querying Engine's live window/present state at frame start. Each PointerEvent and TouchSample is converted with the snapshot committed at that event's position in the ordered queue. Geometry actions increment the epoch before a later sample is routed. BattleScenePaperCameraTouch therefore never calls Engine or repeats coordinate conversion.

One shared pure computePresentLayout() helper produces the present rect from an immutable layout input and target window size. Engine rendering and PresentGeometryAction payload construction both use that helper; there is no second present-layout formula in PointerInput.

Every mutation of a computePresentLayout() input goes through one sole publishPresentGeometryTransition() path: SDL window size, main-texture size, UI size, keep-ratio, rotation, and x/y render ratios. The SDL event watch invokes it for window-origin changes; present-only project mutations call it directly. It allocates one revision/action only when the computed snapshot actually changes, then applying that same snapshot to Engine's live render state is publication-free. Direct present-rect writes are removed. renderMainTextureToWindow() may still ask the central API to ensure layout before drawing, but its current per-draw setPresentPosition() behavior becomes an unchanged no-op rather than an action every frame. The initial snapshot is installed before input becomes active.

PointerInput separately maintains:

- physicalContacts: every accepted native finger currently down, regardless of owner;
- activeDirectContacts: only contacts owned by the current DirectTouch owner and epoch;
- rejectedPhysicalContacts: contacts still physically down after an epoch/reset boundary that cannot be reclaimed until physical up or cancel.
- browserContactsAwaitingFreshDown: contact IDs abandoned by a browser hard reset; their late motion/up/cancel is ignored until a new down establishes a new generation.

The TouchFrame cursor exposes the activeDirectContacts snapshot after each yielded sample. An empty reset cursor is still delivered when required to clear recognizer state.

Every ordered edge and, initially, every motion sample is preserved. A down and up, or a 1-to-2-to-1 transition, can occur within one render; a final snapshot alone would lose short taps, path-dependent threshold crossings, and transition boundaries.

TouchFrame is a render-frame consumption abstraction, not an Android MotionEvent snapshot. SDL sends the pointers from one Android MotionEvent as separate JNI calls, so C++ must not assume they share one timestamp or arrive atomically.

Emscripten is also event-stream based rather than snapshot based. Each browser Pointer Event updates one SDL contact, and PointerInput reconstructs the current active-contact set after each ordered sample.

## Event Pump and Simulation Cadence

The input pump owns a global ordered pending-event queue shared by nested RunNode::run() loops. The queue cannot belong to a scene because a legacy event may synchronously open another run loop, and later input must belong to the new active owner.

At each interactive render:

1. Poll SDL until its queue is empty. Resolve registered action markers into owned pending variants, append events/actions in order, and omit only raw geometry transport copies already correlated with an owned PresentGeometryAction.
2. Start an input frame for the current RunNode::run() owner and its current ownership epoch.
3. Process the global pending queue from its front. Polling performs source normalization and ownership transfer only; ImGui, routing, tracker mutation, and game callbacks never preprocess a future queued event.
4. For one queue-front pointer event at a time:
   - classify its event family without preprocessing or removing a later event;
   - detach that single event into an in-flight slot so a synchronously entered nested run cannot see it again; TouchFrame::next() performs this operation for DirectTouch;
   - reject residual synthesized streams centrally;
   - update physical touch state and select the global primary candidate;
   - process the native mouse event or prescribed primary-finger translation through ImGui exactly once;
   - make the fixed game-ownership decision;
   - dispatch the resulting PointerEvent immediately, or expose the DirectTouch sample through a TouchFrame cursor;
   - return control to PointerInput after the callback.
5. After every PointerEvent callback, yielded TouchSample, or ImGui ownership transition, compare the active run owner, ownership epoch, touch policy, control-layout revision, and exit state with the frame-start values. If any changed, stop immediately. Every event not yet detached remains queued.
6. Stop at the first keyboard, gamepad, text, PresentGeometryAction, window, ApplicationLifecycleAction, ApplicationCancelAction, or other legacy ordering boundary. A legacy event is not converted, passed to ImGui, or marked consumed until it is actually selected for dispatch.
7. If owner identity, epoch, and exit state still match after pointer-only dispatch, detach at most one legacy event and commit it to this render. Process its global/ImGui handling once. The normal RunNode tree update receives the prescribed raw, embedded, or semantic event when unconsumed, or EVENT_FIRST when no legacy event exists or the boundary is consumed by global handling or ImGui.
8. If the frame-start owner exited, was removed, or ceased to be the active synchronous run owner before the normal update call, skip that update. An epoch change caused by the already-detached legacy event does not by itself cancel its committed update.
9. Draw and present once if the run loop remains active.

Detaching creates an RAII in-flight event/sample owned by the current C++ call frame. In-flight storage is stack-reentrant: a nested run loop may detach its own event without overwriting the suspended caller's event.

PointerEvent and TouchFrame callbacks never substitute for the normal update and never call dealEvent(), dealEvent2(), or the once-per-render battle simulation code. BattleSceneHades continues to advance simulation only from its normal dealEvent() call.

Each render consumes the complete consecutive pointer prefix belonging to the current owner before the next legacy or ownership boundary. During touch-only input this removes the one-touch-event-per-render bottleneck: a 120 Hz touch stream can be fully consumed by a 60 Hz render without advancing simulation per sample.

The initial implementation preserves every DirectTouch motion sample in order. Batching is not required to prevent backlog because samples use the non-simulation path. Future coalescing is allowed only after proving semantic equivalence: it must preserve maximum baseline excursion, threshold crossing, active-contact and finger-count transitions, final position, accumulated delta, and pair geometry. It may never cross down/up/cancel, capture, policy, owner, epoch, PointerEvent, control-layout, or legacy-event boundaries.

High-rate gamepad-axis coalescing must not flush unrelated SDL events.

SDL's raw geometry queue is not sufficient because its superseded-event filter can erase a resize boundary across intervening input. PointerInput first establishes its initial committed snapshot from the initialized window/present layout, then installs a source-side SDL event watch before interactive input begins. The adapter covers WINDOW_RESIZED, WINDOW_PIXEL_SIZE_CHANGED, display-scale/safe-area/fullscreen/maximize/restore transitions when they change the computed snapshot, plus every project-owned present-layout mutation named above. It copies each accepted change into a zero-initialized registered PresentGeometryAction and appends it with SDL_PeepEvents(..., SDL_ADDEVENT, ...). Direct queue addition bypasses recursive event watches. SDL_PushEvent() invokes watches before it adds the original event, so the action is ordered before its wrapped raw window event; later SDL coalescing can remove raw resize events but cannot remove the project action.

This marker replacement is the sole transport-normalization exception to strict front dispatch. While polling in order, the adapter may atomically take an action payload, omit its correlated raw wrapper, and coalesce immediately adjacent eligible geometry actions after those wrappers are omitted. It performs no callbacks, pointer conversion, ownership/epoch mutation, or live-layout query, and it never scans past an unmatched event to find a pair or coalescing candidate.

The registered SDL marker carries only an opaque monotonic serial, never a raw payload pointer. A thread-safe project-owned action store owns the immutable PresentGeometrySnapshot plus an optional source window event. Polling atomically takes that payload into the C++ pending-event variant; queue failure rolls back the stored entry, and shutdown/flush clears any untaken entries. The resulting owned PresentGeometryAction drops only the raw geometry event correlated with its wrapper revision; unwrapped startup events are handled normally. It is valid for SDL to have already coalesced a correlated raw copy away. When the action is selected as the one legacy boundary, PointerInput installs its payload and increments the epoch. A wrapped window change is delivered to ImGui and the normal dispatcher through its embedded original event exactly once; a present-only project change produces the normal EVENT_FIRST update. Raw wrapped window events never query live Engine geometry, install a second snapshot, cause a second epoch reset, or add a second normal update.

PresentGeometryAction values are never coalesced across pointer input or another legacy boundary. Consecutive actions of the same geometry event type for the same window may keep only the latest snapshot when no input or other boundary lies between them, matching SDL's harmless resize compression without erasing an input-layout transition.

Engine's live render geometry is deliberately separate from PointerInput's historical committed snapshot. The platform and Engine may apply the newest window/canvas/present layout synchronously so drawing uses the latest state; processing an older queued action never rolls live rendering back. Pointer callbacks and TouchFrame consumers use only the coordinates already converted from the action timeline and never query the live present rect.

The new classifier replaces the old isSpecialEvent() whitelist. It explicitly recognizes all four finger phases plus PresentGeometryAction, SDL window hidden/shown, focus lost/gained, queued quit, ApplicationLifecycleAction, and ApplicationCancelAction. Window hidden/focus lost and application background reset capture before any queued suffix is routed; a committed PresentGeometryAction installs its payload before a later pointer sample is routed.

SDL 3.4.2 delivers terminating, low-memory, and application background/foreground events only through SDL_AddEventWatch(); those SDL event types never enter the normal event queue. For input ordering, the application watch maps SDL_EVENT_DID_ENTER_BACKGROUND and SDL_EVENT_DID_ENTER_FOREGROUND to one registered ApplicationLifecycleAction for each transition and adds it directly to the tail of SDL's queue with SDL_PeepEvents(..., SDL_ADDEVENT, ...). The WILL phases are not copied as additional actions. Direct queue addition avoids recursively invoking event watchers and preserves the action after already-queued input and before events queued later in the same SDL pump. Existing synchronous audio pause/resume and low-memory handling may remain in the watch.

ApplicationLifecycleAction carries Background or Foreground. When selected at the legacy queue front, Background increments the ownership epoch, cancels capture, and rejects Android contacts still physically down before any queued suffix is routed. Foreground is an ordering boundary but never resurrects or reclaims a contact. The normal dispatcher may translate the action to the existing semantic lifecycle event once; neither phase adds a second normal update.

Termination is different: on Emscripten, beforeunload may never return control to the render loop, and Android destruction also flushes the normal queue before emitting SDL_EVENT_TERMINATING. The application watch handles termination synchronously with termination-safe application operations only. It does not enqueue termination into PointerInput, invoke RunNode or scene callbacks, or depend on a later normal update.

All PointerInput state changes, ImGui routing, RunNode callbacks, and camera/control mutation occur on the SDL video/main thread while consuming the pending queue. Android JNI callbacks and the optional Java fallback may only enqueue/copy input. SDL event watches do not route pointer input or mutate tracker/scene state; they only copy immutable PresentGeometryAction/ApplicationLifecycleAction payloads into queue order or perform the isolated synchronous termination handling.

### Nested run loops and ownership epochs

Input after an event that opens a nested menu belongs to the nested run owner, not the still-drawn parent. Detaching only the current raw event leaves the suffix available to the nested loop. When that loop returns, the epoch changes again, so the suspended outer input frame cannot resume with stale capture or geometry.

Increment the ownership epoch and reset direct-touch gesture state when:

- a nested RunNode::run() owner is entered or exited;
- paper view is enabled or disabled;
- an ImGui overlay opens, closes, or invalidates existing game capture;
- the app backgrounds, an Emscripten canvas/window is hidden, or window focus is lost;
- the window/present geometry changes;
- a scene exits;
- noninteractive drawAndPresent() processing discards input.

For ordinary epoch changes, contacts still physically down remain rejected until their physical up or cancel. They must not become a new camera or UI gesture halfway through a sequence.

Emscripten window-hidden or focus-loss is a harder boundary because browsers may drop every terminal pointer event. This rule supersedes the ordinary rejected-until-terminal rule. On that boundary PointerInput idempotently cancels game and ImGui ownership, clears all browser physicalContacts and the global-primary sequence, and records the old contact IDs as requiring a fresh finger-down. Late motion/up/cancel from the abandoned generation is ignored; window-shown/focus-gained does not resurrect it. A new down starts a new physical sequence even if the browser reuses a pointer ID. If SDL's internal finger table still contains that ID, its cancel-then-down repair sequence is handled as an ignored abandoned cancel followed by the accepted new down. This prevents an absent terminal event from permanently sticking primary-touch state.

A queued legacy boundary is eligible only when both owner identity and ownership epoch still match the frame-start values after pointer-only dispatch. If an earlier pointer callback changes either value or the owner exit state, the suspended frame performs no normal update; its next run-loop iteration begins a fresh input frame and the legacy event remains raw in the queue.

Once an eligible legacy event is detached, its dispatch is committed. Geometry, lifecycle, ImGui, or normal-handler side effects may then change the epoch, but the detached event is still handled exactly once unless the owner exits. The new epoch applies to the queued suffix.

### Noninteractive and browser processing

During dealEventSelfChilds(false), poll SDL into the shared queue, cancel captures, release any temporary ImGui touch button, and increment the epoch once. Then preserve strict queue-front order: detach each pointer/finger event in the front segment, update physical state, and discard it without PointerEvent or TouchFrame callbacks. A down observed here is physical-state-only and rejected for later game ownership. At the first eligible legacy boundary, detach and process its global side effects at most once for this noninteractive render. The pump may then discard the immediately following pointer segment in the same way, but it stops before a second legacy boundary. No discarded pointer is replayed later and no suffix is removed by scanning past an unprocessed boundary.

Native and Emscripten builds use the same ordered PointerInput pump and routing state machine. Emscripten has no separate per-event scene-dispatch branch: each browser render consumes its eligible pointer prefix and performs at most one normal update.

## Routing and Capture Order

Touch ownership is keyed by the SDL touch/finger ID pair from down through up/cancel. Physical mouse capture is keyed by mouse ID and button from button-down through button-up/cancel. Unpressed mouse motion and wheel have no persistent sequence owner and are routed against the current owner for each event.

For each touch sequence, ownership is decided on down and remains fixed until up or cancel:

1. ImGui overlay;
2. current RunNode input policy;
3. ignored/outside input.

The global primary candidate is still the first physical finger even when a higher-priority component captures it. Secondary contacts are never promoted to ImGui or generic pointer input within that physical sequence.

### RunNode pointer dispatch and capture

RunNode::dispatchPointerEvent() is a pointer-only traversal. It visits visible children in reverse draw order and then the current node. A pointer handler returns Ignored, Handled, Captured, or Released, and traversal stops at the first handled or captured target.

On button down, the first Captured result becomes the weak capture target for that pointer ID and button. Subsequent motion, up, and cancel for that capture go only to the target. Up/cancel releases capture after dispatch. Uncaptured hover motion is hit-tested each event. A hidden, removed, exited, or epoch-invalidated capture target is canceled if still reachable; its central capture state is cleared in all cases and it cannot activate.

Pointer activation, including migrated mouse/touch OK and physical-right-button cancel behavior, occurs in this pointer-only path. It may call the same semantic action methods as keyboard/gamepad handling, but it never invokes dealEvent() or creates an additional normal update. Legacy isPressOK() retains keyboard/gamepad semantics. isPressCancel() recognizes Escape release, gamepad East release, and an unconsumed ApplicationCancelAction; physical right-button release reaches the same semantic cancel method through PointerEvent instead of that helper.

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

If ImGui captures the global primary sequence, every contact in that physical touch sequence is excluded from logical-pointer and DirectTouch game delivery until the physical contact count returns to zero.

### VirtualStick removal

Delete VirtualStick rather than adapting it:

- delete src/VirtualStick.cpp and src/VirtualStick.h;
- remove RunNode::use_virtual_stick_, setUseVirtualStick(), the singleton, and all draw/event hooks;
- remove Application's game.use_virtual_stick configuration read;
- remove Engine's virtual_stick_button_/virtual_stick_axis_ storage, setters, clear helpers, and fallback reads from gameControllerGetButton()/gameControllerGetAxis(); those queries become physical-gamepad-only and return unpressed/zero when no physical input is active;
- remove Button::button_id_, which exists only for VirtualStick;
- remove the VirtualStick entries and now-empty filter from the Visual Studio project files;
- remove any obsolete configuration documentation without adding a compatibility shim.

CMake and Android source collection require no explicit removal entry because they glob the source tree. No tracked top-level config currently needs value migration because config/chess_*.yaml contains no use_virtual_stick key. Physical gamepad polling, mapping, repeat timing, rumble, and controller APIs remain unchanged. Shared texture-atlas entries are not deleted solely because VirtualStick referenced them.

No VirtualControlAction, virtual controller injection, reserved screen region, or replacement capture layer is introduced. Former VirtualStick areas route normally through ImGui, PrimaryPointer, or DirectTouch.

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

SuperMenuText may retain its intentional mobile two-distinct-tap UX: the first tap locks an item and the second tap commits it. Remove tapLockTime_, lastPageFlipTime_, kDoubleTapMinIntervalMs, canFlipPage() timing suppression, and the raw-touch-plus-synthetic-mouse burst comments. Pointer capture already guarantees one activation per physical sequence, so duplicate-stream timing guards must not remain. Page navigation activates once per captured tap without a time heuristic.

Mouse wheel and physical right-click remain mouse-specific PointerEvent operations. The Android and web cancel overlays are not mice: nativeInjectRightClick() and inject_right_click() are replaced by one ApplicationCancelAction path. ImGui gets the first chance to close an open overlay. If it does not consume the action, the action is the sole legacy event passed to the normal RunNode update and isPressCancel() recognizes it exactly once. Do not invoke cancel globally before passing the same action through the tree. Escape, gamepad East, ApplicationCancelAction, and physical right-button PointerEvent all converge on the same semantic cancel method.

Migration is complete only when no scene, menu, control, or camera handler branches directly on SDL mouse event types or queries SDL_GetMouseState() for interaction coordinates. Native SDL mouse ingestion is confined to PointerInput/Engine. This rule includes Menu.cpp, UI.cpp, Scene pointer-position helpers, SubScene hover, PositionSwapNode, and inherited RunNode UI paths. Direct Escape/right-button cancel checks in MainScene, SubScene, and other consumers migrate to the shared semantic cancel contract so ApplicationCancelAction cannot bypass them.

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

    windowExtentX = windowWidth - 1
    windowExtentY = windowHeight - 1
    windowX = normalizedX * windowExtentX
    windowY = normalizedY * windowExtentY
    windowDx = normalizedDx * windowExtentX
    windowDy = normalizedDy * windowExtentY
    uiX = (windowX - presentX) * uiWidth / presentWidth
    uiY = (windowY - presentY) * uiHeight / presentHeight
    uiDx = windowDx * uiWidth / presentWidth
    uiDy = windowDy * uiHeight / presentHeight

SDL's Android and Emscripten backends normalize against sourceSize - 1, so multiplying by the matching SDL window extent preserves the first and last addressable window coordinates. Interactive routing requires a valid snapshot with positive window and present dimensions. Use floating-point coordinates for gesture math.

A new sequence must begin inside the current present rect. Do not clamp a letterbox or outside touch into the battlefield. Once a camera-owned sequence begins, movement may leave the rect without changing ownership; terminal up/cancel always clears the state.

No Android landscape-rotation transform is added in C++. SDLSurface and SDL already report surface-relative coordinates in the active orientation.

On Emscripten, SDL normalizes browser Pointer Event coordinates against the canvas CSS extent. PointerInput multiplies them by the window-coordinate extent stored in the currently committed PresentGeometrySnapshot, the same coordinate space used by SDL mouse events and that snapshot's present rect, then applies the same present-rect conversion used on Android. It never applies an additional DOM CSS, backing-store, or device-pixel-ratio multiplier. Game code never reads DOM CSS coordinates directly. Device-pixel-ratio and backing-size changes affect live rendering and enqueue ordered geometry/pixel-size snapshots before subsequent browser input.

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

Emscripten finger cancellation is contact-specific: SDL maps browser pointercancel or touch pointer-leave to SDL_EVENT_FINGER_CANCELED for that pointer. Pointer-up calls the final touch-motion update before the terminal up; when normalized x or y changed, the resulting motion event must be preserved and applied first, while an unchanged terminal position produces only up. Emscripten touch pressure remains 1.0. Pointer-cancel has no final motion update. Browser cancellation uses the same terminal-contact and capture-release machinery but never inherits Android's stream-wide-cancel rule. Repeated cancel/leave for an already-retired contact is ignored idempotently. Browser window hidden/focus loss performs the hard physical-contact reset described above; canvas geometry invalidation or another explicit central reset cancels game ownership under the normal epoch rules.

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

- the global ordered pending variant queue of raw SDL events and owned application/geometry actions;
- PointerEvent;
- TouchFingerKey, TouchSample, and TouchFrame;
- physical mouse and touch state;
- PrimaryPointerTracker;
- input ownership epochs, capture records, and rejected-contact tombstones;
- committed PresentGeometrySnapshot state and PresentGeometryAction ingestion;
- queue-front source conversion, routing, and cancellation;
- selection of the global primary finger and calls into ImGuiLayer's touch entry point;
- ApplicationLifecycleAction ingestion and ordering;
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

## Platform Boundaries and Fallbacks

ApplicationCancelAction has one shared C++ enqueueApplicationCancelAction() helper. Android's nativeRequestCancel JNI method and Emscripten's exported request_cancel() function both call it, producing one registered SDL user event in queue order. If an ImGui battle-log/changelog overlay is open, ImGuiLayer consumes the action, closes the overlay, and the normal update receives EVENT_FIRST. Otherwise the action itself is the one normal-update event and isPressCancel() maps it to semantic cancel once. Neither platform fabricates mouse down/up events or changes logical pointer state.

### Android

The primary touch implementation adds no battle-state handling to KysActivity and leaves the locally copied SDL 3.4.2 Java bridge unchanged. The bridge is supplied from the matching vcpkg SDL build as documented in android/README.md and is ignored by git, so the build prerequisite must keep that version match reproducible.

KysActivity's overlay boundary is intentionally changed. Rename addRightClickOverlay() to addCancelOverlay(), update its comments/accessibility description to Cancel, rename nativeInjectRightClick() to nativeRequestCancel(), and call enqueueApplicationCancelAction(). No right-click terminology or device-event injection remains in the Java overlay path.

The Android overlay currently exists before Engine initializes SDL's event queue and registers custom action types. Create the Cancel control disabled and enable it on the Android UI thread only after a native input-ready callback confirms PointerInput, ApplicationCancelAction, ApplicationLifecycleAction, and PresentGeometryAction registration is complete. nativeRequestCancel() is valid only after that boundary and must never attempt to push event type 0. The web control remains shielded by its higher-z loading overlay until the runtime is initialized.

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

### Emscripten mobile web

The primary web implementation uses SDL 3.4.2 raw finger events only. Do not add a parallel JavaScript touch listener, DOM compatibility-mouse adapter, browser-specific camera path, or custom primary-pointer implementation. SDL's canvas Pointer Events backend is the sole browser-touch source for the game.

Update wasm/shell.html as follows:

- rename rclick-btn/rclickBtn and its accessibility label to cancel-btn/cancelBtn and Cancel;
- replace its touchstart-only injection with one pointerdown handler that calls exported request_cancel(), which calls enqueueApplicationCancelAction();
- retain preventDefault()/stopPropagation() so the DOM control never reaches the canvas input stream;
- generalize the document-level DOM exclusion predicate to cover the Cancel control as well as the external-save dialog; the touchstart/click handlers that focus the canvas must ignore Cancel events, so activating Cancel neither focuses the canvas nor changes its focus epoch;
- remove the unused lastTouchX/lastTouchY variables and canvas touchstart/touchmove listeners;
- keep touch-action: none, canvas focus, fullscreen controls, and external-save dialog event exclusion;
- do not use GameUtil::isMobileDevice() to choose the event source; it may still control intentional mobile UI presentation.

Canvas SDL finger coordinates remain authoritative across CSS scaling and device-pixel ratio changes. Browser pointercancel/leave terminates the corresponding contact. SDL window-hidden, focus, resize, fullscreen, and orientation events feed the same epoch/reset rules as Android lifecycle and geometry changes.

resize_to_viewport() may update the canvas, SDL window, and Engine render rect synchronously so the next draw uses the latest live layout. Its SDL window transition invokes the sole publishPresentGeometryTransition() path through the source-side watch before browser input generated after that transition; resize_to_viewport() and the later live-layout application do not publish a second action. PointerInput keeps its historical committed snapshot until the action reaches queue front and never substitutes the already-new live Engine rect for older queued input.

Any future JavaScript finger fallback must be a separately approved, mutually exclusive source mode feeding the same PointerInput queue. It is outside this implementation; unsupported Pointer Events browsers do not silently fall back to compatibility mouse input.

## Testing

### PointerInput unit tests

- Native mouse produces one logical pointer sequence.
- Primary finger produces exactly one logical left-pointer down/motion/up.
- Additional fingers produce no generic pointer actions.
- DirectTouch ownership of the global primary finger prevents a second finger from becoming the generic primary pointer.
- DirectTouch mode produces TouchFrame input and no game-side primary-pointer duplicate.
- Independently interleaved browser events for two contacts update active-contact snapshots and pair geometry one contact at a time.
- An unchanged browser pointer-up yields up only; a changed terminal position yields motion then up, and the motion is applied before control activation, pointer release, or a 2-to-1 gesture transition.
- Browser pointer-cancel/leave yields cancel without a synthetic final motion, camera delta, or click activation; a duplicate terminal event for the retired contact is harmless.
- Normalized Android and browser coordinates map the first/last addressable source positions to the first/last SDL window coordinates without an extra device-pixel-ratio scale.
- A queue containing touch(L0), PresentGeometryAction(L1), touch(L1), PresentGeometryAction(L2), touch(L2), drained after Engine already renders L2, converts the three touches with L0, L1, and L2 respectively.
- SDL removal of a wrapped raw resize event across intervening input leaves its PresentGeometryAction intact; the surviving raw wrapped copy causes no second snapshot, epoch reset, callback, or normal update.
- Multiple browser resizes between renders preserve action order, while only consecutive same-type/same-window actions with no intervening input or legacy boundary may coalesce.
- Changing any present-layout input enqueues one new snapshot; recomputing the identical per-draw layout enqueues none.
- One SDL resize, including resize_to_viewport(), produces exactly one PresentGeometryAction, epoch change, and normal update; applying its live Engine layout does not republish it.
- PresentGeometryAction markers contain only serials; polling transfers payload ownership once, and queue flush/termination leaves no leaked or dangling stored payload.
- Residual SDL_TOUCH_MOUSEID mouse events are rejected.
- SDL_MOUSE_TOUCHID and SDL_PEN_TOUCHID finger streams are rejected.
- Physical pen input never becomes a game finger stream and retains its existing pen/mouse behavior.
- Down and up in one render are both preserved.
- Every initial DirectTouch motion sample is preserved, including a threshold-crossing excursion that returns near its baseline within one render.
- A 120 Hz touch stream consumed by a 60 Hz render loop does not build a motion backlog during an uninterrupted pointer prefix.
- N PointerEvent/TouchFrame samples with no legacy event invoke all N pointer callbacks, invoke no dealEvent() from those callbacks, and then perform exactly one normal dealEvent(EVENT_FIRST) update.
- N pointer samples followed by one legacy event perform exactly one normal update carrying that legacy event.
- An owner or epoch change during the pointer prefix performs zero normal updates for the abandoned owner and leaves the undetached suffix queued.
- Noninteractive pointer / legacy / pointer / legacy input discards the two pointer segments in front order, processes only the first legacy boundary's global effects, stops before the second legacy boundary, and never replays a discarded down as game ownership.
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
- Browser window-hidden/focus-lost cancels game capture and ImGui, clears physical/global-primary state, and remains idempotent if both boundaries arrive.
- Late browser motion/up/cancel from the abandoned generation is ignored; window-shown/focus-gained does not resurrect it, and a fresh down is accepted when the browser reuses the same pointer ID.
- SDL's stale-finger cancel-then-down repair for a reused browser pointer ID ignores the abandoned-generation cancel without clearing the fresh-down requirement, then starts exactly one new generation on the immediately following down. A second hidden/focus-lost reset preserves existing awaiting-fresh-down IDs.
- Pointer A / Android DID_ENTER_BACKGROUND watch copy / pointer B ordering is preserved. Each DID_ENTER_BACKGROUND/DID_ENTER_FOREGROUND transition produces one ApplicationLifecycleAction, WILL phases produce none, SDL_PeepEvents(SDL_ADDEVENT) does not recursively re-enter watchers, only Background resets at the queue boundary, and Foreground never resurrects contacts.
- SDL_EVENT_TERMINATING is handled synchronously by the application watch and is never replayed through PointerInput or a normal scene update.
- Android and web cancel overlays each produce one ApplicationCancelAction, no mouse down/up events, and close ImGui or game UI exactly once.
- Pointer / ApplicationCancelAction / pointer ordering is preserved; an unconsumed action causes exactly one cancel in exactly one normal update, while an ImGui-consumed action causes no game cancel and one EVENT_FIRST normal update.
- An Android Cancel tap before native input-ready cannot invoke nativeRequestCancel() or enqueue an invalid event; enabling the control after registration produces one valid action.
- Platform wrappers contain no addRightClickOverlay(), nativeInjectRightClick(), inject_right_click(), rclick-btn, or fake-right-click comments/device-event shims.
- Equivalent Android and Emscripten finger traces produce the same PointerEvent, TouchFrame, capture, and paper-camera actions after platform normalization.

### Pointer migration tests

- Generic Button activates once per physical touch.
- Button/control down outside and up inside does not activate when capture is required.
- Removing, hiding, exiting, or epoch-invalidating a captured target cancels it without activation.
- ChessSystemSettingsMenu toggles once and sliders receive one motion stream.
- MainScene and SubScene use logical pointer coordinates for pathfinding.
- UIItem dragging no longer depends on SDL_GetMouseState().
- BattleStatsView and SuperMenuText taps remain single actions.
- SuperMenuText mobile double-tap mode locks on one captured tap and commits on the next distinct tap without a time-based duplicate-stream guard; page navigation flips once per tap.
- Two distinct SuperMenuText tap sequences drained in one browser render may lock then commit; one captured physical sequence can never activate twice.
- Classic 2D battle camera and position swap retain their existing behavior.
- The build contains no VirtualStick source, symbol, RunNode hook, Engine virtual-controller state, configuration read, project entry, or VirtualControlAction type.

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
- No VirtualStick is drawn; touches in its former screen regions follow normal UI or paper-camera routing.
- Real mouse input remains unchanged.

### Manual Emscripten smoke test

- Log SDL_EVENT_FINGER_* input and confirm one physical touch produces no game-visible synthetic mouse stream.
- Confirm primary-finger selection, no secondary promotion, independently interleaved two-contact input, and short down/up within one browser render.
- Confirm a changed terminal pointer-up applies motion before up, an unchanged pointer-up produces only up, and contact-specific pointercancel/leave produces no final motion or activation.
- Confirm window-hidden/focus-lost hard reset, ignored late events, no resurrection on show/focus gain, and a fresh down with a reused pointer ID.
- Instrument beforeunload once and confirm SDL_EVENT_TERMINATING is handled synchronously by the application watch without entering PointerInput or causing a normal scene update.
- Emscripten drains the full eligible pointer prefix without simulation-per-event; pointer/legacy ordering and at-most-one normal update per browser render are preserved.
- Ordinary menus, settings, sliders, item dragging, pathfinding, text paging, intentional SuperMenuText two-tap UX, and ImGui work by touch.
- Paper-mode top-right controls, one-finger pan, pinch, combined two-finger rotation/height, and 1-to-2-to-1 transitions match Android behavior.
- A battlefield drag ending over each top-right control activates none of them.
- CSS scaling, device-pixel ratio, canvas backing-size changes, letterboxing, visual-viewport resize, mobile orientation changes, and fullscreen transitions preserve coordinate mapping and reset geometry safely.
- The web Cancel DOM control emits one ApplicationCancelAction, does not focus the canvas, and never creates a mouse or finger event on the canvas.
- No custom JavaScript touch listener feeds game input; the unused last-touch listeners are absent.
- No VirtualStick is drawn; touches in its former screen regions follow normal UI or paper-camera routing.
- Real mouse input remains native and does not create finger input.

## Feasibility Conclusion

This design is cleanly implementable through the existing SDL raw-finger boundaries on Android and Emscripten mobile web, with both platforms feeding the same C++ PointerInput architecture.

The required input refactor is broader than adding a camera helper, but it removes the duplicate source at its origin, gives pointer input a non-simulation dispatch path, preserves render-frame latency, and creates one reusable touch foundation instead of adding paper-camera-specific exceptions.

Do not implement an intermediate state that both enables raw finger game handling and leaves SDL touch-to-mouse synthesis active.
