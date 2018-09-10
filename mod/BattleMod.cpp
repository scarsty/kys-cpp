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
#include "ShowExp.h"
#include "MagicEffectDrawable.h"

#include <numeric>
#include <cassert>
#include <unordered_set>

using namespace BattleMod;


BattleModifier::BattleModifier() {
    printf("this is modified battle scene.\n");
    // 强制半即时
    semi_real_ = true;
}

void BattleMod::BattleModifier::setID(int id)
{
    BattleScene::rng_.set_seed(Save::getInstance()->Seed + id);
    BattleScene::setID(id);
}

void BattleModifier::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->RealID == -1)
        r->RealID = r->ID;
    printf("ID %d realID %d Team %d\n", r->ID, r->RealID, r->Team);
    // TODO 修复这个愚蠢的问题
    conf.battleStatusManager[r->ID].initStatus(r);

    // 暫時起始僅僅允許修改狀態（强制)
    EffectManager startMang;
    conf.tryProcAndAddToManager(conf.startAll, startMang, r, nullptr, nullptr);
}

void BattleMod::BattleModifier::addAtkAnim(Role * r, BP_Color color, const std::vector<EffectIntsPair> & eips)
{
    std::string showStrs;
    for (const auto& eip : eips) {
        if (!eip.description.empty()) {
            if (eip.position == EffectDisplayPosition::on_top) {
                atkNames_.push_back(std::cref(eip.description));
            }
            else if (eip.position == EffectDisplayPosition::on_person) {
                if (showStrs.empty()) {
                    showStrs = eip.description;
                }
                else {
                    showStrs += " + " + eip.description;
                }
            }
            // 直接覆盖?
            if (r->Show.Effect == -1 && eip.getAnimation() != -1) {
                r->Show.Effect = eip.getAnimation();
            }
        }
    }
    if (!showStrs.empty()) {
        r->addShowString(showStrs, color);
    }
}

void BattleMod::BattleModifier::addDefAnim(Role * r, BP_Color color, const std::vector<EffectIntsPair>& eips)
{
    std::string showStrs;
    for (const auto& eip : eips) {
        if (!eip.description.empty() && eip.position != EffectDisplayPosition::no) {
            if (showStrs.empty()) {
                showStrs = eip.description;
            }
            else {
                showStrs += " + " + eip.description;
            }
        }
        if (r->Show.Effect == -1 && eip.getAnimation() != -1) {
            r->Show.Effect = eip.getAnimation();
        }
    }
    if (!showStrs.empty()) {
        r->addShowString(showStrs, color);
    }
}

void BattleMod::BattleModifier::actRest(Role * r)
{
    r->PhysicalPower = GameUtil::limit(r->PhysicalPower + 5, 0, Role::getMaxValue()->PhysicalPower);
    r->HP = GameUtil::limit(r->HP + 0.05 * r->MaxHP, 0, r->MaxHP);
    r->MP = GameUtil::limit(r->MP + 0.05 * r->MaxMP, 0, r->MaxMP);
    r->Acted = 1;
}

void BattleMod::BattleModifier::actUseMagic(Role * r)
{
    BattleMagicMenu magic_menu;
    Role * enemy = r;
    for (auto role : battle_roles_) {
        if (role->Team != r->Team) {
            enemy = role;
            break;
        }
    }
    MagicEffectDrawable med(conf, r, enemy, 420, 120);
    magic_menu.drawable = &med;
    while (true)
    {
        Magic* magic = nullptr;
        if (r->Networked)
        {
            magic = r->Network_Magic;
        }
        else
        {
            magic_menu.setStartItem(r->SelectedMagic);
            magic_menu.runAsRole(r);
            magic = magic_menu.getMagic();
            r->SelectedMagic = magic_menu.getResult();
        }
        if (magic == nullptr)
        {
            r->Network_Action = -1;
            break;
        }    //可能是退出游戏，或者是没有选武功
        r->ActTeam = 1;
        //level_index表示从0到9，而level从0到999
        int level_index = r->getMagicLevelIndex(magic->ID);
        calSelectLayerByMagic(r->X(), r->Y(), r->Team, magic, level_index);
        //选择目标
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r, magic, level_index);
        int selected = battle_cursor_->run();
        //取消选择目标则重新进入选武功
        if (selected < 0)
        {
            continue;
        }
        else
        {
            r->Network_ActionX = select_x_;
            r->Network_ActionY = select_y_;
            r->Network_Magic = magic;
            actUseMagicSub(r, magic);
            break;
        }
    }
}

void BattleMod::BattleModifier::actUseMagicSub(Role * r, Magic * magic)
{
    // 每次攻击，攻击者的攻击抬手..吃屎
    std::vector<decltype(atkNames_)> atk_namess;
    std::vector<Role::ActionShowInfo> atk_shows;
    std::vector<Role::ActionShowInfo> atk_post_shows;
    // 每次攻击，每个人的防御动画数据
    std::vector<std::vector<Role::ActionShowInfo>> defence_shows;
    // 每次攻击，每个人的文字动画数据
    std::vector<std::vector<Role::ActionShowInfo>> number_shows;

    // 连击的判断我要改的
    multiAtk_ = 0;
    for (int i = 0; i <= multiAtk_; i++)
    {
        r->Show.BattleHurt = 0;
        int magic_index = r->getMagicOfRoleIndex(magic);
        int level_index = r->getMagicLevelIndex(magic->ID);

        // 體力 默認-3 TODO 用status
        // r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        // 體力是2，可以在配置裏面寫
        conf.applyStatusFromParams({0, -3}, r);
        
        // 首個武功消耗内力減半（主角無效）
        int mpNeeded = magic->calNeedMP(level_index);
        if (magic_index == 0 && r->RealID != 0)
        {
            mpNeeded /= 2;
        }
        r->Show.MPHurt = mpNeeded;
        calMagiclHurtAllEnemies(r, magic);

        // 这里动画非常复杂
        // 首先，攻击者的抬手动画就是由showNumberAnimation完成，所以攻击者本身必须有一层show
        // 然后攻击完毕，每非攻击者的受伤害人有一层防御特效
        // 防御特效结束后是伤害显示和渐变

        atk_namess.push_back(atkNames_);
        atkNames_.clear();

        // atk_shows 显示什么，加力等，状态不显示
        atk_shows.push_back(r->Show);
        r->Show.clearDisplayOnly();

        defence_shows.emplace_back();
        for (auto r2 : battle_roles_)
        {
            defence_shows.back().push_back(r2->Show);
            r2->Show.clearDisplayOnly();
        }

        number_shows.emplace_back();
        // 攻击者的还是留到最后吧...
        for (auto r2 : battle_roles_)
        {
            if (r2->Show.BattleHurt != 0)
            {
                char sign = '-';
                if (r2->Show.BattleHurt < 0) sign = '+';
                r2->addShowString(convert::formatString("%c%d", sign, r2->Show.BattleHurt), { 255, 20, 20, 255 });
            }
            if (r2->Show.MPHurt != 0)
            {
                char sign = '-';
                if (r2->Show.MPHurt < 0) sign = '+';
                r2->addShowString(convert::formatString("%c%d", sign, r2->Show.MPHurt), { 72,209,204, 255 });
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
            if (r2 != r) {
                number_shows.back().push_back(r2->Show);
            }
            else {
                atk_post_shows.push_back(r->Show);
                number_shows.back().emplace_back();     // 留空
            }
            r2->Show.clear();
        }

    }
    r->Acted = 1;
    // 动画需求，不要在意
    r->Progress -= 1000;
    // 需要复制，因为已经离开此栈
    actionAnimation_ = [this, r, magic, atk_namess, atk_shows, defence_shows, number_shows, atk_post_shows]() mutable
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
            showNumberAnimation(3, false, progress_changes);

            // 渐变效果，生命值
            std::vector<std::pair<int&, int>> animated_changes;
            for (int j = 0; j < number_shows[i].size(); j++)
            {
                // 读取保存的文字动画数据
                battle_roles_[j]->Show = number_shows[i][j];
                // 绑定，生命值和伤害值
                animated_changes.emplace_back(battle_roles_[j]->HP, -battle_roles_[j]->Show.BattleHurt);
                animated_changes.emplace_back(battle_roles_[j]->MP, -battle_roles_[j]->Show.MPHurt);
                // 可绑定其他玩意儿，不过麻烦
            }
            showNumberAnimation(3, true, animated_changes);
            for (auto r2 : battle_roles_)
            {
                r2->clearShowStrings();
            }

            // 攻击后的
            std::vector<std::pair<int&, int>> post_changes;
            r->Show = atk_post_shows[i];
            post_changes.emplace_back(r->HP, -r->Show.BattleHurt);
            post_changes.emplace_back(r->MP, -r->Show.MPHurt);
            showNumberAnimation(2, true, post_changes);
            r->Show.clear();
        }
        // 加回来
        r->Progress += 1000;
    };
}

void BattleMod::BattleModifier::calExpGot()
{
    // 是否发过经验了
    if (info_->Exp != 0 || network_) {
        return;
    }

    int exp = 0;
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r == nullptr) continue;
        // 10倍
        exp += r->Level * 10;
    }
    for (auto r : friends_) {
        r->ExpGot = exp;
        r->Exp += exp;
    }

    ShowExp showExp;
    showExp.setRoles(friends_);
    showExp.run();

    for (auto r : friends_) {
        r->ExpGot = 0;
    }

    info_->Exp = 1;
}

//r1使用武功magic攻击r2的伤害，结果为一正数
int BattleModifier::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    // simulation的话绕过去，不用
    if (simulation_)
        return BattleScene::calMagicHurt(r1, r2, magic);

    assert(defEffectManager_.size() == 0);

    // 强制，這個必須這裏判斷
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(8), r2);

    // 加入防守方特效，注意r2先来
    auto effects = conf.tryProcAndAddToManager(r2->RealID, conf.defRole, conf.defEffectManager, r2, r1, magic);
    addDefAnim(r2, { 255,0,0 }, effects);

    // 加入挨打者的武功被动特效
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
        auto effects = conf.tryProcAndAddToManager(r2->MagicID[i], conf.defMagic,
            conf.defEffectManager, r2, r1, Save::getInstance()->getMagic(r2->MagicID[i]));
        addDefAnim(r2, { 255,215,0 }, effects);
    }

    effects = conf.tryProcAndAddToManager(conf.defAll, conf.defEffectManager, r2, r1, magic);
    addDefAnim(r2, { 238, 232, 170 }, effects);

    // 特效6，特效7 上状态
    // 己方视角
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(6), r2);
    conf.applyStatusFromParams(conf.atkEffectManager.getAllEffectParams(7), r1);
    // 然后倒过来
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(7), r2);
    conf.applyStatusFromParams(conf.defEffectManager.getAllEffectParams(6), r1);

    // 集气伤害，正数代表集气条后退
    int progressHurt = 0;

    // 特效1 气攻
    progressHurt += conf.atkEffectManager.getEffectParam0(1);

    // 特效16, 氣攻百分比增加
    double progressScale = 100;
    progressScale += conf.atkEffectManager.getEffectParam0(16);
    progressScale += conf.defEffectManager.getEffectParam0(16);
    progressHurt *= (progressScale / 100.0);

    // 特效2 护体
    double defProgress = conf.defEffectManager.getEffectParam0(2);
    double defScale = 100;
    // 特效17，氣防百分比增加
    defScale += conf.atkEffectManager.getEffectParam0(17);
    defScale += conf.defEffectManager.getEffectParam0(17);
    defProgress *= (defScale / 100.0);

    progressHurt -= defProgress;

    // 特效18 最終後退
    double finalScale = 100;
    finalScale += conf.atkEffectManager.getEffectParam0(18);
    finalScale += conf.defEffectManager.getEffectParam0(18);
    progressHurt *= (finalScale / 100.0);

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
        int attack = GameUtil::getAtk(r1) + (magic->Attack[level_index] + conf.atkEffectManager.getEffectParam0(0)) / 4;
        int defence = GameUtil::getDef(r2);

        // 伤害太高，减少一点
        int v = attack - defence * 1.5;
        v = std::max(v, attack / 7);

        //距离衰减
        //这个我应该也接手了..
        //思考，怎么改，怎么把calRoleDistance搞成一个变量
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

        // 特效15 百分比反弹
        int reflectPerc = conf.defEffectManager.getEffectParam0(15);
        // 自己反彈，如果負數即吸血（自動抵消反彈）
        reflectPerc += conf.atkEffectManager.getEffectParam0(15);
        r1->Show.BattleHurt += (reflectPerc / 100.0) * hurt;
        
        // 特效19
        int reflect = conf.defEffectManager.getEffectParam0(19);
        reflect += conf.atkEffectManager.getEffectParam0(19);

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
        r2->Show.MPHurt += v;
    }

    // 特效13 生命直接变化，实际上加到BattleHurt
    r2->Show.BattleHurt -= conf.defEffectManager.getEffectParam0(13);


    // 特效14 内力变化
    r2->Show.MPHurt -= conf.defEffectManager.getEffectParam0(14);

    // 特效12 防御方加减集气（无视气防 气攻）
    r2->Show.ProgressChange += conf.defEffectManager.getEffectParam0(12);


    // 狀態10 保命不死
    if (conf.battleStatusManager[r2->ID].getBattleStatusVal(10) > 0)
    {
        if (r2->Show.BattleHurt >= r2->HP) {
            r2->Show.BattleHurt = r2->HP - 1;
            conf.battleStatusManager[r2->ID].incrementBattleStatusVal(10, -1);
        }
        // progressHurt = 0;
        r2->Show.ProgressChange += 500;
    }

    // 不增加，只护体
    if (progressHurt > 0) {
        r2->Show.ProgressChange -= progressHurt;
        // 不允许打负集气，除非升级进度条显示
        r2->Show.ProgressChange = std::max(-r2->Progress, r2->Show.ProgressChange);
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


    // 再添加人物自身的攻击特效，强制特效要立即生效..
    auto effects = conf.tryProcAndAddToManager(r->RealID, conf.atkRole, conf.atkEffectManager, r, nullptr, m);
    // 红色
    addAtkAnim(r, { 255,0,0 }, effects);

    // 抬手时，触发攻击特效
    effects = conf.tryProcAndAddToManager(m->ID, conf.atkMagic, conf.atkEffectManager, r, nullptr, m);
    // 武功，黄色吧
    addAtkAnim(r, { 255,215,0 }, effects);

    // 所有人物的，白色
    effects = conf.tryProcAndAddToManager(conf.atkAll, conf.atkEffectManager, r, nullptr, m);
    addAtkAnim(r, { 238, 232, 170 }, effects);

    // 特效11 连击, 不控制就是无限连
    multiAtk_ += conf.atkEffectManager.getEffectParam0(11);

    int total = 0;
    int targets = 0;
    for (auto r2 : battle_roles_)
    {
        //非我方且被击中（即所在位置的效果层非负）
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            r2->Show.BattleHurt += calMagicHurt(r, r2, m);
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

    if (targets > 0) {
        // 特效13 生命直接变化，实际上加到BattleHurt
        // 貌似存在一個吸空還可以吸的情況，不管了
        r->Show.BattleHurt -= conf.atkEffectManager.getEffectParam0(13);
        // 特效14 内力变化
        r->Show.MPHurt -= conf.atkEffectManager.getEffectParam0(14);
    }

    // 限制生命内力
    for (auto r2 : battle_roles_)
    {
        r2->Show.BattleHurt = GameUtil::limit(r2->Show.BattleHurt, -(r2->MaxHP - r2->HP), r2->HP);
        r2->Show.MPHurt = GameUtil::limit(r2->Show.MPHurt, -(r2->MaxMP - r2->MP), r2->MP);
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

        // 特效7 上状态
        conf.applyStatusFromParams(conf.turnEffectManager.getAllEffectParams(7), role);

        // 特效12 加集气
        role->Show.ProgressChange += conf.turnEffectManager.getEffectParam0(12);

        std::vector<std::pair<int &, int>> animated_changes;

        // 特效13 生命直接变化
        int hp = conf.turnEffectManager.getEffectParam0(13);
        hp = GameUtil::limit(role->HP + hp, 0, role->MaxHP) - role->HP;
        if (hp != 0) {
            role->addShowString(convert::formatString("生命 %d", hp), { 255, 20, 20, 255 });
            animated_changes.emplace_back(role->HP, hp);
        }
            
        // 特效14 内力变化
        int mp = conf.turnEffectManager.getEffectParam0(14);
        mp = GameUtil::limit(role->MP + mp, 0, role->MaxMP) - role->MP;
        if (mp != 0) {
            role->addShowString(convert::formatString("内力 %d", mp), { 255, 20, 20, 255 });
            animated_changes.emplace_back(role->MP, mp);
        }
            

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
        showNumberAnimation(2, true, animated_changes);
        role->Show.clear();

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
        if (result_ == 0)
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

        int progress = 0;

        // 先上/下状态，给自己，给别人也不是不行，再搞个循环，判断不在一个队伍的
        // 特效7
        conf.applyStatusFromParams(conf.speedEffectManager.getAllEffectParams(7), r);

        // 特效13 生命直接变化
        int hp = conf.speedEffectManager.getEffectParam0(13);
        hp = GameUtil::limit(r->HP + hp, 0, r->MaxHP) - r->HP;
        r->HP += hp;

        // 特效14 内力变化
        int mp = conf.speedEffectManager.getEffectParam0(14);
        mp = GameUtil::limit(r->MP + mp, 0, r->MaxMP) - r->MP;
        r->MP += mp;

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
    int w, h;
    int fontSize = 25;
    Engine::getInstance()->getWindowSize(w, h);
    TextBox tb;
    tb.setText(hugeStr);
    tb.setPosition(w / 2.0 - (hugeStr.size() / 4.0) * fontSize, 50);
    tb.setFontSize(fontSize);
    tb.setStayFrame(40);
    tb.run();
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
    int hp_y = y - 70;
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

    perc = ((double)r->MP / r->MaxMP);
    hp_y += 4;
    Engine::getInstance()->fillColor({ 72,209,204, 255 }, hp_x, hp_y, perc * hp_max_w, 2);
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, hp_max_w, 1);
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y + hp_h, hp_max_w, 1);
    Engine::getInstance()->fillColor(outline_color, hp_x, hp_y, 1, hp_h);
    Engine::getInstance()->fillColor(outline_color, hp_x + hp_max_w, hp_y, 1, hp_h);

    hp_y += 4;
    int i = 0;
    for (auto& s : conf.battleStatus) {
        // 再加入一个，状态显示，强行不显示体力...
        if (!s.hide && s.id!=0) {
            int val = conf.battleStatusManager[r->ID].getBattleStatusVal(s.id);
            if (val == 0) continue;
            int actual_w = ((double)val / s.max) * hp_max_w;
            Engine::getInstance()->fillColor(outline_color, hp_x, hp_y + i, hp_max_w, 2);
            Engine::getInstance()->fillColor(s.color, hp_x + 1, hp_y + i, actual_w, 2);
            Engine::getInstance()->fillColor(outline_color, hp_x + 1, hp_y + i + 2, hp_max_w, 1);
            i += 3;
        }
    }
}


