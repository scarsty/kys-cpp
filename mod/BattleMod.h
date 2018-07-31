#pragma once

#include "Random.h"
#include "BattleScene.h"

#include "yaml-cpp/yaml.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>
#include <functional>
#include <optional>

// ��͵����һ��
namespace BattleMod {
    
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
        LinearAdder(double k, Variable&& v);
        int getVal(const Role* attacker, const Role* defender, const Magic* wg) const override;
        // �������һ�£�&&�������ȫ���ᱻmove��
    private:
        double k_;
        Variable v_;
    };

    class VariableParam {
    public:
        VariableParam(int base);
        virtual int getVal(const Role* attacker, const Role* defender, const Magic* wg) const;
        void addAdder(std::unique_ptr<Adder> adder);
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
        std::function<bool(int,int)> binOp_;
    };

    class EffectParamPair {
    public:
        EffectParamPair(const SpecialEffect& effect, const std::string& desc);
        EffectIntsPair materialize(const Role* attacker, const Role* defender, const Magic* wg) const;
        void addParam(VariableParam&& vp);
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
        virtual std::optional<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) = 0;
        // Condition���ݽ�ȥ֮�����������
        void addConditions(Conditions&& c);

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
    class EffectSingle : public ProccableEffect {
    public:
        EffectSingle(VariableParam&& p, EffectParamPair&& epp);
        std::optional<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
    private:
        VariableParam percentProb_;
        EffectParamPair effectPair_;
    };

    // һ����Ч�������ټ̳У�����group��
    class EffectWeightedGroup : public ProccableEffect {
    public:
        std::optional<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        EffectWeightedGroup(VariableParam&& total);
        void addProbEPP(VariableParam&& weight, EffectParamPair&& epp);
    private:
        // ��������������Ч
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
        VariableParam total_;
    };

    class EffectPrioritizedGroup : public ProccableEffect {
    public:
        EffectPrioritizedGroup();
        std::optional<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
        void addProbEPP(VariableParam&& prob, EffectParamPair&& epp);
    private:
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
    };

    // ʵ�ַ�ʽ�ǣ��ٸ�һ��std::unordered_map
    class EffectCounter : public ProccableEffect {
    public:
        EffectCounter(VariableParam&& total, VariableParam&& add, EffectParamPair&& epp);
        std::optional<EffectIntsPair> proc(const Role* attacker, const Role* defender, const Magic* wg) override;
    private:
        VariableParam total_;
        VariableParam add_;
        EffectParamPair effectPair_;
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
        BattleStatus(int id, int max, const std::string& display);
        const int id;
        const int max;
        const std::string& display;
        // ����Ժ���˵
        const int min = 0;
        const bool show = false;
    };

    class BattleStatusManager {
    public:
        int getBattleStatusVal(int statusID);
        void incrementBattleStatusVal(int statusID, int val);
        
        // ��ʼ�������õ�
        void initStatus(Role* r, const std::vector<BattleStatus>* status);

        // �˺������ʱ��Ż��ս���е�״̬ʵЧ�����ҷ�������ֵ��Ч��
        std::vector<std::pair<const BattleStatus&, int>> materialize();
    private:
        Role* r_;
        // ����ܷ�������ptr, reference_wrapper ��������һ����ptr
        const std::vector<BattleStatus>* status_;
        std::map<int, int> tempStatusVal_;
        std::map<int, int> actualStatusVal_;
        int myLimit(int& cur, int add, int min, int max);
    };

    using Effects = std::vector<std::unique_ptr<ProccableEffect>>;
    using EffectsTable = std::unordered_map<int, BattleMod::Effects>;

class BattleModifier : public BattleScene {
    
private:
    const std::string PATH = "../game/config/battle.yaml";

    // �Ͻ�����string
    std::vector<std::string> strPool_;

    std::vector<BattleMod::SpecialEffect> effects_;
    std::vector<BattleStatus> battleStatus_;

    // �书Ч����������Magic����࣬�޸ĵĶ���̫��
    // �����书Ч��������ʹ������书����
    EffectsTable atkMagic_;
    // ���������ʱ���Ч��
    EffectsTable defMagic_;
    // ������Ч
    EffectsTable speedMagic_;
    // �غ�/�ж���Ч�������ж��󷢶�������һ�����ж�ǰ���������
    EffectsTable turnMagic_;

    // ���� ������Ч
    EffectsTable atkRole_;
    // ���� ������Ч
    EffectsTable defRole_;
    // ���� ������Ч
    EffectsTable speedRole_;
    EffectsTable turnRole_;

    // �������ﹲ��
    Effects atkAll_;
    Effects defAll_;
    Effects speedAll_;
    Effects turnAll_;


    // ������Ч
    // ������Ч������ֻ��һ�����ڹ������������
    EffectManager atkEffectManager_;
    // ������Ч������ֻ��һ�����ڰ�����������
    EffectManager defEffectManager_;
    // ������Ч ����Ҳֻ��Ҫһ���ˣ�
    EffectManager speedEffectManager_;
    // �غ� Ҳһ����
    EffectManager turnEffectManager_;

    std::unordered_map<int, BattleStatusManager> battleStatusManager_;

    // simulation��calMagicHurt�ܲ��Ѻ�
    bool simulation_ = false;

    void init();
    Variable readVariable(const YAML::Node& node);
    std::unique_ptr<Adder> readAdder(const YAML::Node& node);
    VariableParam readVariableParam(const YAML::Node& node);
    Conditions readConditions(const YAML::Node& node);
    Condition readCondition(const YAML::Node& node);
    std::unique_ptr<ProccableEffect> readProccableEffect(const YAML::Node& node);
    EffectParamPair readEffectParamPair(const YAML::Node& node);
    void readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects);

    std::vector<EffectIntsPair> tryProcAndAddToManager(const Effects& list, EffectManager& manager,
                                                       const Role* attacker, const Role* defender, const Magic* wg);
    std::vector<EffectIntsPair> tryProcAndAddToManager(int id, const EffectsTable& table, EffectManager& manager,
                                                       const Role* attacker, const Role* defender, const Magic* wg);

    void applyStatusFromParams(std::vector<int> params, Role* target);


public:

    // rng ��Ӧ���Ǹ�ȫ�ֱ���
    static RandomDouble rng;

    // �Ҿ����ⲻ��һ��singleton�������ؿ���Ϸ�Ϳ��Բ���
    BattleModifier();
    BattleModifier(int id);

    void dealEvent(BP_Event & e) override;


    // ���ҿ����޸���Щ����
    virtual void setRoleInitState(Role* r) override;

    virtual void actUseMagic(Role* r);           //��ѧ

    virtual void useMagic(Role* r, Magic* m);

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //����ȫ��������˺�
    virtual void showNumberAnimation();
    // ������¼ӵ�
    virtual Role* semiRealPickOrTick();

    void showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names);

};

}