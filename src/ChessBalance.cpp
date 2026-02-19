#include "ChessBalance.h"

#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

#include <array>
#include <print>
#include <vector>

namespace KysChess
{

bool ChessBalance::applied_ = false;
BalanceConfig ChessBalance::config_;

bool ChessBalance::loadConfig(const std::string& path)
{
    YAML::Node root;
    try { root = YAML::LoadFile(path); }
    catch (const YAML::Exception& e)
    {
        std::print("【平衡配置】无法读取文件 {}: {}\n", path, e.what());
        return false;
    }

    BalanceConfig c;

    if (auto n = root["星级加成"])
    {
        if (n["生命攻击倍率"]) c.starHPAtkMult = n["生命攻击倍率"].as<double>();
        if (n["防御倍率"]) c.starDefMult = n["防御倍率"].as<double>();
        if (n["速度倍率"]) c.starSpdMult = n["速度倍率"].as<double>();
    }

    if (auto n = root["经济"])
    {
        if (n["初始金币"]) c.initialMoney = n["初始金币"].as<int>();
        if (n["刷新费用"]) c.refreshCost = n["刷新费用"].as<int>();
        if (n["购买经验费用"]) c.buyExpCost = n["购买经验费用"].as<int>();
        if (n["购买经验数量"]) c.buyExpAmount = n["购买经验数量"].as<int>();
        if (n["战斗经验"]) c.battleExp = n["战斗经验"].as<int>();
        if (n["Boss战斗经验"]) c.bossBattleExp = n["Boss战斗经验"].as<int>();
        if (n["战斗奖励基础"]) c.rewardBase = n["战斗奖励基础"].as<int>();
        if (n["战斗奖励阶段系数"]) c.rewardStageCoeff = n["战斗奖励阶段系数"].as<int>();
    }

    if (auto n = root["棋子费用"])
    {
        if (n["各费价格"])
            for (int i = 0; i < 5 && i < (int)n["各费价格"].size(); ++i)
                c.tierPrices[i] = n["各费价格"][i].as<int>();
        if (n["星级倍率"]) c.starCostMult = n["星级倍率"].as<int>();
    }

    if (root["经验表"])
    {
        c.expTable.clear();
        for (const auto& v : root["经验表"])
            c.expTable.push_back(v.as<int>());
    }

    if (root["最高等级"]) c.maxLevel = root["最高等级"].as<int>();
    if (root["背包上限"]) c.benchSize = root["背包上限"].as<int>();
    if (root["最低出战人数"]) c.minBattleSize = root["最低出战人数"].as<int>();

    if (root["商店权重"])
    {
        int lvl = 0;
        for (const auto& row : root["商店权重"])
        {
            if (lvl >= 10) break;
            for (int t = 0; t < 5 && t < (int)row.size(); ++t)
                c.shopWeights[lvl][t] = row[t].as<int>();
            lvl++;
        }
    }

    if (auto n = root["敌人"])
    {
        if (n["数量基础"]) c.enemyCountBase = n["数量基础"].as<int>();
        if (n["数量上限"]) c.enemyCountMax = n["数量上限"].as<int>();
        if (n["低级混入概率"]) c.lowerTierMixPct = n["低级混入概率"].as<int>();
        if (n["星级提升起始关卡"]) c.starUpgradeStartSub = n["星级提升起始关卡"].as<int>();
        if (n["星级提升概率"]) c.starUpgradePct = n["星级提升概率"].as<int>();
        if (n["Boss升阶"]) c.bossTierUp = n["Boss升阶"].as<int>();
        if (n["Boss最高费"]) c.bossMaxTier = n["Boss最高费"].as<int>();
        if (n["Boss星级起始阶段"]) c.bossStarStartStage = n["Boss星级起始阶段"].as<int>();
    }

    if (auto n = root["进度"])
    {
        if (n["每阶段关卡数"]) c.substagesPerStage = n["每阶段关卡数"].as<int>();
        if (n["Boss关卡序号"]) c.bossSubstage = n["Boss关卡序号"].as<int>();
        if (n["通关阶段数"]) c.gameCompleteStage = n["通关阶段数"].as<int>();
    }

    config_ = std::move(c);
    std::print("【平衡配置】加载成功\n");
    return true;
}

const BalanceConfig& ChessBalance::config()
{
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        loadConfig(GameUtil::PATH() + "config/chess_balance.yaml");
    }
    return config_;
}

namespace
{

struct BalanceEntry
{
    int roleId;
    int level;
    int maxHP;
    int attack;
    int speed;
    int defence;
    int maxMP;
    // Slot 0 = weak skill (MP < 100), Slot 1 = strong skill (MP = 100)
    int skill0Id, skill0Lvl;
    int skill1Id, skill1Lvl;
    // Weapon proficiency: Fist, Sword, Knife, Unusual, HiddenWeapon
    int fist, sword, knife, unusual, hiddenWeapon;
};

// Tighter bands so star scaling (+80% HP/Atk, +50% Def per star) works:
// 3★ T0 should be able to beat 1★ T4
//
// Tier 0 (cost 1): HP 250-400, Atk 50-70,  Spd 30-55, Def 35-55, MP 400-700
// Tier 1 (cost 2): HP 380-520, Atk 65-90,  Spd 40-65, Def 50-70, MP 700-1200
// Tier 2 (cost 3): HP 500-680, Atk 85-115, Spd 50-80, Def 65-90, MP 1200-2500
// Tier 3 (cost 4): HP 650-850, Atk 105-145,Spd 60-100,Def 80-120,MP 2500-4000
// Tier 4 (cost 5): HP 800-999, Atk 130-180,Spd 80-120,Def 100-150,MP 4000-5000

// clang-format off
const std::array balanceData = std::to_array<BalanceEntry>({
    // ===== TIER 0 (cost 1) =====
    // roleId, lvl, HP, Atk, Spd, Def, MP, weak{id,lvl}, strong{id,lvl}, Fist,Sword,Knife,Unusual,Hidden

    // 柯鎮惡 - tanky brawler, slow. 陰陽倒亂刃(77面)+鷹爪功(4单)
    { 130, 8, 380, 60, 30, 55, 600,   77, 100,   4, 700,   30, 10, 35, 15, 20 },
    // 朱聰 - balanced swordsman. 松風劍法(27线)+滅劍絕劍(41线)
    { 131, 7, 300, 55, 45, 45, 550,   27, 700,  41, 600,   10, 38, 10, 15, 20 },
    // 韓寶駒 - speedy whip. 中平槍法(70线)+黃沙萬里鞭(78线)
    { 132, 7, 280, 52, 55, 42, 550,   70, 200,  78, 600,   15, 10, 35, 20, 10 },
    // 南希仁 - balanced special. 楊家槍法(68线)+棋盤招式(72线)
    { 133, 7, 320, 55, 42, 50, 550,   68, 400,  72, 600,   15, 10, 30, 30, 10 },
    // 張阿生 - glass cannon. 海叟釣法(76线)+鶴蛇八打(74单)
    { 134, 7, 260, 65, 38, 38, 500,   76, 100,  74, 700,   35, 10, 10, 25, 10 },
    // 全金發 - balanced swordsman. 雷震劍法(28线)+滅劍絕劍(41线)
    { 135, 6, 290, 52, 42, 48, 500,   28, 400,  41, 600,   10, 35, 10, 15, 15 },
    // 韓小瑩 - fast, fragile sword. 松風劍法(27线)+三分劍術(29线)
    { 136, 6, 250, 50, 55, 35, 500,   27, 800,  29, 800,   10, 40, 10, 10, 10 },
    // 程英 - fast support sword. 松風劍法(27线)+玉簫劍法(38线)
    {  63, 6, 250, 52, 50, 38, 500,   27, 700,  38, 600,   10, 35, 10, 20, 10 },
    // 霍都 - tanky AoE. 白駝雪山掌(9单)+龍象般若功(103大)
    {  84, 8, 370, 58, 38, 55, 650,    9, 100, 103, 600,   35, 10, 10, 20, 15 },
    // 達爾巴 - strong brawler AoE. 天山六陽掌(8单)+無上大力杵(83大)
    { 160, 8, 400, 65, 32, 52, 650,    8, 100,  83, 600,   38, 10, 10, 15, 10 },
    // 李莫愁 - poison, balanced. 五毒神掌(3单)+玄冥神掌(21单)
    { 161, 7, 310, 58, 45, 45, 600,    3, 200,  21, 500,   35, 10, 10, 15, 20 },
    // 薛慕華 - healer, weak. 羅漢拳(1单)+逍遙游(2线)
    {  45, 6, 270, 48, 42, 40, 500,    1, 800,   2, 700,   30, 10, 10, 25, 10 },
    // 阿紫 - fast, poison. 寒冰綿掌(5单)+玄冥神掌(21单)
    {  47, 6, 250, 52, 55, 35, 450,    5, 200,  21, 500,   30, 10, 10, 15, 25 },
    // 阿朱 - support. 毒龍鞭法(69单)+綿掌(7单)
    { 104, 5, 260, 48, 48, 38, 400,   69, 500,   7, 700,   25, 10, 10, 20, 15 },
    // 阿碧 - support. 毒龍鞭法(69单)+綿掌(7单)
    { 105, 5, 260, 50, 50, 36, 420,   69, 500,   7, 700,   25, 10, 10, 20, 15 },

    // ===== TIER 1 (cost 2) =====
    // 袁承志 - fast dual. 混元功(90大)+金蛇劍法(40面)
    {  54, 12, 440, 75, 65, 55, 1000,  90, 300,  40, 600,   35, 50, 20, 15, 15 },
    // 狄云 - tank AoE. 混元功(90大)+神照功(94大)
    {  37, 11, 500, 70, 45, 70, 900,   90, 300,  94, 700,   45, 20, 15, 30, 10 },
    // 血刀老祖 - glass cannon. 混元功(90大)+血刀大法(63线)
    {  97, 12, 400, 88, 50, 52, 1100,  90, 200,  63, 600,   20, 15, 55, 15, 10 },
    // 黃蓉 - balanced dual. 落英神劍掌(12面)+打狗棒法(80面)
    {  56, 12, 450, 78, 60, 58, 1000,  12, 200,  80, 700,   40, 20, 15, 35, 15 },
    // 岳老三 - tanky brawler. 中平槍法(70线)+大剪刀(75线)
    {  44, 11, 490, 72, 42, 68, 850,   70, 500,  75, 800,   20, 15, 50, 20, 10 },
    // 游坦之 - high HP. 寒冰綿掌(5单)+玄冥神掌(21单)
    {  48, 12, 520, 80, 42, 62, 1100,   5, 400,  21, 700,   50, 15, 10, 20, 15 },
    // 葉二娘 - balanced sword. 雪山劍法(35面)+萬岳朝宗(33线)
    {  99, 10, 440, 74, 55, 58, 900,   35, 500,  33, 800,   15, 50, 15, 20, 10 },
    // 云中鶴 - fastest, fragile. 羅漢拳(1单)+寒冰綿掌(5单)
    { 100, 10, 380, 68, 65, 50, 800,    1, 900,   5, 800,   45, 15, 10, 15, 20 },
    // 枯榮 - tanky monk. 羅漢拳(1单)+一陽指(17线)
    { 102, 10, 480, 68, 40, 70, 800,    1, 900,  17, 700,   50, 10, 10, 25, 10 },
    // 蘇星河 - balanced. 持瑤琴(73大)+混元功(90大)
    { 115, 11, 420, 76, 50, 60, 950,   73, 300,  90, 800,   35, 15, 15, 40, 15 },

    // ===== TIER 2 (cost 3) =====
    // 郭靖 - balanced powerhouse. 空明拳(15单)+降龍十八掌(26单)
    {  55, 16, 650, 110, 55, 85, 2200,  15, 400,  26, 800,   75, 20, 15, 20, 10 },
    // 裘千仞 - fast glass cannon. 鷹爪功(4单)+鐵掌(13单)
    {  67, 17, 540, 115, 75, 70, 1800,   4, 300,  13, 900,   70, 10, 10, 25, 15 },
    // 丘處機 - balanced swordsman. 萬花劍法(30大)+七星劍法(39线)
    {  68, 16, 580, 100, 65, 82, 1600,  30, 500,  39, 800,   20, 70, 15, 20, 10 },
    // 小龍女 - fast evasive. 七星劍法(39线)+玉女素心劍(42线)
    {  59, 16, 520, 95, 80, 72, 1500,  39, 400,  42, 800,   15, 75, 10, 20, 10 },
    // 丁春秋 - high atk poison. 五毒神掌(3单)+玄冥神掌(21单)
    {  46, 17, 560, 112, 68, 78, 2000,   3, 800,  21, 800,   65, 10, 10, 30, 25 },
    // 慕容復 - balanced. 兩儀劍法(37面)+斗轉星移(43线)
    {  51, 15, 560, 98, 65, 82, 1400,  37, 500,  43, 800,   25, 65, 15, 30, 10 },
    // 段譽 - glass cannon ranged. 一陽指(17线)+六脈神劍(49线)
    {  53, 16, 500, 108, 75, 68, 2500,  17, 400,  49, 800,   55, 15, 10, 40, 15 },
    // 玄慈 - pure tank. 鐵掌(13单)+大金剛掌(22单)
    {  70, 17, 680, 95, 52, 90, 2000,  13, 500,  22, 800,   70, 10, 10, 25, 10 },
    // 段延慶 - balanced. 混元功(90大)+一陽指(17线)
    {  98, 15, 580, 102, 62, 82, 1800,  90, 500,  17, 800,   60, 15, 15, 35, 15 },
    // 鳩摩智 - high atk AoE. 火焰刀法(66线)+龍象般若功(103大)
    { 103, 16, 600, 112, 58, 85, 2200,  66, 300, 103, 800,   55, 10, 50, 25, 10 },
    // 蕭遠山 - tanky AoE. 鷹爪功(4单)+易筋神功(108大)
    { 112, 16, 660, 105, 55, 88, 2000,   4, 400, 108, 800,   70, 15, 10, 25, 10 },
    // 慕容博 - balanced. 鷹爪功(4单)+易筋神功(108大)
    { 113, 16, 620, 100, 55, 85, 1800,   4, 400, 108, 800,   65, 15, 10, 30, 10 },

    // ===== TIER 3 (cost 4) =====
    // 黃藥師 - high atk, fast. 落英神劍掌(12面)+彈指神通(18单)
    {  57, 22, 780, 140, 95, 100, 3500,  12, 400,  18, 900,   70, 30, 15, 80, 20 },
    // 歐陽鋒 - tanky AoE. 白駝雪山掌(9单)+蛤蟆功(95大)
    {  60, 22, 830, 125, 72, 118, 3500,   9, 500,  95, 900,   90, 15, 10, 30, 20 },
    // 周伯通 - fast balanced. 逍遙游(2线)+空明拳(15单)
    {  64, 22, 750, 135, 100, 105, 3500,   2, 500,  15, 900,   90, 20, 10, 25, 15 },
    // 一燈 - tank/support. 大金剛掌(22单)+一陽指(17线)
    {  65, 22, 850, 120, 68, 120, 3500,  22, 200,  17, 900,   85, 15, 10, 30, 10 },
    // 洪七公 - balanced powerhouse. 打狗棒法(80面)+降龍十八掌(26单)
    {  69, 22, 830, 140, 78, 115, 3800,  80, 400,  26, 900,   90, 15, 15, 25, 10 },
    // 楊過 - glass cannon. 玄鐵劍法(45面)+黯然銷魂掌(25单)
    {  58, 20, 700, 145, 90, 90, 3200,  45, 400,  25, 900,   75, 80, 15, 20, 10 },
    // 金輪法王 - tanky AoE. 鐵掌(13单)+龍象般若功(103大)
    {  62, 22, 840, 138, 72, 118, 3800,  13, 500, 103, 900,   85, 10, 25, 30, 10 },
    // 虛竹 - balanced high MP. 天山折梅手(14线)+小無相功(98大)
    {  49, 21, 780, 130, 85, 108, 4000,  14, 400,  98, 900,   80, 20, 10, 35, 15 },
    // 喬峰 - highest atk. 鐵掌(13单)+降龍十八掌(26单)
    {  50, 23, 820, 145, 82, 110, 3800,  13, 500,  26, 900,   95, 15, 10, 20, 10 },
    // 天山童姥 - fast, fragile. 天山六陽掌(8单)+天山折梅手(14线)
    { 117, 21, 680, 135, 100, 85, 3200,   8, 500,  14, 900,   80, 15, 10, 40, 20 },
    // 李秋水 - balanced dual. 天山折梅手(14线)+小無相功(98大)
    { 118, 21, 760, 130, 82, 105, 3500,  14, 400,  98, 900,   75, 20, 10, 40, 15 },

    // ===== TIER 4 (cost 5) =====
    // 王重陽 - balanced god. 一陽指(17线)+先天功(100大)
    { 129, 25, 920, 165, 100, 140, 5000,  17, 500, 100, 999,   95, 30, 20, 50, 20 },
    // 掃地老僧 - ultimate tank. 大金剛掌(22单)+易筋神功(108大)
    { 114, 25, 999, 155, 85, 150, 5000,  22, 500, 108, 999,  100, 20, 15, 40, 15 },
    // 無崖子 - fast glass cannon. 天山折梅手(14线)+小無相功(98大)
    { 116, 25, 850, 180, 120, 110, 5000,  14, 400,  98, 999,   90, 25, 15, 55, 20 },
});
// clang-format on

}    // namespace

void ChessBalance::apply()
{
    if (applied_) return;

    auto save = Save::getInstance();
    for (const auto& e : balanceData)
    {
        auto role = save->getRole(e.roleId);
        if (!role) continue;

        role->Level = e.level;
        role->MaxHP = e.maxHP;
        role->HP = e.maxHP;
        role->Attack = e.attack;
        role->Speed = e.speed;
        role->Defence = e.defence;
        role->MaxMP = e.maxMP;
        role->MP = e.maxMP;

        // Set exactly 2 skills, clear the rest
        role->MagicID[0] = e.skill0Id;
        role->MagicLevel[0] = e.skill0Lvl;
        role->MagicID[1] = e.skill1Id;
        role->MagicLevel[1] = e.skill1Lvl;
        role->Fist = e.fist;
        role->Sword = e.sword;
        role->Knife = e.knife;
        role->Unusual = e.unusual;
        role->HiddenWeapon = e.hiddenWeapon;
        for (int i = 2; i < ROLE_MAGIC_COUNT; i++)
        {
            role->MagicID[i] = 0;
            role->MagicLevel[i] = 0;
        }
    }

    applied_ = true;
}

}    // namespace KysChess
