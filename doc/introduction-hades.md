## Hades的说明

### 一些常见问题

Hades分支是为了研究使用这套素材能否制作一个act。所以请记住Hades虽然还是不伦不类，但初衷是一个act。目前目的暂时达到，故停止开发。

* 为什么只能使用4种武功？
这是为了对应手柄上的按键。Hades不是有10个技能栏的arpg。而且也没有打算做成鬼泣那种上百招的。
* 既然是act，为什么还有范围攻击？
因为素材实在没法做出明确的攻击，所以加了范围攻击。
* 为什么没有技能CD？
这不是arpg。在act中，大部分技能一般没有明显的技能cd，包含在出招前摇和后摇中。至少鬼泣、猎天使魔女、黑帝斯原版是这样的。
* 为什么没有超级取消？
目前的设置中，你已经很强了，敌人打不过你。
* 为什么没有使用攻击范围？
改用弹幕性能表示。

### 代码的说明

在角色的属性中，增加了以下字段：

```c++
public:
    Pointf Pos;   //亚像素的直角坐标
    Pointf RealTowards;    //面对的方向，计算攻击位置，击退方向等
    //以下用于一些被动移动的计算，例如闪身，击退等，主动移动可以直接修改坐标
    Pointf Velocity;    //指该质点的速度，每帧据此计算坐标
    Pointf Acceleration;    //加速度
    int VelocitytFrame = 0;    //大于0时质点速度才生效
    int HurtFrame = 0;    //正在受到伤害
    int CoolDown = 0;    //冷却
    int Attention = 0;    //出场
    int Invincible = 0;    //无敌时间
    int Frozen = 0;    //静止时间

    int ActType = -1;    //医拳剑刀特
    int ActFrame = 0;
    int OperationType = -1;    //0-轻攻击，1-重攻击，2-远程，3-闪身
    int HurtThisFrame = 0;    //一帧内受到伤害累积

    Magic* UsingMagic = nullptr;
```

位置，速度，和加速度均为3维向量。其中加速度保持有一个向下的常量。

下面对每一帧中的操作进行解释。

针对主角：
- 检测是否有方向操作。
- 检测是否有使用武功操作，如是，会同时检测是否符合武功发动条件。

针对所有角色：
每一帧中：
- 倒计时Frozen，倒计时中，所有活动暂停，如果是主角，不接受按键，造成行动静止效果。
- 倒计时CoolDown，倒计时中，可以接受被动速度，贴图的变化，不接受按键，用来处理出招时的效果和硬直。
- 更新位置，速度，用来处理强制打退，闪身等。

针对所有角色：
- 当帧数达到前摇值时，创建打击效果，赋予其位置，加速度等属性，此时CoolDown减去前摇即可视为后摇。
- 更新角色的贴图，若CoolDown还没有结束，则停留在最后一帧。
- 对于ai，计算下一步的行动。

针对所有攻击效果
- 更新帧数。
- 检测角色是否被攻击效果打中，若打中，则设置角色的Frozen、击退或闪避速度、加速度等，若角色处于前摇，可以打断出招，计算伤害，生成显示数字。
- 计算伤害包含被抵消的威力，距离衰减，武功固有强化，武功类型强化等。
- 检测是否可以互相抵消。
- 删除过期的。

针对所有角色：
- 计算本帧积累的所有伤害，若角色被击退，则速度的z轴设置为一个向上的值。

针对所有显示文字
- 更新帧数和位置。
- 删除过期的。

其他：
- 更新攻击效果贴图，处理攻击效果的行动，清理失效的效果。
- 更新文字位置，清理失效的文字。
- 根据剩下的角色，进行一些附加操作。

绘图：
- 绘制地面。
- 绘制建筑、人物阴影、人物、效果，文字。

### 武功特效

仅为少数名武功设置效果意义不大，故以下暂时从源码移除：

```c++
    //每一帧的效果，返回值暂时无定义
    special_magic_effect_every_frame_["紫霞神功"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 3 == 1)
        {
            r->PhysicalPower++;
        }
        return 0;
    };
    special_magic_effect_every_frame_["十八泥偶"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 600 == 0)
        {
            r->MP += r->MaxMP * 0.1;
        }
        return 0;
    };
    special_magic_effect_every_frame_["洗髓神功"] =
        [this](Role* r)->int
    {
        if (current_frame_ % 600 == 0)
        {
            r->HP += r->MaxHP * 0.05;
        }
        return 0;
    };
    //攻击时的效果，指发动攻击的时候，此时并不考虑防御者，返回值为消耗内力
    special_magic_effect_attack_["九陽神功"] =
        [this](Role* r)->int
    {
        r->MP += 50;    //定身
        return 0;
    };
    //打击时的效果，注意r是防御者，攻击者可以通过ae.Attacker取得，返回值为伤害
    special_magic_effect_beat_["葵花神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 20;    //定身
        r->MP -= hurt;    //消耗敌人內力
        return hurt;
    };
    special_magic_effect_beat_["獨孤九劍"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 10;    //定身
        r->ActType = -1;    //行动打断
        r->OperationType = -1;
        return hurt;
    };
    special_magic_effect_beat_["降龍十八掌"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Velocity.norm(2);    //更强的击退
        r->VelocitytFrame = 20;
        return hurt;
    };
    special_magic_effect_beat_["小無相功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        r->Frozen += 10;
        return hurt;
    };
    special_magic_effect_beat_["易筋神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        hurt *= 1.1;
        return hurt;
    };
    special_magic_effect_beat_["乾坤大挪移"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        ae.Attacker->HurtThisFrame += hurt * 0.05;
        return hurt;
    };
    special_magic_effect_beat_["太玄神功"] =
        [this](AttackEffect& ae, Role* r)->int
    {
        int hurt = defaultMagicEffect(ae, r);
        if (ae.OperationType == 3) { hurt *= 2; }
        return hurt;
    };
```
