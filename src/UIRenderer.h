#pragma once

#include "Engine.h"
#include "TextureManager.h"
#include <string>
#include <variant>
#include <vector>

class UIRenderer
{
public:
    static UIRenderer* getInstance()
    {
        static UIRenderer renderer;
        return &renderer;
    }

    void drawText(const std::string& text, int size, int x, int y, Color color, uint8_t alpha);
    void fillColor(Color color, int x, int y, int w, int h, BlendMode blend = BLENDMODE_BLEND);
    void drawTexture(TextureWarpper* texture, int x, int y,
        const TextureManager::RenderInfo& info = {}, int w = -1, int h = -1);
    void drawTexture(Texture* texture, const Rect& rect, Color color);
    void execute();
    bool isExecuting() const { return executing_; }

private:
    struct TextCommand
    {
        std::string text;
        int size;
        int x;
        int y;
        Color color;
        uint8_t alpha;
    };

    struct FillCommand
    {
        Color color;
        int x;
        int y;
        int w;
        int h;
        BlendMode blend;
    };

    struct TextureCommand
    {
        TextureWarpper* texture;
        int x;
        int y;
        TextureManager::RenderInfo info;
        int w;
        int h;
    };

    struct RawTextureCommand
    {
        Texture* texture;
        Rect rect;
        Color color;
    };

    using Command = std::variant<TextCommand, FillCommand, TextureCommand, RawTextureCommand>;
    std::vector<Command> commands_;
    bool executing_ = false;
};
