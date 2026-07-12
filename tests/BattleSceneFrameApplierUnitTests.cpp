#include "BattleSceneFrameApplier.h"
#include "BattleSceneTestRuntimeFixture.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace KysChess::Battle;

namespace
{
struct ApplierFixture
{
    BattleSceneTest::StoreFixture fixture;
    std::deque<BattleAttackEffect> attackEffects;
    std::deque<BattleTextEffect> textEffects;
    std::unordered_map<int, int> hurtFlashTimers;
    RandomDouble random;
    BattleSceneCamera camera;
    BattleSceneCameraBounds cameraBounds{ 6400.0f, 6400.0f, 320.0f, 240.0f };
    bool manualCameraEnabled = false;
    Pointf cameraPosition{ 900.0f, 900.0f, 0.0f };
    int battleResult = -1;
    int frozen = 0;
    int slow = 0;
    int shake = 0;
    int jitterX = 0;
    int jitterY = 0;
    BattleSceneFrameApplier applier;

    struct TestEffects
    {
        std::vector<int> effectSounds;
        std::vector<int> attackSounds;
        std::vector<BattleFrameRumbleEvent> rumbles;

        void playEffectSound(int soundId)
        {
            effectSounds.push_back(soundId);
        }

        void playAttackSound(int soundId)
        {
            attackSounds.push_back(soundId);
        }

        void rumble(int lowFrequency, int highFrequency, int durationMs)
        {
            rumbles.push_back({ lowFrequency, highFrequency, durationMs });
        }

        int textDrawSize(const std::string& text)
        {
            return static_cast<int>(text.size());
        }

        int effectFrameCount(const std::string&)
        {
            return 1;
        }
    } effects;

    ApplierFixture()
        : fixture({
              namedUnit(0, 0, "段譽", { 0.0f, 0.0f, 0.0f }),
              namedUnit(1, 1, "岳不群", { 100.0f, 0.0f, 0.0f }),
          }),
          applier(makeBindings())
    {
        random.set_seed(1);
    }

    static BattleSetupUnitInput namedUnit(int id, int team, std::string name, Pointf position)
    {
        auto unit = BattleSceneTest::makeSetupUnit(id, team, id, 0, position);
        unit.realRoleId = 100 + id;
        unit.name = std::move(name);
        return unit;
    }

    BattleSceneFrameApplier::Bindings makeBindings()
    {
        return {
            fixture.store,
            attackEffects,
            textEffects,
            hurtFlashTimers,
            random,
            cameraPosition,
            battleResult,
            frozen,
            slow,
            shake,
            jitterX,
            jitterY,
            camera,
            cameraBounds,
            manualCameraEnabled,
        };
    }
};
}  // namespace

TEST_CASE("BattleSceneFrameApplier_DoesNotOwnBattleReportMutation", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    frame.logEvents = {
        { BattleLogEventType::Damage, 221, 0, 1, 152, BattleLogCategory::Status, BattleLogPerspective::Targeted, {}, "六脈神劍" },
        { BattleLogEventType::UnitDied, 221, 0, 1 },
        { BattleLogEventType::BattleEnded, 221, -1, -1, 0 },
    };
    BattleLogEvent cancelLog;
    cancelLog.type = BattleLogEventType::Status;
    cancelLog.sourceUnitId = 0;
    cancelLog.targetUnitId = 1;
    cancelLog.amount = 35;
    cancelLog.secondaryAmount = 20;
    cancelLog.category = BattleLogCategory::ProjectileCancel;
    frame.logEvents.push_back(cancelLog);

    fixture.applier.apply(frame, fixture.effects);

    CHECK(fixture.attackEffects.size() == 1);
    CHECK(fixture.hurtFlashTimers.at(1) == 15);
}

TEST_CASE("BattleSceneFrameApplier_CoalescesDamageSceneEffectsPerDamagedUnitPerFrame", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    frame.logEvents = {
        { BattleLogEventType::Damage, 90, 0, 1, 4 },
        { BattleLogEventType::Damage, 90, 0, 1, 3 },
    };

    fixture.applier.apply(frame, fixture.effects);

    REQUIRE(fixture.hurtFlashTimers.size() == 1);
    CHECK(fixture.hurtFlashTimers.at(1) == 15);
    REQUIRE(fixture.attackEffects.size() == 1);
    CHECK(fixture.attackEffects[0].FollowUnitId == 1);
    CHECK(fixture.attackEffects[0].Path.starts_with("eft/bld"));
}

TEST_CASE("BattleSceneFrameApplier_AppliesUnitDeathCameraFreezeSlowShakeAndBattleEndState", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    frame.gameplayEvents = {
        { BattleGameplayEventType::UnitDied, 100, 0, 1 },
        { BattleGameplayEventType::BattleEnded, 100, -1, -1, 0 },
    };

    fixture.applier.apply(frame, fixture.effects);

    CHECK(fixture.battleResult == 0);
    CHECK(fixture.frozen == 60);
    CHECK(fixture.slow == 30);
    CHECK(fixture.shake == 60);
    CHECK(fixture.camera.closeUpFrames() == 30);
}

TEST_CASE("BattleSceneFrameApplier_AppliesProjectileVisualEventsToSceneAttackEffects", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    BattleVisualEvent spawned;
    spawned.type = BattleVisualEventType::ProjectileSpawned;
    spawned.sourceUnitId = 0;
    spawned.targetUnitId = 1;
    spawned.effectId = 10;
    spawned.position = { 10.0f, 12.0f, 0.0f };
    spawned.velocity = { 2.0f, 0.0f, 0.0f };
    spawned.durationFrames = 30;
    spawned.through = true;
    frame.visualEvents.push_back(spawned);

    BattleVisualEvent moved = spawned;
    moved.type = BattleVisualEventType::ProjectileMoved;
    moved.position = { 12.0f, 12.0f, 0.0f };
    moved.durationFrames = 40;
    frame.visualEvents.push_back(moved);

    BattleVisualEvent hit = moved;
    hit.type = BattleVisualEventType::ProjectileHit;
    hit.through = false;
    frame.visualEvents.push_back(hit);

    fixture.applier.apply(frame, fixture.effects);

    REQUIRE(fixture.attackEffects.size() == 1);
    const auto& effect = fixture.attackEffects.front();
    CHECK(effect.VisualAttackId == 10);
    CHECK(effect.VisualOnly == 1);
    CHECK(effect.VisualTeam == 0);
    CHECK(effect.Pos.x == 12.0f);
    CHECK(effect.Velocity.x == 2.0f);
    CHECK(effect.TotalFrame == 40);
    CHECK(effect.Frame == 25);
}

TEST_CASE("BattleSceneFrameApplier_AppliesVisualImpactShakeSoundAndRumble", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    BattleVisualEvent hit;
    hit.type = BattleVisualEventType::ProjectileHit;
    hit.targetUnitId = 1;
    hit.effectId = 10;
    hit.impactUnitShake = 7;
    hit.impactSceneShake = 9;
    hit.impactEffectSoundId = 33;
    hit.impactRumble = true;
    frame.visualEvents.push_back(hit);

    fixture.applier.apply(frame, fixture.effects);

    CHECK(fixture.fixture.store.requirePresentation(1).shake == 7);
    CHECK(fixture.shake == 9);
    REQUIRE(fixture.effects.effectSounds.size() == 1);
    CHECK(fixture.effects.effectSounds[0] == 33);
    REQUIRE(fixture.effects.rumbles.size() == 1);
    CHECK(fixture.effects.rumbles[0].lowFrequency == 100);
    CHECK(fixture.effects.rumbles[0].highFrequency == 100);
    CHECK(fixture.effects.rumbles[0].durationMs == 50);
}

TEST_CASE("BattleSceneFrameApplier_StoresSeparatePaperAnchorForTargetedText", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    BattleVisualEvent number;
    number.type = BattleVisualEventType::DamageNumber;
    number.targetUnitId = 1;
    number.amount = 50;
    number.textSize = 24;
    frame.visualEvents.push_back(number);

    fixture.applier.apply(frame, fixture.effects);

    REQUIRE(fixture.textEffects.size() == 1);
    const auto& effect = fixture.textEffects.front();
    CHECK(effect.Text == "-50");
    CHECK(effect.Pos.x == 77.5f);
    CHECK(effect.Pos.y == -50.0f);
    CHECK(effect.PaperFollowUnitId == 1);
    REQUIRE(effect.PaperAnchor.has_value());
    CHECK(effect.PaperAnchor->x == 100.0f);
    CHECK(effect.PaperAnchor->y == 0.0f);
    CHECK(effect.PaperAnchor->z == 0.0f);
    CHECK(effect.PaperScreenOffsetX == -22.5f);
}

TEST_CASE("BattleSceneFrameApplier_DamageNumberShowsCriticalMultiplier", "[battle][scene_frame_applier]")
{
    ApplierFixture fixture;
    BattlePresentationFrame frame;
    BattleVisualEvent number;
    number.type = BattleVisualEventType::DamageNumber;
    number.targetUnitId = 1;
    number.amount = 125;
    number.criticalMultiplier = 150;
    number.textSize = 24;
    frame.visualEvents.push_back(number);

    fixture.applier.apply(frame, fixture.effects);

    REQUIRE(fixture.textEffects.size() == 1);
    CHECK(fixture.textEffects.front().Text == "-125 (x1.5)");
}
