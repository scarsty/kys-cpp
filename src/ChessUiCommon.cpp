#include "ChessUiCommon.h"

#include "Audio.h"
#include "Engine.h"
#include "TextBox.h"

#include <format>

namespace KysChess
{

ChessSelectorPresenter& chessPresenter()
{
    static ChessSelectorPresenter value;
    return value;
}

void showChessMessage(const std::string& text, int fontSize)
{
    auto box = std::make_shared<TextBox>();
    box->setText(text);
    box->setFontSize(fontSize);
    box->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

void playChessUpgradeSound()
{
    Audio::getInstance()->playESound(72);
}

ChessManager makeChessManager(ChessRoster& roster, ChessEquipmentInventory& equipmentInventory, ChessEconomy& economy)
{
    return ChessManager(roster, equipmentInventory, economy);
}

ChessManager makeChessManager(const ChessSelectorServices& services)
{
    return makeChessManager(services.roster, services.equipmentInventory, services.economy);
}

std::string comboEffectDesc(const ComboEffect& eff)
{
    auto triggerPrefix = [&]() -> std::string {
        if (eff.trigger == Trigger::WhileLowHP) return std::format("血量<{}%: ", eff.triggerValue);
        if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("血量<{}%狂暴({}幀): ", eff.triggerValue, eff.duration);
        if (eff.trigger == Trigger::LastAlive) return "最後存活: ";
        return "";
    };
    auto countSuffix = [&]() -> std::string {
        if (eff.maxCount > 0) return std::format(" ({}次)", eff.maxCount);
        return "";
    };
    std::string desc;
    switch (eff.type)
    {
    case EffectType::FlatHP: desc = std::format("生命+{}", eff.value); break;
    case EffectType::FlatATK: desc = std::format("攻擊+{}", eff.value); break;
    case EffectType::FlatDEF: desc = std::format("防禦+{}", eff.value); break;
    case EffectType::FlatSPD: desc = std::format("速度+{}", eff.value); break;
    case EffectType::PctHP: desc = std::format("生命+{}%", eff.value); break;
    case EffectType::PctATK: desc = std::format("攻擊+{}%", eff.value); break;
    case EffectType::PctDEF: desc = std::format("防禦+{}%", eff.value); break;
    case EffectType::NegPctDEF: desc = std::format("防禦-{}%", eff.value); break;
    case EffectType::FlatDmgReduction: desc = std::format("減傷{}", eff.value); break;
    case EffectType::BlockChance: desc = std::format("{}%格擋", eff.value); break;
    case EffectType::DodgeChance: desc = std::format("{}%閃避", eff.value); break;
    case EffectType::DodgeThenCrit: desc = "閃避後暴擊"; break;
    case EffectType::CritChance: desc = std::format("{}%暴擊", eff.value); break;
    case EffectType::CritMultiplier: desc = std::format("暴擊{}%傷害", eff.value); break;
    case EffectType::EveryNthDouble: desc = std::format("每{}次雙倍", eff.value); break;
    case EffectType::ArmorPenChance: desc = std::format("{}%穿甲", eff.value); break;
    case EffectType::ArmorPenPct: desc = std::format("無視%防禦", eff.value); break;
    case EffectType::ArmorPen: desc = std::format("{}%穿甲", eff.triggerValue, eff.value); break;
    case EffectType::Stun: desc = std::format("{}%眩暈({}幀)", eff.triggerValue, eff.value); break;
    case EffectType::KnockbackChance: desc = std::format("{}%擊退", eff.value); break;
    case EffectType::PoisonDOT: desc = eff.value2 ? std::format("中毒{}%×{}次(每30幀)", eff.value, eff.value2) : std::format("中毒{}%", eff.value); break;
    case EffectType::PoisonDmgAmp: desc = std::format("中毒增傷{}%", eff.value); break;
    case EffectType::MPOnHit: desc = std::format("命中回{}MP", eff.value); break;
    case EffectType::MPDrain: desc = std::format("吸取{}MP", eff.value); break;
    case EffectType::MPRecoveryBonus: desc = std::format("回藍+{}%", eff.value); break;
    case EffectType::SkillDmgPct: desc = std::format("技能傷害+{}%", eff.value); break;
    case EffectType::SkillReflectPct: desc = std::format("反彈{}%", eff.value); break;
    case EffectType::CDR: desc = std::format("冷卻-{}%", eff.value); break;
    case EffectType::ShieldPctMaxHP: desc = std::format("護盾{}%生命", eff.value); break;
    case EffectType::ShieldFreezeRes: desc = std::format("護盾僵直抗性{}%", eff.value); break;
    case EffectType::HealAuraPct: desc = eff.value2 ? std::format("治療光環{}%(每{}幀)", eff.value, eff.value2) : std::format("治療光環{}%", eff.value); break;
    case EffectType::HealAuraFlat: desc = eff.value2 ? std::format("治療光環{}(每{}幀)", eff.value, eff.value2) : std::format("治療光環{}", eff.value); break;
    case EffectType::HealedATKSPDBoost: desc = std::format("受治療加速{}%", eff.value); break;
    case EffectType::HPRegenPct: desc = eff.value2 ? std::format("回血{}%(每{}幀)", eff.value, eff.value2) : std::format("回血{}%", eff.value); break;
    case EffectType::FreezeReductionPct: desc = std::format("僵直-{}%", eff.value); break;
    case EffectType::ControlImmunityFrames: desc = std::format("僵直吸收{}幀", eff.value); break;
    case EffectType::KillHealPct: desc = std::format("擊殺回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: desc = std::format("擊殺無敵{}幀", eff.value); break;
    case EffectType::PostSkillInvincFrames: desc = std::format("技能後無敵{}幀", eff.value); break;
    case EffectType::DmgReductionPct: desc = std::format("傷害減免{}%", eff.value); break;
    case EffectType::Bloodlust: desc = std::format("击杀增攻+{}攻", eff.value); break;
    case EffectType::Adaptation: desc = std::format("同敌减伤{}%({}层)", eff.value, eff.value2); break;
    case EffectType::RampingDmg: desc = std::format("连击增伤+{}%({}层)", eff.value, eff.value2); break;
    case EffectType::HealBurst: desc = std::format("回血{}%", eff.value); break;
    default: desc = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + desc + countSuffix();
}

}    // namespace KysChess