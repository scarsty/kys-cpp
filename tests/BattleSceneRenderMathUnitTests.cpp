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

TEST_CASE("BattleSceneRenderMath_ClampsPastActionEndToDeclaredLastFrame", "[battle][scene_render_math]")
{
    std::array<int, 5> frames{ 4, 5, 6, 7, 8 };
    Pointf leftUp{ -1, -1, 0 };
    const int style = 2;
    const int lastFrame = frames[style] - 1;
    const int lastPic = 4 * 4 + 5 * 4 + frames[style] * Towards_LeftUp + lastFrame;

    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, leftUp, style, lastFrame) == lastPic);
    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, leftUp, style, lastFrame + 20) == lastPic);
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

TEST_CASE("BattleSceneRenderMath_MapsWindowPointThroughPresentedGameRect", "[battle][scene_render_math]")
{
    const Rect presentRect{ 160, 0, 1600, 900 };

    CHECK(BattleSceneRenderMath::windowPointToUiPoint({ 160, 0 }, presentRect, 1280, 720).x == 0);
    CHECK(BattleSceneRenderMath::windowPointToUiPoint({ 160, 0 }, presentRect, 1280, 720).y == 0);
    CHECK(BattleSceneRenderMath::windowPointToUiPoint({ 1760, 900 }, presentRect, 1280, 720).x == 1280);
    CHECK(BattleSceneRenderMath::windowPointToUiPoint({ 1760, 900 }, presentRect, 1280, 720).y == 720);
}

TEST_CASE("BattleSceneRenderMath_ClampsPaperCameraWithoutClassicHalfY", "[battle][scene_render_math]")
{
    const Pointf inside{ 2000.0f, 4000.0f, 35.0f };
    const auto unchanged = BattleSceneRenderMath::clampPaperCameraCenter(inside, 4608.0f, 4608.0f);

    CHECK(unchanged.x == 2000.0f);
    CHECK(unchanged.y == 4000.0f);
    CHECK(unchanged.z == 0.0f);

    const Pointf outside{ -20.0f, 4700.0f, 9.0f };
    const auto clamped = BattleSceneRenderMath::clampPaperCameraCenter(outside, 4608.0f, 4608.0f);

    CHECK(clamped.x == 0.0f);
    CHECK(clamped.y == 4608.0f);
    CHECK(clamped.z == 0.0f);
}

TEST_CASE("BattleSceneRenderMath_AnchorsPaperRoleInfoAboveSpriteTop", "[battle][scene_render_math]")
{
    CHECK(BattleSceneRenderMath::paperRoleInfoAnchorZ(120) == 62.0f);
    CHECK(BattleSceneRenderMath::paperRoleInfoAnchorZ(-12) == 42.0f);
    CHECK(BattleSceneRenderMath::paperRoleInfoAnchorZ(300) == 64.0f);
}

TEST_CASE("BattleSceneRenderMath_UsesStablePaperFloatingTextHeight", "[battle][scene_render_math]")
{
    CHECK(BattleSceneRenderMath::paperFloatingTextAnchorZ() == 80.0f);
}

TEST_CASE("BattleSceneRenderMath_DrawsSelectedCursorFloorOverCachedGround", "[battle][scene_render_math]")
{
    const auto draw = BattleSceneRenderMath::battleCursorFloorDraw(
        true,
        false,
        true,
        false,
        true,
        true);

    CHECK(draw.shouldDraw);
    CHECK(draw.color.r == 255);
    CHECK(draw.color.g == 255);
    CHECK(draw.color.b == 255);
    CHECK(draw.color.a == 255);
}

TEST_CASE("BattleSceneRenderMath_PreservesCursorRangeFloorTintRules", "[battle][scene_render_math]")
{
    const auto selectable = BattleSceneRenderMath::battleCursorFloorDraw(
        true,
        false,
        true,
        false,
        false,
        true);
    CHECK(selectable.shouldDraw);
    CHECK(selectable.color.r == 128);

    const auto unavailable = BattleSceneRenderMath::battleCursorFloorDraw(
        true,
        false,
        false,
        false,
        false,
        true);
    CHECK_FALSE(unavailable.shouldDraw);
    CHECK(unavailable.color.r == 64);

    const auto actionEffect = BattleSceneRenderMath::battleCursorFloorDraw(
        true,
        true,
        true,
        true,
        false,
        true);
    CHECK(actionEffect.shouldDraw);
    CHECK(actionEffect.color.r == 192);
}

TEST_CASE("BattleSceneRenderMath_LiftsPaperCursorFloorQuadAboveGround", "[battle][scene_render_math]")
{
    const auto quad = BattleSceneRenderMath::paperFloorQuad(
        { 10.0f, 20.0f, 0.0f },
        { 46.0f, 56.0f, 0.0f },
        { 10.0f, 92.0f, 0.0f },
        { -26.0f, 56.0f, 0.0f },
        1.5f);

    CHECK(quad[0].x == 10.0f);
    CHECK(quad[1].y == 56.0f);
    CHECK(quad[2].x == 10.0f);
    CHECK(quad[3].y == 56.0f);
    for (const auto& point : quad)
    {
        CHECK(point.z == 1.5f);
    }
}
