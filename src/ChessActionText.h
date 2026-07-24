#pragma once

#include "ChessSessionTypes.h"

#include <string_view>
#include <utility>

namespace KysChess
{

inline std::string_view chessActionDescription(ChessActionType type)
{
    switch (type)
    {
    case ChessActionType::BuyShopSlot: return "購買指定商店欄位的棋子";
    case ChessActionType::RefreshShop: return "刷新全部商店欄位";
    case ChessActionType::SetShopLocked: return "設定商店是否跨回合保留";
    case ChessActionType::SellChess: return "出售指定棋子實例";
    case ChessActionType::SetDeployment: return "以棋子實例清單完整取代目前出戰陣容；空陣列代表全部下陣";
    case ChessActionType::BuyExp: return "購買經驗值";
    case ChessActionType::AddBan: return "禁用指定角色；只影響之後生成或刷新的商店，目前商店既有棋子仍可購買";
    case ChessActionType::SkipForcedBans: return "放棄目前獨佔禁棋決策階段的剩餘次數；放棄後不會保留";
    case ChessActionType::Equip: return "將裝備實例交給指定棋子實例；已分配裝備會從原持有者移動";
    case ChessActionType::BuyLegendaryEquipment: return "購買指定神兵";
    case ChessActionType::SetPositionSwapEnabled: return "設定戰前是否允許交換站位";
    case ChessActionType::RerollEnemySeed: return "付費重抽下一戰敵方規劃";
    case ChessActionType::PrepareBattle: return "生成下一場主線戰鬥與敵方預覽";
    case ChessActionType::ChooseMap: return "選擇戰場";
    case ChessActionType::SwapPositions: return "交換兩個我方戰鬥單位的位置";
    case ChessActionType::StartBattle: return "依目前預覽與站位開始並結算戰鬥";
    case ChessActionType::ChooseReward: return "選擇目前獎勵選項";
    case ChessActionType::StartChallenge: return "依遠征名稱開始挑戰，不使用額外英文 ID";
    case ChessActionType::FinishRun: return "結束已通關的本局";
    case ChessActionType::SetFormation: return "設定完整十格出戰陣形並保留空位";
    }
    std::unreachable();
}

}
