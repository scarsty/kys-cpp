#include "ChessRoleSave.h"

#include "Save.h"

namespace KysChess
{

ChessRoleSave::ChessRoleSave(Save& save) : save_(&save)
{
}

Role* ChessRoleSave::getRole(int roleId) const
{
    return save_->getRole(roleId);
}

}