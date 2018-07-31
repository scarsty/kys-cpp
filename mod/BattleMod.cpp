#include "BattleMod.h"
#include "GameUtil.h"
#include "Save.h"
#include "Font.h"
#include "libconvert.h"
#include "PotConv.h"
#include <cassert>
#include <unordered_set>

using namespace BattleMod;

BattleMod::SpecialEffect::SpecialEffect(int id, const std::string & desc) :
    id(id), description(desc)
{
}

BattleMod::EffectIntsPair::EffectIntsPair(const SpecialEffect & effect, const std::string & desc) :
    effect(effect), description(desc)
{
}

int BattleMod::EffectIntsPair::getParam0()
{
    return params_[0];
}

const std::vector<int> & BattleMod::EffectIntsPair::getParams()
{
    return params_;
}

void BattleMod::EffectIntsPair::addParam(int p)
{
    params_.push_back(p);
}


EffectIntsPair & BattleMod::EffectIntsPair::operator+=(const EffectIntsPair & rhs)
{
    // id ������ͬ
    if (effect.id != rhs.effect.id) {
        return *this;
    }

    // ��״̬��ʱ�򣬲���������
    // ��ô�����أ���Ҳ��֪������ô���
    if (effect.id == 6 || effect.id == 7) {
        std::unordered_map<int, int> statusStrMap;
        // ����ÿ��һ���Ǳ�ţ��ڶ���ǿ��
        for (std::size_t i = 0; i < rhs.params_.size() / 2; i += 2) {
            statusStrMap[rhs.params_[i]] = rhs.params_[i + 1];
        }
        for (std::size_t i = 0; i < params_.size() / 2; i += 2) {
            statusStrMap[params_[i]] = params_[i + 1];
        }
        params_.clear();
        for (const auto& p : statusStrMap) {
            params_.push_back(p.first);
            params_.push_back(p.second);
        }
        return *this;
    }

    if (params_.size() != rhs.params_.size()) {
        return *this;
    }

    // һ��ֱ�Ӽ�����
    for (std::size_t i = 0; i < params_.size(); i++) {
        params_[i] += rhs.params_[i];
    }
    return *this;
}

BattleMod::Variable::Variable(BattleInfoFunc func, VarTarget target) : func_(func), target_(target)
{
}

// ��ʵ���attacker defender����һ��������
// TODO rename attacker/defender
int BattleMod::Variable::getVal(const Role * attacker, const Role * defender, const Magic * wg) const
{
    switch (target_) {
    case VarTarget::Other: return func_(defender, wg);
    default: return func_(attacker, wg);
    }
    return 0;
}

BattleMod::RandomAdder::RandomAdder(int min, int max) : min_(min), max_(max)
{
    if (max_ < min_)
        max_ = min_;
}

BattleMod::RandomAdder::RandomAdder(std::vector<int>&& items) : items_(std::move(items))
{
}

int BattleMod::RandomAdder::getVal(const Role * attacker, const Role * defender, const Magic * wg) const
{
    if (items_.empty()) {
        // ����è��ʦ������ܲ����ð�
        // 4, 10: 10-4 = 6 -> 7 -> 0~6
        return BattleModifier::rng.rand_int(max_ - min_ + 1) + min_;
    }
    return items_[BattleModifier::rng.rand_int(items_.size())];
}

BattleMod::LinearAdder::LinearAdder(double k, Variable&& v) : k_(k), v_(std::move(v))
{
}

int BattleMod::LinearAdder::getVal(const Role * attacker, const Role * defender, const Magic * wg) const
{
    return int(k_ * v_.getVal(attacker, defender, wg));
}

BattleMod::VariableParam::VariableParam(int base) : base_(base)
{
}

int BattleMod::VariableParam::getVal(const Role * attacker, const Role * defender, const Magic * wg) const
{
    int sum = 0;
    // Adder ֮�� �ţ����Ҽ��������Ժ���������� max/min��
    for (const auto& adder : adders_) {
        sum += adder->getVal(attacker, defender, wg);
    }
    // �ǲ��ǾͿ�attacker�͹����أ���������Ч��ʱ���Ƿ�����߱����attacker��
    // ���������ʽһ��Ҫ��
    return sum;
}

void BattleMod::VariableParam::addAdder(std::unique_ptr<Adder> adder)
{
    adders_.push_back(std::move(adder));
}


BattleMod::Condition::Condition(Variable left, Variable right, std::function<bool(int, int)> binOp) :
    left_(left), right_(right), binOp_(binOp)
{
}

bool BattleMod::Condition::check(const Role * attacker, const Role * defender, const Magic * wg) const
{
    return binOp_(left_.getVal(attacker, defender, wg), right_.getVal(attacker, defender, wg));
}

BattleMod::EffectParamPair::EffectParamPair(const SpecialEffect & effect, const std::string & desc) : eip_(effect, desc)
{
}

EffectIntsPair BattleMod::EffectParamPair::materialize(const Role * attacker, const Role * defender, const Magic * wg) const
{
    // ��Ҫһ��copy
    EffectIntsPair eip = eip_;
    for (const auto& param : params_) {
        eip.addParam(param.getVal(attacker, defender, wg));
    }
    return eip;
}

void BattleMod::EffectParamPair::addParam(VariableParam && vp)
{
    params_.push_back(std::move(vp));
}

void BattleMod::ProccableEffect::addCondition(Condition&& c)
{
    conditions_.push_back(std::move(c));
}

BattleMod::EffectSingle::EffectSingle(VariableParam && p, EffectParamPair && epp) :
    percentProb_(std::move(p)), effectPair_(std::move(epp))
{
}

// ����EffectIntsPair�Ǹ�copy������ֱ�ӷ���ptr�ˣ�������Ϲ��ֱ���������Ҫ���std::optional(C++17)���������heap allocated std::unique_ptr�Ҳ�ϲ��
std::optional<EffectIntsPair> BattleMod::EffectSingle::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    // rand [0 1)
    // ����100��ȡ��������˵ʵ�ʷ�Χ��0-99
    if (BattleModifier::rng.rand_int(100) < percentProb_.getVal(attacker, defender, wg)) {
        return effectPair_.materialize(attacker, defender, wg);
    }
    return std::nullopt;
}

std::optional<EffectIntsPair> BattleMod::EffectWeightedGroup::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    // printf("���Դ���\n");
    // [0 total)
    int t = BattleModifier::rng.rand_int(total_.getVal(attacker, defender, wg));
    int c = 0;
    for (const auto& p : group_) {
        c += p.first.getVal(attacker, defender, wg);
        if (t < c) {
            return p.second.materialize(attacker, defender, wg);
        }
    }
    return std::nullopt;
}

BattleMod::EffectWeightedGroup::EffectWeightedGroup(VariableParam && total) : total_(std::move(total))
{
}

void BattleMod::EffectWeightedGroup::addProbEPP(VariableParam && weight, EffectParamPair && epp)
{
    group_.emplace_back(std::move(weight), std::move(epp));
}

BattleMod::EffectPrioritizedGroup::EffectPrioritizedGroup()
{
}

// ���￼���ع�һ��
std::optional<EffectIntsPair> BattleMod::EffectPrioritizedGroup::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    for (const auto& p : group_) {
        if (BattleModifier::rng.rand_int(100) < p.first.getVal(attacker, defender, wg)) {
            return p.second.materialize(attacker, defender, wg);
        }
    }
    return std::nullopt;
}

void BattleMod::EffectPrioritizedGroup::addProbEPP(VariableParam && weight, EffectParamPair && epp)
{
    group_.emplace_back(std::move(weight), std::move(epp));
}

BattleMod::EffectCounter::EffectCounter(VariableParam && total, VariableParam && add, EffectParamPair && epp) :
    total_(std::move(total)), add_(std::move(add)), effectPair_(std::move(epp))
{
}

std::optional<EffectIntsPair> BattleMod::EffectCounter::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (attacker == nullptr) return {};

    int id = attacker->ID;
    auto& count = counter_[id];
    count += add_.getVal(attacker, defender, wg);
    if (count > total_.getVal(attacker, defender, wg)) {
        count = 0;
        return effectPair_.materialize(attacker, defender, wg);
    }
    return {};
}

bool BattleMod::EffectManager::hasEffect(int eid)
{
    return epps_.find(eid) != epps_.end();
}

int BattleMod::EffectManager::getEffectParam0(int eid)
{
    auto iter = epps_.find(eid);
    // ����0����Ӧ��û����ɡ���
    if (iter == epps_.end()) return 0;
    return iter->second.getParam0();
}

std::vector<int> BattleMod::EffectManager::getAllEffectParams(int eid)
{
    auto iter = epps_.find(eid);
    if (iter == epps_.end()) return {};
    return iter->second.getParams();
}

EffectIntsPair * BattleMod::EffectManager::getEPP(int eid)
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

void BattleMod::EffectManager::addEPP(const EffectIntsPair & ewp)
{
    epps_.insert({ ewp.effect.id, ewp });
}

std::size_t BattleMod::EffectManager::size()
{
    return epps_.size();
}

void BattleMod::EffectManager::clear()
{
    epps_.clear();
}

RandomDouble BattleModifier::rng;

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
    // ������������try�������������Чȫ���
    try {
        YAML::Node baseNode = YAML::LoadFile(PATH);
    }
    catch (...) {
        printf("yaml config missing\n");
        return;
    }
    // ���������ȫ�����ҵ����ݽṹ��(��STL)
    // Ȼ�����д��validator֮���
    printf("��Ч���� %d\n", baseNode[u8"��Ч"].size());
    for (const auto& spNode : baseNode[u8"��Ч"]) {
        // ���밴˳��������Ϊ������TODO ��һ�ģ����ǲ�����
        assert(effects_.size() == spNode[u8"���"].as<int>());
        auto desc = PotConv::conv(spNode[u8"����"].as<std::string>(), "utf-8", "cp936");
        strPool_.push_back(desc);
        printf("%d %s\n", spNode[u8"���"].as<int>(), desc.c_str());
        effects_.emplace_back(spNode[u8"���"].as<int>(), strPool_.back());
    }

    for (const auto& bNode : baseNode[u8"ս��״̬"]) {
        int id = bNode[u8"���"].as<int>();
        assert(battleStatus_.size() == id);
        auto desc = PotConv::conv(bNode[u8"����"].as<std::string>(), "utf-8", "cp936");
        int max = 100;
        if (const auto& maxNode = bNode[u8"��ֵ"]) {
            max = bNode.as<int>();
        }
        strPool_.push_back(desc);
        battleStatus_.emplace_back(id, max, strPool_.back());
    }

    // ���¿���refactor��TODO �ģ�����
    if (baseNode[u8"�书"]) {
        for (const auto& magicNode : baseNode[u8"�书"]) {
            int wid = magicNode[u8"�书���"].as<int>();
            if (magicNode[u8"����"]) {
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
    if (node[u8"״̬"]) {
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
            var_wg_type = 34
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
        case zzz::var_char_medicine: ATTR(Medcine);
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
    VariableParam vp(node[u8"����"].as<int>());
    if (const auto& adders = node[u8"�ӳ�"]) {
        for (const auto& adder : adders) {
            vp.addAdder(readAdder(adder));
        }
    }
    return vp;
}

Condition BattleMod::BattleModifier::readCondition(const YAML::Node & node)
{
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
    strPool_.push_back(display);
    printf("���� %s\n", display.c_str());
    // ����Ч�������Ժ�Ҫ������
    EffectParamPair epp(effects_[id], strPool_.back());
    if (node[u8"Ч������"].IsScalar()) {
        epp.addParam(readVariableParam(node[u8"Ч������"]));
    }
    else {
        for (const auto& param : node[u8"Ч������"]) {
            epp.addParam(readVariableParam(param));
        }
    }
    return epp;
}



std::unique_ptr<ProccableEffect> BattleModifier::readProccableEffect(const YAML::Node& node) {
    int probType = node[u8"������ʽ"].as<int>();
    ProcProbability prob = static_cast<ProcProbability>(probType);
    std::unique_ptr<ProccableEffect> pe;
    switch (prob) {
    case ProcProbability::random: {
        pe = std::make_unique<EffectSingle>(readVariableParam(node[u8"��Ч"][u8"��������"]), readEffectParamPair(node[u8"��Ч"]));
        break;
    }
    case ProcProbability::distributed: {
        auto group = std::make_unique<EffectWeightedGroup>(node[u8"�ܱ���"].as<int>());
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
                                             readVariableParam(node[u8"��Ч"][u8"��������"]), 
                                             readEffectParamPair(node[u8"��Ч"]));
        break;
    }
    }
    if (pe) {
        if (const auto& reqNode = node[u8"����"]) {
            for (const auto& condNode : reqNode) {
                pe->addCondition(readCondition(condNode));
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
        if (auto epp = effect->proc(attacker, defender, wg)) {
            manager.addEPP(epp.value());
            procd.push_back(epp.value());
            printf("����Ч�� %s\n", epp->description.c_str());
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
            if (auto epp = effect->proc(attacker, defender, wg)) {
                manager.addEPP(epp.value());
                procd.push_back(epp.value());
                printf("����Ч�� %s\n", epp->description.c_str());
            }
        }
    }
    return procd;
}

BattleMod::BattleStatus::BattleStatus(int id, int max, const std::string & display) : id(id), max(max), display(display)
{
}



int BattleMod::BattleStatusManager::myLimit(int & cur, int add, int min, int max)
{
    // ����èд�Ĳ�����
    int curTemp = cur;
    cur = GameUtil::limit(cur + add, min, max);
    return curTemp - cur;
}

int BattleMod::BattleStatusManager::getBattleStatusVal(int statusID)
{
    switch (statusID) {
        // �⼸��ƾʲô��һ����Ӧ��ȫ��ͳһ��
        case 0: return r_->Hurt;
        case 1: return r_->Poison;
        case 2: return r_->PhysicalPower;
        default: break;
    }
    return actualStatusVal_[statusID];
}

void BattleMod::BattleStatusManager::incrementBattleStatusVal(int statusID, int val)
{
    tempStatusVal_[statusID] += val;
}

void BattleMod::BattleStatusManager::initStatus(Role * r, const std::vector<BattleStatus>* status)
{
    status_ = status;
    r_ = r;
}

std::vector<std::pair<const BattleStatus&, int>> BattleMod::BattleStatusManager::materialize()
{
    std::vector<std::pair<const BattleStatus&, int>> changes;
    for (const auto& p : tempStatusVal_) {
        int add = 0;
        switch (p.first) {
        case 0: add = myLimit(r_->Hurt, p.second, 0, Role::getMaxValue()->Hurt); break;
        case 1: add = myLimit(r_->Poison, p.second, 0, Role::getMaxValue()->Poison); break;
        case 2: add = myLimit(r_->PhysicalPower, p.second, 0, Role::getMaxValue()->PhysicalPower); break;
        default: add = myLimit(actualStatusVal_[p.first], p.second, (*status_)[p.first].min, (*status_)[p.first].max); break;
        }
        changes.emplace_back((*status_)[p.first], add);
    }
    return changes;
}

void BattleModifier::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);

    battleStatusManager_[r->ID].initStatus(r, &status_);
}

void BattleMod::BattleModifier::useMagic(Role * r, Magic * magic)
{
    int level_index = r->getMagicLevelIndex(magic->ID);
    // �������ж���Ҫ�ĵ�
    for (int i = 0; i <= GameUtil::sign(r->AttackTwice); i++)
    {
        calMagiclHurtAllEnemies(r, magic);

        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);
        
        useMagicAnimation(r, magic);
        showNumberAnimation();
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
        int attack = r1->Attack + magic->Attack[level_index] / 3;
        // ��Ч0 �书��������
        attack += atkEffectManager_.getEffectParam0(0) / 3;
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

    // ��Ч6����Ч7 ��״̬
    auto enemyStat = atkEffectManager_.getAllEffectParams(6);
    applyStatusFromParams(enemyStat, r2);
    auto selfStat = atkEffectManager_.getAllEffectParams(7);
    applyStatusFromParams(selfStat, r1);
    // Ȼ�󵹹���
    enemyStat = defEffectManager_.getAllEffectParams(7);
    applyStatusFromParams(enemyStat, r2);
    selfStat = defEffectManager_.getAllEffectParams(6);
    applyStatusFromParams(selfStat, r1);

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
        names.push_back(std::cref(effect.description));
    }

    // �������������Ĺ�����Ч
    effects = tryProcAndAddToManager(r->ID, atkRole_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        names.push_back(std::cref(effect.description));
    }

    // ���������
    effects = tryProcAndAddToManager(atkAll_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        names.push_back(effect.description);
    }

    showMagicNames(names);

    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //���ҷ��ұ����У�������λ�õ�Ч����Ǹ���
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
                // r2->ProgressChange -= -hurt / 5;
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

        // �����ȶ��ӽ�ȥ���˺�����������״̬
        auto result = battleStatusManager_[r2->ID].materialize();
        for (auto const& p : result) {
            if (p.first.show) {
                // �����Ҫ��
                r2->ShowString += convert::formatString(" %s %d", p.first.display, p.second);
            }
        }
    }
    
    // ����Ҫ�ú���һ����ʾЧ����ô��

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

        // ��Ч7 ��״̬
        applyStatusFromParams(turnEffectManager_.getAllEffectParams(7), role);

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

        // �ж��� ����Ц����������ЧȫȨ�ӹ��ˣ�����ʲô��
        // poisonEffect(role);

        auto const& result = battleStatusManager_[role->ID].materialize();
        for (auto const& p : result) {
            // ���������ô��ʾ��
            if (p.first.show) {
                // TODO ������ѡ��ɫ��������
                role->ShowString = convert::formatString("%s %d", p.first.display, p.second);
                // �����ã���Ȼֻ��һ���˻�����ʾ����������ν����������һ����ʾ��Ҳ����ν�ɣ�
                // TODO ��дһ�����˵�
                showNumberAnimation();
            }
        }

        // ����غϽ������һЩ״̬������˵��Ѫ���� ��һϵ��

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


void BattleModifier::showNumberAnimation()
{
    //�ж��Ƿ�����Ҫ��ʾ������
    bool need_show = false;
    for (auto r : battle_roles_)
    {
        if (!r->ShowString.empty() || r->ProgressChange != 0)
        {
            need_show = true;
            break;
        }
    }
    if (!need_show)
    {
        return;
    }

    int size = 28;
    for (int i = 0; i < 10; i++)
    {
        if (exit_)
        {
            break;
        }
        auto drawNumber = [&](void*) -> void
        {
            for (auto r : battle_roles_)
            {
                if (!r->ShowString.empty())
                {
                    auto p = getPositionOnWindow(r->X(), r->Y(), man_x_, man_y_);
                    int x = p.x - size * r->ShowString.size() / 4;
                    int y = p.y - 75 - i * 2;
                    Font::getInstance()->draw(r->ShowString, size, x, y, r->ShowColor, 255 - 20 * i);
                }
                r->Progress += r->ProgressChange / 10;
            }
        };
        drawAndPresent(animation_delay_, drawNumber);
    }
    //��������˵���ʾ�������ܱ������Ļ���ֵ
    for (auto r : battle_roles_)
    {
        r->ShowString.clear();
        r->Progress += r->ProgressChange - 10 * (r->ProgressChange / 10);
        r->ProgressChange = 0;
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

        int progress = 0;
        
        // ����/��״̬�����Լ���������Ҳ���ǲ��У��ٸ��ѭ�����жϲ���һ�������
        speedEffectManager_.getAllEffectParams(7);

        // ��Ч3 �����ٶ�
        progress = speedEffectManager_.getEffectParam0(3);

        // ��Ч8 �����ٶȰٷֱ�����
        progress = int((speedEffectManager_.getEffectParam0(8) / 100.0) * progress);


        progress = progress < 0 ? 0 : progress;

        r->Progress += progress;

        battleStatusManager_[r->ID].materialize();
        speedEffectManager_.clear();
    }
    return nullptr;
}

void BattleMod::BattleModifier::applyStatusFromParams(std::vector<int> params, Role * target)
{
    if (params.empty()) return;
    // [״̬id, ״̬ǿ��, ״̬id, ״̬ǿ��...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].incrementBattleStatusVal(params[i], params[i + 1]);
    }
}

void BattleMod::BattleModifier::showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names)
{
    // UI����д��������Ϲ��
    std::vector<std::unique_ptr<TextBox>> boxes;
    int fontSize = 30;
    int stayFrame = 40;
    int i = 0;
    for (const std::string& name : names) {
        if (name.empty()) continue;
        auto box = std::make_unique<TextBox>();
        box->setText(name);
        box->setPosition(450, 150 + (fontSize + 1) * i);
        box->setFontSize(fontSize);
        box->setStayFrame(stayFrame * (i + 1));
        boxes.push_back(std::move(box));
        i++;
    }
    for (auto& box : boxes) {
        box->run();
    }
}