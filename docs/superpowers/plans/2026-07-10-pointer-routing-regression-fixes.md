# Pointer Routing Regression Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore menu wheel bubbling, anywhere-left dialogue advancement, and desktop 2D manual-camera dragging without weakening child pointer ownership.

**Architecture:** `RunNode` will build the deepest-target-to-owner route and invoke nodes until the first non-ignored result. A small protected route-loop template keeps the stop rule independently testable, while `RunNode` retains tree ownership and capture. `Talk` will use a focused pointer-sequence state object, and the camera regression test will prove an ignored child can yield the initiating press to the camera-owning ancestor.

**Tech Stack:** C++23, SDL 3.4.2 pointer events, Catch2, MSBuild, Android Gradle, Emscripten.

---

## File Structure

- Modify `src/RunNode.h`: add the reusable first-accepted route loop and private target-path collection/routing declarations.
- Modify `src/RunNode.cpp`: route uncaptured Wheel and ButtonDown from the deepest target toward ancestors, preserving exclusive capture.
- Modify `src/Talk.h`: define and store the dialogue pointer-sequence state and declare `onPointerEvent()`.
- Modify `src/Talk.cpp`: translate dialogue pointer actions into capture, activation, and cancellation results.
- Modify `tests/PointerInputUnitTests.cpp`: add routing, dialogue, and camera regression tests alongside the existing pointer-migration tests.
- Modify `tests/kys_tests.vcxproj`: compile `BattleSceneCameraInput.cpp` into the test target for the focused camera test.

The affected production files already contain uncommitted mobile-touch implementation work. Do not create per-task implementation commits that would accidentally absorb those pre-existing hunks; use scoped diffs and test checkpoints instead.

### Task 1: First-accepted ancestor routing

**Files:**
- Modify: `src/RunNode.h`
- Modify: `src/RunNode.cpp`
- Modify: `tests/PointerInputUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add failing route and camera tests**

Add `BattleSceneCamera.h` and `Talk.h` includes to `tests/PointerInputUnitTests.cpp`, plus a probe subclass that exposes only the protected route loop:

```cpp
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
```

Add focused tests:

```cpp
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
        if (index == 0) return RunNode::PointerResult::Captured;
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
        if (index == 0) return RunNode::PointerResult::Ignored;
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
```

Add this source to the test project's production-source group:

```xml
<ClCompile Include="..\src\BattleSceneCameraInput.cpp" />
```

- [ ] **Step 2: Build the focused tests and verify RED**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests
```

Expected: compilation fails because `RunNode::routePointerEventPath` does not exist. Confirm the failure is from the missing routing API, not the camera source entry or a test typo.

- [ ] **Step 3: Add the minimal reusable route loop**

In `src/RunNode.h`, add `<optional>` and `<utility>`, then add this protected helper:

```cpp
template <class Handler>
static std::optional<std::pair<std::size_t, PointerResult>> routePointerEventPath(
    std::size_t count,
    Handler&& handler)
{
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto result = handler(index);
        if (result != PointerResult::Ignored)
        {
            return std::pair{index, result};
        }
    }
    return std::nullopt;
}
```

Add the private routed result and declarations:

```cpp
struct RoutedPointerEvent
{
    std::shared_ptr<RunNode> target;
    PointerResult result{};
};

bool collectPointerTargetPath(
    const std::shared_ptr<RunNode>& target,
    std::vector<std::shared_ptr<RunNode>>& path);
std::optional<RoutedPointerEvent> routePointerEventToAncestors(
    const std::shared_ptr<RunNode>& target,
    const PointerEvent& event);
```

- [ ] **Step 4: Route the actual tree without duplicate forwarding**

Implement target-first path collection in `src/RunNode.cpp`:

```cpp
bool RunNode::collectPointerTargetPath(
    const std::shared_ptr<RunNode>& target,
    std::vector<std::shared_ptr<RunNode>>& path)
{
    if (shared_from_this() == target)
    {
        path.push_back(shared_from_this());
        return true;
    }
    for (auto& child : childs_)
    {
        if (child->collectPointerTargetPath(target, path))
        {
            path.push_back(shared_from_this());
            return true;
        }
    }
    return false;
}

std::optional<RunNode::RoutedPointerEvent> RunNode::routePointerEventToAncestors(
    const std::shared_ptr<RunNode>& target,
    const PointerEvent& event)
{
    std::vector<std::shared_ptr<RunNode>> path;
    const bool found = collectPointerTargetPath(target, path);
    assert(found);
    const auto routed = routePointerEventPath(path.size(), [&](std::size_t index)
    {
        return path[index]->onPointerEvent(event);
    });
    if (!routed)
    {
        return std::nullopt;
    }
    return RoutedPointerEvent{path[routed->first], routed->second};
}
```

In `dispatchPointerEvent()`, use `routePointerEventToAncestors()` for ButtonDown. Store capture on the returned node, not the original deepest target, and call `bubblePointerEventPath()` only above that capturing node. Use the same routed call for Wheel and return immediately. Leave uncaptured Motion and ButtonUp targeting unchanged.

```cpp
if (event.phase == PointerPhase::ButtonDown)
{
    auto target = findPointerTarget(event.uiPosition);
    if (!target) target = shared_from_this();
    const auto routed = routePointerEventToAncestors(target, event);
    if (routed && routed->result == PointerResult::Captured)
    {
        pointer_capture_ = routed->target;
        pointer_capture_identity_ = identity;
        bubblePointerEventPath(routed->target, event);
        if (!routed->target->visible_ || routed->target->exit_
            || !containsPointerTarget(routed->target))
        {
            cancelPointerCapture();
        }
    }
    return;
}

if (event.phase == PointerPhase::Wheel)
{
    auto target = findPointerTarget(event.uiPosition);
    if (!target) target = shared_from_this();
    routePointerEventToAncestors(target, event);
    return;
}
```

- [ ] **Step 5: Build and run the routing tests GREEN**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests
x64/Debug/kys_tests.exe "[pointer_routing]" --reporter compact
```

Expected: build succeeds and all `[pointer_routing]` cases pass.

- [ ] **Step 6: Inspect the scoped diff**

Run:

```powershell
git diff --check -- src/RunNode.h src/RunNode.cpp tests/PointerInputUnitTests.cpp tests/kys_tests.vcxproj
git diff -- src/RunNode.h src/RunNode.cpp tests/PointerInputUnitTests.cpp tests/kys_tests.vcxproj
```

Confirm Wheel and ButtonDown share the route helper, child capture remains first, and no widget-specific forwarding was added.

### Task 2: Anywhere-left dialogue sequence

**Files:**
- Modify: `src/Talk.h`
- Modify: `src/Talk.cpp`
- Modify: `tests/PointerInputUnitTests.cpp`

- [ ] **Step 1: Add failing dialogue-state tests**

Add:

```cpp
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
```

- [ ] **Step 2: Build and verify RED**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests
```

Expected: compilation fails because `TalkPointerState` and `TalkPointerAction` do not exist.

- [ ] **Step 3: Implement the dialogue sequence state**

In `src/Talk.h`, add:

```cpp
enum class TalkPointerAction { Ignore, Capture, Activate, Cancel };

class TalkPointerState
{
public:
    TalkPointerAction process(const PointerEvent& event)
    {
        const PointerIdentity identity{event.source, event.pointerId, event.button};
        if (event.phase == PointerPhase::ButtonDown && event.button == SDL_BUTTON_LEFT && !pointer_)
        {
            pointer_ = identity;
            return TalkPointerAction::Capture;
        }
        if (event.phase == PointerPhase::ButtonUp && pointer_ && *pointer_ == identity)
        {
            pointer_.reset();
            return TalkPointerAction::Activate;
        }
        if (event.phase == PointerPhase::Cancel && pointer_
            && pointer_->source == identity.source
            && pointer_->pointerId == identity.pointerId)
        {
            pointer_.reset();
            return TalkPointerAction::Cancel;
        }
        return TalkPointerAction::Ignore;
    }

private:
    std::optional<PointerIdentity> pointer_;
};
```

Declare `PointerResult onPointerEvent(const PointerEvent& event) override;` and add `TalkPointerState pointer_state_;` to `Talk`.

- [ ] **Step 4: Connect the state to Talk behavior**

Implement in `src/Talk.cpp`:

```cpp
RunNode::PointerResult Talk::onPointerEvent(const PointerEvent& event)
{
    switch (pointer_state_.process(event))
    {
    case TalkPointerAction::Capture:
        return PointerResult::Captured;
    case TalkPointerAction::Activate:
        onPressedOK();
        return PointerResult::Handled;
    case TalkPointerAction::Cancel:
        return PointerResult::Released;
    case TalkPointerAction::Ignore:
        return RunNode::onPointerEvent(event);
    }
    assert(false);
    return PointerResult::Ignored;
}
```

Add `<cassert>` to `Talk.cpp` and `<optional>` to `Talk.h`. Do not alter the global right-click Cancel branch.

- [ ] **Step 5: Build and run dialogue tests GREEN**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1 -Target kys_tests
x64/Debug/kys_tests.exe "[talk_pointer]" --reporter compact
```

Expected: build succeeds and both dialogue pointer cases pass.

- [ ] **Step 6: Inspect the scoped diff**

Run:

```powershell
git diff --check -- src/Talk.h src/Talk.cpp tests/PointerInputUnitTests.cpp
git diff -- src/Talk.h src/Talk.cpp tests/PointerInputUnitTests.cpp
```

Confirm only `Talk` receives the anywhere-left behavior and unrelated menus retain hit-target activation.

### Task 3: Full regression verification

**Files:**
- Modify only if verification exposes a defect in the approved scope.

- [ ] **Step 1: Run all focused regression tests**

```powershell
x64/Debug/kys_tests.exe "[pointer_routing],[talk_pointer]" --reporter compact
```

Expected: zero failed test cases.

- [ ] **Step 2: Run the complete desktop build and test suite**

```powershell
powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1
x64/Debug/kys_tests.exe --reporter compact
```

Expected: `kys` and `kys_tests` compile, and all Catch2 cases pass. If final linking of `kys.exe` alone fails because the running game holds the executable, treat compilation as successful only when the output explicitly shows that file-lock condition and `kys_tests` still builds and passes.

- [ ] **Step 3: Run shared-platform builds**

```powershell
Push-Location android
./gradlew.bat assembleDebug
Pop-Location
powershell -ExecutionPolicy Bypass -File ./wasm/build.ps1
```

Expected: Android Debug APK and WebAssembly build complete successfully.

- [ ] **Step 4: Run final source and diff checks**

```powershell
git diff --check
rg -n "routePointerEventPath|routePointerEventToAncestors|TalkPointerState|BattleSceneCameraInput.cpp" src tests
git status --short
```

Confirm there are no whitespace errors, the routing implementation is centralized, the dialogue state is scoped to `Talk`, and all pre-existing uncommitted touch files remain present.

- [ ] **Step 5: Request code review**

Invoke `superpowers:requesting-code-review` against the approved design and this plan. Address only verified findings within the three-regression scope, rerunning the smallest affected focused test after each correction and the full desktop suite afterward.
