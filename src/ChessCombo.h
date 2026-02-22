#pragma once

#include "ChessBattleEffects.h"

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
    int memberCount = 0;
    int activeThresholdIdx = -1;
    std::set<int> memberRoleIds;
};

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

private:
    static inline std::map<int, RoleComboState> activeStates_;
};

}  // namespace KysChess
