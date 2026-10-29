#include "TextureUpload.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <memory>
#include <vector>

namespace
{
using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

struct UploadFixture
{
    SurfacePtr output{SDL_CreateSurface(4, 1, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{
        SDL_CreateSoftwareRenderer(output.get()), SDL_DestroyRenderer};

    UploadFixture()
    {
        REQUIRE(output);
        REQUIRE(renderer);
    }

    std::vector<Uint8> draw(SDL_Texture* texture, SDL_PixelFormat targetFormat, int copies)
    {
        std::vector<TexturePtr> targets;
        for (int i = 0; i < copies; ++i)
        {
            targets.emplace_back(SDL_CreateTexture(renderer.get(), targetFormat,
                SDL_TEXTUREACCESS_TARGET, 4, 1), SDL_DestroyTexture);
            REQUIRE(targets.back());
            REQUIRE(SDL_SetRenderTarget(renderer.get(), targets.back().get()));
            REQUIRE(SDL_SetRenderDrawColor(renderer.get(), 13, 27, 41, 59));
            REQUIRE(SDL_RenderClear(renderer.get()));
            REQUIRE(SDL_RenderTexture(renderer.get(), texture, nullptr, nullptr));
            texture = targets.back().get();
        }
        REQUIRE(SDL_SetRenderTarget(renderer.get(), nullptr));
        REQUIRE(SDL_SetRenderDrawColor(renderer.get(), 83, 61, 37, 101));
        REQUIRE(SDL_RenderClear(renderer.get()));
        REQUIRE(SDL_RenderTexture(renderer.get(), texture, nullptr, nullptr));
        SurfacePtr pixels{SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface};
        REQUIRE(pixels);
        std::vector<Uint8> result;
        for (int x = 0; x < 4; ++x)
        {
            Uint8 r{}, g{}, b{}, a{};
            REQUIRE(SDL_ReadSurfacePixel(pixels.get(), x, 0, &r, &g, &b, &a));
            result.insert(result.end(), {r, g, b, a});
        }
        return result;
    }
};

struct SurfaceState
{
    SDL_PixelFormat format{};
    SDL_Colorspace colorspace{};
    SDL_BlendMode blend{};
    std::array<Uint8, 4> modulation{};
    bool hasKey{};
    Uint32 key{};
    std::vector<Uint8> pixels;
    bool operator==(const SurfaceState&) const = default;
};

SurfaceState snapshot(SDL_Surface* surface)
{
    SurfaceState state;
    state.format = surface->format;
    state.colorspace = SDL_GetSurfaceColorspace(surface);
    SDL_GetSurfaceBlendMode(surface, &state.blend);
    SDL_GetSurfaceColorMod(surface, &state.modulation[0], &state.modulation[1], &state.modulation[2]);
    SDL_GetSurfaceAlphaMod(surface, &state.modulation[3]);
    state.hasKey = SDL_SurfaceHasColorKey(surface);
    if (state.hasKey) SDL_GetSurfaceColorKey(surface, &state.key);
    const auto* begin = static_cast<const Uint8*>(surface->pixels);
    state.pixels.assign(begin, begin + surface->pitch * surface->h);
    return state;
}

void checkUpload(UploadFixture& fixture, SDL_Surface* surface)
{
    const auto before = snapshot(surface);
    TexturePtr reference{SDL_CreateTextureFromSurface(fixture.renderer.get(), surface), SDL_DestroyTexture};
    TexturePtr normalized{createNormalizedTextureFromSurface(fixture.renderer.get(), surface), SDL_DestroyTexture};
    REQUIRE(reference);
    REQUIRE(normalized);
    CHECK(normalized->format == SDL_PIXELFORMAT_RGBA32);
    CHECK(snapshot(surface) == before);
    SDL_BlendMode oldBlend{}, newBlend{};
    SDL_GetTextureBlendMode(reference.get(), &oldBlend);
    SDL_GetTextureBlendMode(normalized.get(), &newBlend);
    CHECK(newBlend == oldBlend);
    std::array<Uint8, 4> oldMod{}, newMod{};
    SDL_GetTextureColorMod(reference.get(), &oldMod[0], &oldMod[1], &oldMod[2]);
    SDL_GetTextureAlphaMod(reference.get(), &oldMod[3]);
    SDL_GetTextureColorMod(normalized.get(), &newMod[0], &newMod[1], &newMod[2]);
    SDL_GetTextureAlphaMod(normalized.get(), &newMod[3]);
    CHECK(newMod == oldMod);
    auto oldProps = SDL_GetTextureProperties(reference.get());
    auto newProps = SDL_GetTextureProperties(normalized.get());
    CHECK(SDL_GetNumberProperty(newProps, SDL_PROP_TEXTURE_COLORSPACE_NUMBER, 0)
        == SDL_GetNumberProperty(oldProps, SDL_PROP_TEXTURE_COLORSPACE_NUMBER, 0));
    CHECK(SDL_GetFloatProperty(newProps, SDL_PROP_TEXTURE_SDR_WHITE_POINT_FLOAT, 0)
        == SDL_GetFloatProperty(oldProps, SDL_PROP_TEXTURE_SDR_WHITE_POINT_FLOAT, 0));
    CHECK(SDL_GetFloatProperty(newProps, SDL_PROP_TEXTURE_HDR_HEADROOM_FLOAT, 0)
        == SDL_GetFloatProperty(oldProps, SDL_PROP_TEXTURE_HDR_HEADROOM_FLOAT, 0));
    for (auto scale : {SDL_SCALEMODE_NEAREST, SDL_SCALEMODE_LINEAR})
    {
        SDL_SetTextureScaleMode(reference.get(), scale);
        SDL_SetTextureScaleMode(normalized.get(), scale);
        for (int copies : {0, 1, 3})
        {
            INFO("target copies: " << copies);
            CHECK(fixture.draw(reference.get(), SDL_PIXELFORMAT_RGBA8888, copies)
                == fixture.draw(normalized.get(), SDL_PIXELFORMAT_RGBA32, copies));
        }
    }
}
}

TEST_CASE("texture normalization preserves pixels and effective upload state", "[render][texture]")
{
    UploadFixture fixture;
    for (auto format : {SDL_PIXELFORMAT_RGB24, SDL_PIXELFORMAT_BGRA32, SDL_PIXELFORMAT_RGBA32})
    for (auto blend : {SDL_BLENDMODE_NONE, SDL_BLENDMODE_BLEND, SDL_BLENDMODE_ADD})
    for (bool keyed : {false, true})
    {
        INFO("format: " << SDL_GetPixelFormatName(format) << " blend: " << blend << " keyed: " << keyed);
        SurfacePtr surface{SDL_CreateSurface(4, 1, format), SDL_DestroySurface};
        REQUIRE(surface);
        for (int x = 0; x < 4; ++x)
            REQUIRE(SDL_WriteSurfacePixel(surface.get(), x, 0, 211 - x * 37, 31 + x * 23, 83, x * 85));
        REQUIRE(SDL_SetSurfaceBlendMode(surface.get(), blend));
        REQUIRE(SDL_SetSurfaceColorMod(surface.get(), 137, 219, 71));
        REQUIRE(SDL_SetSurfaceAlphaMod(surface.get(), 173));
        if (keyed)
            REQUIRE(SDL_SetSurfaceColorKey(surface.get(), true, SDL_MapSurfaceRGBA(surface.get(), 174, 54, 83, 85)));
        checkUpload(fixture, surface.get());
    }
}

TEST_CASE("texture normalization preserves indexed color keys without keying duplicate colors", "[render][texture]")
{
    UploadFixture fixture;
    SurfacePtr surface{SDL_CreateSurface(4, 1, SDL_PIXELFORMAT_INDEX8), SDL_DestroySurface};
    REQUIRE(surface);
    auto* palette = SDL_CreateSurfacePalette(surface.get());
    REQUIRE(palette);
    const SDL_Color colors[]{{200, 30, 70, 255}, {200, 30, 70, 255}, {17, 213, 91, 128}, {0, 0, 0, 0}};
    REQUIRE(SDL_SetPaletteColors(palette, colors, 0, 4));
    auto* pixels = static_cast<Uint8*>(surface->pixels);
    for (int x = 0; x < 4; ++x) pixels[x] = static_cast<Uint8>(x);
    REQUIRE(SDL_SetSurfaceColorKey(surface.get(), true, 0));
    REQUIRE(SDL_SetSurfaceColorMod(surface.get(), 177, 233, 91));
    REQUIRE(SDL_SetSurfaceAlphaMod(surface.get(), 151));
    checkUpload(fixture, surface.get());
    for (int i = 0; i < 4; ++i)
    {
        CHECK(palette->colors[i].r == colors[i].r);
        CHECK(palette->colors[i].g == colors[i].g);
        CHECK(palette->colors[i].b == colors[i].b);
        CHECK(palette->colors[i].a == colors[i].a);
    }
}

TEST_CASE("texture normalization retains source color management", "[render][texture]")
{
    UploadFixture fixture;
    for (auto format : {SDL_PIXELFORMAT_BGRA32, SDL_PIXELFORMAT_RGBA32})
    {
        SurfacePtr surface{SDL_CreateSurface(4, 1, format), SDL_DestroySurface};
        REQUIRE(surface);
        REQUIRE(SDL_SetSurfaceColorspace(surface.get(), SDL_COLORSPACE_SRGB_LINEAR));
        auto props = SDL_GetSurfaceProperties(surface.get());
        REQUIRE(SDL_SetFloatProperty(props, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 1.0f));
        REQUIRE(SDL_SetFloatProperty(props, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 2.0f));
        for (int x = 0; x < 4; ++x)
            REQUIRE(SDL_WriteSurfacePixel(surface.get(), x, 0, 32 + x * 48, 64, 128, 255));
        checkUpload(fixture, surface.get());
        CHECK(SDL_GetFloatProperty(props, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 0) == 1.0f);
        CHECK(SDL_GetFloatProperty(props, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 0) == 2.0f);
    }
}
