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
    // id 必须相同
    if (effect.id != rhs.effect.id) {
        return *this;
    }

    // 上状态的时候，不这样处理！
    // 怎么处理呢，我也不知道，怎么搞好
    if (effect.id == 6 || effect.id == 7) {
        std::unordered_map<int, int> statusStrMap;
        // 参数每第一个是编号，第二个强度
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

    // 一般直接加完事
    for (std::size_t i = 0; i < params_.size(); i++) {
        params_[i] += rhs.params_[i];
    }
    return *this;
}

BattleMod::Variable::Variable(BattleInfoFunc func, VarTarget target) : func_(func), target_(target)
{
}

// 其实这个attacker defender并不一定是这样
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
        // 机器猫老师，这个很不好用啊
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
    // Adder 之间 嗯，暂且加起来，以后有心情加入 max/min等
    for (const auto& adder : adders_) {
        sum += adder->getVal(attacker, defender, wg);
    }
    // 是不是就看attacker就够了呢？防御者特效的时候是否防御者变成了attacker？
    // 这个命名方式一定要改
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
    // 需要一个copy
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

// 现在EffectIntsPair是个copy，不能直接返回ptr了，反正我瞎搞直接上最符合要求的std::optional(C++17)，另外就是heap allocated std::unique_ptr我不喜欢
std::optional<EffectIntsPair> BattleMod::EffectSingle::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    // rand [0 1)
    // 乘以100，取整，就是说实际范围是0-99
    if (BattleModifier::rng.rand_int(100) < percentProb_.getVal(attacker, defender, wg)) {
        return effectPair_.materialize(attacker, defender, wg);
    }
    return std::nullopt;
}

std::optional<EffectIntsPair> BattleMod::EffectWeightedGroup::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    // printf("尝试触发\n");
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

// 这里考虑重构一下
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
    // 返回0这样应该没问题吧。。
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
        // 必须按顺序来，因为我懒，TODO 改一改？还是不改呢
        assert(effects_.size() == spNode[u8"编号"].as<int>());
        auto desc = PotConv::conv(spNode[u8"描述"].as<std::string>(), "utf-8", "cp936");
        strPool_.push_back(desc);
        printf("%d %s\n", spNode[u8"编号"].as<int>(), desc.c_str());
        effects_.emplace_back(spNode[u8"编号"].as<int>(), strPool_.back());
    }

    for (const auto& bNode : baseNode[u8"战场状态"]) {
        int id = bNode[u8"编号"].as<int>();
        assert(battleStatus_.size() == id);
        auto desc = PotConv::conv(bNode[u8"描述"].as<std::string>(), "utf-8", "cp936");
        int max = 100;
        if (const auto& maxNode = bNode[u8"满值"]) {
            max = bNode.as<int>();
        }
        strPool_.push_back(desc);
        battleStatus_.emplace_back(id, max, strPool_.back());
    }

    // 以下可以refactor，TODO 改！！！
    if (baseNode[u8"武功"]) {
        for (const auto& magicNode : baseNode[u8"武功"]) {
            int wid = magicNode[u8"武功编号"].as<int>();
            if (magicNode[u8"攻击"]) {
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
    if (node[u8"状态"]) {
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
    VariableParam vp(node[u8"基础"].as<int>());
    if (const auto& adders = node[u8"加成"]) {
        for (const auto& adder : adders) {
            vp.addAdder(readAdder(adder));
        }
    }
    return vp;
}

Condition BattleMod::BattleModifier::readCondition(const YAML::Node & node)
{
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
    strPool_.push_back(display);
    printf("读入 %s\n", display.c_str());
    // 这里效果参数以后要可配置
    EffectParamPair epp(effects_[id], strPool_.back());
    if (node[u8"效果参数"].IsScalar()) {
        epp.addParam(readVariableParam(node[u8"效果参数"]));
    }
    else {
        for (const auto& param : node[u8"效果参数"]) {
            epp.addParam(readVariableParam(param));
        }
    }
    return epp;
}



std::unique_ptr<ProccableEffect> BattleModifier::readProccableEffect(const YAML::Node& node) {
    int probType = node[u8"发动方式"].as<int>();
    ProcProbability prob = static_cast<ProcProbability>(probType);
    std::unique_ptr<ProccableEffect> pe;
    switch (prob) {
    case ProcProbability::random: {
        pe = std::make_unique<EffectSingle>(readVariableParam(node[u8"特效"][u8"发动参数"]), readEffectParamPair(node[u8"特效"]));
        break;
    }
    case ProcProbability::distributed: {
        auto group = std::make_unique<EffectWeightedGroup>(node[u8"总比重"].as<int>());
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
                                             readVariableParam(node[u8"特效"][u8"发动参数"]), 
                                             readEffectParamPair(node[u8"特效"]));
        break;
    }
    }
    if (pe) {
        if (const auto& reqNode = node[u8"需求"]) {
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
            printf("触发效果 %s\n", epp->description.c_str());
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
            if (auto epp = effect->proc(attacker, defender, wg)) {
                manager.addEPP(epp.value());
                procd.push_back(epp.value());
                printf("触发效果 %s\n", epp->description.c_str());
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
    // 机器猫写的不好用
    int curTemp = cur;
    cur = GameUtil::limit(cur + add, min, max);
    return curTemp - cur;
}

int BattleMod::BattleStatusManager::getBattleStatusVal(int statusID)
{
    switch (statusID) {
        // 这几个凭什么不一样，应该全部统一？
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
    // 连击的判断我要改的
    for (int i = 0; i <= GameUtil::sign(r->AttackTwice); i++)
    {
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
    
    // 这里考虑怎么加入显示效果
    // 先加入防守方特效，注意r2先来，其实这里传递magic也行，效果不明就是的
    tryProcAndAddToManager(r2->ID, defRole_, defEffectManager_, r2, r1, nullptr);
    
    // 再加入挨打者的武功被动特效
    for (int i = 0; i < r2->getLearnedMagicCount(); i++) {
        tryProcAndAddToManager(r2->MagicID[i], defMagic_, defEffectManager_, r2, r1, Save::getInstance()->getMagic(r2->MagicID[i]));
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

    // 特效6，特效7 上状态
    auto enemyStat = atkEffectManager_.getAllEffectParams(6);
    applyStatusFromParams(enemyStat, r2);
    auto selfStat = atkEffectManager_.getAllEffectParams(7);
    applyStatusFromParams(selfStat, r1);
    // 然后倒过来
    enemyStat = defEffectManager_.getAllEffectParams(7);
    applyStatusFromParams(enemyStat, r2);
    selfStat = defEffectManager_.getAllEffectParams(6);
    applyStatusFromParams(selfStat, r1);

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
        names.push_back(std::cref(effect.description));
    }

    // 再添加人物自身的攻击特效
    effects = tryProcAndAddToManager(r->ID, atkRole_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        names.push_back(std::cref(effect.description));
    }

    // 所有人物的
    effects = tryProcAndAddToManager(atkAll_, atkEffectManager_, r, nullptr, m);
    for (const auto& effect : effects) {
        names.push_back(effect.description);
    }

    showMagicNames(names);

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

        // 这里先都扔进去，伤害结算完了算状态
        auto result = battleStatusManager_[r2->ID].materialize();
        for (auto const& p : result) {
            if (p.first.show) {
                // 这个我要改
                r2->ShowString += convert::formatString(" %s %d", p.first.display, p.second);
            }
        }
    }
    
    // 我需要好好想一想显示效果怎么做

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

        // 特效7 上状态
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

        // 中毒？ 开玩笑，我这里特效全权接管了，有你什么事
        // poisonEffect(role);

        auto const& result = battleStatusManager_[role->ID].materialize();
        for (auto const& p : result) {
            // 这玩意儿怎么显示呢
            if (p.first.show) {
                // TODO 还可以选颜色！慢慢来
                role->ShowString = convert::formatString("%s %d", p.first.display, p.second);
                // 疯狂调用，虽然只有一个人会有显示，不过无所谓，减集气第一次显示，也无所谓吧？
                // TODO 再写一个单人的
                showNumberAnimation();
            }
        }

        // 结算回合结束后的一些状态，比如说回血内力 等一系列

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


void BattleModifier::showNumberAnimation()
{
    //判断是否有需要显示的数字
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
    //清除所有人的显示，处理不能被整除的击退值
    for (auto r : battle_roles_)
    {
        r->ShowString.clear();
        r->Progress += r->ProgressChange - 10 * (r->ProgressChange / 10);
        r->ProgressChange = 0;
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

        int progress = 0;
        
        // 先上/下状态，给自己，给别人也不是不行，再搞个循环，判断不在一个队伍的
        speedEffectManager_.getAllEffectParams(7);

        // 特效3 集气速度
        progress = speedEffectManager_.getEffectParam0(3);

        // 特效8 集气速度百分比增加
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
    // [状态id, 状态强度, 状态id, 状态强度...]
    for (std::size_t i = 0; i < params.size() / 2; i += 2) {
        battleStatusManager_[target->ID].incrementBattleStatusVal(params[i], params[i + 1]);
    }
}

void BattleMod::BattleModifier::showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names)
{
    // UI不好写啊，算了瞎搞
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