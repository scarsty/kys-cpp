#pragma once

#include "Save.h"

namespace KysChess
{

struct Chess

{
    Role* role = nullptr;
    int star = 1;
    friend auto operator<=>(const Chess&, const Chess&) = default;
};

}    //namespace KysChess