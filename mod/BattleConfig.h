#pragma once

#include "Types.h"
#include "Save.h"

#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include <functional>

// 大致触发流程
// 先把所有东西读入 EffectsTable，战斗中就查询他
// EffectsTable中保存 ProccableEffect
// ProccableEffect 有个proc函数，参数是人物武功信息，没有就nullptr
// proc先判断特效需求是否达到，一个vector的Conditions
// 只要有一项Conditions达标即可
// 其中Conditions是一个vector的Condition，所有Condition必须全部达标
// Condition含有Variable-是一个函数指针实现获取相应数值
// 跑check判断Condition是否通过
// 然后根据发动方式，ProccableEffect可以是SingleEffect或者Pooled
// 再判断EffectParamPair是否触发，大致上是一个 pair<Param, EffectParamPair> 既概率是个Param...
// EffectParamPair 本身由 EffectIntsPair和Params组成
// 发动后返回EffectIntsPair，并且保证其中的Params全部转换成了Ints
// 现在解释Param是啥
// 可以是单个整数
// 或者是个VariableParam - 一个vector的<Adder> 
// LinearAdder 取线性组合 for k,var in varParam: sum += k*var.getVal()
// RandomAdder 随机
// 然后VariableParam是可以有编号的，这里会再用函数指针的方式透漏BattleMod私人信息来查表，有的人物或者武功可以对VariableParam加成！
// 最后EffectIntsPair会被添加到相对应的EffectManager
// 再多解释一下Variable 读取参数的时候就会根据编号等找到合适的函数指针，就等到时候传递人物id和武功id(函数不一定用)，再调用

namespace BattleMod {

    // 单个特效，描述特效功能
    class SpecialEffect {
    public:
        SpecialEffect(int id, const std::string& desc);
        const std::string& description;
        const int id;
    };

    // 特效绑定 整数参数
    class EffectIntsPair {
    public:
        EffectIntsPair(const SpecialEffect& effect, const std::string& desc);

        int getParam0();
        const std::vector<int>& getParams();

        void addParam(int p);
        // 叠加方式可不同，现在就都一样吧
        virtual EffectIntsPair & operator+=(const EffectIntsPair & rhs);

        const SpecialEffect& effect;
        // 特效解说(显示文字?)
        // TODO 这个也要复制就难受了，想想怎么搞，字符串池? Yes
        const std::string& description;

    protected:
        // 可以有多个参数
        // 现在每个参数是个int，可以继续配置
        // 比如根据武功/人物等级增加等
        // 到时候改上面的typedef
        std::vector<int> params_;
    };

    // 这个的实现我想想
    enum class VarType {
        CharOrBase = 0,
        Battle = 1,
    };
    enum class VarTarget {
        Self = 0,
        Other = 1,
    };

    // 我听闻用std::function比较慢，再研究研究
    using BattleInfoFunc = std::function<int(const Role* character, const Magic* wg)>;

    class Variable {
    public:
        Variable(BattleInfoFunc func, VarTarget target = VarTarget::Self);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const;

    private:
        VarTarget target_;
        BattleInfoFunc func_;
    };

    // 有个东西在哪里move失败，很难受 -> return { readXXX }; 这个readXXX不是move到vector里的 
    // 从现在起，所有move constructor我亲手写 copy constructor全删除
    // 虽然应该自己就处理好全部问题
    class Adder {
    public:
        virtual int getVal(const Role* attacker, const Role* defender, const Magic* wg) const = 0;
        virtual ~Adder() = default;
    };

    class RandomAdder : public Adder {
    public:
        RandomAdder(int min, int max);
        RandomAdder(std::vector<int>&& items);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const override;
    private:
        int min_;
        int max_;
        std::vector<int> items_;
    };

    class LinearAdder : public Adder {
    public:
        // 这里解释一下，&&进来后的全部会被move掉
        LinearAdder(double k, Variable&& v);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const override;

    private:
        double k_;
        Variable v_;
    };

    class VariableParam {
    public:
        VariableParam(int base);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const;
        void addAdder(std::unique_ptr<Adder> adder);
        VariableParam(const VariableParam&) = delete;
        VariableParam(VariableParam&& o) noexcept : base_(o.base_), adders_(std::move(o.adders_)) { }
    private:
        int base_;
        std::vector<std::unique_ptr<Adder>> adders_;
    };


    enum class ConditionOp {
        greater_than = 0,
        greater_than_equal = 1,
        equal = 2
    };
    class Condition {
    public:
        // 不知道std::greater是个啥type
        Condition(Variable left, Variable right, std::function<bool(int, int)> binOp);
        bool check(const Role* attacker, const Role* defender, const Magic* wg) const;
    private:
        Variable left_;
        Variable right_;
        std::function<bool(int, int)> binOp_;
    };

    class EffectParamPair {
    public:
        EffectParamPair(const SpecialEffect& effect, const std::string& desc);
        EffectIntsPair materialize(const Role* attacker, const Role* defender, const Magic* wg) const;
        void addParam(VariableParam&& vp);
        EffectParamPair(EffectParamPair&& o) noexcept : params_(std::move(o.params_)), eip_(o.eip_) { }
        EffectParamPair(const EffectParamPair&) = delete;
    private:
        std::vector<VariableParam> params_;
        EffectIntsPair eip_;
    };

    using Conditions = std::vector<Condition>;
    class ProccableEffect {
    public:
        virtual ~ProccableEffect() = default;
        // 这里可以添加其他条件检测
        // 比如武功组合 是否修炼其他武功
        // 是否拥有物品，天书数量，内力属性等等等等
        // proc返回触发的效果，nullptr为触发失败
        virtual std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) = 0;
        // Condition传递进去之后就是垃圾了
        void addConditions(Conditions&& c);
        ProccableEffect() {}
        ProccableEffect(ProccableEffect&& o)  noexcept : conditionz_(std::move(o.conditionz_)) {}
        ProccableEffect(const ProccableEffect&) = delete;

    protected:
        bool checkConditions(const Role * attacker, const Role * defender, const Magic * wg);
        std::vector<Conditions> conditionz_;
    };

    enum class ProcProbability {
        random = 0,
        distributed = 1,
        prioritized = 2,
        counter = 3,
    };

    // 单个计算
    // 升级了，从此单个指单次判断，可以多个特效同时发动!
    // 以后就不用std::optional了
    class EffectSingle : public ProccableEffect {
    public:
        EffectSingle(VariableParam&& p, std::vector<EffectParamPair>&& epps);
        std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        EffectSingle(EffectSingle&& o) noexcept : ProccableEffect(std::move(o)), percentProb_(std::move(o.percentProb_)), effectPairs_(std::move(o.effectPairs_)) { }
        EffectSingle(const EffectSingle&) = delete;
    private:
        VariableParam percentProb_;
        std::vector<EffectParamPair> effectPairs_;
    };

    // 一组特效，可以再继承，其他group类
    class EffectWeightedGroup : public ProccableEffect {
    public:
        std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        EffectWeightedGroup(VariableParam&& total);
        EffectWeightedGroup(EffectWeightedGroup&& o) noexcept : ProccableEffect(std::move(o)), group_(std::move(o.group_)), total_(std::move(o.total_)) { }
        void addProbEPP(VariableParam&& weight, EffectParamPair&& epp);
        EffectWeightedGroup(const EffectWeightedGroup&) = delete;
    private:
        // 发动参数，和特效
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
        VariableParam total_;
    };

    class EffectPrioritizedGroup : public ProccableEffect {
    public:
        EffectPrioritizedGroup();
        EffectPrioritizedGroup(EffectPrioritizedGroup&& o) noexcept :ProccableEffect(std::move(o)), group_(std::move(o.group_)) { }
        std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        void addProbEPP(VariableParam&& prob, EffectParamPair&& epp);
        EffectPrioritizedGroup(const EffectPrioritizedGroup&) = delete;
    private:
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
    };

    // 实现方式是，再搞一个std::unordered_map
    class EffectCounter : public ProccableEffect {
    public:
        EffectCounter(VariableParam&& total, VariableParam&& add, std::vector<EffectParamPair>&& epps);
        std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        EffectCounter(EffectCounter&& o) noexcept : ProccableEffect(std::move(o)), total_(std::move(o.total_)),
                                                    add_(std::move(o.add_)), 
                                                    effectPairs_(std::move(o.effectPairs_)), counter_(counter_){ }
        EffectCounter(const EffectCounter&) = delete;
    private:
        VariableParam total_;
        VariableParam add_;
        std::vector<EffectParamPair> effectPairs_;
        std::unordered_map<int, int> counter_;
    };

    class EffectManager {
    public:
        bool hasEffect(int eid);
        int getEffectParam0(int eid);
        std::vector<int> getAllEffectParams(int eid);
        EffectIntsPair* getEPP(int eid);    // TODO 用std::optional
                                            // 合并管理的特效
        void unionEffects(const EffectManager& other);
        // 现在全部采取一个直接加的效果
        void addEPP(const EffectIntsPair& ewp);
        std::size_t size();
        void clear();
    private:
        std::unordered_map<int, EffectIntsPair> epps_;
    };


    class BattleStatus {
    public:
        // 不一定要显示，可以做别的处理
        BattleStatus(int id, int max, const std::string& display, bool hide, BP_Color color);
        const int id;
        const int max;
        const std::string& display;
        const int min = 0;
        const bool hide;
        BP_Color color;
    };

    class BattleStatusManager {
    public:
        int getBattleStatusVal(int statusID);
        void incrementBattleStatusVal(int statusID, int val);
        void setBattleStatusVal(int statusID, int val);

        // 初始化数据用的
        void initStatus(Role* r, const std::vector<BattleStatus>* status);

        // 伤害结算的时候才会把战斗中的状态实效，并且返还各种值的效果
        std::vector<std::pair<const BattleStatus&, int>> materialize();
    private:
        Role * r_;
        // 这里很烦必须用ptr, reference_wrapper 除非另外一边用ptr
        const std::vector<BattleStatus>* status_;
        std::map<int, int> tempStatusVal_;
        std::map<int, int> actualStatusVal_;
        int myLimit(int& cur, int add, int min, int max);
    };

    using Effects = std::vector<std::unique_ptr<ProccableEffect>>;
    using EffectsTable = std::unordered_map<int, BattleMod::Effects>;

}