#include "UIRenderer.h"
#include "Font.h"
#include <algorithm>
#include <type_traits>

namespace
{
int scalePosition(int origin, int value, float scale)
{
    return origin + int(value * scale);
}

int scaleSize(int value, float scale)
{
    return value < 0 ? value : int(value * scale);
}

int scaleRadius(int radius, float sx, float sy)
{
    return int(radius * (std::min)(sx, sy));
}
}

void UIRenderer::drawText(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    commands_.emplace_back(TextCommand{ text, size, x, y, color, alpha });
}

void UIRenderer::fillColor(Color color, int x, int y, int w, int h, BlendMode blend)
{
    commands_.emplace_back(FillCommand{ color, x, y, w, h, blend });
}

void UIRenderer::fillRoundedRect(Color color, int x, int y, int w, int h, int radius, BlendMode blend)
{
    commands_.emplace_back(RoundedRectCommand{ color, x, y, w, h, radius, blend, true });
}

void UIRenderer::drawRoundedRect(Color color, int x, int y, int w, int h, int radius, BlendMode blend)
{
    commands_.emplace_back(RoundedRectCommand{ color, x, y, w, h, radius, blend, false });
}

void UIRenderer::drawAnimatedRoundedRect(Color color, int x, int y, int w, int h, int radius,
    double phase, int dotCount, double dotLength, BlendMode blend)
{
    commands_.emplace_back(AnimatedRoundedRectCommand{
        color, x, y, w, h, radius, phase, dotCount, dotLength, blend });
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
    auto* engine = Engine::getInstance();
    int presentX = 0, presentY = 0, presentW = 0, presentH = 0;
    engine->getPresentRect(presentX, presentY, presentW, presentH);
    int uiW = 0, uiH = 0;
    engine->getUISize(uiW, uiH);

    if (presentW <= 0 || presentH <= 0 || uiW <= 0 || uiH <= 0)
    {
        commands_.clear();
        executing_ = false;
        return;
    }

    const float sx = float(presentW) / uiW;
    const float sy = float(presentH) / uiH;

    for (auto& command : commands_)
    {
        std::visit([&](auto& draw)
        {
            using T = std::decay_t<decltype(draw)>;
            if constexpr (std::is_same_v<T, TextCommand>)
            {
                Font::getInstance()->renderText(draw.text, (std::max)(1, int(draw.size * sx)),
                    scalePosition(presentX, draw.x, sx), scalePosition(presentY, draw.y, sy),
                    draw.color, draw.alpha);
            }
            else if constexpr (std::is_same_v<T, FillCommand>)
            {
                const int w = draw.w < 0 ? presentW : scaleSize(draw.w, sx);
                const int h = draw.h < 0 ? presentH : scaleSize(draw.h, sy);
                engine->fillColor(draw.color,
                    scalePosition(presentX, draw.x, sx), scalePosition(presentY, draw.y, sy),
                    w, h, draw.blend);
            }
            else if constexpr (std::is_same_v<T, RoundedRectCommand>)
            {
                const int x = scalePosition(presentX, draw.x, sx);
                const int y = scalePosition(presentY, draw.y, sy);
                const int w = scaleSize(draw.w, sx);
                const int h = scaleSize(draw.h, sy);
                const int radius = scaleRadius(draw.radius, sx, sy);
                if (draw.filled)
                {
                    engine->fillRoundedRect(draw.color, x, y, w, h, radius, draw.blend);
                }
                else
                {
                    engine->drawRoundedRect(draw.color, x, y, w, h, radius, draw.blend);
                }
            }
            else if constexpr (std::is_same_v<T, AnimatedRoundedRectCommand>)
            {
                engine->drawAnimatedRoundedRect(draw.color,
                    scalePosition(presentX, draw.x, sx), scalePosition(presentY, draw.y, sy),
                    scaleSize(draw.w, sx), scaleSize(draw.h, sy), scaleRadius(draw.radius, sx, sy),
                    draw.phase, draw.dotCount, draw.dotLength, draw.blend);
            }
            else if constexpr (std::is_same_v<T, TextureCommand>)
            {
                draw.texture->load();
                auto info = draw.info;
                info.zoom_x *= sx;
                info.zoom_y *= sy;
                const int w = scaleSize(draw.w, sx);
                const int h = scaleSize(draw.h, sy);
                const int x = presentX + int((draw.x - draw.texture->dx) * sx) + draw.texture->dx;
                const int y = presentY + int((draw.y - draw.texture->dy) * sy) + draw.texture->dy;
                TextureManager::getInstance()->renderTexture(draw.texture, x, y, info, w, h);
            }
            else
            {
                Rect rect = {
                    scalePosition(presentX, draw.rect.x, sx),
                    scalePosition(presentY, draw.rect.y, sy),
                    scaleSize(draw.rect.w, sx),
                    scaleSize(draw.rect.h, sy)
                };
                engine->setColor(draw.texture, draw.color);
                engine->renderTexture(draw.texture, nullptr, &rect);
            }
        }, command);
    }

    commands_.clear();
    executing_ = false;
}
