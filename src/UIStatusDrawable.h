#pragma once
#include "DrawableOnCall.h"
#include "UIStatus.h"
#include <memory>

class UIStatusDrawable : public DrawableOnCall {
public:
    UIStatusDrawable();
    void updateScreenWithID(int id) override;
private:
    std::shared_ptr<UIStatus> uiStatus_;
};