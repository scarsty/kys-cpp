#include "PointerInput.h"
#include "BattleCursor.h"
#include "BattleSceneCamera.h"
#include "SuperMenuText.h"
#include "Talk.h"
#include "ImGuiLayer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
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

    static bool uncapturedPointerActivation(const PointerEvent& event)
    {
        return isUncapturedPointerActivation(event);
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
    CHECK(input.popPending().type == SDL_EVENT_FINGER_DOWN);
    CHECK_FALSE(input.frontIsPointer());
    CHECK(input.popPending().type == SDL_EVENT_KEY_UP);
    CHECK(input.frontIsPointer());
    CHECK(input.popPending().type == SDL_EVENT_FINGER_UP);
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
    input.setPresentGeometry(geometry);

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

TEST_CASE("PointerInput commits queued present geometry before discarding its correlated raw wrapper", "[pointer_input]")
{
    REQUIRE(SDL_InitSubSystem(SDL_INIT_EVENTS));
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    PointerInput input;
    REQUIRE(input.initializeActions());
    PresentGeometrySnapshot initial{1, 1280, 720, {0, 0, 1280, 720}, 1280, 720};
    REQUIRE(input.publishPresentGeometry(initial));

    SDL_Event resize = {};
    resize.type = SDL_EVENT_WINDOW_RESIZED;
    resize.window.windowID = 5;
    PresentGeometrySnapshot resized{2, 1920, 1080, {0, 0, 1920, 1080}, 1280, 720};
    REQUIRE(input.publishPresentGeometry(resized, &resize));
    input.pumpSdlEvents();
    input.enqueueForTest(resize);

    REQUIRE(input.pendingCount() == 2);
    const auto marker = input.popPending();
    REQUIRE(input.isPresentGeometryEvent(marker));
    const auto embedded = input.applyPresentGeometryEvent(marker);
    REQUIRE(embedded.has_value());
    CHECK(embedded->type == SDL_EVENT_WINDOW_RESIZED);
    CHECK(input.presentGeometry().revision == 2);
    CHECK(input.discardCorrelatedGeometryEvent(*embedded));
    CHECK(input.empty());

}

TEST_CASE("PointerInput rejects late contact events after a hard reset until a fresh down", "[pointer_input]")
{
    PointerInput input;
    input.setPresentGeometry({1, 1280, 720, {0, 0, 1280, 720}, 1280, 720});
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
    input.setPresentGeometry({1, 1280, 720, {0, 0, 1280, 720}, 1280, 720});

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

TEST_CASE("Talk activates for a matching left press and release anywhere", "[talk_pointer]")
{
    TalkPointerState state;
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 4;
    event.button = SDL_BUTTON_LEFT;
    event.insidePresent = false;

    event.phase = PointerPhase::ButtonDown;
    CHECK(state.process(event) == TalkPointerAction::Capture);
    event.phase = PointerPhase::ButtonUp;
    CHECK(state.process(event) == TalkPointerAction::Activate);
}

TEST_CASE("Talk cancel clears its left press without activation", "[talk_pointer]")
{
    TalkPointerState state;
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.pointerId = 4;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonDown;
    REQUIRE(state.process(event) == TalkPointerAction::Capture);

    event.phase = PointerPhase::Cancel;
    CHECK(state.process(event) == TalkPointerAction::Cancel);
    event.phase = PointerPhase::ButtonUp;
    CHECK(state.process(event) == TalkPointerAction::Ignore);
}

TEST_CASE("Uncaptured left release activates the run owner", "[pointer_routing][popup_text]")
{
    PointerEvent event;
    event.source = PointerSource::Mouse;
    event.button = SDL_BUTTON_LEFT;
    event.phase = PointerPhase::ButtonUp;
    CHECK(PointerRouteProbe::uncapturedPointerActivation(event));

    event.phase = PointerPhase::ButtonDown;
    CHECK_FALSE(PointerRouteProbe::uncapturedPointerActivation(event));
    event.phase = PointerPhase::ButtonUp;
    event.source = PointerSource::Touch;
    CHECK(PointerRouteProbe::uncapturedPointerActivation(event));
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
