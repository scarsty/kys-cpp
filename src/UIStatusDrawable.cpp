#include "UIStatusDrawable.h"
#include "Save.h"

UIStatusDrawable::UIStatusDrawable()
    : DrawableOnCall([this](DrawableOnCall*) { if (uiStatus_) uiStatus_->draw(); }),
      uiStatus_(std::make_shared<ChessUIStatus>()) {
}

void UIStatusDrawable::updateScreenWithID(int id) {
    if (id < 0) {
        uiStatus_->setRole(nullptr);
        return;
    }
    DrawableOnCall::updateScreenWithID(id);
    Role* role = Save::getInstance()->getRole(id);
    uiStatus_->setRole(role);
}