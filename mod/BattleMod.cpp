#include "BattleMod.h"
#include "GameUtil.h"
#include "Save.h"
#include "Font.h"
#include "libconvert.h"
#include "PotConv.h"
#include "Event.h"
#include "DrawableOnCall.h"
#include "TeamMenu.h"
#include "BattleConfig.h"

#include <numeric>
#include <cassert>
#include <unordered_set>

using namespace BattleMod;


BattleModifier::BattleModifier() {
    printf("this is modified battle scene.\n");
    // 强制半即时
    semi_real_ = true;
}

void BattleModifier::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->RealID == -1)
        r->RealID = r->ID;
    printf("ID %d realID %d Team %d\n", r->ID, r->RealID, r->Team);
	// TODO 修复这个愚蠢的问题
	conf.battleStatusManager[r->ID].initStatus(r, &conf.battleStatus);
}


void BattleMod::BattleModifier::actUseMagicSub(Role * r, Magic * magic)
{
    // 每次攻击，攻击者的攻击抬手..吃屎
    std::vector<decltype(atkNames_)> atk_namess;
    std::vector<Role::ActionShowInfo> atk_shows;
    // 每次攻击，每个人的防御动画数据
    std::vector<std::vector<Role::ActionShowInfo>> defence_shows;
    // 每次攻击，每个人的文字动画数据
    std::vector<std::vector<Role::ActionShowInfo>> number_shows;

    // 连击的判断我要改的
    multiAtk_ = 0;
    for (int i = 0; i <= multiAtk_; i++)
    {
        int level_index = r->getMagicLevelIndex(magic->ID);
        //计算伤害
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);
        calMagiclHurtAllEnemies(r, magic);

        // 这里动画非常复杂
        // 首先，攻击者的抬手动画就是由showNumberAnimation完成，所以攻击者本身必须有一层show
        // 然后攻击完毕，每非攻击者的受伤害人有一层防御特效
        // 防御特效结束后是伤害显示和渐变

        atk_namess.push_back(atkNames_);
        atkNames_.clear();

        r->Show.Effect = 10;
        r->addShowString("废物");
        // 攻击者也可以集气后退，伤血，所以这里仅清除Effect和文字
        atk_shows.push_back(r->Show);
        r->Show.clearDisplayOnly();
        
        defence_shows.emplace_back();
        for (auto r2 : battle_roles_) 
        {
            if (r2->Show.BattleHurt != 0) {
                r2->Show.Effect = 6;
                r2->addShowString("啊机器猫技术帝", { 255, 20, 20, 255 });
                r2->addShowString("劳资无敌", { 160, 32, 240, 255 });
                r2->addShowString("废物");
            }
            // 同一半理，因为这里使用集气变化，但是留着也无所谓
            defence_shows.back().push_back(r2->Show);
            r2->Show.clearDisplayOnly();
        }
        
        number_shows.emplace_back();
        for (auto r2 : battle_roles_)
        {
            if (r2->Show.BattleHurt != 0)
            {
                if (magic->HurtType == 0)
                {
                    r2->addShowString(convert::formatString("-%d", r2->Show.BattleHurt), { 255, 20, 20, 255 });
                }
                else if (magic->HurtType == 1)
                {
                    r2->addShowString(convert::formatString("-%d", r2->Show.BattleHurt), { 160, 32, 240, 255 });
                    // 吸内力不做渐变显示，麻烦
                    r2->Show.BattleHurt = 0;
                }
            }
            // 其他显示的特效
            auto result = conf.battleStatusManager[r2->ID].materialize();
            for (auto const& p : result) 
            {
                if (!p.first.hide) 
                {
                    r2->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
                }
            }
            number_shows.back().push_back(r2->Show);
            r2->Show.clear();
        }

        //武学等级增加
        auto index = r->getMagicOfRoleIndex(magic);
        if (index >= 0)
        {
            r->MagicLevel[index] += 1 + rng_.rand_int(2);
            GameUtil::limit2(r->MagicLevel[index], 0, MAX_MAGIC_LEVEL);
        }
        printf("%s %s level is %d\n", PotConv::to_read(r->Name).c_str(), PotConv::to_read(magic->Name).c_str(), r->MagicLevel[index]);
    }
    r->Acted = 1;

    // 需要复制，因为已经离开此栈
    actionAnimation_ = [this, r, magic, atk_namess, atk_shows, defence_shows, number_shows]() mutable
    {
        for (int i = 0; i < atk_shows.size(); i++)
        {
            // 播放攻击画面
            showMagicNames(atk_namess[i]);
            r->Show = atk_shows[i];
            useMagicAnimation(r, magic);

            // 先护体，不显示伤害，护体阶段显示集气条变化
            std::vector<std::pair<int&, int>> progress_changes;
            for (int j = 0; j < defence_shows[i].size(); j++)
            {
                // 读取保存的文字动画数据
                battle_roles_[j]->Show = defence_shows[i][j];
                progress_changes.emplace_back(battle_roles_[j]->Progress, battle_roles_[j]->Show.ProgressChange);
            }
            showNumberAnimation(2, false);

            // 渐变效果，生命值
            std::vector<std::pair<int&, int>> animated_changes;
            for (int j = 0; j < number_shows[i].size(); j++)
            {
                // 读取保存的文字动画数据
                battle_roles_[j]->Show = number_shows[i][j];
                // 绑定，生命值和伤害值
                animated_changes.emplace_back(battle_roles_[j]->HP, -battle_roles_[j]->Show.BattleHurt);
                // 可绑定其他玩意儿，不过麻烦
            }
            showNumberAnimation(2, true, animated_changes);
            for (auto r2 : battle_roles_)
            {
                r2->clearShowStrings();
            }
        }
    };
}

//r1使用武功magic攻击r2的伤害，结果为一正数
int BattleModifier::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    // simulation的话绕过去，不用
    if (simulation_)
        return BattleScene::calMagicHurt(r1, r2, magic);

    assert(defEffectManager_.size() == 0);
    
    // 这里考虑怎么加入显示效果
    // 先加入防守方特效，注意r2先来，其实这里传递magic也行，效果不明就是的
	conf.tryProcAndAddToManager(r2->RealID, conf.defRole, conf.defEffectManager, r2, r1, nullptr);
    
    // 再加入挨打者的武功被动特效
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
		conf.tryProcAndAddToManager(r2->MagicID[i], conf.defMagic, conf.defEffectManager, r2, r1, Save::getInstance()->getMagic(r2->MagicID[i]));
    }

	conf.tryProcAndAddToManager(conf.defAll, conf.defEffectManager, r2, r1, magic);

    // 强制特效直接判断
    // 特效8 9
    // 8 攻击方的敌人的
    conf.setStatusFromParams(conf.atkEffectManager.getAllEffectParams(8), r2);
    // 9 攻击方自己
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(9), r1);
    // 然后倒过来 8 防御方的敌人
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(8), r1);
    // 9 防御方自己
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(9), r2);

    // 集气伤害，正数代表集气条后退
    int progressHurt = 0;

    // 特效1 气攻
    progressHurt += conf.atkEffectManager.getEffectParam0(1);
    // 特效2 护体
    progressHurt -= conf.defEffectManager.getEffectParam0(2);


    int hurt = 0;
    int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(r1, magic);
    level_index = magic->calMaxLevelIndexByMP(r1->MP, level_index);
    if (magic->HurtType == 0)
    {
        if (r1->MP <= 10)
        {
            return 1 + rng_.rand_int(10);
        }

        // 特效0 武功威力增加，其实防御方也可以
        int attack = r1->Attack + (magic->Attack[level_index]+ conf.atkEffectManager.getEffectParam0(0)) / 4;

        int defence = r2->Defence;

        //装备的效果
        if (r1->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip0);
            attack += i->AddAttack;
        }
        if (r1->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip1);
            attack += i->AddAttack;
        }
        if (r2->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip0);
            defence += i->AddDefence;
        }
        if (r2->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip1);
            defence += i->AddDefence;
        }

        // 伤害太高，减少一点
        int v = attack - defence * 2;

        //距离衰减
        //这个我应该也接手了..
        //思考，怎么改，怎么把calRoleDistance搞成一个变量，修改变量函数指针签名了只有
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rng_.rand_int(10) - rng_.rand_int(10);

        // 特效4 暴击
        double amplify = 100;
        amplify += conf.atkEffectManager.getEffectParam0(4);
        amplify += conf.defEffectManager.getEffectParam0(4);
        v = int(v * (amplify / 100));

        // 特效5 直接加伤害
        v += conf.atkEffectManager.getEffectParam0(5) + conf.defEffectManager.getEffectParam0(5);

        // 这个不管
        if (v < 10)
        {
            v = 1 + rng_.rand_int(10);
        }
        hurt = v;
    }
    else if (magic->HurtType == 1)
    {
        int v = magic->HurtMP[level_index];
        v += rng_.rand_int(10) - rng_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rng_.rand_int(10);
        }
        hurt = v;
    }

    // 特效12 防御方加减集气（无视气防 气攻）
    r2->Show.ProgressChange += conf.defEffectManager.getEffectParam0(12);

    // 特效6，特效7 上状态
    // 己方视角
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(6), r2);
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(7), r1);
    // 然后倒过来
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(7), r2);
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(6), r1);

    // 不增加，只护体
    if (progressHurt > 0) {
        r2->Show.ProgressChange -= progressHurt;
        printf("集气变化%d\n", r2->Show.ProgressChange);
    }
    conf.defEffectManager.clear();
    return hurt;
}

int BattleModifier::calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation)
{
    // simulation我就假装不知道，嘿嘿
    // 这里原代码不重构一下，难搞
    // 我忘了BattleScene是会用谁的calMagicHurt
    // 应该还是override后的，加个simulation_绕过去。。
    if (simulation) {
        simulation_ = true;
        return BattleScene::calMagiclHurtAllEnemies(r, m, true);
    }
    else {
        simulation_ = false;
    }

    assert(conf.atkEffectManager.size() == 0);

    // 触发，被动攻击特效，这里被动也是每次攻击再判断一次
    // 我忘了为什么把被动攻击效果给取消了。。。哦，好像是添加到全人物攻击里面按需求判断了
    /*
    for (int i = 0; i < r->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r->MagicID[i], atkMagic_, atkEffectManager_);
    }
    */

    std::string mName(m->Name);
    atkNames_.push_back(mName);
    // 抬手时，触发攻击特效
    auto effects = conf.tryProcAndAddToManager(m->ID, conf.atkMagic, conf.atkEffectManager, r, nullptr, m);
    // 简单的做个显示~ TODO 修好它
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            atkNames_.push_back(std::cref(effect.description));
    }

    // 再添加人物自身的攻击特效
    effects = conf.tryProcAndAddToManager(r->RealID, conf.atkRole, conf.atkEffectManager, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            atkNames_.push_back(std::cref(effect.description));
    }

    // 所有人物的
    effects = conf.tryProcAndAddToManager(conf.atkAll, conf.atkEffectManager, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            atkNames_.push_back(std::cref(effect.description));
    }

    // 特效11 连击, 不控制就是无限连
    multiAtk_ += conf.atkEffectManager.getEffectParam0(11);

    int total = 0;
	int targets = 0;
    for (auto r2 : battle_roles_)
    {
        //非我方且被击中（即所在位置的效果层非负）
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            r2->Show.BattleHurt = calMagicHurt(r, r2, m);
            if (m->HurtType == 0)
            {
                // r2->addShowString(convert::formatString("-%d", hurt), { 255, 20, 20, 255 }, 30);
                int prevHP = r2->HP;
                r2->Show.BattleHurt = GameUtil::limit(r2->Show.BattleHurt, -(r2->MaxHP - r2->HP), r2->HP);
                // r2->HP = GameUtil::limit(r2->HP - r2->BattleHurt, 0, r2->MaxHP);
                int hurt1 = prevHP - r2->HP;
                r->ExpGot += hurt1;
                if (r2->HP <= 0)
                {
                    r->ExpGot += hurt1 / 2;
                }
                // r2->ProgressChange -= -hurt / 5;
            }
            else if (m->HurtType == 1)
            {
                // r2->addShowString(convert::formatString("-%d", hurt), { 160, 32, 240, 255 }, 30);
                int prevMP = r2->MP;
                r2->MP = GameUtil::limit(r2->MP - r2->Show.BattleHurt, 0, r2->MaxMP);
                r->MP = GameUtil::limit(r->MP + r2->Show.BattleHurt, 0, r->MaxMP);
                int hurt1 = prevMP - r2->MP;
                r->ExpGot += hurt1 / 2;
            }
			if (r2->HP > 0)
			{
				targets += 1;
			}
        }
    }
	// 没打到人，终止攻击
	if (targets == 0)
	{
		multiAtk_ = 0;
	}
    
    // 我需要好好想一想显示效果怎么做
    // 特效12 加集气
    r->Show.ProgressChange += conf.atkEffectManager.getEffectParam0(12);

    // 打完了，清空攻击效果
    conf.atkEffectManager.clear();
    return total;
}

void BattleModifier::dealEvent(BP_Event& e)
{
    if (battle_roles_.empty())
    {
        exitWithResult(0);
    }

    Role* role = nullptr;
    if (semi_real_ == 0)
    {
        //选择位于人物数组中的第一个人
        role = battle_roles_[0];

        //若第一个人已经行动过，说明所有人都行动了，则清除行动状态，重排人物
        if (role->Acted != 0)
        {
            resetRolesAct();
            sortRoles();
            role = battle_roles_[0];
        }
    }
    else
    {
        role = semiRealPickOrTick();
        if (role == nullptr) return;
    }

    acting_role_ = role;

    //定位
    man_x_ = role->X();
    man_y_ = role->Y();
    select_x_ = role->X();
    select_y_ = role->Y();
    head_self_->setRole(role);
    head_self_->setState(Element::Pass);

    //行动
    action(role);

    //如果此人成功行动过，则放到队尾
    if (role->Acted)
    {
        // 行动后在这里操作
        // 武功被动行动特效
        for (int i = 0; i < role->getLearnedMagicCount(); i++) {
            conf.tryProcAndAddToManager(role->MagicID[i], conf.turnMagic, conf.turnEffectManager, role, nullptr, Save::getInstance()->getMagic(role->MagicID[i]));
        }
        conf.tryProcAndAddToManager(role->RealID, conf.turnRole, conf.turnEffectManager, role, nullptr, nullptr);
        conf.tryProcAndAddToManager(conf.turnAll, conf.turnEffectManager, role, nullptr, nullptr);

        // 特效9 强制
        conf.setStatusFromParams(conf.turnEffectManager.getAllEffectParams(9), role);

        // 特效7 上状态
        conf.applyStatusFromParams(conf.turnEffectManager.getAllEffectParams(7), role);

        // 特效12 加集气
        role->Show.ProgressChange += conf.turnEffectManager.getEffectParam0(12);

        if (semi_real_ == 0)
        {
            battle_roles_.erase(battle_roles_.begin());
            battle_roles_.push_back(role);
        }
        else
        {
            role->Progress -= 1000;
            resetRolesAct();
        }

        // 中毒？ 开玩笑，我这里理论上特效全权接管了，有你什么事
        // 算了.. TODO 接管
        poisonEffect(role);

        // 这里无所谓了
        auto const& result = conf.battleStatusManager[role->ID].materialize();
        for (auto const& p : result) {
            printf("%d自己上状态%s %d %d\n", role->ID, p.first.display.c_str(), p.first.id, p.second);
            if (!p.first.hide)
                role->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
        }
        showNumberAnimation();
        role->clearShowStrings();
        // 结算回合结束后的一些特效，比如说回血内力 等一系列
        // 等我有心情再做

        conf.turnEffectManager.clear();
    }

    //清除被击退的人物
    clearDead();

    //检测战斗结果
    int battle_result = checkResult();

    //我方胜
    if (battle_result >= 0)
    {
        result_ = battle_result;
        if (result_ == 0 || result_ == 1 && fail_exp_)
        {
            calExpGot();
        }
        setExit(true);
    }
}

void BattleMod::BattleModifier::setupRolePosition(Role * r, int team, int x, int y)
{
    if (r == nullptr) return;
    r->setPosition(x, y);
    r->Team = team;
    readFightFrame(r);
    r->FaceTowards = rng_.rand_int(4);
    battle_roles_.push_back(r);
}

// 集气在这里
Role* BattleModifier::semiRealPickOrTick() {
    //首先试图选出一人
    for (auto r : battle_roles_)
    {
        if (r->Progress > 1000)
        {
            return r;
        }
    }
    
    //无法选出人，增加所有人进度条，继续
    for (auto r : battle_roles_)
    {
        // 这里是个什么排序来着
        
        assert(speedEffectManager_.size() == 0);

        // 先是全部效果
        conf.tryProcAndAddToManager(conf.speedAll, conf.speedEffectManager, r, nullptr, nullptr);
        // 自己的效果
        conf.tryProcAndAddToManager(r->RealID, conf.speedRole, conf.speedEffectManager, r, nullptr, nullptr);
        // 自己武功的效果，每次集气这么算一遍 累趴了
        for (int i = 0; i < r->getLearnedMagicCount(); i++) {
            conf.tryProcAndAddToManager(r->MagicID[i], conf.speedMagic, conf.speedEffectManager, r, nullptr, Save::getInstance()->getMagic(r->MagicID[i]));
        }
        // 强制状态 特效9
        auto forceStat = conf.speedEffectManager.getAllEffectParams(9);
        conf.setStatusFromParams(forceStat, r);

        int progress = 0;
        
        // 先上/下状态，给自己，给别人也不是不行，再搞个循环，判断不在一个队伍的
        // 特效7
        conf.applyStatusFromParams(conf.speedEffectManager.getAllEffectParams(7), r);

        // 特效3 集气速度
        progress = conf.speedEffectManager.getEffectParam0(3);
        // printf("基础集气速度%d\n", progress);
        // printf("%d集气加成 %d%%\n", r->ID, speedEffectManager_.getEffectParam0(10));
        // 特效10 集气速度百分比增加
        progress = int((conf.speedEffectManager.getEffectParam0(10) / 100.0) * progress) + progress;

        // 允许逆行，手动斜眼
        // progress = progress < 0 ? 0 : progress;

        r->Progress += progress;

        conf.battleStatusManager[r->ID].materialize();
        conf.speedEffectManager.clear();
    }
    return nullptr;
}

void BattleMod::BattleModifier::showMagicNames(const std::vector<std::string>& names)
{
    // UI不好写啊，搞不出来，放弃
    if (names.empty()) return;

    std::string hugeStr = std::accumulate(std::next(names.begin()), names.end(), names[0], 
                                          [](std::string a, const std::string& b) {
                                            return a + " - " + b;
                                          });
    int fontSize = 25;
    int stayFrame = 40;
    std::unique_ptr<TextBox> boxRoot = std::make_unique<TextBox>();
    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    boxRoot->setHaveBox(false);
    // 丑的我不能直视
    boxRoot->setText(hugeStr);
    boxRoot->setPosition(w/2.0 - (hugeStr.size()/4.0) * fontSize, 50);
    boxRoot->setFontSize(fontSize);
    boxRoot->setStayFrame(stayFrame);
    boxRoot->setAlphaBox({ 255, 165, 79, 255 }, { 0, 0, 0, 128 });
    // 允许设定颜色吧。。。
    boxRoot->setTextColor({ 255, 165, 79, 255 });
    boxRoot->run();
}

void BattleMod::BattleModifier::renderExtraRoleInfo(Role * r, int x, int y)
{
    if (r == nullptr || r->HP <= 0)
    {
        return;
    }
    // 画个血条
    BP_Color outline_color = { 0, 0, 0, 128 };
    BP_Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int hp_x = x - 20;
    int hp_y = y - 63;
    int hp_max_w = 40;
    int hp_h = 3;
    double perc = ((double)r->HP / r->MaxHP);
    if (perc < 0)
    {
        perc = 0;
    }
    BP_Rect r1 = { hp_x, hp_y, perc * hp_max_w, hp_h };

    Engine::getInstance()->fillColor(background_color, hp_x, hp_y, perc * hp_max_w, hp_h);
    // 严禁吐槽，画框框
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, hp_max_w, 1);
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y + hp_h, hp_max_w, 1);
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, 1, hp_h);
    Engine::getInstance()->fillColor(outline_color, hp_x + hp_max_w, hp_y, 1, hp_h);
    int i = 4;
    for (auto& s : conf.battleStatus) {
        if (!s.hide) {
            int val = conf.battleStatusManager[r->ID].getBattleStatusVal(s.id);
            if (val == 0) continue;
            int actual_w = ((double)val / s.max) * hp_max_w;
            Engine::getInstance()->fillColor(outline_color, hp_x, hp_y + i, hp_max_w, 1);
            Engine::getInstance()->fillColor(s.color, hp_x + 1, hp_y + i, actual_w, 1);
            i += 1;
        }
    }
}


