#pragma once

#include "Types.h"
#include "Save.h"

#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include <functional>

// ���´�������
// �Ȱ����ж������� EffectsTable��ս���оͲ�ѯ��
// EffectsTable�б��� ProccableEffect
// ProccableEffect �и�proc�����������������书��Ϣ��û�о�nullptr
// proc���ж���Ч�����Ƿ�ﵽ��һ��vector��Conditions
// ֻҪ��һ��Conditions��꼴��
// ����Conditions��һ��vector��Condition������Condition����ȫ�����
// Condition����Variable-��һ������ָ��ʵ�ֻ�ȡ��Ӧ��ֵ
// ��check�ж�Condition�Ƿ�ͨ��
// Ȼ����ݷ�����ʽ��ProccableEffect������SingleEffect����Pooled
// ���ж�EffectParamPair�Ƿ񴥷�����������һ�� pair<Param, EffectParamPair> �ȸ����Ǹ�Param...
// EffectParamPair ������ EffectIntsPair��Params���
// �����󷵻�EffectIntsPair�����ұ�֤���е�Paramsȫ��ת������Ints
// ���ڽ���Param��ɶ
// �����ǵ�������
// �����Ǹ�VariableParam - һ��vector��<Adder> 
// LinearAdder ȡ������� for k,var in varParam: sum += k*var.getVal()
// RandomAdder ���
// Ȼ��VariableParam�ǿ����б�ŵģ���������ú���ָ��ķ�ʽ͸©BattleMod˽����Ϣ������е���������书���Զ�VariableParam�ӳɣ�
// ���EffectIntsPair�ᱻ��ӵ����Ӧ��EffectManager
// �ٶ����һ��Variable ��ȡ������ʱ��ͻ���ݱ�ŵ��ҵ����ʵĺ���ָ�룬�͵ȵ�ʱ�򴫵�����id���书id(������һ����)���ٵ���

namespace BattleMod {

    // ������Ч��������Ч����
    class SpecialEffect {
    public:
        SpecialEffect(int id, const std::string& desc);
        const std::string& description;
        const int id;
    };

    // ��Ч�� ��������
    class EffectIntsPair {
    public:
        EffectIntsPair(const SpecialEffect& effect, const std::string& desc);

        int getParam0();
        const std::vector<int>& getParams();

        void addParam(int p);
        // ���ӷ�ʽ�ɲ�ͬ�����ھͶ�һ����
        virtual EffectIntsPair & operator+=(const EffectIntsPair & rhs);

        const SpecialEffect& effect;
        // ��Ч��˵(��ʾ����?)
        // TODO ���ҲҪ���ƾ������ˣ�������ô�㣬�ַ�����? Yes
        const std::string& description;

    protected:
        // �����ж������
        // ����ÿ�������Ǹ�int�����Լ�������
        // ��������书/����ȼ����ӵ�
        // ��ʱ��������typedef
        std::vector<int> params_;
    };

    // �����ʵ��������
    enum class VarType {
        CharOrBase = 0,
        Battle = 1,
    };
    enum class VarTarget {
        Self = 0,
        Other = 1,
    };

    // ��������std::function�Ƚ��������о��о�
    using BattleInfoFunc = std::function<int(const Role* character, const Magic* wg)>;

    class Variable {
    public:
        Variable(BattleInfoFunc func, VarTarget target = VarTarget::Self);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const;

    private:
        VarTarget target_;
        BattleInfoFunc func_;
    };

    // �и�����������moveʧ�ܣ������� -> return { readXXX }; ���readXXX����move��vector��� 
    // ������������move constructor������д copy constructorȫɾ��
    // ��ȻӦ���Լ��ʹ����ȫ������
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
        // �������һ�£�&&�������ȫ���ᱻmove��
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
        // ��֪��std::greater�Ǹ�ɶtype
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
        // ���������������������
        // �����书��� �Ƿ����������书
        // �Ƿ�ӵ����Ʒ�������������������Եȵȵȵ�
        // proc���ش�����Ч����nullptrΪ����ʧ��
        virtual std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) = 0;
        // Condition���ݽ�ȥ֮�����������
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

    // ��������
    // �����ˣ��Ӵ˵���ָ�����жϣ����Զ����Чͬʱ����!
    // �Ժ�Ͳ���std::optional��
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

    // һ����Ч�������ټ̳У�����group��
    class EffectWeightedGroup : public ProccableEffect {
    public:
        std::vector<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        EffectWeightedGroup(VariableParam&& total);
        EffectWeightedGroup(EffectWeightedGroup&& o) noexcept : ProccableEffect(std::move(o)), group_(std::move(o.group_)), total_(std::move(o.total_)) { }
        void addProbEPP(VariableParam&& weight, EffectParamPair&& epp);
        EffectWeightedGroup(const EffectWeightedGroup&) = delete;
    private:
        // ��������������Ч
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

    // ʵ�ַ�ʽ�ǣ��ٸ�һ��std::unordered_map
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
        EffectIntsPair* getEPP(int eid);    // TODO ��std::optional
                                            // �ϲ��������Ч
        void unionEffects(const EffectManager& other);
        // ����ȫ����ȡһ��ֱ�Ӽӵ�Ч��
        void addEPP(const EffectIntsPair& ewp);
        std::size_t size();
        void clear();
    private:
        std::unordered_map<int, EffectIntsPair> epps_;
    };


    class BattleStatus {
    public:
        // ��һ��Ҫ��ʾ����������Ĵ���
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

        // ��ʼ�������õ�
        void initStatus(Role* r, const std::vector<BattleStatus>* status);

        // �˺������ʱ��Ż��ս���е�״̬ʵЧ�����ҷ�������ֵ��Ч��
        std::vector<std::pair<const BattleStatus&, int>> materialize();
    private:
        Role * r_;
        // ����ܷ�������ptr, reference_wrapper ��������һ����ptr
        const std::vector<BattleStatus>* status_;
        std::map<int, int> tempStatusVal_;
        std::map<int, int> actualStatusVal_;
        int myLimit(int& cur, int add, int min, int max);
    };

    using Effects = std::vector<std::unique_ptr<ProccableEffect>>;
    using EffectsTable = std::unordered_map<int, BattleMod::Effects>;

}