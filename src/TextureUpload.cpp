#include "TextureUpload.h"

SDL_Texture* createNormalizedTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface)
{
    // 字型產生或解碼失敗時，沿用 SDL 的錯誤處理。
    if (!surface)
    {
        return SDL_CreateTextureFromSurface(renderer, surface);
    }

    const bool hasColorKey = SDL_SurfaceHasColorKey(surface);
    SDL_BlendMode blendMode{};
    Uint8 r{}, g{}, b{}, a{};
    SDL_GetSurfaceBlendMode(surface, &blendMode);
    SDL_GetSurfaceColorMod(surface, &r, &g, &b);
    SDL_GetSurfaceAlphaMod(surface, &a);

    SDL_Surface* converted{};
    if (surface->format != SDL_PIXELFORMAT_RGBA32)
    {
        converted = SDL_ConvertSurfaceAndColorspace(surface, SDL_PIXELFORMAT_RGBA32, nullptr,
            SDL_GetSurfaceColorspace(surface), SDL_GetSurfaceProperties(surface));
        if (!converted)
        {
            return nullptr;
        }
    }

    // SDL 處理色鍵轉透明度及最終紋理色彩空間；轉換不將調色烘入像素。
    auto* texture = SDL_CreateTextureFromSurface(renderer, converted ? converted : surface);
    SDL_DestroySurface(converted);
    if (!texture)
    {
        return nullptr;
    }

    // 格式轉換會重設混合模式，必須恢復原始上傳的有效狀態。
    if (!SDL_SetTextureBlendMode(texture, hasColorKey ? SDL_BLENDMODE_BLEND : blendMode)
        || !SDL_SetTextureColorMod(texture, r, g, b)
        || !SDL_SetTextureAlphaMod(texture, a))
    {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}
