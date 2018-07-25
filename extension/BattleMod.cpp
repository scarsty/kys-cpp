#include "BattleMod.h"
#include "GameUtil.h"
#include "Save.h"
#include "Font.h"
#include "libconvert.h"
#include "PotConv.h"
#include <cassert>

using namespace BattleMod;

BattleMod::SpecialEffect::SpecialEffect(int id, const std::string & desc) :
    id_(id), description_(desc)
{
}

int BattleMod::SpecialEffect::getId() const
{
    return id_;
}

std::string BattleMod::SpecialEffect::getDescription() const
{
    return description_;
}


BattleMod::EffectParamPair::EffectParamPair(const SpecialEffect & effect, Param p, const std::string & desc) :
    effect(effect), description(desc)
{
    params_.push_back(p);
}

Param BattleMod::EffectParamPair::getParam0()
{
    return params_[0];
}

const Params & BattleMod::EffectParamPair::getParams()
{
    return params_;
}

EffectParamPair & BattleMod::EffectParamPair::operator+=(const EffectParamPair & rhs)
{
    // id 必须相同
    if (effect.getId() != rhs.effect.getId() || params_.size() != rhs.params_.size()) {
        return *this;
    }
    for (std::size_t i = 0; i < params_.size(); i++) {
        params_[i] += rhs.params_[i];
    }
    return *this;
}

BattleMod::EffectSingle::EffectSingle(int p, EffectParamPair epp) : 
    percentProb_(p), effectPair_(epp)
{
}

EffectParamPair * BattleMod::EffectSingle::proc(RandomDouble & rand)
{
    // rand [0 1)
    // 乘以100，取整，就是说实际范围是0-99
    if (rand.rand_int(100) < percentProb_) {
        return &effectPair_;
    }
    return nullptr;
}

EffectParamPair * BattleMod::EffectWeightedGroup::proc(RandomDouble & rand)
{
    printf("尝试触发\n");
    // [0 total)
    int t = rand.rand_int(total_);
    int c = 0;
    for (auto& p : group_) {
        c += p.first;
        if (t < c) {
            return &p.second;
        }
    }
    return nullptr;
}

BattleMod::EffectWeightedGroup::EffectWeightedGroup(int total) :
    total_(total)
{
}

void BattleMod::EffectWeightedGroup::addProbEPP(int weight, EffectParamPair epp)
{
    group_.emplace_back(weight, epp);
}

bool BattleMod::EffectManager::hasEffect(int eid)
{
    return epps_.find(eid) != epps_.end();
}

Param BattleMod::EffectManager::getEffectParam0(int eid)
{
    auto iter = epps_.find(eid);
    // 返回0这样应该没问题吧。。
    if (iter == epps_.end()) return 0;
    return iter->second.getParam0();
}

Params BattleMod::EffectManager::getAllEffectParams(int eid)
{
    auto iter = epps_.find(eid);
    if (iter == epps_.end()) return {};
    return iter->second.getParams();
}

EffectParamPair * BattleMod::EffectManager::getEPP(int eid)
{
    auto iter = epps_.find(eid);
    if (iter == epps_.end())
        return nullptr;
    return &(iter->second);
}

void BattleMod::EffectManager::unionEffects(const EffectManager & other)
{
    for (const auto& idPair : other.epps_) {
        auto myEpp = getEPP(idPair.first);
        if (myEpp) {
            *myEpp += idPair.second;
        }
        else {
            epps_.insert(idPair);
        }
    }
}

void BattleMod::EffectManager::addEPP(const EffectParamPair & ewp)
{
    epps_.insert({ ewp.effect.getId(), ewp });
}

std::size_t BattleMod::EffectManager::size()
{
    return epps_.size();
}

void BattleMod::EffectManager::clear()
{
    epps_.clear();
}



BattleModifier::BattleModifier() {
    printf("this is modified battle scene.\n");
    init();
}

BattleMod::BattleModifier::BattleModifier(int id) : BattleScene(id)
{
    init();
}

void BattleMod::BattleModifier::init()
{
    YAML::Node baseNode;
    // 可以整个都套try，出了问题就特效全清空
    try {
        YAML::Node baseNode = YAML::LoadFile(PATH);
    }
    catch (...) {
        printf("yaml config missing\n");
        return;
    }
    // 这里把数据全读到我的数据结构里(用STL)
    // 然后可以写个validator之类的
    printf("特效数量 %d\n", baseNode[u8"特效"].size());
    for (const auto& spNode : baseNode[u8"特效"]) {
        // 必须按顺序来，因为我懒，可以考虑改一改
        assert(effects_.size() == spNode[u8"编号"].as<int>());
        auto desc = PotConv::conv(spNode[u8"描述"].as<std::string>(), "utf-8", "cp936");
        printf("%d %s\n", spNode[u8"编号"].as<int>(), desc.c_str());
        effects_.emplace_back(spNode[u8"编号"].as<int>(), desc);
    }

    if (baseNode[u8"武功"]) {
        for (const auto& magicNode : baseNode[u8"武功"]) {
            int wid = magicNode[u8"武功编号"].as<int>();
            if (magicNode[u8"主动攻击"]) {
                readIntoMapping(magicNode[u8"主动攻击"], activeAtkMagic_[wid]);
            }
            if (magicNode[u8"被动攻击"]) {
                readIntoMapping(magicNode[u8"被动攻击"], passiveAtkMagic_[wid]);
            }
            if (magicNode[u8"被动防守"]) {
                readIntoMapping(magicNode[u8"被动防守"], passiveDefMagic_[wid]);
            }
            if (magicNode[u8"全局特效"]) {
                readIntoMapping(magicNode[u8"全局特效"], globalMagic_[wid]);
            }
        }
    }

    if (baseNode[u8"人物"]) {
        for (const auto& personNode : baseNode[u8"人物"]) {
            int pid = personNode[u8"人物编号"].as<int>();
            if (personNode[u8"攻击"]) {
                readIntoMapping(personNode[u8"攻击"], atkRole_[pid]);
            }
            if (personNode[u8"防守"]) {
                readIntoMapping(personNode[u8"防守"], defRole_[pid]);
            }
            if (personNode[u8"全局"]) {
                readIntoMapping(personNode[u8"全局"], globalRole_[pid]);
            }
        }
    }
    printf("load yaml config complete\n");
}

EffectParamPair BattleModifier::readEffectParamPairs(const YAML::Node& node) {
    int id = node[u8"编号"].as<int>();
    auto display = PotConv::conv(node[u8"显示"].as<std::string>(), "utf-8", "cp936");
    printf("读入 %s\n", display.c_str());
    // 这里效果参数以后要可配置
    EffectParamPair epp(effects_[id], node[u8"效果参数"].as<int>(), display);
    return epp;
}


std::unique_ptr<ProccableEffect> BattleModifier::readProccableEffect(const YAML::Node& node) {
    int probType = node[u8"发动概率"].as<int>();
    ProcProbability prob = static_cast<ProcProbability>(probType);
    std::unique_ptr<ProccableEffect> pe;
    switch (prob) {
    case ProcProbability::random: {
        pe = std::make_unique<EffectSingle>(node[u8"特效"][u8"发动参数"].as<int>(), readEffectParamPairs(node[u8"特效"]));
        break;
    }
    case ProcProbability::distributed: {
        auto group = std::make_unique<EffectWeightedGroup>(node[u8"总比重"].as<int>());
        for (auto& eNode : node[u8"特效"]) {
            group->addProbEPP(eNode[u8"发动参数"].as<int>(), readEffectParamPairs(eNode));
        }
        pe = std::move(group);
        break;
    }
    }
    return pe;
}

void BattleModifier::readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects) {
    for (auto& spNode : node) {
        effects.push_back(std::move(readProccableEffect(spNode)));
    }
}

std::vector<std::string> BattleMod::BattleModifier::tryProcAndAddToManager(int id, const EffectsTable & table, EffectManager & manager)
{
    // 这里完全没有考虑特效文字显示
    // 怎么显示包括显示什么，都是个问题
    // 然后就是如果有特效动画（护体？），又要怎么办
    // 有个办法就是打个包把这些全部return
    // 谁调用谁处理
    // TODO 返还所有触发特效文字
    std::vector<std::string> effectStrs;
    auto iter = table.find(id);
    if (iter != table.end()) {
        for (const auto& effect : iter->second) {
            if (auto epp = effect->proc(rand_)) {
                manager.addEPP(*epp);
                effectStrs.push_back(epp->description);
                printf("触发效果 %s\n", epp->description.c_str());
            }
        }
    }
    return effectStrs;
}


void BattleModifier::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);

    // 人物的全局特效，开战时需要判断是否触发。。。
    // S/L大法，愚蠢的设计
    tryProcAndAddToManager(r->ID, globalRole_, globalEffectManagers_[r->ID]);

    // 检测人物武功，同样检测才触发。。
    for (int i = 0; i < r->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r->MagicID[i], globalMagic_, globalEffectManagers_[r->ID]);
    }
}

void BattleMod::BattleModifier::useMagic(Role * r, Magic * magic)
{
    int level_index = r->getMagicLevelIndex(magic->ID);
    for (int i = 0; i <= GameUtil::sign(r->AttackTwice); i++)
    {
        //播放攻击画面，计算伤害
        showMagicName(magic->Name);
        calMagiclHurtAllEnemies(r, magic);
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);
        useMagicAnimation(r, magic);
        showNumberAnimation();
        //武学等级增加
        auto index = r->getMagicOfRoleIndex(magic);
        if (index >= 0)
        {
            r->MagicLevel[index] += 1 + rand_.rand_int(2);
            GameUtil::limit2(r->MagicLevel[index], 0, MAX_MAGIC_LEVEL);
        }
        printf("%s %s level is %d\n", PotConv::to_read(r->Name).c_str(), PotConv::to_read(magic->Name).c_str(), r->MagicLevel[index]);
    }
    r->Acted = 1;
}

//r1使用武功magic攻击r2的伤害，结果为一正数
int BattleModifier::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    // simulation的话绕过去，不用
    if (simulation_)
        return BattleScene::calMagicHurt(r1, r2, magic);

    assert(defEffectManager_.size() == 0);
    
    // 先加入防守方特效
    tryProcAndAddToManager(r2->ID, defRole_, defEffectManager_);
    
    // 再加入挨打者的武功被动特效
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r2->MagicID[i], passiveDefMagic_, defEffectManager_);
    }

    // 集气伤害，正数代表集气条后退
    int progressHurt = 0;

    // 特效1 气攻
    progressHurt += atkEffectManager_.getEffectParam0(1);
    // 特效2 护体
    progressHurt -= defEffectManager_.getEffectParam0(2);


    int hurt = 0;
    int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(r1, magic);
    level_index = magic->calMaxLevelIndexByMP(r1->MP, level_index);
    if (magic->HurtType == 0)
    {
        if (r1->MP <= 10)
        {
            return 1 + rand_.rand_int(10);
        }
        int attack = r1->Attack + magic->Attack[level_index] / 3;
        // 特效0 武功威力增加
        attack += atkEffectManager_.getEffectParam0(0) / 3;
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

        int v = attack - defence;
        //距离衰减
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rand_.rand_int(10) - rand_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rand_.rand_int(10);
        }
        hurt = v;
    }
    else if (magic->HurtType == 1)
    {
        int v = magic->HurtMP[level_index];
        v += rand_.rand_int(10) - rand_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rand_.rand_int(10);
        }
        hurt = v;
    }

    // 不增加，只护体
    if (progressHurt > 0) {
        r2->ProgressChange -= progressHurt;
        printf("集气变化%d\n", r2->ProgressChange);
    }
    defEffectManager_.clear();
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

    assert(atkEffectManager_.size() == 0);

    // 触发，被动攻击特效，这里被动也是每次攻击再判断一次
    for (int i = 0; i < r->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r->MagicID[i], passiveAtkMagic_, atkEffectManager_);
    }

    // 抬手时，触发攻击特效
    auto strs = tryProcAndAddToManager(m->ID, activeAtkMagic_, atkEffectManager_);
    // 简单的做个显示~
    for (const auto& str : strs) {
        showMagicName(str);
    }

    // 再添加人物自身的攻击特效
    tryProcAndAddToManager(r->ID, atkRole_, atkEffectManager_);

    // 全局特效合并进来，不再做判断
    atkEffectManager_.unionEffects(globalEffectManagers_[r->ID]);

    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //非我方且被击中（即所在位置的效果层非负）
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            int hurt = calMagicHurt(r, r2, m);
            if (m->HurtType == 0)
            {
                r2->ShowString = convert::formatString("-%d", hurt);
                r2->ShowColor = { 255, 20, 20, 255 };
                int prevHP = r2->HP;
                r2->HP = GameUtil::limit(r2->HP - hurt, 0, r2->MaxHP);
                int hurt1 = prevHP - r2->HP;
                r->ExpGot += hurt1;
                if (r2->HP <= 0)
                {
                    r->ExpGot += hurt1 / 2;
                }
                r2->ProgressChange -= -hurt / 5;
            }
            else if (m->HurtType == 1)
            {
                r2->ShowString = convert::formatString("-%d", hurt);
                r2->ShowColor = { 160, 32, 240, 255 };
                int prevMP = r2->MP;
                r2->MP = GameUtil::limit(r2->MP - hurt, 0, r2->MaxMP);
                r->MP = GameUtil::limit(r->MP + hurt, 0, r->MaxMP);
                int hurt1 = prevMP - r2->MP;
                r->ExpGot += hurt1 / 2;
            }
        }
    }

    // 打完了，清空攻击效果
    atkEffectManager_.clear();
    return total;
}

void BattleModifier::showNumberAnimation()
{
    BattleScene::showNumberAnimation();
}

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
        r->Progress += r->Speed / 4;

        auto iter = globalEffectManagers_.find(r->ID);
        if (iter != globalEffectManagers_.end()) {
            // 特效3 增加集气速度
            r->Progress += iter->second.getEffectParam0(3);
        }
    }
    return nullptr;
}