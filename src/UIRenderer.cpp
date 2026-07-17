#include "UIRenderer.h"
#include "Font.h"
#include <algorithm>
#include <type_traits>

void UIRenderer::drawText(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    commands_.emplace_back(TextCommand{ text, size, x, y, color, alpha });
}

void UIRenderer::fillColor(Color color, int x, int y, int w, int h, BlendMode blend)
{
    commands_.emplace_back(FillCommand{ color, x, y, w, h, blend });
}

void UIRenderer::drawTexture(TextureWarpper* texture, int x, int y,
    const TextureManager::RenderInfo& info, int w, int h)
{
    if (texture == nullptr)
    {
        return;
    }
    commands_.emplace_back(TextureCommand{ texture, x, y, info, w, h });
}

void UIRenderer::drawTexture(Texture* texture, const Rect& rect, Color color)
{
    if (texture == nullptr)
    {
        return;
    }
    commands_.emplace_back(RawTextureCommand{ texture, rect, color });
}

void UIRenderer::execute()
{
    executing_ = true;
    auto engine = Engine::getInstance();
    int px, py, pw, ph;
    engine->getPresentRect(px, py, pw, ph);
    int ui_w, ui_h;
    engine->getUISize(ui_w, ui_h);

    if (pw <= 0 || ph <= 0 || ui_w <= 0 || ui_h <= 0)
    {
        commands_.clear();
        executing_ = false;
        return;
    }

    float sx = float(pw) / ui_w;
    float sy = float(ph) / ui_h;

    for (auto& command : commands_)
    {
        std::visit([&](auto& draw)
        {
            using T = std::decay_t<decltype(draw)>;
            if constexpr (std::is_same_v<T, TextCommand>)
            {
                Font::getInstance()->renderText(draw.text, std::max(1, int(draw.size * sx)),
                    px + int(draw.x * sx), py + int(draw.y * sy), draw.color, draw.alpha);
            }
            else if constexpr (std::is_same_v<T, FillCommand>)
            {
                int w = draw.w < 0 ? pw : int(draw.w * sx);
                int h = draw.h < 0 ? ph : int(draw.h * sy);
                engine->fillColor(draw.color, px + int(draw.x * sx), py + int(draw.y * sy), w, h, draw.blend);
            }
            else if constexpr (std::is_same_v<T, TextureCommand>)
            {
                draw.texture->load();
                auto info = draw.info;
                info.zoom_x *= sx;
                info.zoom_y *= sy;
                int w = draw.w < 0 ? -1 : int(draw.w * sx);
                int h = draw.h < 0 ? -1 : int(draw.h * sy);
                int x = px + int((draw.x - draw.texture->dx) * sx) + draw.texture->dx;
                int y = py + int((draw.y - draw.texture->dy) * sy) + draw.texture->dy;
                TextureManager::getInstance()->renderTexture(draw.texture,
                    x, y, info, w, h);
            }
            else
            {
                Rect rect = {
                    px + int(draw.rect.x * sx),
                    py + int(draw.rect.y * sy),
                    int(draw.rect.w * sx),
                    int(draw.rect.h * sy)
                };
                engine->setColor(draw.texture, draw.color);
                engine->renderTexture(draw.texture, nullptr, &rect);
            }
        }, command);
    }

    commands_.clear();
    executing_ = false;
}
