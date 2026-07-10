#include "BattleScenePaperCameraTouch.h"

#include <cmath>

namespace
{
constexpr float PanThreshold = 10.0f;
SDL_FPoint subtract(SDL_FPoint lhs, SDL_FPoint rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y}; }
}

std::vector<PaperCameraTouchAction> BattleScenePaperCameraTouch::process(
    const TouchSample& sample,
    const BattleControlLayout& controls)
{
    switch (sample.phase)
    {
    case TouchPhase::Down:
        handleDown(sample, controls);
        return {};
    case TouchPhase::Motion:
        return handleMotion(sample);
    case TouchPhase::Up:
        return handleUp(sample, controls);
    case TouchPhase::Canceled:
        if (sample.cancelScope == TouchCancelScope::All)
        {
            reset();
        }
        else
        {
            handleContactCanceled(sample);
        }
        return {};
    }
    return {};
}

void BattleScenePaperCameraTouch::reset()
{
    contacts_.clear();
    armedControlKey_.reset();
    armedControl_ = BattleControlId::None;
    controlContactCount_ = 0;
    singleBaseline_ = {};
    singlePrevious_ = {};
    singlePanActive_ = false;
    pairPrevious_ = {};
    pairBaselineValid_ = false;
}

std::vector<TouchFingerKey> BattleScenePaperCameraTouch::cameraKeys() const
{
    std::vector<TouchFingerKey> keys;
    for (const auto& [key, contact] : contacts_)
    {
        if (contact.ownership == Ownership::Camera) keys.push_back(key);
    }
    return keys;
}

BattleScenePaperCameraTouch::PairGeometry BattleScenePaperCameraTouch::pairGeometry(
    const std::vector<TouchFingerKey>& keys) const
{
    const auto first = contacts_.at(keys[0]).position;
    const auto second = contacts_.at(keys[1]).position;
    return {
        {(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f},
        std::hypot(second.x - first.x, second.y - first.y),
    };
}

void BattleScenePaperCameraTouch::rebaselineCamera()
{
    const auto keys = cameraKeys();
    singlePanActive_ = false;
    pairBaselineValid_ = false;
    if (controlContactCount_ > 0) return;
    if (keys.size() == 1)
    {
        singleBaseline_ = contacts_.at(keys[0]).position;
        singlePrevious_ = singleBaseline_;
    }
    else if (keys.size() == 2)
    {
        pairPrevious_ = pairGeometry(keys);
        pairBaselineValid_ = true;
    }
}

void BattleScenePaperCameraTouch::handleDown(const TouchSample& sample, const BattleControlLayout& controls)
{
    if (contacts_.contains(sample.key)) return;
    Contact contact;
    contact.position = sample.uiPosition;
    contact.control = controls.hitTest(sample.uiPosition);
    if (!sample.insidePresent)
    {
        contact.ownership = Ownership::Ignored;
    }
    else if (contact.control != BattleControlId::None)
    {
        contact.ownership = Ownership::Control;
        if (controlContactCount_ == 0)
        {
            armedControlKey_ = sample.key;
            armedControl_ = contact.control;
        }
        ++controlContactCount_;
    }
    else
    {
        contact.ownership = Ownership::Camera;
    }
    contacts_.emplace(sample.key, contact);
    rebaselineCamera();
}

std::vector<PaperCameraTouchAction> BattleScenePaperCameraTouch::handleMotion(const TouchSample& sample)
{
    const auto it = contacts_.find(sample.key);
    if (it == contacts_.end()) return {};
    it->second.position = sample.uiPosition;
    if (it->second.ownership != Ownership::Camera || controlContactCount_ > 0) return {};

    const auto keys = cameraKeys();
    if (keys.size() == 1)
    {
        const auto current = contacts_.at(keys[0]).position;
        if (!singlePanActive_)
        {
            const auto accumulated = subtract(current, singleBaseline_);
            if (std::hypot(accumulated.x, accumulated.y) < PanThreshold)
            {
                singlePrevious_ = current;
                return {};
            }
            singlePanActive_ = true;
            singlePrevious_ = current;
            return {
                {PaperCameraTouchActionType::PanActivated},
                {PaperCameraTouchActionType::Pan, BattleControlId::None, accumulated},
            };
        }
        const auto delta = subtract(current, singlePrevious_);
        singlePrevious_ = current;
        return {{PaperCameraTouchActionType::Pan, BattleControlId::None, delta}};
    }
    if (keys.size() == 2)
    {
        const auto current = pairGeometry(keys);
        if (!pairBaselineValid_)
        {
            pairPrevious_ = current;
            pairBaselineValid_ = true;
            return {};
        }
        const auto delta = subtract(current.centroid, pairPrevious_.centroid);
        const PaperCameraTouchAction action{
            PaperCameraTouchActionType::Pair,
            BattleControlId::None,
            delta,
            pairPrevious_.span,
            current.span,
        };
        pairPrevious_ = current;
        return {action};
    }
    pairBaselineValid_ = false;
    singlePanActive_ = false;
    return {};
}

std::vector<PaperCameraTouchAction> BattleScenePaperCameraTouch::handleUp(
    const TouchSample& sample,
    const BattleControlLayout& controls)
{
    const auto it = contacts_.find(sample.key);
    if (it == contacts_.end()) return {};

    std::vector<PaperCameraTouchAction> actions;
    if (it->second.ownership == Ownership::Control)
    {
        const bool armed = armedControlKey_ && *armedControlKey_ == sample.key;
        const auto armedControl = armedControl_;
        if (armed)
        {
            armedControlKey_.reset();
            armedControl_ = BattleControlId::None;
        }
        --controlContactCount_;
        if (armed && controls.hitTest(sample.uiPosition) == armedControl)
        {
            actions.push_back({PaperCameraTouchActionType::ControlActivated, armedControl});
        }
    }
    contacts_.erase(it);
    rebaselineCamera();
    return actions;
}

void BattleScenePaperCameraTouch::handleContactCanceled(const TouchSample& sample)
{
    const auto it = contacts_.find(sample.key);
    if (it == contacts_.end()) return;
    if (it->second.ownership == Ownership::Control)
    {
        if (armedControlKey_ && *armedControlKey_ == sample.key)
        {
            armedControlKey_.reset();
            armedControl_ = BattleControlId::None;
        }
        --controlContactCount_;
    }
    contacts_.erase(it);
    rebaselineCamera();
}
