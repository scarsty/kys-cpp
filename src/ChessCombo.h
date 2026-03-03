#pragma once

#include "ChessBattleEffects.h"

#include <format>
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
    bool starSynergyBonus = false;
};

struct ActiveCombo
{
    ComboId id;
    int memberCount = 0;          // effective count (star-augmented when starSynergyBonus)
    int physicalMemberCount = 0;  // raw count of distinct heroes selected
    int activeThresholdIdx = -1;
    bool isAntiCombo = false;
    std::set<int> memberRoleIds;
};

// Returned by computeOwnership — physical owned count and star-augmented effective count.
struct ComboOwnership
{
    int owned = 0;      // distinct heroes from this combo present on the field
    int effective = 0;  // star-augmented value (== owned when starSynergyBonus is false)
};

// Compute how many heroes from `combo` are owned (present in starByRole),
// and the effective (star-augmented) count used for threshold evaluation.
// An entry in starByRole means the hero is on the field; its value is the star level.
inline ComboOwnership computeOwnership(
    const ComboDef& combo, const std::map<int, int>& starByRole)
{
    ComboOwnership result;
    for (int rid : combo.memberRoleIds)
    {
        auto it = starByRole.find(rid);
        if (it == starByRole.end()) continue;
        result.owned++;
        result.effective += combo.starSynergyBonus ? it->second : 1;
    }
    return result;
}

// Shared combo count formatter used by all display sites.
// owned        = physical heroes on field
// effective    = star-augmented count (== owned when no starSynergyBonus)
// denominator  = target to compare against (total pool or threshold count)
// Returns e.g. "3+2/7" (star synergy), "2/7" (regular), "独/1" (anti-combo)
inline std::string formatComboCount(
    int owned, int effective, int denominator,
    bool starSynergyBonus, bool isAntiCombo)
{
    if (isAntiCombo)
        return std::format("独/{}", denominator);
    int extra = effective - owned;
    if (starSynergyBonus && extra > 0)
        return std::format("{}+{}/{}", owned, extra, denominator);
    return std::format("{}/{}", owned, denominator);
}

class ChessCombo
{
public:
    static const std::vector<ComboDef>& getAllCombos();
    static std::vector<ActiveCombo> detectCombos(const std::vector<Chess>& selected);
    static std::map<int, RoleComboState> buildComboStates(const std::vector<ActiveCombo>& active);
    static void applyStatBuffs(const std::map<int, RoleComboState>& states);
    static const std::map<int, RoleComboState>& getActiveStates();
    static std::map<int, RoleComboState>& getMutableStates();
    static void clearActiveStates();
    static std::vector<ComboId> getCombosForRole(int roleId);
    // Build { roleId -> starLevel } map from a selection of Chess pieces.
    // An entry in the returned map means the hero is on the field.
    static std::map<int, int> buildStarMap(const std::vector<Chess>& selected);
    // Transfer anti-combo buff to next strongest alive member when current holder dies
    static void transferAntiCombo(int deadRoleId, const std::vector<Role*>& allRoles);

private:
    static inline std::map<int, RoleComboState> activeStates_;
};

}  // namespace KysChess
