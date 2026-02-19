#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

struct Role;

namespace KysChess
{

struct Chess;

enum class ComboId
{
    JiangNanQiGuai,     // 1. 江南七怪
    ShenDiaoXiaLv,      // 2. 神雕侠侣
    GuSuMuRong,         // 3. 姑苏慕容
    TianLongSanXiongDi, // 4. 天龙三兄弟
    WuJue,              // 5. 五绝
    XingXuXiaoYao,      // 6. 星宿逍遥
    DaLiDuanShi,        // 7. 大理段氏
    SiDaEren,           // 8. 四大恶人
    TaoHuaDao,          // 9. 桃花岛
    ShaoLinSi,          // 10. 少林寺
    QuanZhenJiao,       // 11. 全真教
    MengGuShuangXiong,  // 12. 蒙古双雄
    LianChengJue,       // 13. 连城诀
    DuZong,             // 14. 毒宗
    BeiMingShenGong,    // 15. 北冥神功
    SheDiaoYingXiong,   // 16. 射雕英雄
    WuLinWaiDao,        // 17. 武林外道
    JianKe,             // 18. 剑客
    QuanShi,            // 19. 拳师
    DaoKe,              // 20. 刀客
    HongYan,            // 21. 红颜
    QinQiShuHua,        // 22. 琴棋书画
    DuXing,             // 23. 独行
    COUNT
};

enum class Trigger
{
    Always,         // passive, always active
    WhileLowHP,     // self: active while own HP < trigger_value%
    AllyLowHPBurst, // cross-role: triggers on ally when own HP < trigger_value%, lasts value2 frames
};

enum class EffectType
{
    // Stat buffs (pre-battle)
    FlatHP, FlatATK, FlatDEF, FlatSPD,
    PctHP, PctATK, PctDEF, NegPctDEF,

    // Trigger effects (runtime)
    FlatDmgReduction,
    BlockChance,
    DodgeChance,
    DodgeThenCrit,
    CritChance,
    CritMultiplier,
    EveryNthDouble,
    ArmorPenChance,
    ArmorPenPct,
    StunChance,     // value=chance%, value2=frames
    KnockbackChance,
    PoisonDOT,      // value=dot%, value2=duration frames
    PoisonDmgAmp,
    MPOnHit,
    MPDrain,
    MPRecoveryBonus,
    SkillDmgPct,
    SkillReflectPct,
    CDR,
    ShieldPctMaxHP,
    ShieldCDR,
    HealAuraPct,    // value=heal%, value2=interval frames
    HealedATKSPDBoost,
    HPRegenPct,     // value=regen%, value2=interval frames
    FreezeReductionPct,
    ControlImmunityFrames,
    KillHealPct,
    KillInvincFrames,
    PostSkillInvincFrames,
    DmgReductionPct,
};

struct ComboEffect
{
    EffectType type;
    int value;
    int value2 = 0;       // paired parameter (duration, interval, etc.)
    Trigger trigger = Trigger::Always;
    int trigger_value = 0; // trigger parameter (e.g. HP threshold %)
};

struct ComboThreshold
{
    int count;
    std::string name;
    std::vector<ComboEffect> effects;
};

struct ComboDef
{
    ComboId id;
    std::string name;
    std::vector<int> memberRoleIds;
    std::vector<ComboThreshold> thresholds;
    bool isAntiCombo = false;
};

struct ActiveCombo
{
    ComboId id;
    int memberCount = 0;
    int activeThresholdIdx = -1;
    std::set<int> memberRoleIds;
};

struct RoleComboState
{
    // Stat buffs
    int flatHP = 0, flatATK = 0, flatDEF = 0, flatSPD = 0;
    double pctHP = 0, pctATK = 0, pctDEF = 0;

    // Trigger values
    int flatDmgReduction = 0;
    int blockChancePct = 0;
    int dodgeChancePct = 0;
    bool dodgeThenCrit = false;
    int critChancePct = 0;
    int critMultiplier = 100;
    int everyNthDouble = 0;
    int armorPenChancePct = 0;
    int armorPenPct = 0;
    int stunChancePct = 0;
    int stunFrames = 0;
    int knockbackChancePct = 0;
    int poisonDOTPct = 0;
    int poisonDuration = 0;
    int poisonDmgAmpPct = 0;
    int mpOnHit = 0;
    int mpDrain = 0;
    int mpRecoveryBonusPct = 0;
    int skillDmgPct = 0;
    int skillReflectPct = 0;
    int cdrPct = 0;
    int shieldPctMaxHP = 0;
    int shieldCDRPct = 0;
    int healAuraPct = 0;
    int healAuraInterval = 0;
    int healedATKSPDBoostPct = 0;
    int hpRegenPct = 0;
    int hpRegenInterval = 0;
    int lowHPThresholdPct = 0;
    int lowHPAtkPct = 0;
    int lowHPSpdFlat = 0;
    int berserkATKPct = 0;
    int berserkDuration = 0;
    int freezeReductionPct = 0;
    int controlImmunityFrames = 0;
    int killHealPct = 0;
    int killInvincFrames = 0;
    int postSkillInvincFrames = 0;
    int dmgReductionPct = 0;

    // Mutable runtime state
    int hitCounter = 0;
    bool dodgedLast = false;
    int shield = 0;
    int poisonTimer = 0;
    int poisonTickDmg = 0;
    int berserkTimer = 0;
};

class ChessCombo
{
public:
    static bool loadFromYaml(const std::string& path);
    static const std::vector<ComboDef>& getAllCombos();
    static std::vector<ActiveCombo> detectCombos(const std::vector<Chess>& selected);
    static std::map<int, RoleComboState> buildComboStates(const std::vector<ActiveCombo>& active);
    static void applyStatBuffs(const std::map<int, RoleComboState>& states);
    static const std::map<int, RoleComboState>& getActiveStates();
    static std::map<int, RoleComboState>& getMutableStates();
    static void clearActiveStates();
    static std::vector<ComboId> getCombosForRole(int roleId);

private:
    static std::vector<ComboDef> allCombos_;
    static std::map<int, RoleComboState> activeStates_;
};

}  // namespace KysChess
