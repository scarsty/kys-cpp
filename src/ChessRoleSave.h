#pragma once

class Save;
struct Role;

namespace KysChess
{

class ChessRoleSave
{
public:
    explicit ChessRoleSave(Save& save);

    Role* getRole(int roleId) const;

private:
    Save* save_ = nullptr;
};

}