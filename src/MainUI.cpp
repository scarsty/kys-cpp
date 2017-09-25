#include "MainUI.h"
#include "Types.h"
#include "Save.h"
#include "Head.h"

MainUI::MainUI()
{
    for (int i = 0; i < MAX_TEAMMATE_COUNT; i++)
    {
        if (Save::getInstance()->getTeamMate(i))
        {
            auto h = new Head(Save::getInstance()->getTeamMate(i));
            addChild(h, 10, i*70);
        }
    }
}


MainUI::~MainUI()
{
}
