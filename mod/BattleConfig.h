#pragma once

#include "Types.h"
#include "Save.h"

#define YAML_CPP_DLL
#include "yaml-cpp/yaml.h"

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

    // nb
    // https://stackoverflow.com/questions/476272/how-to-properly-overload-the-operator-for-an-ostream
    template<class T>
    auto operator<<(std::ostream& os, const T& t) -> decltype(t.print(os), os)
    {
        t.print(os);
        return os;
    }

    const std::string CONFIGPATH = "../game/config/battle.yaml";

    // 单个特效，描述特效功能
    class SpecialEffect {
    public:
        SpecialEffect(int id, const std::string& desc);
        void print(std::ostream& os) const;
        const std::string& description;
        const int id;
    };

	class BattleStatus {
	public:
		// 不一定要显示，可以做别的处理
		BattleStatus(int id, int max, const std::string& display, bool hide, BP_Color color);
        void print(std::ostream& os) const;
		const int id;
		const int max;
		const std::string& display;
		const int min = 0;
		const bool hide;
		BP_Color color;
	};

	class BattleStatusManager {
	public:
		int getBattleStatusVal(int statusID) const;
		void incrementBattleStatusVal(int statusID, int val);
		void setBattleStatusVal(int statusID, int val);

		// 初始化数据用的
		void initStatus(Role* r, const std::vector<BattleStatus>* status);

		// 伤害结算的时候才会把战斗中的状态实效，并且返还各种值的效果
		std::vector<std::pair<const BattleStatus&, int>> materialize();
        // void print(std::ostream& os) const;

	private:
		Role * r_;
		const std::vector<BattleStatus>* status_;
		std::map<int, int> tempStatusVal_;
		std::map<int, int> actualStatusVal_;
		int myLimit(int& cur, int add, int min, int max);
	};

    enum class EffectDisplayPosition {
        no,
        on_person,
        on_top,
    };

    // 特效绑定 整数参数
    class EffectIntsPair {
    public:
        EffectIntsPair(const SpecialEffect& effect, const std::string& desc, 
            EffectDisplayPosition displayOnPerson = EffectDisplayPosition::no, int animationEffect = -1);

        int getParam0();
        const std::vector<int>& getParams();
        int getAnimation() const;
        void addParam(int p);
        // 叠加方式可不同，现在就都一样吧
        virtual EffectIntsPair & operator+=(const EffectIntsPair & rhs);
        void print(std::ostream& os) const;

        const EffectDisplayPosition position;
        const SpecialEffect& effect;
        // 特效解说(显示文字?)
        const std::string& description;

    protected:
        // 可以有多个参数
        // 现在每个参数是个int，可以继续配置
        // 比如根据武功/人物等级增加等
        // 到时候改上面的typedef
        std::vector<int> params_;
        int animationEffect_;
    };


	class BattleConfManager;

	class Variable {
	public:
		virtual ~Variable() = default;
		virtual int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const = 0;
        virtual void print(std::ostream& os) const = 0;
	};

	class FlatValue : public Variable {
	public:
		FlatValue(int val);
		int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const override;
        void print(std::ostream& os) const;
	private:
		int val_;
	};

	enum class VarTarget {
		Self = 0,
		Other = 1,
	};

	enum class DynamicVarEnum {
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

	class DynamicVariable : public Variable {
	public:
		DynamicVariable(DynamicVarEnum dynamicCode, VarTarget target);
		int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const override;
		void addParam(int p);
        void print(std::ostream& os) const;
	private:
		DynamicVarEnum dynamicCode_;
		VarTarget target_;
		std::vector<int> params_;
	};

	class StatusVariable : public Variable {
	public:
		StatusVariable(const BattleStatus& status, VarTarget target);
		int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const override;
        void print(std::ostream& os) const;
	private:
		const BattleStatus& status_;
		VarTarget target_;
	};

    // 从现在起，所有move constructor我亲手写 copy constructor全删除
    class Adder {
    public:
        virtual int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const = 0;
        Adder(const std::string& description);
        virtual ~Adder() = default;
        virtual void print(std::ostream& os) const;
    protected:
        const std::string& description_;
    };

    class RandomAdder : public Adder {
    public:
        RandomAdder(const std::string & description, int min, int max);
        RandomAdder(const std::string & description, std::vector<int>&& items);
        int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const override;
        void print(std::ostream& os) const;
    private:
        int min_;
        int max_;
        std::vector<int> items_;
    };

    class LinearAdder : public Adder {
    public:
        // 这里解释一下，&&进来后的全部会被move掉
        LinearAdder(const std::string & description, double k, std::unique_ptr<Variable> v);
        int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const override;
        void print(std::ostream& os) const;
    private:
        double k_;
        std::unique_ptr<Variable> v_;
    };

    class VariableParam {
    public:
        // 用int最大值其实反而不好，最大最小判断很麻烦
        static const int DEFAULTMIN = -999999;
        static const int DEFAULTMAX = 999999;
        VariableParam(int base, int min = VariableParam::DEFAULTMIN, int max = VariableParam::DEFAULTMAX);
        void setMin(int min);
        void setMax(int max);
        int getBase() const;
        int getVal(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const;
        void addAdder(std::unique_ptr<Adder> adder);
        VariableParam(const VariableParam&) = delete;
        VariableParam(VariableParam&& o) noexcept : base_(o.base_), min_(o.min_), max_(o.max_), adders_(std::move(o.adders_)) { }
        void print(std::ostream& os) const;
    private:
        int base_;
        int min_;
        int max_;
        std::vector<std::unique_ptr<Adder>> adders_;
    };

    enum class ConditionOp {
        greater_than = 0,
        greater_than_equal = 1,
        equal = 2
    };
    class Condition {
    public:
        Condition(std::unique_ptr<Variable> left, std::unique_ptr<Variable> right, ConditionOp op);
		Condition(const Condition&) = delete;
        Condition(Condition&& o) noexcept : left_(std::move(o.left_)), right_(std::move(o.right_)), op_(std::move(o.op_)) {}
        bool check(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const;
        void print(std::ostream& os) const;
    private:
		std::unique_ptr<Variable> left_;
		std::unique_ptr<Variable> right_;
        ConditionOp op_;
    };

    class EffectParamPair {
    public:
        EffectParamPair(const SpecialEffect& effect, const std::string& desc, EffectDisplayPosition displayOnPerson, int animationEffect);
        EffectIntsPair materialize(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) const;
        void addParam(VariableParam&& vp);
        EffectParamPair(EffectParamPair&& o) noexcept : params_(std::move(o.params_)), eip_(o.eip_) { }
        EffectParamPair(const EffectParamPair&) = delete;
        void print(std::ostream& os) const;
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
        virtual std::vector<EffectIntsPair> proc(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) = 0;
        // Condition传递进去之后就是垃圾了
        void addConditions(Conditions&& c);
        ProccableEffect() {}
        ProccableEffect(ProccableEffect&& o)  noexcept : conditionz_(std::move(o.conditionz_)) {}
        ProccableEffect(const ProccableEffect&) = delete;
        // 僅打印需求
        virtual void print(std::ostream& os) const;

    protected:
        bool checkConditions(BattleConfManager& conf, Role * attacker, Role * defender, const Magic * wg);
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
        std::vector<EffectIntsPair> proc(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) override;
        EffectSingle(EffectSingle&& o) noexcept : ProccableEffect(std::move(o)), percentProb_(std::move(o.percentProb_)), effectPairs_(std::move(o.effectPairs_)) { }
        EffectSingle(const EffectSingle&) = delete;
        void print(std::ostream& os) const;
    private:
        VariableParam percentProb_;
        std::vector<EffectParamPair> effectPairs_;
    };

    // 一组特效，可以再继承，其他group类
    class EffectWeightedGroup : public ProccableEffect {
    public:
        std::vector<EffectIntsPair> proc(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) override;
        EffectWeightedGroup(VariableParam&& total);
        EffectWeightedGroup(EffectWeightedGroup&& o) noexcept : ProccableEffect(std::move(o)), group_(std::move(o.group_)), total_(std::move(o.total_)) { }
        void addProbEPP(VariableParam&& weight, EffectParamPair&& epp);
        EffectWeightedGroup(const EffectWeightedGroup&) = delete;
        void print(std::ostream& os) const;
    private:
        // 发动参数，和特效
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
        VariableParam total_;
    };

    class EffectPrioritizedGroup : public ProccableEffect {
    public:
        EffectPrioritizedGroup();
        EffectPrioritizedGroup(EffectPrioritizedGroup&& o) noexcept :ProccableEffect(std::move(o)), group_(std::move(o.group_)) { }
        std::vector<EffectIntsPair> proc(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) override;
        void addProbEPP(VariableParam&& prob, EffectParamPair&& epp);
        EffectPrioritizedGroup(const EffectPrioritizedGroup&) = delete;
        void print(std::ostream& os) const;
    private:
        std::vector<std::pair<VariableParam, EffectParamPair>> group_;
    };

    // 实现方式是，再搞一个std::unordered_map
    class EffectCounter : public ProccableEffect {
    public:
        EffectCounter(VariableParam&& total, VariableParam&& add, std::vector<EffectParamPair>&& epps);
        std::vector<EffectIntsPair> proc(BattleConfManager& conf, Role * attacker, Role * defender, const Magic* wg) override;
        EffectCounter(EffectCounter&& o) noexcept : ProccableEffect(std::move(o)), total_(std::move(o.total_)),
                                                    add_(std::move(o.add_)), 
                                                    effectPairs_(std::move(o.effectPairs_)), counter_(std::move(counter_)){ }
        EffectCounter(const EffectCounter&) = delete;
        void print(std::ostream& os) const;
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

    using Effects = std::vector<std::unique_ptr<ProccableEffect>>;
    using EffectsTable = std::unordered_map<int, BattleMod::Effects>;

	// 不知道取啥名字了
	class BattleConfManager {
	public:
		BattleConfManager();
		const BattleStatusManager * getStatusManager(int id) const;
        std::vector<EffectIntsPair> tryProcAndAddToManager(const Effects& list, EffectManager& manager,
            Role * attacker, Role * defender, const Magic* wg);
        std::vector<EffectIntsPair> tryProcAndAddToManager(int id, const EffectsTable& table, EffectManager& manager,
            Role * attacker, Role * defender, const Magic* wg);

        void applyStatusFromParams(const std::vector<int>& params, Role* target);
        void setStatusFromParams(const std::vector<int>& params, Role* target);

		// 还需要一个重置函数
		static std::vector<SpecialEffect> effects;
		static std::vector<BattleStatus> battleStatus;

		// 武功效果，不想碰Magic这个类，修改的东西太多
		// 主动武功效果，必须使用这个武功才行
		EffectsTable atkMagic;
		// 被动挨打的时候的效果
		EffectsTable defMagic;
		// 集气特效
		EffectsTable speedMagic;
		// 回合/行动特效，仅在行动后发动，还有一个在行动前的慢慢添加
		EffectsTable turnMagic;

		// 人物 攻击特效
		EffectsTable atkRole;
		// 人物 挨打特效
		EffectsTable defRole;
		// 人物 集气特效
		EffectsTable speedRole;
		EffectsTable turnRole;

		// 所有人物共享
		Effects atkAll;
		Effects defAll;
		Effects speedAll;
		Effects turnAll;

        // 特效说明
        std::unordered_map<int, std::vector<std::unique_ptr<Adder>>> magicAdder;
        std::unordered_map<int, std::vector<std::unique_ptr<Adder>>> roleAdder;

		// 管理特效
		// 攻击特效管理器只有一个，在攻击结束后清空
		EffectManager atkEffectManager;
		// 防御特效管理器只有一个，在挨打结束后清空
		EffectManager defEffectManager;
		// 集气特效 现在也只需要一个了！
		EffectManager speedEffectManager;
		// 回合 也一个！
		EffectManager turnEffectManager;
		std::unordered_map<int, BattleStatusManager> battleStatusManager;

	private:
		// 严禁复制string
		std::vector<std::string> strPool_;

		void init();
		std::unique_ptr<Variable> readVariable(const YAML::Node& node);
		std::unique_ptr<Adder> readAdder(const YAML::Node& node, bool copy = true);
        const std::string& adderDescription(const YAML::Node & adderNode, const YAML::Node & subNode, bool copy);
		VariableParam readVariableParam(const YAML::Node& node);
		Conditions readConditions(const YAML::Node& node);
		Condition readCondition(const YAML::Node& node);
		std::unique_ptr<ProccableEffect> readProccableEffect(const YAML::Node& node);
		EffectParamPair readEffectParamPair(const YAML::Node& node);
		std::vector<EffectParamPair> readEffectParamPairs(const YAML::Node& node);
		void readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects);

        void printEffects(const Effects& t);
	};
}