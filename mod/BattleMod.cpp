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

    // ����
    Event::getInstance()->setRoleMagic(0, 1, 2, 999);
    Save::getInstance()->getRole(0)->MaxHP = 999;
    Save::getInstance()->getRole(0)->HP = 999;
    Save::getInstance()->getRole(0)->Defence = 80;

    YAML::Node baseNode;
    // ������������try�������������Чȫ���
    try {
        baseNode = YAML::LoadFile(PATH);
    }
    catch (...) {
        printf("yaml config missing\n");
        return;
    }

    strPool_.reserve(baseNode[u8"�ַ��������С"].as<int>());
    strPool_.push_back("");

    printf("��Ч���� %d\n", baseNode[u8"��Ч"].size());
    for (const auto& spNode : baseNode[u8"��Ч"]) {
        // ���밴˳��������Ϊ������TODO ��һ�ģ����ǲ����أ��п��ٸģ�
        assert(effects_.size() == spNode[u8"���"].as<int>());
        auto desc = PotConv::conv(spNode[u8"����"].as<std::string>(), "utf-8", "cp936");
        std::reference_wrapper<std::string> descRef(strPool_.front());
        if (!desc.empty()) {
            strPool_.push_back(desc);
            descRef = strPool_.back();
        }
        printf("%d %s\n", spNode[u8"���"].as<int>(), descRef.get().c_str());
        effects_.emplace_back(spNode[u8"���"].as<int>(), descRef);
    }

    for (const auto& bNode : baseNode[u8"ս��״̬"]) {
        int id = bNode[u8"���"].as<int>();
        assert(battleStatus_.size() == id);
        auto desc = PotConv::conv(bNode[u8"����"].as<std::string>(), "utf-8", "cp936");
        
        int max = 100;
        if (const auto& maxNode = bNode[u8"��ֵ"]) {
            max = maxNode.as<int>();
        }

        int hide = false;
        if (const auto& hideNode = bNode[u8"����"]) {
            hide = hideNode.as<bool>();
        }

        // Ĭ�ϰ�ɫ
        BP_Color c = { 255, 255, 255, 255 };
        if (const auto& cNode = bNode[u8"��ɫ"]) {
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

    // ���¿���refactor��TODO �ģ�����
    if (baseNode[u8"�书"]) {
        printf("number of wg %d\n", baseNode[u8"�书"].size());
        for (const auto& magicNode : baseNode[u8"�书"]) {
            int wid = magicNode[u8"�书���"].as<int>();
            if (magicNode[u8"����"]) {
                printf("number of atk %d\n", magicNode[u8"����"].size());
                readIntoMapping(magicNode[u8"����"], atkMagic_[wid]);
            }
            if (magicNode[u8"����"]) {
                readIntoMapping(magicNode[u8"����"], defMagic_[wid]);
            }
            if (magicNode[u8"����"]) {
                readIntoMapping(magicNode[u8"����"], speedMagic_[wid]);
            }
            if (magicNode[u8"�غ�"]) {
                readIntoMapping(magicNode[u8"�غ�"], turnMagic_[wid]);
            }
        }
    }

    if (auto const& allNode = baseNode[u8"ȫ����"]) {
        printf("size %d\n", allNode.size());
        if (allNode[u8"����"]) {
            readIntoMapping(allNode[u8"����"], atkAll_);
        }
        if (allNode[u8"����"]) {
            readIntoMapping(allNode[u8"����"], defAll_);
        }
        if (allNode[u8"����"]) {
            readIntoMapping(allNode[u8"����"], speedAll_);
        }
        if (allNode[u8"�غ�"]) {
            readIntoMapping(allNode[u8"�غ�"], turnAll_);
        }
    }

    if (baseNode[u8"����"]) {
        for (const auto& personNode : baseNode[u8"����"]) {
            int pid = personNode[u8"������"].as<int>();
            if (personNode[u8"����"]) {
                readIntoMapping(personNode[u8"����"], atkRole_[pid]);
            }
            if (personNode[u8"����"]) {
                readIntoMapping(personNode[u8"����"], defRole_[pid]);
            }
            if (personNode[u8"����"]) {
                readIntoMapping(personNode[u8"����"], speedRole_[pid]);
            }
            if (personNode[u8"�غ�"]) {
                readIntoMapping(personNode[u8"�غ�"], turnRole_[pid]);
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
    else if (node[u8"״̬"]) {
        // �����
        int statusID = node[u8"״̬"].as<int>();
        f = [this, statusID](const Role* character, const Magic* wg) {
            if (character == nullptr) return 0;
            return battleStatusManager_[character->ID].getBattleStatusVal(statusID);
        };
    }
    else if (const auto& varNode = node[u8"����"]) {
        if (const auto& targetNode = varNode[u8"����"]) {
            target = static_cast<VarTarget>(targetNode.as<int>());
        }
        int varID = varNode[u8"���"].as<int>();
        // �������
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
            // ����MOD��ϲ������ҹ�
            // Ӳ���룬д�鱾id
            // ��lua�汾���ﳭ�ģ�id��144��ʼ
            // �ܼ����ľ��� ������booksֻ����һ�飬��Ϊս���ڼ䲻�����������
            int books = 0;
            for (int i = 144; i < 144 + 14; i++)
                books += Save::getInstance()->getItemCountInBag(i);
            f = [books](const Role* c, const Magic* wg) {
                // �����ѷ���Ч����
                // ���Ҫ�Եз���Ч������Ӹ�����id
                if (c->Team != 0) return 0;
                return books;
            };
            break;
        }
        case zzz::var_cur_wg_level: {
            // getMagicOfRoleIndex ���Ǹ�const���Һܷ�
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
            // ��ȡ�������
            int wgID = varNode[u8"����"].as<int>();
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
            int itemID = varNode[u8"����"].as<int>();
            int num = Save::getInstance()->getItemCountInBag(itemID);
            f = [num](const Role* c, const Magic* wg) {
                // �����ѷ���Ч����
                // ���Ҫ�Եз���Ч������Ӹ�����id
                if (c->Team != 0) return 0;
                return num;
            };
            break;
        }
        case zzz::var_has_wg: {
            const auto& paramNode = varNode[u8"����"];
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
            int pid = varNode[u8"����"].as<int>();
            f = [this, pid](const Role* c, const Magic* wg) {
                if (c == nullptr) return 0;
                if (c->ID == pid) return 1;
                return 0;
            };
            break;
        }
        case zzz::var_has_status: {
            const auto& paramNode = varNode[u8"����"];
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
                // �����䣬������const ��û��
                Role* cc = const_cast<Role*>(c);
                Magic* mm = const_cast<Magic*>(wg);
                // ���߿�id�� Save:: Ҳ����
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
    if (const auto& randNode = node[u8"���"]) {
        const auto& rangeNode = randNode[u8"��Χ"];
        if (rangeNode.IsSequence()) {
            std::vector<int> range;
            for (const auto& n : rangeNode) {
                range.push_back(n.as<int>());
            }
            adder = std::make_unique<RandomAdder>(std::move(range));
        }
        else {
            // ��ͻȻ��Ӧ�������Ҳ������variable TODO �ĳɶ�variable
            int min = rangeNode[u8"��С"].as<int>();
            int max = rangeNode[u8"���"].as<int>();
            adder = std::make_unique<RandomAdder>(min, max);
        }
    }
    else if (const auto& linearNode = node[u8"����"]) {
        adder = std::make_unique<LinearAdder>(linearNode[u8"ϵ��"].as<double>(), readVariable(linearNode));
    }
    return adder;
}

VariableParam BattleMod::BattleModifier::readVariableParam(const YAML::Node & node)
{
    if (node.IsScalar()) {
        return VariableParam(node.as<int>());
    }
    const auto& pNode = node[u8"�ɱ����"];
    VariableParam vp(pNode[u8"����"].as<int>());
    if (const auto& adders = pNode[u8"�ӳ�"]) {
        for (const auto& adder : adders) {
            vp.addAdder(readAdder(adder));
        }
    }
    return vp;
}

Conditions BattleMod::BattleModifier::readConditions(const YAML::Node & node)
{
    Conditions conditions;
    const auto& condNode = node[u8"����"];
    for (const auto& cond : condNode) {
        conditions.push_back(std::move(readCondition(cond)));
    }
    return conditions;
}

Condition BattleMod::BattleModifier::readCondition(const YAML::Node & node)
{
    const auto& condNode = node[u8"����"];
    printf("%d\n", node.size());
    Variable lhs = readVariable(node[u8"���"]);
    Variable rhs = readVariable(node[u8"�ұ�"]);
    ConditionOp opID = static_cast<ConditionOp>(node[u8"�Ա�"].as<int>());
    std::function<bool(int, int)> op;
    switch (opID) {
    case ConditionOp::equal : op = std::equal_to<int>(); break;
    case ConditionOp::greater_than: op = std::greater<int>(); break;
    case ConditionOp::greater_than_equal: op = std::greater_equal<int>(); break;
    }
    return Condition(lhs, rhs, op);
}

EffectParamPair BattleModifier::readEffectParamPair(const YAML::Node& node) {
    int id = node[u8"���"].as<int>();
    auto display = PotConv::conv(node[u8"��ʾ"].as<std::string>(), "utf-8", "cp936");
    std::reference_wrapper<std::string> displayRef(strPool_.front());
    if (!display.empty()) {
        strPool_.push_back(display);
        displayRef = strPool_.back();
    }
    printf("���� %d %s\n", id, displayRef.get().c_str());
    // ����Ч�������Ժ�Ҫ������
    EffectParamPair epp(effects_[id], displayRef);
    if (node[u8"Ч������"].IsScalar() || node[u8"Ч������"].IsMap()) {
        epp.addParam(readVariableParam(node[u8"Ч������"]));
    }
    else {
        for (const auto& param : node[u8"Ч������"]) {
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
    int probType = node[u8"������ʽ"].as<int>();
    ProcProbability prob = static_cast<ProcProbability>(probType);
    std::unique_ptr<ProccableEffect> pe;
    switch (prob) {
    case ProcProbability::random: {
        pe = std::make_unique<EffectSingle>(readVariableParam(node[u8"��������"]), readEffectParamPairs(node[u8"��Ч"]));
        break;
    }
    case ProcProbability::distributed: {
        auto group = std::make_unique<EffectWeightedGroup>(readVariableParam(node[u8"�ܱ���"]));
        for (const auto& eNode : node[u8"��Ч"]) {
            group->addProbEPP(readVariableParam(eNode[u8"��������"]), readEffectParamPair(eNode));
        }
        pe = std::move(group);
        break;
    }
    case ProcProbability::prioritized: {
        auto group = std::make_unique<EffectPrioritizedGroup>();
        for (const auto& eNode : node[u8"��Ч"]) {
            group->addProbEPP(readVariableParam(eNode[u8"��������"]), readEffectParamPair(eNode));
        }
        pe = std::move(group);
        break;
    }
    case ProcProbability::counter: {
        pe = std::make_unique<EffectCounter>(readVariableParam(node[u8"����"]), 
                                             readVariableParam(node[u8"��������"]), 
                                             readEffectParamPairs(node[u8"��Ч"]));
        break;
    }
    }
    if (pe) {
        if (const auto& reqNode = node[u8"����"]) {
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

// �ٸ���EffectIntsPair�������Ҿ��ø��������������
std::vector<EffectIntsPair> BattleMod::BattleModifier::tryProcAndAddToManager(int id, const EffectsTable & table, EffectManager & manager, 
                                                                              const Role * attacker, const Role * defender, const Magic * wg)
{
    std::vector<EffectIntsPair> procd;
    auto iter = table.find(id);
    if (iter != table.end()) {
        // TODO �������Ǹ�
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
        }    //�������˳���Ϸ��������û��ѡ�书
        r->ActTeam = 1;
        //level_index��ʾ��0��9����level��0��999
        int level_index = r->getMagicLevelIndex(magic->ID);
        calSelectLayerByMagic(r->X(), r->Y(), r->Team, magic, level_index);
        //ѡ��Ŀ��
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r, magic, level_index);
        int selected = battle_cursor_->run();
        //ȡ��ѡ��Ŀ�������½���ѡ�书
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
    // �������ж���Ҫ�ĵ�
    multiAtk_ = 0;
    for (int i = 0; i <= multiAtk_; i++)
    {
        calMagiclHurtAllEnemies(r, magic);

        useMagicAnimation(r, magic);

        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);

        for (auto r2 : battle_roles_) {
            if (r2 != r) {
                r2->Eft = 6;
                r2->addShowString("������è������", { 255, 20, 20, 255 });
                r2->addShowString("�����޵�", { 160, 32, 240, 255 });
                r2->addShowString("����");
            }
        }

        // Ӧ���Ȼ��壬����ʾ�˺�
        showNumberAnimation(2, false);

        // Ȼ����ʾ�˺�
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

        // ������һ��״̬
        for (auto r2 : battle_roles_) {
            // �����ȶ��ӽ�ȥ���˺�����������״̬��������
            auto result = battleStatusManager_[r2->ID].materialize();
            for (auto const& p : result) {
                if (!p.first.hide)
                    r2->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
            }
        }
        showNumberAnimation(3);

        //��ѧ�ȼ�����
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

//r1ʹ���书magic����r2���˺������Ϊһ����
int BattleModifier::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    // simulation�Ļ��ƹ�ȥ������
    if (simulation_)
        return BattleScene::calMagicHurt(r1, r2, magic);

    assert(defEffectManager_.size() == 0);
    
    // ���￼����ô������ʾЧ��
    // �ȼ�����ط���Ч��ע��r2��������ʵ���ﴫ��magicҲ�У�Ч���������ǵ�
    tryProcAndAddToManager(r2->ID, defRole_, defEffectManager_, r2, r1, nullptr);
    
    // �ټ��밤���ߵ��书������Ч
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r2->MagicID[i], defMagic_, defEffectManager_, r2, r1, Save::getInstance()->getMagic(r2->MagicID[i]));
    }

    tryProcAndAddToManager(defAll_, defEffectManager_, r2, r1, magic);

    // ǿ����Чֱ���ж�
    // ��Ч8 9
    // 8 �������ĵ��˵�
    setStatusFromParams(atkEffectManager_.getAllEffectParams(8), r2);
    // 9 �������Լ�
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(9), r1);
    // Ȼ�󵹹��� 8 �������ĵ���
    applyStatusFromParams(defEffectManager_.getAllEffectParams(8), r1);
    // 9 �������Լ�
    applyStatusFromParams(defEffectManager_.getAllEffectParams(9), r2);

    // �����˺�������������������
    int progressHurt = 0;

    // ��Ч1 ����
    progressHurt += atkEffectManager_.getEffectParam0(1);
    // ��Ч2 ����
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

        // ��Ч0 �书�������ӣ���ʵ������Ҳ����
        int attack = r1->Attack + (magic->Attack[level_index]+ atkEffectManager_.getEffectParam0(0)) / 4;

        int defence = r2->Defence;

        //װ����Ч��
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
        //����˥��
        //�����Ӧ��Ҳ������..
        //˼������ô�ģ���ô��calRoleDistance���һ���������޸ı�������ָ��ǩ����ֻ��
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rand_.rand_int(10) - rand_.rand_int(10);

        // ��Ч4 ����
        double amplify = 100;
        amplify += atkEffectManager_.getEffectParam0(4);
        amplify += defEffectManager_.getEffectParam0(4);
        v = int(v * (amplify / 100));

        // ��Ч5 ֱ�Ӽ��˺�
        v += atkEffectManager_.getEffectParam0(5) + defEffectManager_.getEffectParam0(5);

        // �������
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

    // ��Ч12 �������Ӽ��������������� ������
    r2->ProgressChange += defEffectManager_.getEffectParam0(12);

    // ��Ч6����Ч7 ��״̬
    // �����ӽ�
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(6), r2);
    applyStatusFromParams(atkEffectManager_.getAllEffectParams(7), r1);
    // Ȼ�󵹹���
    applyStatusFromParams(defEffectManager_.getAllEffectParams(7), r2);
    applyStatusFromParams(defEffectManager_.getAllEffectParams(6), r1);

    // �����ӣ�ֻ����
    if (progressHurt > 0) {
        r2->ProgressChange -= progressHurt;
        printf("�����仯%d\n", r2->ProgressChange);
    }
    defEffectManager_.clear();
    return hurt;
}

int BattleModifier::calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation)
{
    // simulation�Ҿͼ�װ��֪�����ٺ�
    // ����ԭ���벻�ع�һ�£��Ѹ�
    // ������BattleScene�ǻ���˭��calMagicHurt
    // Ӧ�û���override��ģ��Ӹ�simulation_�ƹ�ȥ����
    if (simulation) {
        simulation_ = true;
        return BattleScene::calMagiclHurtAllEnemies(r, m, true);
    }
    else {
        simulation_ = false;
    }

    assert(atkEffectManager_.size() == 0);

    // ����������������Ч�����ﱻ��Ҳ��ÿ�ι������ж�һ��
    // ������Ϊʲô�ѱ�������Ч����ȡ���ˡ�����Ŷ����������ӵ�ȫ���﹥�����水�����ж���
    /*
    for (int i = 0; i < r->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r->MagicID[i], atkMagic_, atkEffectManager_);
    }
    */

    std::vector<std::reference_wrapper<const std::string>> names;
    std::string mName(m->Name);
    names.push_back(mName);
    // ̧��ʱ������������Ч
    auto effects = tryProcAndAddToManager(m->ID, atkMagic_, atkEffectManager_, r, nullptr, m);
    // �򵥵�������ʾ~ TODO �޺���
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    // �������������Ĺ�����Ч
    effects = tryProcAndAddToManager(r->ID, atkRole_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    // ���������
    effects = tryProcAndAddToManager(atkAll_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        if (!effect.description.empty())
            names.push_back(std::cref(effect.description));
    }

    showMagicNames(names);

    // ��Ч11 ����, �����ƾ���������
    multiAtk_ += atkEffectManager_.getEffectParam0(11);

    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //���ҷ��ұ����У�������λ�õ�Ч����Ǹ���
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
    
    // ����Ҫ�ú���һ����ʾЧ����ô��
    // ��Ч12 �Ӽ���
    r->ProgressChange += atkEffectManager_.getEffectParam0(12);

    // �����ˣ���չ���Ч��
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
        //ѡ��λ�����������еĵ�һ����
        role = battle_roles_[0];

        //����һ�����Ѿ��ж�����˵�������˶��ж��ˣ�������ж�״̬����������
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

    //��λ
    man_x_ = role->X();
    man_y_ = role->Y();
    select_x_ = role->X();
    select_y_ = role->Y();
    head_self_->setRole(role);
    head_self_->setState(Element::Pass);

    //�ж�
    action(role);

    //������˳ɹ��ж�������ŵ���β
    if (role->Acted)
    {
        // �ж������������
        // �书�����ж���Ч
        for (int i = 0; i < role->getLearnedMagicCount(); i++) {
            tryProcAndAddToManager(role->MagicID[i], turnMagic_, turnEffectManager_, role, nullptr, Save::getInstance()->getMagic(role->MagicID[i]));
        }
        tryProcAndAddToManager(role->ID, turnRole_, turnEffectManager_, role, nullptr, nullptr);
        tryProcAndAddToManager(turnAll_, turnEffectManager_, role, nullptr, nullptr);

        // ��Ч9 ǿ��
        setStatusFromParams(speedEffectManager_.getAllEffectParams(9), role);

        // ��Ч7 ��״̬
        applyStatusFromParams(turnEffectManager_.getAllEffectParams(7), role);

        // ��Ч12 �Ӽ���
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

        // �ж��� ����Ц����������������ЧȫȨ�ӹ��ˣ�����ʲô��
        // ����.. TODO �ӹ�
        poisonEffect(role);

        auto const& result = battleStatusManager_[role->ID].materialize();
        for (auto const& p : result) {
            printf("%d�Լ���״̬%d %d\n", role->ID, p.first.id, p.second);
            if (!p.first.hide)
                role->addShowString(convert::formatString("%s %d", p.first.display.c_str(), p.second), p.first.color);
        }
        showNumberAnimation();
        // ����غϽ������һЩ��Ч������˵��Ѫ���� ��һϵ��
        // ��������������

        turnEffectManager_.clear();
    }

    //��������˵�����
    clearDead();

    //���ս�����
    int battle_result = checkResult();

    //�ҷ�ʤ
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

// ����������
Role* BattleModifier::semiRealPickOrTick() {
    //������ͼѡ��һ��
    for (auto r : battle_roles_)
    {
        if (r->Progress > 1000)
        {
            return r;
        }
    }
    
    //�޷�ѡ���ˣ����������˽�����������
    for (auto r : battle_roles_)
    {
        // �����Ǹ�ʲô��������
        
        assert(speedEffectManager_.size() == 0);

        // ����ȫ��Ч��
        tryProcAndAddToManager(speedAll_, speedEffectManager_, r, nullptr, nullptr);
        // �Լ���Ч��
        tryProcAndAddToManager(r->ID, speedRole_, speedEffectManager_, r, nullptr, nullptr);
        // �Լ��书��Ч����ÿ�μ�����ô��һ�� ��ſ��
        for (int i = 0; i < r->getLearnedMagicCount(); i++) {
            tryProcAndAddToManager(r->MagicID[i], speedMagic_, speedEffectManager_, r, nullptr, Save::getInstance()->getMagic(r->MagicID[i]));
        }
        // ǿ��״̬ ��Ч9
        auto forceStat = speedEffectManager_.getAllEffectParams(9);
        setStatusFromParams(forceStat, r);

        int progress = 0;
        
        // ����/��״̬�����Լ���������Ҳ���ǲ��У��ٸ��ѭ�����жϲ���һ�������
        // ��Ч7
        applyStatusFromParams(speedEffectManager_.getAllEffectParams(7), r);

        // ��Ч3 �����ٶ�
        progress = speedEffectManager_.getEffectParam0(3);
        // printf("���������ٶ�%d\n", progress);
        printf("%d�����ӳ� %d%%\n", r->ID, speedEffectManager_.getEffectParam0(10));
        // ��Ч10 �����ٶȰٷֱ�����
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
    // [״̬id, ״̬ǿ��, ״̬id, ״̬ǿ��...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].incrementBattleStatusVal(params[i], params[i + 1]);
    }
}


void BattleMod::BattleModifier::setStatusFromParams(const std::vector<int>& params, Role * target)
{
    if (params.empty()) return;
    // [״̬id, ״̬ǿ��, ״̬id, ״̬ǿ��...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].setBattleStatusVal(params[i], params[i + 1]);
    }
}

void BattleMod::BattleModifier::showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names)
{
    // UI����д�����㲻����������
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
    // ����Ҳ���ֱ��
    boxRoot->setText(hugeStr);
    boxRoot->setPosition(w/2.0 - (hugeStr.size()/4.0) * fontSize, 50);
    boxRoot->setFontSize(fontSize);
    boxRoot->setStayFrame(stayFrame);
    boxRoot->setAlphaBox({ 255, 165, 79, 255 }, { 0, 0, 0, 128 });
    // �����趨��ɫ�ɡ�����
    boxRoot->setTextColor({ 255, 165, 79, 255 });
    boxRoot->run();
}