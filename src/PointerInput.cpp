#include "PointerInput.h"

#include <algorithm>
#include <cassert>

PresentGeometrySnapshot computePresentLayout(const PresentLayoutInput& input, std::uint64_t revision)
{
    assert(input.windowWidth > 0 && input.windowHeight > 0);
    assert(input.textureWidth > 0 && input.textureHeight > 0);
    assert(input.uiWidth > 0 && input.uiHeight > 0);
    assert(input.ratioX > 0.0 && input.ratioY > 0.0);

    PresentGeometrySnapshot geometry;
    geometry.revision = revision;
    geometry.windowWidth = input.windowWidth;
    geometry.windowHeight = input.windowHeight;
    geometry.uiWidth = input.uiWidth;
    geometry.uiHeight = input.uiHeight;

    const double sourceWidth = input.textureWidth * input.ratioX;
    const double sourceHeight = input.textureHeight * input.ratioY;
    const bool rotated = input.rotation == 90.0 || input.rotation == 270.0;
    if (input.keepRatio)
    {
        const double scale = rotated
            ? std::min(input.windowWidth / sourceHeight, input.windowHeight / sourceWidth)
            : std::min(input.windowWidth / sourceWidth, input.windowHeight / sourceHeight);
        geometry.presentRect.w = static_cast<int>(sourceWidth * scale);
        geometry.presentRect.h = static_cast<int>(sourceHeight * scale);
        geometry.presentRect.x = (input.windowWidth - geometry.presentRect.w) / 2;
        geometry.presentRect.y = (input.windowHeight - geometry.presentRect.h) / 2;
    }
    else if (rotated)
    {
        geometry.presentRect = {
            (input.windowHeight - input.windowWidth) / 2,
            (input.windowWidth - input.windowHeight) / 2,
            input.windowHeight,
            input.windowWidth,
        };
    }
    else
    {
        geometry.presentRect = {0, 0, input.windowWidth, input.windowHeight};
    }
    return geometry;
}

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
    const PresentGeometrySnapshot& geometry)
{
    assert(geometry.windowWidth > 0 && geometry.windowHeight > 0);
    assert(geometry.presentRect.w > 0 && geometry.presentRect.h > 0);
    assert(geometry.uiWidth > 0 && geometry.uiHeight > 0);

    TouchSample sample;
    sample.phase = phase;
    sample.key = key;
#ifdef __ANDROID__
    sample.cancelScope = TouchCancelScope::All;
#endif
    sample.normalizedPosition = {normalizedX, normalizedY};
    sample.normalizedDelta = {normalizedDx, normalizedDy};
    sample.windowPosition = {
        normalizedX * static_cast<float>(geometry.windowWidth - 1),
        normalizedY * static_cast<float>(geometry.windowHeight - 1),
    };
    sample.windowDelta = {
        normalizedDx * static_cast<float>(geometry.windowWidth - 1),
        normalizedDy * static_cast<float>(geometry.windowHeight - 1),
    };
    sample.uiPosition = {
        (sample.windowPosition.x - geometry.presentRect.x) * geometry.uiWidth / geometry.presentRect.w,
        (sample.windowPosition.y - geometry.presentRect.y) * geometry.uiHeight / geometry.presentRect.h,
    };
    sample.uiDelta = {
        sample.windowDelta.x * geometry.uiWidth / geometry.presentRect.w,
        sample.windowDelta.y * geometry.uiHeight / geometry.presentRect.h,
    };
    sample.insidePresent = sample.windowPosition.x >= geometry.presentRect.x
        && sample.windowPosition.x < geometry.presentRect.x + geometry.presentRect.w
        && sample.windowPosition.y >= geometry.presentRect.y
        && sample.windowPosition.y < geometry.presentRect.y + geometry.presentRect.h;
    sample.pressure = pressure;
    sample.windowId = windowId;
    sample.timestamp = timestamp;
    return sample;
}

bool isResidualSyntheticPointerEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        return event.motion.which == SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event.button.which == SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_WHEEL:
        return event.wheel.which == SDL_TOUCH_MOUSEID;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return event.tfinger.touchID == SDL_MOUSE_TOUCHID || event.tfinger.touchID == SDL_PEN_TOUCHID;
    default:
        return false;
    }
}

bool PrimaryPointerTracker::onDown(TouchFingerKey key)
{
    const bool startsSequence = contacts_.empty();
    contacts_.insert(key);
    if (startsSequence)
    {
        primary_ = key;
        hasPrimary_ = true;
        return true;
    }
    return false;
}

void PrimaryPointerTracker::onTerminal(TouchFingerKey key)
{
    contacts_.erase(key);
    if (hasPrimary_ && primary_ == key)
    {
        hasPrimary_ = false;
    }
    if (contacts_.empty())
    {
        primary_ = {};
    }
}

bool PrimaryPointerTracker::isPrimary(TouchFingerKey key) const
{
    return hasPrimary_ && primary_ == key;
}

void PrimaryPointerTracker::reset()
{
    contacts_.clear();
    primary_ = {};
    hasPrimary_ = false;
}

PointerInput& PointerInput::instance()
{
    static PointerInput input;
    return input;
}

bool PointerInput::initializeActions()
{
    if (applicationCancelEventType_ != 0)
    {
        return true;
    }
    const auto firstType = SDL_RegisterEvents(2);
    if (firstType == 0)
    {
        return false;
    }
    applicationCancelEventType_ = firstType;
    presentGeometryEventType_ = firstType + 1;
    return true;
}

bool PointerInput::publishPresentGeometry(PresentGeometrySnapshot geometry, const SDL_Event* sourceEvent)
{
    if (geometry_.windowWidth == 0)
    {
        geometry_ = geometry;
        return true;
    }
    const bool unchanged = geometry_.windowWidth == geometry.windowWidth
        && geometry_.windowHeight == geometry.windowHeight
        && geometry_.presentRect.x == geometry.presentRect.x
        && geometry_.presentRect.y == geometry.presentRect.y
        && geometry_.presentRect.w == geometry.presentRect.w
        && geometry_.presentRect.h == geometry.presentRect.h
        && geometry_.uiWidth == geometry.uiWidth
        && geometry_.uiHeight == geometry.uiHeight;
    if (unchanged || presentGeometryEventType_ == 0)
    {
        return unchanged;
    }

    std::uint32_t serial{};
    {
        const std::scoped_lock lock(geometryPayloadMutex_);
        serial = nextGeometrySerial_++;
        PresentGeometryPayload payload;
        payload.geometry = geometry;
        if (sourceEvent) payload.sourceEvent = *sourceEvent;
        geometryPayloads_.emplace(serial, std::move(payload));
    }

    SDL_Event marker = {};
    marker.type = presentGeometryEventType_;
    marker.user.code = static_cast<Sint32>(serial);
    if (SDL_PeepEvents(&marker, 1, SDL_ADDEVENT, 0, 0) == 1)
    {
        return true;
    }
    const std::scoped_lock lock(geometryPayloadMutex_);
    geometryPayloads_.erase(serial);
    return false;
}

bool PointerInput::isPresentGeometryEvent(const SDL_Event& event) const
{
    return presentGeometryEventType_ != 0 && event.type == presentGeometryEventType_;
}

std::optional<SDL_Event> PointerInput::applyPresentGeometryEvent(const SDL_Event& event)
{
    if (!isPresentGeometryEvent(event))
    {
        return std::nullopt;
    }
    PresentGeometryPayload payload;
    {
        const std::scoped_lock lock(geometryPayloadMutex_);
        const auto serial = static_cast<std::uint32_t>(event.user.code);
        const auto it = geometryPayloads_.find(serial);
        if (it == geometryPayloads_.end())
        {
            return std::nullopt;
        }
        payload = std::move(it->second);
        geometryPayloads_.erase(it);
    }
    geometry_ = payload.geometry;
    return payload.sourceEvent;
}

bool PointerInput::discardCorrelatedGeometryEvent(const SDL_Event& sourceEvent)
{
    if (pending_.empty() || pending_.front().type != sourceEvent.type)
    {
        return false;
    }
    if (sourceEvent.type >= SDL_EVENT_WINDOW_FIRST && sourceEvent.type <= SDL_EVENT_WINDOW_LAST
        && pending_.front().window.windowID != sourceEvent.window.windowID)
    {
        return false;
    }
    pending_.pop_front();
    return true;
}

void PointerInput::clearActionPayloads()
{
    const std::scoped_lock lock(geometryPayloadMutex_);
    geometryPayloads_.clear();
}

bool PointerInput::enqueueApplicationCancelAction() const
{
    if (applicationCancelEventType_ == 0)
    {
        return false;
    }
    SDL_Event event = {};
    event.type = applicationCancelEventType_;
    return SDL_PushEvent(&event);
}

bool PointerInput::isApplicationCancelEvent(const SDL_Event& event) const
{
    return applicationCancelEventType_ != 0 && event.type == applicationCancelEventType_;
}

void PointerInput::pumpSdlEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        pending_.push_back(event);
    }
}

bool PointerInput::isPointerEventType(std::uint32_t type)
{
    switch (type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return true;
    default:
        return false;
    }
}

bool PointerInput::frontIsPointer() const
{
    return !pending_.empty() && isPointerEventType(pending_.front().type);
}

SDL_Event PointerInput::popPending()
{
    assert(!pending_.empty());
    const auto event = pending_.front();
    pending_.pop_front();
    return event;
}

SDL_FPoint PointerInput::toUiPosition(SDL_FPoint windowPosition) const
{
    assert(geometry_.presentRect.w > 0 && geometry_.presentRect.h > 0);
    return {
        (windowPosition.x - geometry_.presentRect.x) * geometry_.uiWidth / geometry_.presentRect.w,
        (windowPosition.y - geometry_.presentRect.y) * geometry_.uiHeight / geometry_.presentRect.h,
    };
}

SDL_FPoint PointerInput::toUiDelta(SDL_FPoint windowDelta) const
{
    assert(geometry_.presentRect.w > 0 && geometry_.presentRect.h > 0);
    return {
        windowDelta.x * geometry_.uiWidth / geometry_.presentRect.w,
        windowDelta.y * geometry_.uiHeight / geometry_.presentRect.h,
    };
}

bool PointerInput::insidePresent(SDL_FPoint windowPosition) const
{
    return windowPosition.x >= geometry_.presentRect.x
        && windowPosition.x < geometry_.presentRect.x + geometry_.presentRect.w
        && windowPosition.y >= geometry_.presentRect.y
        && windowPosition.y < geometry_.presentRect.y + geometry_.presentRect.h;
}

PointerEvent PointerInput::makeMousePointerEvent(const SDL_Event& event)
{
    assert(geometry_.presentRect.w > 0 && geometry_.presentRect.h > 0);
    PointerEvent pointer;
    pointer.source = PointerSource::Mouse;
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        pointer.phase = PointerPhase::Motion;
        pointer.pointerId = event.motion.which;
        pointer.windowPosition = {event.motion.x, event.motion.y};
        pointer.windowDelta = {event.motion.xrel, event.motion.yrel};
        pointer.timestamp = event.motion.timestamp;
        pointer.windowId = event.motion.windowID;
        mouseWindowPosition_ = pointer.windowPosition;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        pointer.phase = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
            ? PointerPhase::ButtonDown
            : PointerPhase::ButtonUp;
        pointer.pointerId = event.button.which;
        pointer.button = event.button.button;
        pointer.buttonDown = event.button.down;
        pointer.windowPosition = {event.button.x, event.button.y};
        pointer.timestamp = event.button.timestamp;
        pointer.windowId = event.button.windowID;
        mouseWindowPosition_ = pointer.windowPosition;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        pointer.phase = PointerPhase::Wheel;
        pointer.pointerId = event.wheel.which;
        pointer.windowPosition = {event.wheel.mouse_x, event.wheel.mouse_y};
        pointer.wheel = {event.wheel.x, event.wheel.y};
        pointer.timestamp = event.wheel.timestamp;
        pointer.windowId = event.wheel.windowID;
        mouseWindowPosition_ = pointer.windowPosition;
        break;
    default:
        assert(false);
    }
    pointer.uiPosition = toUiPosition(pointer.windowPosition);
    pointer.uiDelta = toUiDelta(pointer.windowDelta);
    pointer.insidePresent = insidePresent(pointer.windowPosition);
    logicalPointerUiPosition_ = pointer.uiPosition;
    return pointer;
}

PointerEvent PointerInput::makeTouchPointerEvent(const TouchSample& sample) const
{
    PointerEvent pointer;
    switch (sample.phase)
    {
    case TouchPhase::Down:
        pointer.phase = PointerPhase::ButtonDown;
        pointer.buttonDown = true;
        break;
    case TouchPhase::Motion:
        pointer.phase = PointerPhase::Motion;
        break;
    case TouchPhase::Up:
        pointer.phase = PointerPhase::ButtonUp;
        break;
    case TouchPhase::Canceled:
        pointer.phase = PointerPhase::Cancel;
        break;
    }
    pointer.source = PointerSource::Touch;
    pointer.pointerId = static_cast<std::uint64_t>(sample.key.fingerId);
    pointer.button = SDL_BUTTON_LEFT;
    pointer.windowPosition = sample.windowPosition;
    pointer.windowDelta = sample.windowDelta;
    pointer.uiPosition = sample.uiPosition;
    pointer.uiDelta = sample.uiDelta;
    pointer.insidePresent = sample.insidePresent;
    pointer.timestamp = sample.timestamp;
    pointer.windowId = sample.windowId;
    return pointer;
}

std::optional<FingerEventDispatch> PointerInput::processFingerEvent(const SDL_Event& event)
{
    if (!isPointerEventType(event.type) || isResidualSyntheticPointerEvent(event))
    {
        return std::nullopt;
    }
    if (event.type != SDL_EVENT_FINGER_DOWN
        && event.type != SDL_EVENT_FINGER_MOTION
        && event.type != SDL_EVENT_FINGER_UP
        && event.type != SDL_EVENT_FINGER_CANCELED)
    {
        return std::nullopt;
    }

    const TouchFingerKey key{event.tfinger.touchID, event.tfinger.fingerID};
    if (rejectedPhysicalContacts_.contains(key))
    {
        if (event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED)
        {
            rejectedPhysicalContacts_.erase(key);
            physicalContacts_.erase(key);
            primaryTracker_.onTerminal(key);
        }
        return std::nullopt;
    }
    if (event.type != SDL_EVENT_FINGER_DOWN && !physicalContacts_.contains(key))
    {
        return std::nullopt;
    }
    bool primary = primaryTracker_.isPrimary(key);
    if (event.type == SDL_EVENT_FINGER_DOWN)
    {
        physicalContacts_.insert(key);
        primary = primaryTracker_.onDown(key);
    }

    TouchPhase phase{};
    switch (event.type)
    {
    case SDL_EVENT_FINGER_DOWN: phase = TouchPhase::Down; break;
    case SDL_EVENT_FINGER_MOTION: phase = TouchPhase::Motion; break;
    case SDL_EVENT_FINGER_UP: phase = TouchPhase::Up; break;
    case SDL_EVENT_FINGER_CANCELED: phase = TouchPhase::Canceled; break;
    default: assert(false);
    }

    FingerEventDispatch dispatch;
    dispatch.primary = primary;
    dispatch.sample = makeTouchSample(
        phase,
        key,
        event.tfinger.x,
        event.tfinger.y,
        event.tfinger.dx,
        event.tfinger.dy,
        event.tfinger.pressure,
        event.tfinger.windowID,
        event.tfinger.timestamp,
        geometry_);

    if (primary && phase == TouchPhase::Down)
    {
        primaryGameSequenceActive_ = dispatch.sample.insidePresent;
    }
    dispatch.primaryGameEligible = primary && primaryGameSequenceActive_;

    if (primary)
    {
        logicalPointerUiPosition_ = dispatch.sample.uiPosition;
    }
    if (phase == TouchPhase::Up || phase == TouchPhase::Canceled)
    {
        physicalContacts_.erase(key);
        primaryTracker_.onTerminal(key);
        dispatch.physicalSequenceEnded = physicalContacts_.empty();
        if (dispatch.physicalSequenceEnded)
        {
            primaryGameSequenceActive_ = false;
        }
    }
    return dispatch;
}

void PointerInput::resetTouchState()
{
    primaryTracker_.reset();
    physicalContacts_.clear();
    rejectedPhysicalContacts_.clear();
    imguiOwnsTouchSequence_ = false;
    primaryGameSequenceActive_ = false;
}

void PointerInput::rejectActiveTouchContacts()
{
    rejectedPhysicalContacts_.insert(physicalContacts_.begin(), physicalContacts_.end());
    imguiOwnsTouchSequence_ = false;
    primaryGameSequenceActive_ = false;
}
