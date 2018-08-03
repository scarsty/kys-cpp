#include "BattleMod.h"
#include "GameUtil.h"
#include "Save.h"
#include "Font.h"
#include "libconvert.h"
#include "PotConv.h"
#include "Event.h"

#include <numeric>
#include <cassert>
#include <unordered_set>

using namespace BattleMod;

RandomDouble BattleModifier::rng;

BattleModifier::BattleModifier() {
    printf("this is modified battle scene.\n");
    init();
}

void BattleMod::BattleModifier::init()
{

    // 测试
    Event::getInstance()->setRoleMagic(0, 1, 2, 999);
    Save::getInstance()->getRole(0)->MaxHP = 999;
    Save::getInstance()->getRole(0)->HP = 999;
    Save::getInstance()->getRole(0)->Defence = 80;

    YAML::Node baseNode;
    // 可以整个都套try，出了问题就特效全清空
    try {
        baseNode = YAML::LoadFile(PATH);
    }
    catch (...) {
        printf("yaml config missing\n");
        return;
    }

    strPool_.reserve(baseNode[u8"字符串数组大小"].as<int>());
    strPool_.push_back("");

    printf("特效数量 %d\n", baseNode[u8"特效"].size());
    for (const auto& spNode : baseNode[u8"特效"]) {
        // 必须按顺序来，因为我懒，TODO 改一改？还是不改呢，有空再改！
        assert(effects_.size() == spNode[u8"编号"].as<int>());
        auto desc = PotConv::conv(spNode[u8"描述"].as<std::string>(), "utf-8", "cp936");
        std::reference_wrapper<std::string> descRef(strPool_.front());
        if (!desc.empty()) {
            strPool_.push_back(desc);
            descRef = strPool_.back();
        }
        printf("%d %s\n", spNode[u8"编号"].as<int>(), descRef.get().c_str());
        effects_.emplace_back(spNode[u8"编号"].as<int>(), descRef);
    }

    for (const auto& bNode : baseNode[u8"战场状态"]) {
        int id = bNode[u8"编号"].as<int>();
        assert(battleStatus_.size() == id);
        auto desc = PotConv::conv(bNode[u8"描述"].as<std::string>(), "utf-8", "cp936");
        
        int max = 100;
        if (const auto& maxNode = bNode[u8"满值"]) {
            max = maxNode.as<int>();
        }

        int hide = false;
        if (const auto& hideNode = bNode[u8"隐藏"]) {
            hide = hideNode.as<bool>();
        }

        // 默认白色
        BP_Color c = { 255, 255, 255, 255 };
        if (const auto& cNode = bNode[u8"颜色"]) {
            c = { (Uint8)cNode[0].as<int>(), (Uint8)cNode[1].as<int>(), (Uint8)cNode[2].as<int>(), 255 };
            if (cNode.size() >= 4) {
                c.a = (Uint8)cNode[3].as<int>();
            }
        }

        std::reference_wrapper<std::string> descRef(strPool_.front());
        if (!desc.empty()) {
            strPool_.push_back(desc);
            descRef = strPool_.back();
        }
        printf("%d %s\n", id, descRef.get().c_str());
        battleStatus_.emplace_back(id, max, descRef, hide, c);
    }

    // 以下可以refactor，TODO 改！！！
    if (baseNode[u8"武功"]) {
        printf("number of wg %d\n", baseNode[u8"武功"].size());
        for (const auto& magicNode : baseNode[u8"武功"]) {
            int wid = magicNode[u8"武功编号"].as<int>();
            if (magicNode[u8"攻击"]) {
                printf("number of atk %d\n", magicNode[u8"攻击"].size());
                readIntoMapping(magicNode[u8"攻击"], atkMagic_[wid]);
            }
            if (magicNode[u8"防守"]) {
                readIntoMapping(magicNode[u8"防守"], defMagic_[wid]);
            }
            if (magicNode[u8"集气"]) {
                readIntoMapping(magicNode[u8"集气"], speedMagic_[wid]);
            }
            if (magicNode[u8"回合"]) {
                readIntoMapping(magicNode[u8"回合"], turnMagic_[wid]);
            }
        }
    }

    if (auto const& allNode = baseNode[u8"全人物"]) {
        printf("size %d\n", allNode.size());
        if (allNode[u8"攻击"]) {
            readIntoMapping(allNode[u8"攻击"], atkAll_);
        }
        if (allNode[u8"防守"]) {
            readIntoMapping(allNode[u8"防守"], defAll_);
        }
        if (allNode[u8"集气"]) {
            readIntoMapping(allNode[u8"集气"], speedAll_);
        }
        if (allNode[u8"回合"]) {
            readIntoMapping(allNode[u8"回合"], turnAll_);
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
            if (personNode[u8"集气"]) {
                readIntoMapping(personNode[u8"集气"], speedRole_[pid]);
            }
            if (personNode[u8"回合"]) {
                readIntoMapping(personNode[u8"回合"], turnRole_[pid]);
            }
        }
    }
    printf("load yaml config complete\n");
}

Variable BattleMod::BattleModifier::readVariable(const YAML::Node & node)
{
    BattleInfoFunc f;
    VarTarget target = VarTarget::Self;
    if (node.IsScalar()) {
        int v = node.as<int>();
        f = [v](const Role* character, const Magic* wg) {
            return v;
        };
    }
    else if (node[u8"状态"]) {
        // 这个简单
        int statusID = node[u8"状态"].as<int>();
        f = [this, statusID](const Role* character, const Magic* wg) {
            if (character == nullptr) return 0;
            return battleStatusManager_[character->ID].getBattleStatusVal(statusID);
        };
    }
    else if (const auto& varNode = node[u8"变量"]) {
        if (const auto& targetNode = varNode[u8"对象"]) {
            target = static_cast<VarTarget>(targetNode.as<int>());
        }
        int varID = varNode[u8"编号"].as<int>();
        // 唉，填坑
        enum class zzz {
            var_char_sex = 0,
            var_char_level = 1,
            var_char_exp = 2,
            var_char_hp = 3,
            var_char_maxhp = 4,
            var_char_equip0 = 5,
            var_char_equip1 = 6,
            var_char_mptype = 7,
            var_char_mp = 8,
            var_char_maxmp = 9,
            var_char_attack = 10,
            var_char_speed = 11,
            var_char_defence = 12,
            var_char_medicine = 13,
            var_char_usepoison = 14,
            var_char_detoxification = 15,
            var_char_antipoison = 16,
            var_char_fist = 17,
            var_char_sword = 18,
            var_char_blade = 19,
            var_char_unusual = 20,
            var_char_hiddenweapon = 21,
            var_char_knowledge = 22,
            var_char_morality = 23,
            var_char_attackwithpoison = 24,
            var_char_attacktwice = 25,
            var_char_fame = 26,
            var_char_iq = 27,
            var_using_char = 28,
            var_books = 29,
            var_cur_wg_level = 30,
            var_wg_level = 31,
            var_count_item = 32,
            var_has_wg = 33,
            var_wg_type = 34,
            var_is_person = 35,
            var_has_status = 36,
            var_wg_power = 37,
        };
        zzz varType = static_cast<zzz>(varID);
#define ATTR( attribute ) f = [this](const Role* c, const Magic* wg){ if (c == nullptr) return 0; return c->attribute; }; break;
        switch (varType) {
        case zzz::var_char_sex: ATTR(Sexual);
        case zzz::var_char_level: ATTR(Level);
        case zzz::var_char_exp: ATTR(Exp);
        case zzz::var_char_hp: ATTR(HP);
        case zzz::var_char_maxhp: ATTR(MaxHP);
        case zzz::var_char_equip0: ATTR(Equip0);
        case zzz::var_char_equip1: ATTR(Equip1);
        case zzz::var_char_mptype: ATTR(MPType);
        case zzz::var_char_mp: ATTR(MP);
        case zzz::var_char_maxmp: ATTR(MaxMP);
        case zzz::var_char_attack: ATTR(Attack);
        case zzz::var_char_speed: ATTR(Speed);
        case zzz::var_char_defence: ATTR(Defence);
        case zzz::var_char_medicine: ATTR(Medicine);
        case zzz::var_char_usepoison: ATTR(UsePoison);
        case zzz::var_char_detoxification: ATTR(Detoxification);
        case zzz::var_char_antipoison: ATTR(AntiPoison);
        case zzz::var_char_fist: ATTR(Fist);
        case zzz::var_char_sword: ATTR(Sword);
        case zzz::var_char_blade: ATTR(Knife);
        case zzz::var_char_unusual: ATTR(Unusual);
        case zzz::var_char_hiddenweapon: ATTR(HiddenWeapon);
        case zzz::var_char_knowledge: ATTR(Knowledge);
        case zzz::var_char_morality: ATTR(Morality);
        case zzz::var_char_attackwithpoison: ATTR(AttackWithPoison);
        case zzz::var_char_attacktwice: ATTR(AttackTwice);
        case zzz::var_char_fame: ATTR(Fame);
        case zzz::var_char_iq: ATTR(IQ);
        case zzz::var_using_char: ATTR(ID);
        case zzz::var_books: {
            // 现在MOD都喜欢和书挂钩
            // 硬代码，写书本id
            // 从lua版本哪里抄的，id从144开始
            // 很鸡贼的就是 我这里books只用算一遍，因为战斗期间不可能添加天书
            int books = 0;
            for (int i = 144; i < 144 + 14; i++)
                books += Save::getInstance()->getItemCountInBag(i);
            f = [books](const Role* c, const Magic* wg) {
                // 仅对友方有效！！
                // 如果要对敌方有效，不如加个变量id
                if (c->Team != 0) return 0;
                return books;
            };
            break;
        }
        case zzz::var_cur_wg_level: {
            // getMagicOfRoleIndex 不是个const，我很烦
            f = [this](const Role* c, const Magic* wg) {
                if (c == nullptr || wg == nullptr) return 0;
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    if (c->MagicID[i] == wg->ID)
                    {
                        return c->MagicLevel[i];
                    }
                }
                return 0;
            };
            break;
        }
        case zzz::var_wg_level: {
            // 读取额外参数
            int wgID = varNode[u8"参数"].as<int>();
            f = [this, wgID](const Role* c, const Magic* wg) {
                if (c == nullptr) return 0;
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    if (c->MagicID[i] == wgID)
                    {
                        return c->MagicLevel[i];
                    }
                }
                return 0;
            };
            break;
        }
        case zzz::var_count_item: {
            int itemID = varNode[u8"参数"].as<int>();
            int num = Save::getInstance()->getItemCountInBag(itemID);
            f = [num](const Role* c, const Magic* wg) {
                // 仅对友方有效！！
                // 如果要对敌方有效，不如加个变量id
                if (c->Team != 0) return 0;
                return num;
            };
            break;
        }
        case zzz::var_has_wg: {
            const auto& paramNode = varNode[u8"参数"];
            std::unordered_set<int> ids;
            if (paramNode.IsScalar()) {
                ids.insert(paramNode.as<int>());
            }
            else {
                for (const auto& p : paramNode) {
                    ids.insert(p.as<int>());
                }
            }
            f = [this, ids](const Role* c, const Magic* wg) {
                if (c == nullptr) return 0;
                int count = 0;
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    if (ids.find(c->MagicID[i]) != ids.end())
                        count++;
                }
                if (count == ids.size())
                    return 1;
                return 0;
            };
            break;
        }
        case zzz::var_wg_type: {
            f = [this](const Role* c, const Magic* wg) {
                if (wg == nullptr) return 0;
                return wg->MagicType;
            };
            break;
        }
        case zzz::var_is_person: {
            int pid = varNode[u8"参数"].as<int>();
            f = [this, pid](const Role* c, const Magic* wg) {
                if (c == nullptr) return 0;
                if (c->ID == pid) return 1;
                return 0;
            };
            break;
        }
        case zzz::var_has_status: {
            const auto& paramNode = varNode[u8"参数"];
            int statusID = paramNode[0].as<int>();
            int val = paramNode[1].as<int>();
            f = [this, statusID, val](const Role* c, const Magic* wg) {
                if (c == nullptr) return 0;
                if (battleStatusManager_[c->ID].getBattleStatusVal(statusID) >= val)
                    return 1;
                return 0;
            };
            break;
        }
        case zzz::var_wg_power: {
            f = [this](const Role* c, const Magic* wg) {
                if (c == nullptr || wg == nullptr) return 0;
                // 问世间，我用了const 他没有
                Role* cc = const_cast<Role*>(c);
                Magic* mm = const_cast<Magic*>(wg);
                // 或者靠id走 Save:: 也可以
                int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(cc, mm);
                level_index = mm->calMaxLevelIndexByMP(cc->MP, level_index);
                return mm->Attack[level_index];
            };
        }
        default: {
            printf("%d variable id not found\n", varID);
            break;
        }
        }

    }
    
    return Variable(f, target);
}

std::unique_ptr<Adder> BattleMod::BattleModifier::readAdder(const YAML::Node & node)
{
    std::unique_ptr<Adder> adder;
    if (const auto& randNode = node[u8"随机"]) {
        const auto& rangeNode = randNode[u8"范围"];
        if (rangeNode.IsSequence()) {
            std::vector<int> range;
            for (const auto& n : rangeNode) {
                range.push_back(n.as<int>());
            }
            adder = std::make_unique<RandomAdder>(std::move(range));
        }
        else {
            // 我突然反应过来这个也可以是variable TODO 改成读variable
            int min = rangeNode[u8"最小"].as<int>();
            int max = rangeNode[u8"最大"].as<int>();
            adder = std::make_unique<RandomAdder>(min, max);
        }
    }
    else if (const auto& linearNode = node[u8"线性"]) {
        adder = std::make_unique<LinearAdder>(linearNode[u8"系数"].as<double>(), readVariable(linearNode));
    }
    return adder;
}

VariableParam BattleMod::BattleModifier::readVariableParam(const YAML::Node & node)
{
    if (node.IsScalar()) {
        return VariableParam(node.as<int>());
    }
    const auto& pNode = node[u8"可变参数"];
    VariableParam vp(pNode[u8"基础"].as<int>());
    if (const auto& adders = pNode[u8"加成"]) {
        for (const auto& adder : adders) {
            vp.addAdder(readAdder(adder));
        }
    }
    return vp;
}

Conditions BattleMod::BattleModifier::readConditions(const YAML::Node & node)
{
    Conditions conditions;
    const auto& condNode = node[u8"条件"];
    for (const auto& cond : condNode) {
        conditions.push_back(std::move(readCondition(cond)));
    }
    return conditions;
}

Condition BattleMod::BattleModifier::readCondition(const YAML::Node & node)
{
    const auto& condNode = node[u8"条件"];
    printf("%d\n", node.size());
    Variable lhs = readVariable(node[u8"左边"]);
    Variable rhs = readVariable(node[u8"右边"]);
    ConditionOp opID = static_cast<ConditionOp>(node[u8"对比"].as<int>());
    std::function<bool(int, int)> op;
    switch (opID) {
    case ConditionOp::equal : op = std::equal_to<int>(); break;
    case ConditionOp::greater_than: op = std::greater<int>(); break;
    case ConditionOp::greater_than_equal: op = std::greater_equal<int>(); break;
    }
    return Condition(lhs, rhs, op);
}

EffectParamPair BattleModifier::readEffectParamPair(const YAML::Node& node) {
    int id = node[u8"编号"].as<int>();
    auto display = PotConv::conv(node[u8"显示"].as<std::string>(), "utf-8", "cp936");
    std::reference_wrapper<std::string> displayRef(strPool_.front());
    if (!display.empty()) {
        strPool_.push_back(display);
        displayRef = strPool_.back();
    }
    printf("读入 %d %s\n", id, displayRef.get().c_str());
    // 这里效果参数以后要可配置
    EffectParamPair epp(effects_[id], displayRef);
    if (node[u8"效果参数"].IsScalar() || node[u8"效果参数"].IsMap()) {
        epp.addParam(readVariableParam(node[u8"效果参数"]));
    }
    else {
        for (const auto& param : node[u8"效果参数"]) {
            epp.addParam(readVariableParam(param));
        }
    }
    return epp;
}

std::vector<EffectParamPair> BattleModifier::readEffectParamPairs(const YAML::Node& node) {
    std::vector<EffectParamPair> pairs;
    if (node.IsMap()) {
        pairs.push_back(std::move(readEffectParamPair(node)));
    }
    else {
        for (const auto& enode : node) {
            pairs.push_back(std::move(readEffectParamPair(enode)));
        }
    }
    return pairs;
}



std::unique_ptr<ProccableEffect> BattleModifier::readProccableEffect(const YAML::Node& node) {
    int probType = node[u8"发动方式"].as<int>();
    ProcProbability prob = static_cast<ProcProbability>(probType);
    std::unique_ptr<ProccableEffect> pe;
    switch (prob) {
    case ProcProbability::random: {
        pe = std::make_unique<EffectSingle>(readVariableParam(node[u8"发动参数"]), readEffectParamPairs(node[u8"特效"]));
        break;
    }
    case ProcProbability::distributed: {
        auto group = std::make_unique<EffectWeightedGroup>(readVariableParam(node[u8"总比重"]));
        for (const auto& eNode : node[u8"特效"]) {
            group->addProbEPP(readVariableParam(eNode[u8"发动参数"]), readEffectParamPair(eNode));
        }
        pe = std::move(group);
        break;
    }
    case ProcProbability::prioritized: {
        auto group = std::make_unique<EffectPrioritizedGroup>();
        for (const auto& eNode : node[u8"特效"]) {
            group->addProbEPP(readVariableParam(eNode[u8"发动参数"]), readEffectParamPair(eNode));
        }
        pe = std::move(group);
        break;
    }
    case ProcProbability::counter: {
        pe = std::make_unique<EffectCounter>(readVariableParam(node[u8"计数"]), 
                                             readVariableParam(node[u8"发动参数"]), 
                                             readEffectParamPairs(node[u8"特效"]));
        break;
    }
    }
    if (pe) {
        if (const auto& reqNode = node[u8"需求"]) {
            for (const auto& condNode : reqNode) {
                pe->addConditions(readConditions(condNode));
            }
        }
    }
    return pe;
}

void BattleModifier::readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects) {
    for (auto& spNode : node) {
        effects.push_back(std::move(readProccableEffect(spNode)));
    }
}


std::vector<EffectIntsPair> BattleMod::BattleModifier::tryProcAndAddToManager(const Effects & list, EffectManager & manager, const Role * attacker, const Role * defender, const Magic * wg)
{
    std::vector<EffectIntsPair> procd;
    for (const auto& effect : list) {
        auto epps = effect->proc(attacker, defender, wg);
        for (const auto& epp : epps) {
            manager.addEPP(epp);
            procd.push_back(epp);
        }
    }
    return procd;
}

// 再复制EffectIntsPair，不过我觉得复制这玩意儿不贵
std::vector<EffectIntsPair> BattleMod::BattleModifier::tryProcAndAddToManager(int id, const EffectsTable & table, EffectManager & manager, 
                                                                              const Role * attacker, const Role * defender, const Magic * wg)
{
    std::vector<EffectIntsPair> procd;
    auto iter = table.find(id);
    if (iter != table.end()) {
        // TODO 用上面那个
        for (const auto& effect : iter->second) {
            auto epps = effect->proc(attacker, defender, wg);
            for (const auto& epp : epps) {
                manager.addEPP(epp);
                procd.push_back(epp);
            }
        }
    }
    return procd;
}

void BattleModifier::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);

    battleStatusManager_[r->ID].initStatus(r, &battleStatus_);
}

void BattleMod::BattleModifier::actUseMagic(Role * r)
{
    auto magic_menu = new BattleMagicMenu();
    while (true)
    {
        magic_menu->setStartItem(r->SelectedMagic);
        magic_menu->runAsRole(r);
        auto magic = magic_menu->getMagic();
        r->SelectedMagic = magic_menu->getResult();
        if (magic == nullptr)
        {
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
            useMagic(r, magic);
            break;
        }
    }
    delete magic_menu;
}

void BattleMod::BattleModifier::useMagic(Role * r, Magic * magic)
{
    int level_index = r->getMagicLevelIndex(magic->ID);
    // 连击的判断我要改的
    multiAtk_ = 0;
    for (int i = 0; i <= multiAtk_; i++)
    {
        calMagiclHurtAllEnemies(r, magic);

        useMagicAnimation(r, magic);

        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);

        for (auto r2 : battle_roles_) {
            if (r2 != r) {
                r2->Effect = 6;
                r2->addShowString("啊机器猫技术帝", { 255, 20, 20, 255 });
                r2->addShowString("劳资无敌", { 160, 32, 240, 255 });
                r2->addShowString("废物");
            }
        }

        // 应该先护体，不显示伤害
        showNumberAnimation(2, false);

        // 然后显示伤害
        for (auto r2 : battle_roles_) {
            if (r2->BattleHurt != 0) {
                if (magic->HurtType == 0)
                {
                    r2->addShowString(convert::formatString("-%d", r2->BattleHurt), { 255, 20, 20, 255 });
                }
                else if (magic->HurtType == 1)
                {
                    r2->addShowString(convert::formatString("-%d", r2->BattleHurt), { 160, 32, 240, 255 });
                }
            }
            r2->BattleHurt = 0;
        }

        // 再来搞一波状态
        for (auto r2 : battle_roles_) {
            // 这里先都扔进去，伤害结算完了算状态，憋算了
            auto result = battleStatusManager_[r2->ID].materialize();
            for (auto const& p : result) {
                if (!p.first.hide)
                    r2->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
            }
        }
        showNumberAnimation(3);

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
    
    // 这里考虑怎么加入显示效果
    // 先加入防守方特效，注意r2先来，其实这里传递magic也行，效果不明就是的
    tryProcAndAddToManager(r2->ID, defRole_, defEffectManager_, r2, r1, nullptr);
    
    // 再加入挨打者的武功被动特效
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r2->MagicID[i], defMagic_, defEffectManager_, r2, r1, Save::getInstance()->getMagic(r2->MagicID[i]));
    }

    tryProcAndAddToManager(defAll_, defEffectManager_, r2, r1, magic);

    // 强制特效直接判断
    // 特效8 9
    // 8 攻击方的敌人的
    setStatusFromParams(atkEffectManager_.getAllEffectParams(8), r2);
    // 9 攻击方自己
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(9), r1);
    // 然后倒过来 8 防御方的敌人
    applyStatusFromParams(defEffectManager_.getAllEffectParams(8), r1);
    // 9 防御方自己
    applyStatusFromParams(defEffectManager_.getAllEffectParams(9), r2);

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

        // 特效0 武功威力增加，其实防御方也可以
        int attack = r1->Attack + (magic->Attack[level_index]+ atkEffectManager_.getEffectParam0(0)) / 4;

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
        //这个我应该也接手了..
        //思考，怎么改，怎么把calRoleDistance搞成一个变量，修改变量函数指针签名了只有
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rand_.rand_int(10) - rand_.rand_int(10);

        // 特效4 暴击
        double amplify = 100;
        amplify += atkEffectManager_.getEffectParam0(4);
        amplify += defEffectManager_.getEffectParam0(4);
        v = int(v * (amplify / 100));

        // 特效5 直接加伤害
        v += atkEffectManager_.getEffectParam0(5) + defEffectManager_.getEffectParam0(5);

        // 这个不管
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

    // 特效12 防御方加减集气（无视气防 气攻）
    r2->ProgressChange += defEffectManager_.getEffectParam0(12);

    // 特效6，特效7 上状态
    // 己方视角
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(6), r2);
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(7), r1);
    // 然后倒过来
    applyStatusFromParams(defEffectManager_.getAllEffectParams(7), r2);
    applyStatusFromParams(defEffectManager_.getAllEffectParams(6), r1);

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
    // 我忘了为什么把被动攻击效果给取消了。。。哦，好像是添加到全人物攻击里面按需求判断了
    /*
    for (int i = 0; i < r->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r->MagicID[i], atkMagic_, atkEffectManager_);
    }
    */

    std::vector<std::reference_wrapper<const std::string>> names;
    std::string mName(m->Name);
    names.push_back(mName);
    // 抬手时，触发攻击特效
    auto effects = tryProcAndAddToManager(m->ID, atkMagic_, atkEffectManager_, r, nullptr, m);
    // 简单的做个显示~ TODO 修好它
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    // 再添加人物自身的攻击特效
    effects = tryProcAndAddToManager(r->ID, atkRole_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    // 所有人物的
    effects = tryProcAndAddToManager(atkAll_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    showMagicNames(names);

    // 特效11 连击, 不控制就是无限连
    multiAtk_ += atkEffectManager_.getEffectParam0(11);

    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //非我方且被击中（即所在位置的效果层非负）
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            r2->BattleHurt = calMagicHurt(r, r2, m);
            if (m->HurtType == 0)
            {
                // r2->addShowString(convert::formatString("-%d", hurt), { 255, 20, 20, 255 }, 30);
                int prevHP = r2->HP;
                r2->HP = GameUtil::limit(r2->HP - r2->BattleHurt, 0, r2->MaxHP);
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
                r2->MP = GameUtil::limit(r2->MP - r2->BattleHurt, 0, r2->MaxMP);
                r->MP = GameUtil::limit(r->MP + r2->BattleHurt, 0, r->MaxMP);
                int hurt1 = prevMP - r2->MP;
                r->ExpGot += hurt1 / 2;
            }
        }
    }
    
    // 我需要好好想一想显示效果怎么做
    // 特效12 加集气
    r->ProgressChange += atkEffectManager_.getEffectParam0(12);

    // 打完了，清空攻击效果
    atkEffectManager_.clear();
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
            tryProcAndAddToManager(role->MagicID[i], turnMagic_, turnEffectManager_, role, nullptr, Save::getInstance()->getMagic(role->MagicID[i]));
        }
        tryProcAndAddToManager(role->ID, turnRole_, turnEffectManager_, role, nullptr, nullptr);
        tryProcAndAddToManager(turnAll_, turnEffectManager_, role, nullptr, nullptr);

        // 特效9 强制
        setStatusFromParams(speedEffectManager_.getAllEffectParams(9), role);

        // 特效7 上状态
        applyStatusFromParams(turnEffectManager_.getAllEffectParams(7), role);

        // 特效12 加集气
        role->ProgressChange += turnEffectManager_.getEffectParam0(12);

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

        auto const& result = battleStatusManager_[role->ID].materialize();
        for (auto const& p : result) {
            printf("%d自己上状态%d %d\n", role->ID, p.first.id, p.second);
            if (!p.first.hide)
                role->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
        }
        showNumberAnimation();
        // 结算回合结束后的一些特效，比如说回血内力 等一系列
        // 等我有心情再做

        turnEffectManager_.clear();
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
        tryProcAndAddToManager(speedAll_, speedEffectManager_, r, nullptr, nullptr);
        // 自己的效果
        tryProcAndAddToManager(r->ID, speedRole_, speedEffectManager_, r, nullptr, nullptr);
        // 自己武功的效果，每次集气这么算一遍 累趴了
        for (int i = 0; i < r->getLearnedMagicCount(); i++) {
            tryProcAndAddToManager(r->MagicID[i], speedMagic_, speedEffectManager_, r, nullptr, Save::getInstance()->getMagic(r->MagicID[i]));
        }
        // 强制状态 特效9
        auto forceStat = speedEffectManager_.getAllEffectParams(9);
        setStatusFromParams(forceStat, r);

        int progress = 0;
        
        // 先上/下状态，给自己，给别人也不是不行，再搞个循环，判断不在一个队伍的
        // 特效7
        applyStatusFromParams(speedEffectManager_.getAllEffectParams(7), r);

        // 特效3 集气速度
        progress = speedEffectManager_.getEffectParam0(3);
        // printf("基础集气速度%d\n", progress);
        printf("%d集气加成 %d%%\n", r->ID, speedEffectManager_.getEffectParam0(10));
        // 特效10 集气速度百分比增加
        progress = int((speedEffectManager_.getEffectParam0(10) / 100.0) * progress) + progress;

        progress = progress < 0 ? 0 : progress;

        r->Progress += progress;

        battleStatusManager_[r->ID].materialize();
        speedEffectManager_.clear();
    }
    return nullptr;
}

void BattleMod::BattleModifier::applyStatusFromParams(const std::vector<int>& params, Role * target)
{
    if (params.empty()) return;
    // [状态id, 状态强度, 状态id, 状态强度...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].incrementBattleStatusVal(params[i], params[i + 1]);
    }
}


void BattleMod::BattleModifier::setStatusFromParams(const std::vector<int>& params, Role * target)
{
    if (params.empty()) return;
    // [状态id, 状态强度, 状态id, 状态强度...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].setBattleStatusVal(params[i], params[i + 1]);
    }
}

void BattleMod::BattleModifier::showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names)
{
    // UI不好写啊，搞不出来，放弃
    if (names.empty()) return;

    std::string hugeStr = std::accumulate(std::next(names.begin()), names.end(), names[0].get(), 
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