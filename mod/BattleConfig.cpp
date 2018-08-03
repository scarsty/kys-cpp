#include "BattleConfig.h"
#include "BattleMod.h"
#include "GameUtil.h"

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
    // 说实话强制状态可能换个方式，min/max之类的 不过不管了
    if (effect.id == 6 || effect.id == 7 || effect.id == 8 || effect.id == 9) {
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

BattleMod::RandomAdder::RandomAdder(int min, int max) : Adder(), min_(min), max_(max)
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
    int sum = base_;
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
    if (!eip.description.empty())
        printf("成功触发%d %s\n", eip.effect.id, eip.description.c_str());
    return eip;
}

void BattleMod::EffectParamPair::addParam(VariableParam && vp)
{
    params_.push_back(std::move(vp));
}

void BattleMod::ProccableEffect::addConditions(Conditions&& c)
{
    conditionz_.push_back(std::move(c));
}

bool BattleMod::ProccableEffect::checkConditions(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (conditionz_.empty()) return true;
    // 在"需求"内，每一项之间的关系是"或"（既OR）
    // 在"条件"内，每一项之间的关系是"和"（既AND）
    for (auto const& conds : conditionz_) {             // 需求
        for (auto const& cond : conds) {                // 条件
            if (!cond.check(attacker, defender, wg)) {
                // goto的最后一片净土
                goto nextConds;
            }
        }
        // 单个检查通过 ok
        return true;
    nextConds:;
    }
    return false;
}


BattleMod::EffectSingle::EffectSingle(VariableParam && p, std::vector<EffectParamPair> && epps) :
    percentProb_(std::move(p)), effectPairs_(std::move(epps))
{
}

// 现在EffectIntsPair是个copy，不能直接返回ptr了，反正我瞎搞直接上最符合要求的std::optional(C++17)，另外就是heap allocated std::unique_ptr我不喜欢
std::vector<EffectIntsPair> BattleMod::EffectSingle::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (!ProccableEffect::checkConditions(attacker, defender, wg)) return {};
    // rand [0 1)
    // 乘以100，取整，就是说实际范围是0-99，也就是说必须加1
    int prob = percentProb_.getVal(attacker, defender, wg);
    if (prob <= 0) return {};
    // if (prob != 100)
    //     printf("触发概率 %d\n", prob);
    std::vector<EffectIntsPair> procs;
    if (BattleModifier::rng.rand_int(100) + 1 <= prob) {
        for (auto& effectPair : effectPairs_) {
            procs.push_back(std::move(effectPair.materialize(attacker, defender, wg)));
        }
    }
    return procs;
}

std::vector<EffectIntsPair> BattleMod::EffectWeightedGroup::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (!ProccableEffect::checkConditions(attacker, defender, wg)) return {};
    // printf("尝试触发\n");
    // [0 total)
    int t = BattleModifier::rng.rand_int(total_.getVal(attacker, defender, wg));
    int c = 0;
    for (auto& p : group_) {
        c += p.first.getVal(attacker, defender, wg);
        if (t < c) {
            return { std::move(p.second.materialize(attacker, defender, wg)) };
        }
    }
    return {};
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
std::vector<EffectIntsPair> BattleMod::EffectPrioritizedGroup::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (!ProccableEffect::checkConditions(attacker, defender, wg)) return {};
    for (auto& p : group_) {
        if (BattleModifier::rng.rand_int(100) < p.first.getVal(attacker, defender, wg)) {
            return { std::move(p.second.materialize(attacker, defender, wg)) };
        }
    }
    return {};
}

void BattleMod::EffectPrioritizedGroup::addProbEPP(VariableParam && weight, EffectParamPair && epp)
{
    group_.emplace_back(std::move(weight), std::move(epp));
}

BattleMod::EffectCounter::EffectCounter(VariableParam && total, VariableParam && add, std::vector<EffectParamPair> && epps) :
    total_(std::move(total)), add_(std::move(add)), effectPairs_(std::move(epps))
{
}

std::vector<EffectIntsPair> BattleMod::EffectCounter::proc(const Role * attacker, const Role * defender, const Magic * wg)
{
    if (attacker == nullptr) return {};
    if (!ProccableEffect::checkConditions(attacker, defender, wg)) return {};
    int id = attacker->ID;
    auto& count = counter_[id];
    count += add_.getVal(attacker, defender, wg);
    if (count > total_.getVal(attacker, defender, wg)) {
        count = 0;
        std::vector<EffectIntsPair> procs;
        for (auto& effectPair : effectPairs_) {
            procs.push_back(std::move(effectPair.materialize(attacker, defender, wg)));
        } 
        return procs;
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
    auto iter = epps_.find(ewp.effect.id);
    if (iter == epps_.end())
        epps_.insert({ ewp.effect.id, ewp });
    else {
        iter->second += ewp;
    }
}

std::size_t BattleMod::EffectManager::size()
{
    return epps_.size();
}

void BattleMod::EffectManager::clear()
{
    epps_.clear();
}

BattleMod::BattleStatus::BattleStatus(int id, int max, const std::string & display, bool hide, BP_Color color) : 
    id(id), max(max), display(display), hide(hide), color(color)
{
}



int BattleMod::BattleStatusManager::myLimit(int & cur, int add, int min, int max)
{
    // 机器猫写的不好用
    int curTemp = cur;
    cur = GameUtil::limit(cur + add, min, max);
    return cur - curTemp;
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
    if (val != 0) {
        printf("pid %d add %s %d\n", r_->ID, (*status_)[statusID].display.c_str(), val);
    }
    tempStatusVal_[statusID] += val;
}

void BattleMod::BattleStatusManager::setBattleStatusVal(int statusID, int val)
{
    actualStatusVal_[statusID] = val;
    tempStatusVal_[statusID] = 0;
}

void BattleMod::BattleStatusManager::initStatus(Role * r, const std::vector<BattleStatus>* status)
{
    status_ = status;
    r_ = r;
}

std::vector<std::pair<const BattleStatus&, int>> BattleMod::BattleStatusManager::materialize()
{
    std::vector<std::pair<const BattleStatus&, int>> changes;
    for (auto& p : tempStatusVal_) {
        int add = 0;
        switch (p.first) {
        case 0: add = myLimit(r_->Hurt, p.second, 0, Role::getMaxValue()->Hurt); break;
        case 1: add = myLimit(r_->Poison, p.second, 0, Role::getMaxValue()->Poison); break;
        case 2: add = myLimit(r_->PhysicalPower, p.second, 0, Role::getMaxValue()->PhysicalPower); break;
        default: add = myLimit(actualStatusVal_[p.first], p.second, (*status_)[p.first].min, (*status_)[p.first].max); break;
        }
        if (add != 0)
            changes.emplace_back((*status_)[p.first], add);
        printf("%d 当前 %s %d\n", r_->ID, (*status_)[p.first].display.c_str(), actualStatusVal_[p.first]);
        p.second = 0;
    }
    return changes;
}