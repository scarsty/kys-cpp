#pragma once

#include "Random.h"
#include "BattleScene.h"
#include "BattleConfig.h"
#include "BattleNetwork.h"
#include "BattleConfig.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>
#include <functional>

// 先偷懒放一起
namespace BattleMod {

class BattleModifier : public BattleScene {
    
private:
	BattleConfManager conf;
	// simulation对calMagicHurt很不友好
    bool simulation_ = false;
    // 多重攻击（别称 连击 左右互搏）
    int multiAtk_ = 0;
    // 攻击者，跨函数所需要的攻击特效名字...复制字符串吧
    std::vector<std::string> atkNames_;

public:

    // 我决定这不是一个singleton，不用重开游戏就可以测试
    BattleModifier();

	// 此非virtual，不过不重要
	void setID(int id);

    void dealEvent(BP_Event & e) override;

    // 暂且考虑修改这些函数
    virtual void setRoleInitState(Role* r) override;

    virtual void actRest(Role* r);
    virtual void actUseMagicSub(Role* r, Magic* magic) override;
    virtual void calExpGot() override;
    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //计算全部人物的伤害
    // virtual void showNumberAnimation();
    // 这个是新加的
    virtual Role* semiRealPickOrTick();
    void setupRolePosition(Role* r, int team, int x, int y);

    void showMagicNames(const std::vector<std::string>& names);

    virtual void renderExtraRoleInfo(Role* r, int x, int y);    // 在人物上，显示血条等

private:
    void addAtkAnim(Role * r, BP_Color color, const std::vector<EffectIntsPair> & eips);
	void addDefAnim(Role * r, BP_Color color, const std::vector<EffectIntsPair> & eips);
};

}