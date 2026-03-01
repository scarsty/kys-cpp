#pragma once
#include "BattleSceneAct.h"
#include <SDL3/SDL.h>

// 武功范围特效渲染器
// 真气飘带效果：沿弹道飘动的半透明飘带
class AuraEffectRenderer
{
public:
    AuraEffectRenderer() = default;
    ~AuraEffectRenderer() = default;

    // 渲染单个攻击特效的范围提示
    void render(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae, int current_frame);

private:
    // 绘制真气飘带（沿攻击方向/弹道）
    void renderQiRibbon(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                       Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 绘制飘带末端散点粒子
    void renderRibbonParticles(SDL_Renderer* renderer, const BattleSceneAct::AttackEffect& ae,
                               Uint8 r, Uint8 g, Uint8 b, Uint8 alpha);

    // 生成确定性随机种子（基于位置和角色ID）
    uint32_t generateSeed(const BattleSceneAct::AttackEffect& ae) const;
};
