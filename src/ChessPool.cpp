#include "ChessPool.h"

#include <array>
#include "Random.h"
#include "Save.h"

namespace KysChess
{

namespace
{

using ProbArray = std::array<int, 5>;

// Level - leveled externally, the higher the level
// the better the weights
constexpr std::array<std::array<int, 5>, 10> weights = {
    ProbArray{ 100, 0, 0, 0, 0 },
    ProbArray{ 70, 30, 0, 0, 0 },
    ProbArray{ 60, 35, 5, 0, 0 },
    ProbArray{ 50, 35, 15, 0, 0 },
    ProbArray{ 40, 35, 23, 2, 0 },
    ProbArray{ 33, 30, 30, 7, 0 },
    ProbArray{ 30, 30, 30, 10, 0 },
    ProbArray{ 24, 30, 30, 15, 1 },
    ProbArray{ 22, 30, 25, 20, 3 },
    ProbArray{ 19, 25, 25, 25, 6 }
};

// TODO: 改成从r数据读取?
const std::array<std::vector<int>, 5> chessIdxOfPrice = {
    std::vector<int>{
        130,    // 柯镇恶
        131,    // 朱聪
        132,    // 韩宝驹
        133,    // 南希仁
        134,    // 张阿生
        135,    // 全金发
        136,    // 韩小莹
        63,     // 程英
        84,     // 霍都
        160,    // 达尔巴
        161,    // 李莫愁
        45,     // 薛慕华
        47,     // 阿紫
        104,    // 阿朱
        105     // 阿碧
    },
    std::vector<int>{
        54,     // 袁承志
        37,     // 狄云
        97,     // 血刀老祖
        56,     // 黄蓉
        44,     // 岳老三
        48,     // 游坦之
        99,     // 叶二娘
        100,    // 云中鹤
        102,    // 枯荣
        115     // 苏星河
    },
    std::vector<int>{
        55,     // 郭靖
        67,     // 裘千仞
        68,     // 丘处机
        59,     // 小龙女
        46,     // 丁春秋
        51,     // 慕容复
        53,     // 段誉
        70,     // 玄慈
        98,     // 段延庆
        103,    // 鸠摩智
        112,    // 萧远山
        113     // 慕容博
    },
    std::vector<int>{
        57,     // 黄药师
        60,     // 欧阳锋
        64,     // 周伯通
        65,     // 一灯
        69,     // 洪七公
        58,     // 杨过
        62,     // 金轮法王
        49,     // 虚竹
        50,     // 乔峰
        117,    // 天山童姥
        118     // 李秋水
    },
    std::vector<int>{
        129,    // 王重阳
        592,    // 独孤求败
        114,    // 扫地老僧
        116     // 无崖子
    },
};

}    // namespace


int ChessPool::GetChessTier(int roleId) {
    for (int i = 0; i < chessIdxOfPrice.size(); ++i) {
        if (std::ranges::find(chessIdxOfPrice[i], roleId) != chessIdxOfPrice[i].end()) {
            return i;
        }
    }
    return -1;
}

Role* ChessPool::selectFromPool(int tier)
{
    static Random<> rand;

    for (;;)
    {
        auto idx = chessIdxOfPrice[tier][rand.rand_int(chessIdxOfPrice[tier].size())];
        auto role = Save::getInstance()->getRole(idx);
        if (tier <= 3 && rejected_.contains(role))
        {
            continue;
        }
        return role;
    }
}

std::vector<std::pair<Role*, int>> ChessPool::getChessFromPool(int level)
{
    if (!getNewChess_) {
        return current_;
    }

    getNewChess_ = false;

    std::vector<std::pair<Role*, int>> roles;
    static Random<> rand;

    for (int i = 0; i < 5; ++i)
    {
        // 应该是 0~99
        auto val = rand.rand_int(100);
        auto cur = 0;
        for (int tier = 0; tier < weights[level].size(); ++tier)
        {
            auto w = weights[level][tier];
            cur += w;
            if (val < cur)
            {
                roles.emplace_back(selectFromPool(tier), tier);
                break;
            }
        }
    }

    rejected_.clear();
    for (auto [r, price] : roles)
    {
        rejected_.insert(r);
    }

    current_ = roles;

    return roles;
}

void ChessPool::removeChessAt(int idx) {
    current_.erase(current_.begin() + idx);
}

void ChessPool::refresh() {
    getNewChess_ = true;
}

Role* ChessPool::selectEnemyFromPool(int tier)
{
    static Random<> rand;
    auto idx = chessIdxOfPrice[tier][rand.rand_int(chessIdxOfPrice[tier].size())];
    return Save::getInstance()->getRole(idx);
}

}    // namespace KysChess