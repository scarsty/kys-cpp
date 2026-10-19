#pragma once

#include "ChessGameContent.h"
#include "ChessManagementRules.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

enum class ChessGameGuideLineTone { Standard, Information, Highlight };

struct ChessGameGuideLine
{
    std::string text;
    ChessGameGuideLineTone tone = ChessGameGuideLineTone::Standard;
};

struct ChessGameGuideSection
{
    std::string title;
    std::vector<ChessGameGuideLine> lines;
};

inline std::vector<ChessGameGuideSection> buildChessGameGuideSections(
    const ChessGameContent& content, std::optional<ChessTalentId> selectedTalent = std::nullopt)
{
    using Tone = ChessGameGuideLineTone;
    const auto& balance = content.balance();
    const auto& neigong = content.neigongConfig();
    const auto talentId = selectedTalent.value_or(balance.defaultTalent);
    const auto& talent = balance.talent(talentId);
    std::string prices;
    for (int tier = 0; tier < static_cast<int>(balance.tierPrices.size()); ++tier)
    {
        if (!prices.empty()) prices += "、";
        prices += std::format("{}費 {}金", tier + 1, balance.tierPrices[tier]);
    }

    auto equipmentRewards = balance.playerEquipmentRewards;
    if (const auto extra = balance.talentEquipmentRewards.find(talentId); extra != balance.talentEquipmentRewards.end())
        equipmentRewards.insert(equipmentRewards.end(), extra->second.begin(), extra->second.end());
    const auto equipmentRewardText = equipmentRewards.empty()
        ? std::string("固定獎勵：本局沒有固定裝備獎勵；可查看遠征的獎勵內容。")
        : std::format("固定獎勵：共 {} 次，最早第 {} 關，最高 {} 階；已計入本局天賦。",
            equipmentRewards.size(), std::ranges::min(equipmentRewards, {}, &BalanceConfig::PlayerEquipmentReward::fight).fight,
            std::ranges::max(equipmentRewards, {}, &BalanceConfig::PlayerEquipmentReward::maxTier).maxTier);
    const auto legendaryShopText = !talent.legendaryShop
        ? std::string("神兵商店：此天賦沒有神兵商店。")
        : balance.legendaryShop.unlockFight <= 0
            ? std::string("神兵商店：本局未開放。")
            : std::format("神兵商店：通過第 {} 關後，由「神兵」天賦說明或「裝備」進入；每件 {} 金。",
                balance.legendaryShop.unlockFight, balance.legendaryShop.price);
    const bool bansEnabled = balance.banBaseCount > 0 || balance.banCountPerLevel > 0
        || !balance.banUnlocks.empty() || talent.openingBans > 0;

    return {
        {"基本流程", {
            {"整備 → 戰前總覽 → 佈陣 → 自動戰鬥 → 領取獎勵", Tone::Highlight},
            {"整備時招募棋子、安排出戰與候補、分配裝備。點擊棋子，在右側檢視器查看屬性與操作。"},
            {balance.bossInterval > 0
                ? std::format("主線共 {} 關，每 {} 關一場強敵戰。失敗後可調整陣容，再挑戰目前關卡。", balance.totalFights, balance.bossInterval)
                : std::format("主線共 {} 關，本難度不設固定強敵戰。失敗後可調整陣容再試。", balance.totalFights), Tone::Information},
            {std::format("主線勝利獲得 {} 經驗；強敵勝利獲得 {} 經驗。金幣、利息與額外羈絆收入可在整備頂列查看。", balance.battleExp, balance.bossBattleExp)},
            {"戰前總覽保留雙方名稱、星級、生命、攻擊、防禦、輕功及裝備；顯示的是已計入開戰加成的數值。"},
            {"資源載入時先顯示靜態總覽與載入階段，完成後才開放檢視及繼續；不用反覆點擊。"},
        }},
        {"棋子與升星", {
            {std::format("商店有 {} 個欄位；價格為 {}。", balance.shopSlotCount, prices), Tone::Highlight},
            {"電腦上懸停可預覽，點擊卡片可固定檢視；直接點金幣價格即可購買，也可使用檢視器的購買按鈕。"},
            {"一般合成：三枚相同角色、相同星級自動合成下一星，通常可合成至三星；天賦額外升星依天賦規則。"},
            {std::format("出售價值按費用計算，每增加一星乘 {}；出售會歸還棋子所穿戴的裝備。", balance.starCostMult)},
            {"主線勝利或首次通關遠征時，所有出戰棋子勝場 +1；合成取最高勝場，不會把三枚的勝場相加。", Tone::Information},
            {std::format("基礎勝場成長：生命 +{}、攻擊 +{}、防禦 +{}、兵器熟練度 +{}、輕功 +{}；小數累計後取整，天賦可再調整成長。",
                balance.fightWinGrowthHP, balance.fightWinGrowthAtk, balance.fightWinGrowthDef, balance.fightWinGrowthWeapon, balance.fightWinGrowthSpeed)},
        }},
        {"金幣與等級", {
            {std::format("開局 {} 金；刷新商店 {} 金；購買經驗 {} 金換 {} 點。", balance.initialMoney, balance.refreshCost, balance.buyExpCost, balance.buyExpAmount), Tone::Highlight},
            {balance.interestPercent > 0
                ? std::format("主線勝利結算利息：目前存款 × {}%，小數捨去，上限 {} 金；不是每次刷新或每場遠征都發放。", balance.interestPercent, balance.interestMax)
                : "本難度不計存款利息。", Tone::Information},
            {std::format("主線金幣隨進度成長，強敵額外 +{} 金；頂列「金幣與利息」會列出本關預計收入及下一個利息門檻。", balance.bossRewardBonus)},
            {std::format("最高 {} 級；出戰容量隨等級增加，最多 {} 人；候補可放 {} 人，開戰至少需要 {} 人。",
                balance.maxLevel + 1, ChessManagementRules::maximumDeploymentAtLevel(content, balance.maxLevel), balance.benchSize, balance.minBattleSize)},
            {"頂列「等級與經驗」可比較目前與下一級的商店機率；費用色彩與棋子名稱一致。"},
            {"鎖定商店可保留目前商品到下一回合；合成、出售及經驗支出都會影響後續購買安排。"},
        }},
        {"羈絆與禁棋", {
            {"羈絆以出戰陣容計算；候補不提供羈絆，重複角色不當成不同成員。", Tone::Highlight},
            {"圖示旁的 2/4 表示目前計點與目標門檻。點擊圖示開啟完整羈絆一覽，查看成員、已啟動及下一階效果。"},
            {"江南七絕等星級羈絆按其規則計點；合縱會調整羈絆計點及門檻。實際數字以本局羈絆一覽為準。", Tone::Information},
            {bansEnabled
                ? std::format("一般禁棋：開局 {} 個名額，每升級增加 {} 個；由整備底列「封禁」管理已見角色，也可選中商店棋子後按右下「封禁」，確認後生效。", balance.banBaseCount, balance.banCountPerLevel)
                : "本局沒有一般禁棋名額或額外禁棋獎勵。"},
            {bansEnabled ? "禁棋會影響後續抽取，不會移除已持有棋子，也不會立刻替換目前商店商品；已禁角色不能解除。" : "不同難度及天賦可能開放禁棋，規則依本局設定。"},
            {std::format("額外禁棋：關卡獎勵共 {} 段；本天賦開局另提供 {} 個。獎勵會列明可禁費用，可選擇放棄剩餘名額。", balance.banUnlocks.size(), talent.openingBans)},
        }},
        {"裝備與內功", {
            {"每枚棋子各有一個武器與防具欄；在「裝備」分配或更換，點圖示查看完整效果與專屬加成。", Tone::Highlight},
            {equipmentRewardText, Tone::Information},
            {legendaryShopText},
            {balance.bossInterval > 0
                ? std::format("強敵內功獎勵：最多 {} 本免費候選中選一本；選擇額外候選需 {} 金，並非一次取得所有候選。", neigong.choiceCount, neigong.additionalOptionCost)
                : "本難度沒有固定強敵內功獎勵；其他獎勵以各關說明為準。"},
            {"內功取得後按其效果生效，不占棋子的裝備欄；整備頂列書本圖示可查看已獲得的內功。"},
            {"棋子詳情顯示棋戰的輕擊、重擊或遠程類型；只有絕招列出額外配置效果，具體觸發條件以說明為準。"},
        }},
        {"遠征與對戰", {
            {balance.challenges.empty() ? "本局沒有遠征挑戰。"
                : std::format("遠征共 {} 關，不推進主線；首次勝利可領取該關獎勵，重打不重複發獎或累加勝場。", balance.challenges.size()), Tone::Highlight},
            {"挑戰前可切換頁籤查看敵人、獎勵與戰場；不同關卡可能提供金幣、棋子、升星、內功或裝備。"},
            {"離線對戰需使用困難模式、處於整備階段的進度；由整備底列進入。", Tone::Information},
            {"匯出自己的對戰檢查點，或匯入對手存檔並驗證；確認雙方陣容及戰鬥種子後開戰。"},
            {"離線對戰使用獨立戰鬥，不消耗主線進度、不改變金幣，也不累加主線勝場或獎勵。"},
        }},
        {"戰鬥與日誌", {
            {"佈陣預設開啟；戰前可選「地圖佈陣」或「列表佈陣」，返回後仍可切換另一種方式。", Tone::Highlight},
            {"開戰後自動戰鬥，右上圖示可切換速度、視角、鏡頭及暫停。"},
            {"日誌隨時可開啟，戰鬥會繼續，內容即時更新；需要仔細閱讀時，可在日誌內暫停戰鬥。", Tone::Information},
            {"用我方、敵方及事件類型篩選。捲動到較早內容會關閉「追蹤最新」；重新勾選即可跟隨新事件。"},
            {"關閉日誌保留當前暫停狀態。若戰鬥已結束，關閉日誌後才進入戰後總覽。"},
            {"日誌的幀數是戰鬥模擬時間；命中停格、慢動作及畫面更新次數不會改變事件時間。"},
        }},
        {"操作與存檔", {
            {"整備底列可直接存檔、讀檔；頂列「系統」包含設定、遊戲說明及返回開頭。", Tone::Highlight},
            {"一般存檔在整備階段使用。主線或遠征戰鬥結束、領完所有獎勵並回到整備後，才會更新自動檔。"},
            {"跨裝置接續請使用外部存檔匯出／匯入；離線對戰的匯入功能是載入對手，不會替換你的主線進度。", Tone::Information},
            {"點「收起」可回到擂鼓山；點右上「整備」或按 Q 可重新開啟，保留目前棋局。"},
            {"電腦可懸停查看提示、點擊固定內容；觸控請點選項目後使用畫面上的操作按鈕。"},
            {"本說明頂部頁籤直接切換章節；方向鍵可切換章節，PageUp／PageDown 或滾輪翻閱長文，返回關閉說明。"},
        }},
    };
}

} // namespace KysChess
