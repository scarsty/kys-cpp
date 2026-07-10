#pragma once

#include "SDL3/SDL.h"

#include <compare>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <unordered_map>
#include <mutex>

enum class PointerPhase { Motion, ButtonDown, ButtonUp, Cancel, Wheel };
enum class PointerSource { Mouse, Touch };
enum class TouchPhase { Down, Motion, Up, Canceled };
enum class TouchCancelScope { Contact, All };
enum class TouchPolicy { PrimaryPointer, DirectTouch };

struct PointerDispatchRevision
{
    std::uint64_t ownershipEpoch{};
    std::uint64_t controlLayoutRevision{};
    auto operator<=>(const PointerDispatchRevision&) const = default;
};

constexpr bool pointerDispatchCanContinue(
    const PointerDispatchRevision& frameStart,
    const PointerDispatchRevision& current)
{
    return frameStart == current;
}

constexpr bool pointerCaptureInvalidationNeeded(bool captureActive, bool capturedInSubtree)
{
    return captureActive && capturedInSubtree;
}

constexpr bool pointerCaptureContactRejectionNeeded(PointerSource source)
{
    return source == PointerSource::Touch;
}

struct TouchFingerKey
{
    SDL_TouchID touchId{};
    SDL_FingerID fingerId{};
    auto operator<=>(const TouchFingerKey&) const = default;
};

struct PointerIdentity
{
    PointerSource source{};
    std::uint64_t pointerId{};
    std::uint8_t button{};
    auto operator<=>(const PointerIdentity&) const = default;
};

struct PointerEvent
{
    PointerPhase phase{};
    PointerSource source{};
    std::uint64_t pointerId{};
    std::uint8_t button{};
    bool buttonDown{};
    SDL_FPoint windowPosition{};
    SDL_FPoint windowDelta{};
    SDL_FPoint uiPosition{};
    SDL_FPoint uiDelta{};
    bool insidePresent{};
    SDL_FPoint wheel{};
    std::uint64_t timestamp{};
    SDL_WindowID windowId{};
};

struct PresentLayoutInput
{
    int windowWidth{};
    int windowHeight{};
    int textureWidth{};
    int textureHeight{};
    int uiWidth{};
    int uiHeight{};
    bool keepRatio = true;
    double rotation{};
    double ratioX = 1.0;
    double ratioY = 1.0;
    auto operator<=>(const PresentLayoutInput&) const = default;
};

struct PresentGeometrySnapshot
{
    std::uint64_t revision{};
    int windowWidth{};
    int windowHeight{};
    SDL_Rect presentRect{};
    int uiWidth{};
    int uiHeight{};
};

struct TouchSample
{
    TouchPhase phase{};
    TouchFingerKey key{};
    SDL_FPoint normalizedPosition{};
    SDL_FPoint normalizedDelta{};
    SDL_FPoint windowPosition{};
    SDL_FPoint windowDelta{};
    SDL_FPoint uiPosition{};
    SDL_FPoint uiDelta{};
    bool insidePresent{};
    float pressure{};
    TouchCancelScope cancelScope = TouchCancelScope::Contact;
    SDL_WindowID windowId{};
    std::uint64_t timestamp{};
};

PresentGeometrySnapshot computePresentLayout(const PresentLayoutInput& input, std::uint64_t revision);
TouchSample makeTouchSample(
    TouchPhase phase,
    TouchFingerKey key,
    float normalizedX,
    float normalizedY,
    float normalizedDx,
    float normalizedDy,
    float pressure,
    SDL_WindowID windowId,
    std::uint64_t timestamp,
    const PresentGeometrySnapshot& geometry);
bool isResidualSyntheticPointerEvent(const SDL_Event& event);

class PrimaryPointerTracker
{
public:
    bool onDown(TouchFingerKey key);
    void onTerminal(TouchFingerKey key);
    bool isPrimary(TouchFingerKey key) const;
    bool hasPrimary() const { return hasPrimary_; }
    void reset();

private:
    std::set<TouchFingerKey> contacts_;
    TouchFingerKey primary_{};
    bool hasPrimary_{};
};

struct FingerEventDispatch
{
    TouchSample sample{};
    bool primary{};
    bool primaryGameEligible{};
    bool physicalSequenceEnded{};
};

class PointerInput
{
public:
    static PointerInput& instance();

    bool initializeActions();
    bool enqueueApplicationCancelAction() const;
    bool isApplicationCancelEvent(const SDL_Event& event) const;
    bool publishPresentGeometry(PresentGeometrySnapshot geometry, const SDL_Event* sourceEvent = nullptr);
    bool isPresentGeometryEvent(const SDL_Event& event) const;
    std::optional<SDL_Event> applyPresentGeometryEvent(const SDL_Event& event);
    bool discardCorrelatedGeometryEvent(const SDL_Event& sourceEvent);
    void clearActionPayloads();
    void pumpSdlEvents();
    void enqueueForTest(const SDL_Event& event) { pending_.push_back(event); }
    std::size_t pendingCount() const { return pending_.size(); }
    bool empty() const { return pending_.empty(); }
    bool frontIsPointer() const;
    SDL_Event popPending();

    void setPresentGeometry(PresentGeometrySnapshot geometry) { geometry_ = geometry; }
    const PresentGeometrySnapshot& presentGeometry() const { return geometry_; }
    PointerEvent makeMousePointerEvent(const SDL_Event& event);
    PointerEvent makeTouchPointerEvent(const TouchSample& sample) const;
    std::optional<FingerEventDispatch> processFingerEvent(const SDL_Event& event);
    SDL_FPoint logicalPointerUiPosition() const { return logicalPointerUiPosition_; }
    void resetTouchState();
    void rejectActiveTouchContacts();
    bool imguiOwnsTouchSequence() const { return imguiOwnsTouchSequence_; }
    void setImGuiOwnsTouchSequence(bool owns) { imguiOwnsTouchSequence_ = owns; }

    static bool isPointerEventType(std::uint32_t type);

private:
    struct PresentGeometryPayload
    {
        PresentGeometrySnapshot geometry{};
        std::optional<SDL_Event> sourceEvent;
    };

    SDL_FPoint toUiPosition(SDL_FPoint windowPosition) const;
    SDL_FPoint toUiDelta(SDL_FPoint windowDelta) const;
    bool insidePresent(SDL_FPoint windowPosition) const;

private:
    std::deque<SDL_Event> pending_;
    PresentGeometrySnapshot geometry_{};
    PrimaryPointerTracker primaryTracker_;
    std::set<TouchFingerKey> physicalContacts_;
    std::set<TouchFingerKey> rejectedPhysicalContacts_;
    SDL_FPoint mouseWindowPosition_{};
    SDL_FPoint logicalPointerUiPosition_{};
    bool imguiOwnsTouchSequence_{};
    bool primaryGameSequenceActive_{};
    std::uint32_t applicationCancelEventType_{};
    std::uint32_t presentGeometryEventType_{};
    std::uint32_t nextGeometrySerial_ = 1;
    std::mutex geometryPayloadMutex_;
    std::unordered_map<std::uint32_t, PresentGeometryPayload> geometryPayloads_;
};
