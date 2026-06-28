#include "ChessChallengeFlow.h"

#include "ChessBalance.h"
#include "ChessBattleFlow.h"
#include "ChessChallengeMenu.h"
#include "ChessDetailPanels.h"
#include "ChessMenuHelpers.h"
#include "ChessRewardFlow.h"
#include "ChessScreenLayout.h"
#include "DynamicChessMap.h"
#include "UISave.h"

namespace KysChess
{

ChessChallengeFlow::ChessChallengeFlow(const ChessSelectorServices& services, ChessBattleFlow& battleFlow, ChessRewardFlow& rewardFlow)
    : services_(services)
    , battleFlow_(battleFlow)
    , rewardFlow_(rewardFlow)
{
}

void ChessChallengeFlow::showExpeditionChallenge()
{
    auto& config = ChessBalance::config();
    if (config.challenges.empty())
    {
        return;
    }

    auto manager = makeChessManager(services_);
    for (;;)
    {
        const auto menuSetup = buildChallengeMenuSetup(config.challenges, services_.progress, ChessScreenLayout::browseMenuAnchor());
        auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(
            {menuSetup.config.x, menuSetup.config.y},
            menuSetup.data.labels,
            menuSetup.config.fontSize);
        auto detailPanel = std::make_shared<ChallengeDetailPanel>(config.challenges, services_.progress, services_.roleSave, detailFrame);
        auto menu = makeIndexedMenu("遠征挑戰", menuSetup.data, menuSetup.config, {detailPanel});
        menu->run();

        int sel = menu->getResult();
        if (sel < 0)
        {
            return;
        }

        auto& challenge = config.challenges[sel];
        auto selectedChess = manager.getSelectedForBattle();
        if (selectedChess.empty())
        {
            showChessMessage("請先選擇出戰棋子");
            continue;
        }

        bool alreadyCompleted = services_.progress.isChallengeCompleted(sel);
        auto savedEnemyCallCount = services_.random.getEnemyCallCount();
        auto restoreChallengeSeed = [&]()
        {
            services_.random.setEnemyCallCount(savedEnemyCallCount);
            services_.random.restore();
        };
        DynamicBattleRoles roles;
        for (auto& chess : selectedChess)
        {
            roles.teammate_ids.push_back(chess.role->ID);
            roles.teammate_stars.push_back(chess.star);
            roles.teammate_instances.push_back(chess.id.value);
        }
        for (auto& enemy : challenge.enemies)
        {
            roles.enemy_ids.push_back(enemy.roleId);
            roles.enemy_stars.push_back(enemy.star);
            roles.enemy_weapons.push_back(enemy.weaponId);
            roles.enemy_armors.push_back(enemy.armorId);
        }

        const unsigned int battleSeed = static_cast<unsigned int>(services_.random.enemyRandInt(INT_MAX));
        int result = battleFlow_.runBattle(roles, selectedChess, battleSeed, -1, !alreadyCompleted);
        for (const auto& chess : selectedChess)
        {
            chess.role->HP = chess.role->MaxHP;
            chess.role->MP = chess.role->MaxMP;
        }

        if (result != 0)
        {
            restoreChallengeSeed();
            showChessMessage("挑戰失敗！請調整陣容後再試");
            continue;
        }

        if (alreadyCompleted)
        {
            restoreChallengeSeed();
            showChessMessage("挑戰勝利！(已領取過獎勵)");
            continue;
        }

        int rewardIndex = rewardFlow_.selectChallengeReward(challenge.rewards);
        if (!rewardFlow_.applyReward(challenge.rewards[rewardIndex]))
        {
            continue;
        }

        services_.progress.completeChallenge(sel);
        UISave::autoSave();
    }
}

}    // namespace KysChess
