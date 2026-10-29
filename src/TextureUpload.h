#pragma once

#include <SDL3/SDL.h>

// 借用表面，以統一格式上傳；保留原始上傳的色彩與混合語意。
SDL_Texture* createNormalizedTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface);
