#include "DrawNode.h"
#include "Font.h"
#include "TextureManager.h"

void DrawNode::draw()
{
    for (auto& i : Infos)
    {
        if (i.type == 0)
        {
            Font::getInstance()->draw(i.text, 20, i.x, i.y /*BP_Color(e5)*/);
        }
        else if (i.type == 1)
        {
            TextureManager::getInstance()->renderTexture(i.text, i.num, i.x, i.y);
        }
    }
}
