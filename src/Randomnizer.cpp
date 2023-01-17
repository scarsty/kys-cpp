#include <array>

#include "Save.h"
#include "Randomnizer.h"
#include "Random.h"

namespace KysChess {

namespace {


static std::unordered_set<Role*>& getRejected()
{
    // 每次随机时 上次没有被购买的不会再出现
    static std::unordered_set<Role*> rejected;
    return rejected;
}

void removeFromRejected(Role* r)
{
    auto& rejected = getRejected();
    rejected.erase(r);
}

using ProbArray = std::array<int, 5>;

constexpr std::array<std::array<int, 5>, 10> weights = {
    ProbArray{100, 0, 0, 0, 0},
    ProbArray{70, 30, 0, 0, 0},
    ProbArray{60, 35, 5, 0, 0},
    ProbArray{50, 35, 15, 0, 0},
    ProbArray{40, 35, 23, 2, 0},
    ProbArray{33, 30, 30, 7, 0},
    ProbArray{30, 30, 30, 10, 0},
    ProbArray{24, 30, 30, 15, 1},
    ProbArray{22, 30, 25, 20, 3},
    ProbArray{19, 25, 25, 25, 6}};


// TODO: 改成从r数据读取?
const std::array<std::vector<int>, 5> chessIdxOfPrice = {
    std::vector<int>{
        130, // 柯镇恶
        131, // 朱聪
        132, // 韩宝驹
        133, // 南希仁
        134, // 张阿生
        135, // 全金发
        136, // 韩小莹
        63,  // 程英
        84,  // 霍都
        160, // 达尔巴
        161, // 李莫愁
        45,  // 薛慕华
        47,  // 阿紫
        104, // 阿朱
        105  // 阿碧
    },
    std::vector<int>{
        54,  // 袁承志
        37,  // 狄云
        97,  // 血刀老祖
        56,  // 黄蓉
        44,  // 岳老三
        48,  // 游坦之
        99,  // 叶二娘
        100, // 云中鹤
        102, // 枯荣
        115  // 苏星河
    },
    std::vector<int>{
        55,  // 郭靖
        67,  // 裘千仞
        68,  // 丘处机
        59,  // 小龙女
        46,  // 丁春秋
        51,  // 慕容复
        53,  // 段誉
        70,  // 玄慈
        98,  // 段延庆
        103, // 鸠摩智
        112, // 萧远山
        113  // 慕容博
    },
    std::vector<int>{
        57,  // 黄药师
        60,  // 欧阳锋
        64,  // 周伯通
        65,  // 一灯
        69,  // 洪七公
        58,  // 杨过
        62,  // 金轮法王
        49,  // 虚竹
        50,  // 乔峰
        117, // 天山童姥
        118  // 李秋水
    },
    std::vector<int>{
        129, // 王重阳
        592, // 独孤求败
        114, // 扫地老僧
        116  // 无崖子
    },
};

} // namespace


Role* selectFromPool(int price)
{
    static Random<> rand;

    for (;;) {
        auto idx = chessIdxOfPrice[price][rand.rand_int(chessIdxOfPrice[price].size())];
        auto role = Save::getInstance()->getRole(idx);
        auto& rejected = getRejected();
        if (price <= 3 && rejected.contains(role)) {
            continue;
        }
        return role;
    }
}

std::vector<Role*> getChess(int level, int pieces)
{
    std::vector<Role*> roles;
    static Random<> rand;
    
    for (int i = 0; i < pieces; ++i) {
        // 应该是 0~99
        auto val = rand.rand_int(100);
        auto cur = 0;
        for (int price = 0; price < weights[level].size(); ++price)
        {
            auto w = weights[level][price];
            cur += w;
            if (val < cur)
            {
                roles.push_back(selectFromPool(price));
            }
        }
    }

    auto& rejected = getRejected();
    rejected.clear();
    for (auto r : roles) {
        rejected.insert(r); 
    }

    return roles;
}

} // namespace KysChess