#include "UIStatusDrawable.h"
#include "Save.h"

UIStatusDrawable::UIStatusDrawable()
    : DrawableOnCall([this](DrawableOnCall*) { if (uiStatus_) uiStatus_->draw(); }),
      uiStatus_(std::make_shared<UIStatus>()) {
    uiStatus_->setShowButton(false);
}

void UIStatusDrawable::updateScreenWithID(int id) {
    DrawableOnCall::updateScreenWithID(id);
    Role* role = Save::getInstance()->getRole(id);
    uiStatus_->setRole(role);
}