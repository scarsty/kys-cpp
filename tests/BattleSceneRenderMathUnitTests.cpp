#include "BattleSceneRenderMath.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneRenderMath_ConvertsFaceTowardsToRenderVector", "[battle][scene_render_math]")
{
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightDown).x == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightDown).y == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightUp).x == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightUp).y == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftDown).x == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftDown).y == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftUp).x == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftUp).y == -1);
}

TEST_CASE("BattleSceneRenderMath_ResolvesFallbackFightStyle", "[battle][scene_render_math]")
{
    std::array<int, 5> frames{ 0, 0, 0, 6, 0 };

    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, 0) == 3);
    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, 2) == 3);
    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, -1) == 3);
}

TEST_CASE("BattleSceneRenderMath_CalculatesRenderPicFromFramesAndFacing", "[battle][scene_render_math]")
{
    std::array<int, 5> frames{ 4, 5, 6, 7, 8 };
    Pointf rightDown{ 1, 1, 0 };
    Pointf leftUp{ -1, -1, 0 };

    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, rightDown, -1, 0) == 4 * Towards_RightDown);
    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, leftUp, 2, 3) == 4 * 4 + 5 * 4 + 6 * Towards_LeftUp + 3);
}

TEST_CASE("BattleSceneRenderMath_DecreasesValuesToZero", "[battle][scene_render_math]")
{
    int integerValue = 2;
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 1);
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 0);
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 0);

    float floatValue = 0.25f;
    BattleSceneRenderMath::decreaseToZero(floatValue, 0.5f);
    CHECK(floatValue == 0.0f);
}

TEST_CASE("BattleSceneRenderMath_AppliesHurtFlashColorOnlyDuringActiveTimer", "[battle][scene_render_math]")
{
    Color base{ 10, 20, 30, 40 };

    CHECK(BattleSceneRenderMath::hurtFlashColor(0, base).r == 10);
    CHECK(BattleSceneRenderMath::hurtFlashColor(3, base).r == 255);
    CHECK(BattleSceneRenderMath::hurtFlashColor(3, base).a == 40);
}
