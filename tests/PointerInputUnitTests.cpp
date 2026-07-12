#include "PointerInput.h"
#include "BattleCursor.h"
#include "BattleSceneCamera.h"
#include "Menu.h"
#include "ShowExp.h"
#include "SuperMenuText.h"
#include "TextBox.h"
#include "ImGuiLayer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <utility>

using Catch::Approx;

namespace
{
class PointerRouteProbe : public RunNode
{
public:
    template <class Handler>
    static auto route(std::size_t count, Handler&& handler)
    {
        return routePointerEventPath(count, std::forward<Handler>(handler));
    }

};
}

TEST_CASE("PointerInput maps normalized touch coordinates through the committed present geometry", "[pointer_input]")
{
    PresentGeometrySnapshot geometry;
    geometry.revision = 7;
    geometry.windowWidth = 1920;
    geometry.windowHeight = 1080;
    geometry.presentRect = {160, 0, 1600, 900};
    geometry.uiWidth = 1280;
    geometry.uiHeight = 720;

    const auto first = makeTouchSample(
        TouchPhase::Down,
        {1, 2},
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        9,
        11,
        geometry);
    const auto last = makeTouchSample(
        TouchPhase::Motion,
        {1, 2},
        1.0f,
        1.0f,
        0.25f,
        -0.5f,
        1.0f,
        9,
        12,
        geometry);

    CHECK(first.windowPosition.x == 0.0f);
    CHECK(first.windowPosition.y == 0.0f);
    CHECK_FALSE(first.insidePresent);
    CHECK(last.windowPosition.x == 1919.0f);
    CHECK(last.windowPosition.y == 1079.0f);
    CHECK(last.windowDelta.x == 479.75f);
    CHECK(last.windowDelta.y == -539.5f);
    CHECK(last.uiPosition.x == Approx(1407.2f));
    CHECK(last.uiPosition.y == Approx(863.2f));
}

TEST_CASE("PrimaryPointerTracker never promotes a secondary finger during one physical sequence", "[pointer_input]")
{
    PrimaryPointerTracker tracker;
    const TouchFingerKey first{1, 10};
    const TouchFingerKey second{1, 11};

    CHECK(tracker.onDown(first));
    CHECK_FALSE(tracker.onDown(second));
    CHECK(tracker.isPrimary(first));
    CHECK_FALSE(tracker.isPrimary(second));

    tracker.onTerminal(first);
    CHECK_FALSE(tracker.hasPrimary());
    CHECK_FALSE(tracker.isPrimary(second));

    tracker.onTerminal(second);
    const TouchFingerKey next{1, 12};
    CHECK(tracker.onDown(next));
    CHECK(tracker.isPrimary(next));
}

TEST_CASE("PointerInput rejects residual synthesized touch and pen streams", "[pointer_input]")
{
    SDL_Event mouse = {};
    mouse.type = SDL_EVENT_MOUSE_MOTION;
    mouse.motion.which = SDL_TOUCH_MOUSEID;
    CHECK(isResidualSyntheticPointerEvent(mouse));

    SDL_Event touch = {};
    touch.type = SDL_EVENT_FINGER_DOWN;
    touch.tfinger.touchID = SDL_MOUSE_TOUCHID;
    CHECK(isResidualSyntheticPointerEvent(touch));

    touch.tfinger.touchID = SDL_PEN_TOUCHID;
    CHECK(isResidualSyntheticPointerEvent(touch));

    touch.tfinger.touchID = 42;
    CHECK_FALSE(isResidualSyntheticPointerEvent(touch));
}

TEST_CASE("computePresentLayout uses one stable formula for rendering and pointer conversion", "[pointer_input]")
{
    PresentLayoutInput input;
    input.windowWidth = 1920;
    input.windowHeight = 1080;
    input.textureWidth = 1280;
    input.textureHeight = 720;
    input.uiWidth = 1280;
    input.uiHeight = 720;

    const auto geometry = computePresentLayout(input, 3);
    CHECK(geometry.revision == 3);
    CHECK(geometry.presentRect.x == 0);
    CHECK(geometry.presentRect.y == 0);
    CHECK(geometry.presentRect.w == 1920);
    CHECK(geometry.presentRect.h == 1080);

    input.windowWidth = 2000;
    const auto letterboxed = computePresentLayout(input, 4);
    CHECK(letterboxed.presentRect.x == 40);
    CHECK(letterboxed.presentRect.y == 0);
    CHECK(letterboxed.presentRect.w == 1920);
    CHECK(letterboxed.presentRect.h == 1080);
}

TEST_CASE("PointerInput preserves pointer and legacy queue order", "[pointer_input]")
{
    PointerInput input;
    SDL_Event down = {};
    down.type = SDL_EVENT_FINGER_DOWN;
    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_UP;
    SDL_Event up = {};
    up.type = SDL_EVENT_FINGER_UP;

    input.enqueueForTest(down);
    input.enqueueForTest(key);
    input.enqueueForTest(up);

    REQUIRE(input.pendingCount() == 3);
    CHECK(input.frontIsPointer());
    CHECK(input.popPending().event().type == SDL_EVENT_FINGER_DOWN);
    CHECK_FALSE(input.frontIsPointer());
    CHECK(input.popPending().event().type == SDL_EVENT_KEY_UP);
    CHECK(input.frontIsPointer());
    CHECK(input.popPending().event().type == SDL_EVENT_FINGER_UP);
}

TEST_CASE("PointerInput owns SDL temporary event payloads", "[pointer_input][event_lifetime]")
{
    SECTION("text input and editing")
    {
        PointerInput input;
        char inputText[] = "showmethemoney";
        SDL_Event textInput = {};
        textInput.type = SDL_EVENT_TEXT_INPUT;
        textInput.text.text = inputText;
        input.enqueueForTest(textInput);

        char editingText[] = "候選";
        SDL_Event textEditing = {};
        textEditing.type = SDL_EVENT_TEXT_EDITING;
        textEditing.edit.text = editingText;
        input.enqueueForTest(textEditing);

        inputText[0] = 'X';
        editingText[0] = 'X';

        const auto queuedInput = input.popPending();
        CHECK(std::string(queuedInput.event().text.text) == "showmethemoney");
        const auto queuedEditing = input.popPending();
        CHECK(std::string(queuedEditing.event().edit.text) == "候選");
    }

    SECTION("IME candidates")
    {
        PointerInput input;
        char first[] = "第一";
        char second[] = "第二";
        const char* candidates[] = {first, second};
        SDL_Event event = {};
        event.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
        event.edit_candidates.candidates = candidates;
        event.edit_candidates.num_candidates = 2;
        input.enqueueForTest(event);

        first[0] = 'X';
        second[0] = 'X';

        const auto queued = input.popPending();
        REQUIRE(queued.event().edit_candidates.candidates != nullptr);
        CHECK(std::string(queued.event().edit_candidates.candidates[0]) == "第一");
        CHECK(std::string(queued.event().edit_candidates.candidates[1]) == "第二");
    }

    SECTION("drop and clipboard data")
    {
        PointerInput input;
        char source[] = "Explorer";
        char path[] = "save.dat";
        SDL_Event drop = {};
        drop.type = SDL_EVENT_DROP_FILE;
        drop.drop.source = source;
        drop.drop.data = path;
        input.enqueueForTest(drop);

        char firstMime[] = "text/plain";
        char secondMime[] = "text/html";
        const char* mimeTypes[] = {firstMime, secondMime};
        SDL_Event clipboard = {};
        clipboard.type = SDL_EVENT_CLIPBOARD_UPDATE;
        clipboard.clipboard.mime_types = mimeTypes;
        clipboard.clipboard.num_mime_types = 2;
        input.enqueueForTest(clipboard);

        source[0] = 'X';
        path[0] = 'X';
        firstMime[0] = 'X';
        secondMime[0] = 'X';

        const auto queuedDrop = input.popPending();
        CHECK(std::string(queuedDrop.event().drop.source) == "Explorer");
        CHECK(std::string(queuedDrop.event().drop.data) == "save.dat");
        const auto queuedClipboard = input.popPending();
        REQUIRE(queuedClipboard.event().clipboard.mime_types != nullptr);
        CHECK(std::string(queuedClipboard.event().clipboard.mime_types[0]) == "text/plain");
        CHECK(std::string(queuedClipboard.event().clipboard.mime_types[1]) == "text/html");
    }
}

TEST_CASE("PointerInput converts native mouse events without querying device state", "[pointer_input]")
{
    PointerInput input;
    PresentGeometrySnapshot geometry;
    geometry.revision = 1;
    geometry.windowWidth = 1920;
    geometry.windowHeight = 1080;
    geometry.presentRect = {160, 0, 1600, 900};
    geometry.uiWidth = 1280;
    geometry.uiHeight = 720;
    input.commitPresentGeometry(geometry);

    SDL_Event down = {};
    down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.which = 4;
    down.button.button = SDL_BUTTON_LEFT;
    down.button.down = true;
    down.button.x = 960.0f;
    down.button.y = 450.0f;
    down.button.windowID = 8;
    down.button.timestamp = 10;

    const auto pointer = input.makeMousePointerEvent(down);
    CHECK(pointer.phase == PointerPhase::ButtonDown);
    CHECK(pointer.source == PointerSource::Mouse);
    CHECK(pointer.pointerId == 4);
    CHECK(pointer.uiPosition.x == Approx(640.0f));
    CHECK(pointer.uiPosition.y == Approx(360.0f));
    CHECK(pointer.insidePresent);

    SDL_Event wheel = {};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.which = 4;
    wheel.wheel.x = -1.0f;
    wheel.wheel.y = 2.0f;
    wheel.wheel.mouse_x = 560.0f;
    wheel.wheel.mouse_y = 225.0f;
    const auto wheelPointer = input.makeMousePointerEvent(wheel);
    CHECK(wheelPointer.uiPosition.x == Approx(320.0f));
    CHECK(wheelPointer.uiPosition.y == Approx(180.0f));
}

TEST_CASE("PointerInput commits present geometry synchronously on the game thread", "[pointer_input]")
{
    PointerInput input;
    PresentGeometrySnapshot initial{1, 1280, 720, {0, 0, 1280, 720}, 1280, 720};
    input.commitPresentGeometry(initial);
    PresentGeometrySnapshot resized{2, 1920, 1080, {0, 0, 1920, 1080}, 1280, 720};
    input.commitPresentGeometry(resized);

    CHECK(input.presentGeometry().revision == 2);
    CHECK(input.empty());
}

TEST_CASE("Queued pointer keeps old geometry until its following resize is consumed", "[pointer_input][present_geometry]")
{
    PointerInput input;
    const PresentGeometrySnapshot layout0{1, 1280, 720, {0, 0, 1280, 720}, 1280, 720};
    input.commitPresentGeometry(layout0);

    SDL_Event unrelated = {};
    unrelated.type = SDL_EVENT_USER;
    input.enqueueForTest(unrelated);

    SDL_Event pointer = {};
    pointer.type = SDL_EVENT_MOUSE_MOTION;
    pointer.motion.x = 640.0f;
    pointer.motion.y = 360.0f;
    input.enqueueForTest(pointer);

    SDL_Event resize = {};
    resize.type = SDL_EVENT_WINDOW_RESIZED;
    resize.window.data1 = 1920;
    resize.window.data2 = 1080;
    input.enqueueForTest(resize);

    CHECK(input.popPending().event().type == SDL_EVENT_USER);
    const auto pointerEvent = input.popPending();
    const auto queuedPointer = input.makeMousePointerEvent(pointerEvent.event());
    CHECK(queuedPointer.uiPosition.x == Approx(640.0f));
    CHECK(queuedPointer.uiPosition.y == Approx(360.0f));

    PresentLayoutInput resizedLayout;
    resizedLayout.textureWidth = 1280;
    resizedLayout.textureHeight = 720;
    resizedLayout.uiWidth = 1280;
    resizedLayout.uiHeight = 720;
    const auto resizeEvent = input.popPending();
    REQUIRE(applyWindowResizeToPresentLayout(resizedLayout, resizeEvent.event()));
    input.commitPresentGeometry(computePresentLayout(resizedLayout, 2));

    CHECK(input.presentGeometry().revision == 2);
    CHECK(input.presentGeometry().windowWidth == 1920);
    CHECK(input.presentGeometry().windowHeight == 1080);
}

TEST_CASE("PointerInput rejects late contact events after a hard reset until a fresh down", "[pointer_input]")
{
    PointerInput input;
    input.commitPresentGeometry({1, 1280, 720, {0, 0, 1280, 720}, 1280, 720});
    SDL_Event down = {};
    down.type = SDL_EVENT_FINGER_DOWN;
    down.tfinger.touchID = SDL_TouchID{1};
    down.tfinger.fingerID = SDL_FingerID{7};
    down.tfinger.x = 0.5f;
    down.tfinger.y = 0.5f;
    REQUIRE(input.processFingerEvent(down).has_value());

    input.resetTouchState();
    SDL_Event motion = down;
    motion.type = SDL_EVENT_FINGER_MOTION;
    CHECK_FALSE(input.processFingerEvent(motion).has_value());

    CHECK(input.processFingerEvent(down).has_value());
}

TEST_CASE("Pointer dispatch stops when the control layout changes without rejecting contacts", "[pointer_input]")
{
    const PointerDispatchRevision frameStart{12, 4};

    CHECK(pointerDispatchCanContinue(frameStart, {12, 4}));
    CHECK_FALSE(pointerDispatchCanContinue(frameStart, {12, 5}));
    CHECK_FALSE(pointerDispatchCanContinue(frameStart, {13, 4}));
}

TEST_CASE("Pointer route offers an ignored wheel to its ancestor exactly once", "[pointer_routing]")
{
    int childCalls = 0;
    int parentCalls = 0;
    const auto routed = PointerRouteProbe::route(2, [&](std::size_t index)
    {
        if (index == 0)
        {
            ++childCalls;
            return RunNode::PointerResult::Ignored;
        }
        ++parentCalls;
        return RunNode::PointerResult::Handled;
    });

    REQUIRE(routed.has_value());
    CHECK(routed->first == 1);
    CHECK(routed->second == RunNode::PointerResult::Handled);
    CHECK(childCalls == 1);
    CHECK(parentCalls == 1);
}

TEST_CASE("Pointer route keeps a capturing child ahead of its ancestor", "[pointer_routing]")
{
    int parentCalls = 0;
    const auto routed = PointerRouteProbe::route(2, [&](std::size_t index)
    {
        if (index == 0)
        {
            return RunNode::PointerResult::Captured;
        }
        ++parentCalls;
        return RunNode::PointerResult::Captured;
    });

    REQUIRE(routed.has_value());
    CHECK(routed->first == 0);
    CHECK(routed->second == RunNode::PointerResult::Captured);
    CHECK(parentCalls == 0);
}

TEST_CASE("Ignored child lets the 2D camera ancestor capture and drag", "[pointer_routing][battle_camera]")
{
    auto& input = PointerInput::instance();
    input.commitPresentGeometry({1, 1280, 720, {0, 0, 1280, 720}, 1280, 720});

    BattleSceneCamera camera;
    const BattleSceneCameraBounds bounds{6400.0f, 6400.0f, 320.0f, 240.0f};
    Pointf center{1000.0f, 1000.0f, 0.0f};
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 1;
    event.button = SDL_BUTTON_LEFT;
    event.insidePresent = true;
    event.phase = PointerPhase::ButtonDown;

    const auto routed = PointerRouteProbe::route(2, [&](std::size_t index)
    {
        if (index == 0)
        {
            return RunNode::PointerResult::Ignored;
        }
        camera.handleManualInput(event, center, bounds, true);
        return camera.manualDragging()
            ? RunNode::PointerResult::Captured
            : RunNode::PointerResult::Ignored;
    });

    REQUIRE(routed.has_value());
    CHECK(routed->first == 1);
    CHECK(camera.manualDragging());

    event.phase = PointerPhase::Motion;
    event.uiDelta = {10.0f, 5.0f};
    const auto moved = camera.handleManualInput(event, center, bounds, true);
    REQUIRE(moved.has_value());
    CHECK(moved->x < center.x);
    CHECK(moved->y < center.y);

    event.phase = PointerPhase::ButtonUp;
    camera.handleManualInput(event, *moved, bounds, true);
    CHECK_FALSE(camera.manualDragging());
}

TEST_CASE("Popup activation advances once for a matching left press and release", "[pointer_routing][popup_text]")
{
    PointerActivationSequence sequence;
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 4;
    event.button = SDL_BUTTON_LEFT;
    event.insidePresent = false;

    event.phase = PointerPhase::ButtonDown;
    REQUIRE(sequence.start(event));
    event.phase = PointerPhase::ButtonUp;
    CHECK(sequence.process(event) == PointerActivationAction::Activate);
    CHECK(sequence.process(event) == PointerActivationAction::Ignore);
}

TEST_CASE("Popup release without a preceding press does nothing", "[pointer_routing][popup_text]")
{
    PointerActivationSequence sequence;
    PointerEvent event;
    event.source = PointerSource::Touch;
    event.pointerId = 9;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonUp;

    CHECK(sequence.process(event) == PointerActivationAction::Ignore);
}

TEST_CASE("Popup cancel clears its press without activation", "[pointer_routing][popup_text]")
{
    PointerActivationSequence sequence;
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 4;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonDown;
    REQUIRE(sequence.start(event));

    event.phase = PointerPhase::Cancel;
    CHECK(sequence.process(event) == PointerActivationAction::Cancel);
    event.phase = PointerPhase::ButtonUp;
    CHECK(sequence.process(event) == PointerActivationAction::Ignore);
}

TEST_CASE("A child capture prevents popup background activation", "[pointer_routing][popup_text]")
{
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 2;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonDown;

    CHECK_FALSE(pointerActivationSequenceShouldStart(
        PointerActivationScope::Anywhere,
        event,
        true));
}

TEST_CASE("Normal owners do not accept blank-space activation", "[pointer_routing][popup_text]")
{
    PointerEvent event;
    event.source = PointerSource::Touch;
    event.pointerId = 7;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonDown;

    CHECK(pointerActivationSequenceShouldStart(
        PointerActivationScope::Anywhere,
        event,
        false));
    CHECK_FALSE(pointerActivationSequenceShouldStart(
        PointerActivationScope::HitTargetOnly,
        event,
        false));
    CHECK(TextBox::kPointerActivationScope == PointerActivationScope::HitTargetOnly);
    CHECK(Menu::kPointerActivationScope == PointerActivationScope::HitTargetOnly);
    CHECK(DismissibleTextBox::kPointerActivationScope == PointerActivationScope::Anywhere);
    CHECK(ShowExp::kPointerActivationScope == PointerActivationScope::Anywhere);
}

TEST_CASE("Battle cursor updates the target for short press sequences", "[pointer_migration]")
{
    BattleCursorPointerState state;
    PointerEvent event;
    event.source = PointerSource::Touch;
    event.button = SDL_BUTTON_LEFT;
    event.insidePresent = true;

    event.phase = PointerPhase::ButtonDown;
    CHECK(state.process(event) == BattleCursorPointerAction::UpdateAndCapture);

    event.phase = PointerPhase::ButtonUp;
    CHECK(state.process(event) == BattleCursorPointerAction::UpdateAndConfirm);

    event.phase = PointerPhase::Motion;
    CHECK(state.process(event) == BattleCursorPointerAction::Update);
}

TEST_CASE("SuperMenuText can lock and commit two tap activations in one pointer prefix", "[pointer_migration]")
{
    int lockedItem = -1;
    CHECK(superMenuTapAction(true, 7, lockedItem) == SuperMenuTapAction::Lock);
    lockedItem = 7;
    CHECK(superMenuTapAction(true, 7, lockedItem) == SuperMenuTapAction::Commit);
}

TEST_CASE("Pointer capture is canceled when its target subtree is invalidated", "[pointer_input]")
{
    CHECK(pointerCaptureInvalidationNeeded(true, true));
    CHECK_FALSE(pointerCaptureInvalidationNeeded(true, false));
    CHECK_FALSE(pointerCaptureInvalidationNeeded(false, true));
    CHECK(pointerCaptureContactRejectionNeeded(PointerSource::Touch));
    CHECK_FALSE(pointerCaptureContactRejectionNeeded(PointerSource::Mouse));
}

TEST_CASE("ImGui primary touch keeps a guarded sequence suppressed after the guard expires", "[pointer_input]")
{
    ImGuiPrimaryTouchRouting routing;

    const auto down = routing.route(TouchPhase::Down, true);
    CHECK_FALSE(down.forwardToBackend);
    CHECK(down.consumed);

    const auto motion = routing.route(TouchPhase::Motion, false);
    CHECK_FALSE(motion.forwardToBackend);
    CHECK(motion.consumed);

    const auto up = routing.route(TouchPhase::Up, false);
    CHECK_FALSE(up.forwardToBackend);
    CHECK(up.consumed);

    CHECK(routing.route(TouchPhase::Down, false).forwardToBackend);
    CHECK(routing.cancelNeedsBackendRelease());
}
