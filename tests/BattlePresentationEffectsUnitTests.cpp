#include "BattlePresentationEffects.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>

TEST_CASE("BattleAttackEffect_RenderTeamUsesVisualTeamOnly", "[battle][presentation][unit]")
{
    BattleAttackEffect effect;
    CHECK(effect.renderTeam() == -1);

    effect.VisualTeam = 1;

    CHECK(effect.renderTeam() == 1);
}

TEST_CASE("BattleAttackEffect_AdvanceVisualOnlyEffectsTicksOnlyVisualLifetime", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect visualRoleEffect;
    visualRoleEffect.VisualOnly = 1;
    visualRoleEffect.TotalFrame = 3;
    effects.push_back(visualRoleEffect);

    BattleAttackEffect gameplayProjectile;
    gameplayProjectile.TotalFrame = 3;
    effects.push_back(gameplayProjectile);

    advanceBattleVisualOnlyEffects(effects);

    CHECK(effects[0].Frame == 1);
    CHECK(effects[1].Frame == 0);
}

TEST_CASE("BattleAttackEffect_VisualOnlyProjectileFadeAdvancesToExpiry", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect impactedVisualProjectile;
    impactedVisualProjectile.VisualOnly = 1;
    impactedVisualProjectile.TotalFrame = 30;
    impactedVisualProjectile.Frame = 28;
    effects.push_back(impactedVisualProjectile);

    advanceBattleVisualOnlyEffects(effects);
    CHECK(effects.front().Frame == 29);

    advanceBattleVisualOnlyEffects(effects);
    CHECK(effects.front().Frame == 30);
    CHECK(effects.front().Frame >= effects.front().TotalFrame);
}

TEST_CASE("BattleAttackEffect_PresentationLifetimeAdvancesOnlyOnBattleFrameTicks", "[battle][presentation][unit]")
{
    std::deque<BattleAttackEffect> effects;

    BattleAttackEffect roleEffect;
    roleEffect.VisualOnly = 1;
    roleEffect.TotalFrame = 2;
    roleEffect.Frame = 1;
    effects.push_back(roleEffect);

    BattleAttackEffect runtimeOwnedEffect;
    runtimeOwnedEffect.VisualOnly = 0;
    runtimeOwnedEffect.TotalFrame = 2;
    runtimeOwnedEffect.Frame = 1;
    effects.push_back(runtimeOwnedEffect);

    advanceBattlePresentationEffects(effects, false);

    REQUIRE(effects.size() == 2);
    CHECK(effects[0].Frame == 1);
    CHECK(effects[1].Frame == 1);

    advanceBattlePresentationEffects(effects, true);

    REQUIRE(effects.size() == 1);
    CHECK(effects.front().VisualOnly == 0);
    CHECK(effects.front().Frame == 1);
}
