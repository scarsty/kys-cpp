#include "ChessUiCommon.h"

#include "Audio.h"
#include "Engine.h"
#include "Font.h"
#include "TextBox.h"

#include <array>
#include <format>
#include <vector>

namespace KysChess
{

namespace
{

constexpr std::array<int, 20> kChessMusicIds = {0, 1, 2, 8, 9, 13, 14, 16, 18, 19, 20, 21, 23, 24, 25, 31, 59, 60, 61, 62};
constexpr std::array<int, 17> kBattleMusicIds = {3, 4, 5, 6, 7, 10, 17, 48, 53, 55, 70, 75, 79, 80, 84, 88, 90};

template <size_t N>
int pickRandomMusic(const std::array<int, N>& musicIds)
{
    static int lastPlayed = -1;

    int idx = rand() % musicIds.size();
    if (musicIds.size() > 1 && musicIds[idx] == lastPlayed)
    {
        idx = (idx + 1) % musicIds.size();
    }
    lastPlayed = musicIds[idx];
    return musicIds[idx];
}

template <size_t N>
bool containsMusicId(const std::array<int, N>& musicIds, int musicId)
{
    for (int value : musicIds)
    {
        if (value == musicId)
        {
            return true;
        }
    }
    return false;
}

struct DisplayGlyph
{
    std::string text;
    int width;
    bool preferredBreakAfter;
};

DisplayGlyph readDisplayGlyph(const std::string& text, size_t index)
{
    size_t charSize = Font::utf8CharLength(static_cast<unsigned char>(text[index]));
    std::string glyph = text.substr(index, charSize);
    int charWidth = Font::getTextDrawSize(glyph);
    bool preferredBreak = glyph == " " || glyph == ":" || glyph == "：" || glyph == "(" || glyph == ")" || glyph == "（" || glyph == "）" || glyph == "·" || glyph == "/";
    return {glyph, charWidth, preferredBreak};
}

std::string trimLine(std::string line)
{
    while (!line.empty() && line.back() == ' ')
    {
        line.pop_back();
    }
    return line;
}

std::string comboEffectLabel(const ComboEffect& eff, bool compact)
{
    auto triggerPrefix = [&]() -> std::string {
        if (compact)
        {
            if (eff.trigger == Trigger::WhileLowHP) return std::format("血<{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("友血<{}%·狂暴{}幀·", eff.triggerValue, eff.duration);
            if (eff.trigger == Trigger::LastAlive) return "最後存活·";
            if (eff.trigger == Trigger::OnUltimate) return "絕招·";
            if (eff.trigger == Trigger::OnHit && eff.triggerValue < 100) return std::format("擊中{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnBeingHit && eff.triggerValue < 100) return std::format("被擊{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnShieldBreak && eff.triggerValue < 100) return std::format("盾爆{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnShieldBreak) return "盾爆·";
            return "";
        }
        if (eff.trigger == Trigger::WhileLowHP) return std::format("血量<{}%: ", eff.triggerValue);
        if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("血量<{}%狂暴({}幀): ", eff.triggerValue, eff.duration);
        if (eff.trigger == Trigger::LastAlive) return "最後存活: ";
        if (eff.trigger == Trigger::OnUltimate) return "絕招觸發: ";
        if (eff.trigger == Trigger::OnHit && eff.triggerValue < 100) return std::format("{}%擊中觸發", eff.triggerValue);
        if (eff.trigger == Trigger::OnBeingHit && eff.triggerValue < 100) return std::format("{}%被擊中觸發", eff.triggerValue);
        if (eff.trigger == Trigger::OnShieldBreak && eff.triggerValue != 0) return std::format("{}%護盾解除時", eff.triggerValue);
        if (eff.trigger == Trigger::OnShieldBreak) return "護盾解除時";
        return "";
    };
    auto countSuffix = [&]() -> std::string {
        if (eff.maxCount > 0)
        {
            return compact ? std::format("·{}次", eff.maxCount) : std::format(" ({}次)", eff.maxCount);
        }
        return "";
    };
    auto durationSuffix = [&]() -> std::string {
        if (eff.duration > 0 && eff.trigger != Trigger::AllyLowHPBurst)
        {
            return compact ? std::format("·{}幀", eff.duration) : std::format(" ({}幀)", eff.duration);
        }
        return "";
    };

    std::string desc;
    switch (eff.type)
    {
    case EffectType::FlatHP: desc = compact ? std::format("生+{}", eff.value) : std::format("生命+{}", eff.value); break;
    case EffectType::FlatATK: desc = compact ? std::format("攻+{}", eff.value) : std::format("攻擊+{}", eff.value); break;
    case EffectType::FlatDEF: desc = compact ? std::format("防+{}", eff.value) : std::format("防禦+{}", eff.value); break;
    case EffectType::FlatSPD: desc = compact ? std::format("速+{}", eff.value) : std::format("速度+{}", eff.value); break;
    case EffectType::PctHP: desc = compact ? std::format("生+{}%", eff.value) : std::format("生命+{}%", eff.value); break;
    case EffectType::PctATK: desc = compact ? std::format("攻+{}%", eff.value) : std::format("攻擊+{}%", eff.value); break;
    case EffectType::PctDEF: desc = compact ? std::format("防+{}%", eff.value) : std::format("防禦+{}%", eff.value); break;
    case EffectType::NegPctDEF: desc = compact ? std::format("防-{}%", eff.value) : std::format("防禦-{}%", eff.value); break;
    case EffectType::FlatDmgReduction: desc = std::format("減傷{}", eff.value); break;
    case EffectType::FlatDmgIncrease: desc = std::format("增傷{}", eff.value); break;
    case EffectType::BlockChance: desc = std::format("{}%格擋", eff.value); break;
    case EffectType::DodgeChance: desc = std::format("{}%閃避", eff.value); break;
    case EffectType::DodgeThenCrit: desc = "閃避後暴擊"; break;
    case EffectType::CritChance: desc = std::format("{}%暴擊", eff.value); break;
    case EffectType::CritMultiplier: desc = compact ? std::format("暴傷+{}%", eff.value) : std::format("暴擊{}%傷害", eff.value); break;
    case EffectType::EveryNthDouble: desc = std::format("每{}次雙倍", eff.value); break;
    case EffectType::ArmorPenChance: desc = std::format("{}%穿甲", eff.value); break;
    case EffectType::ArmorPenPct: desc = compact ? std::format("無視防{}%", eff.value) : std::format("無視%防禦", eff.value); break;
    case EffectType::ArmorPen: desc = std::format("{}%穿甲", eff.value); break;
    case EffectType::Stun: desc = std::format("眩暈({}幀)", eff.value); break;
    case EffectType::KnockbackChance: desc = std::format("{}%擊退", eff.value); break;
    case EffectType::PoisonDOT: desc = eff.value2 ? std::format("中毒{}%×{}次", eff.value, eff.value2) : std::format("中毒{}%", eff.value); break;
    case EffectType::PoisonDmgAmp: desc = std::format("中毒增傷{}%", eff.value); break;
    case EffectType::MPOnHit: desc = std::format("命中回{}MP", eff.value); break;
    case EffectType::HPOnHit: desc = std::format("命中回{}HP", eff.value); break;
    case EffectType::MPDrain: desc = std::format("吸取{}MP", eff.value); break;
    case EffectType::MPRecoveryBonus: desc = std::format("回藍+{}%", eff.value); break;
    case EffectType::SkillDmgPct: desc = compact ? std::format("技傷+{}%", eff.value) : std::format("技能傷害+{}%", eff.value); break;
    case EffectType::SkillReflectPct: desc = compact ? std::format("反彈{}%", eff.value) : std::format("反彈{}%", eff.value); break;
    case EffectType::CDR: desc = std::format("冷卻-{}%", eff.value); break;
    case EffectType::ShieldPctMaxHP: desc = compact ? std::format("護盾{}%生", eff.value) : std::format("護盾{}%生命", eff.value); break;
    case EffectType::ShieldFreezeRes: desc = compact ? std::format("護盾僵抗{}%", eff.value) : std::format("護盾僵直抗性{}%", eff.value); break;
    case EffectType::HealAuraPct: desc = eff.value2 ? (compact ? std::format("治療環{}%/{}幀", eff.value, eff.value2) : std::format("治療光環{}%(每{}幀)", eff.value, eff.value2)) : std::format("治療光環{}%", eff.value); break;
    case EffectType::HealAuraFlat: desc = eff.value2 ? (compact ? std::format("治療環{}/{}幀", eff.value, eff.value2) : std::format("治療光環{}(每{}幀)", eff.value, eff.value2)) : std::format("治療光環{}", eff.value); break;
    case EffectType::HealedATKSPDBoost: desc = std::format("受治療加速{}%", eff.value); break;
    case EffectType::HPRegenPct: desc = eff.value2 ? (compact ? std::format("回血{}%/{}幀", eff.value, eff.value2) : std::format("回血{}%(每{}幀)", eff.value, eff.value2)) : std::format("回血{}%", eff.value); break;
    case EffectType::FreezeReductionPct: desc = std::format("僵直-{}%", eff.value); break;
    case EffectType::ControlImmunityFrames: desc = std::format("僵直吸收{}幀", eff.value); break;
    case EffectType::KillHealPct: desc = std::format("擊殺回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: desc = std::format("擊殺無敵{}幀", eff.value); break;
    case EffectType::PostSkillInvincFrames: desc = std::format("技能後無敵{}幀", eff.value); break;
    case EffectType::DmgReductionPct: desc = compact ? std::format("減傷{}%", eff.value) : std::format("傷害減免{}%", eff.value); break;
    case EffectType::Bloodlust: desc = std::format("擊殺增攻+{}", eff.value); break;
    case EffectType::PctSPD: desc = std::format("速度+{}%", eff.value); break;
    case EffectType::Adaptation: desc = std::format("同敵減傷{}%({}層)", eff.value, eff.value2); break;
    case EffectType::DodgeAdaptation: desc = std::format("同敵閃避{}%({}層)", eff.value, eff.value2); break;
    case EffectType::RampingDmg: desc = std::format("連擊增傷+{}%({}層)", eff.value, eff.value2); break;
    case EffectType::HealBurst: desc = std::format("回血{}%", eff.value); break;
    case EffectType::BleedChance: desc = std::format("{}%流血", eff.value); break;
    case EffectType::BleedPersist: desc = "流血持續"; break;
    case EffectType::PostSkillDash: desc = "絕招後疾退"; break;
    case EffectType::EnemyTopDebuff: desc = std::format("敵方前{}名攻防-{}×存活數", eff.value, eff.value2); break;
    case EffectType::BlinkAttack: desc = "閃現攻擊"; break;
    case EffectType::AllyDeathStatBoost: desc = compact ? std::format("友死增{}攻防", eff.value) : std::format("友死增攻防{}", eff.value); break;
    case EffectType::CloneSummon: desc = eff.value > 1 ? std::format("分身×{}", eff.value) : "分身"; break;
    case EffectType::ProjectileReflect: desc = std::format("{}%彈反", eff.value); break;
    case EffectType::OnSkillTeamHeal: desc = compact ? std::format("群療{}HP", eff.value) : std::format("全隊回{}HP", eff.value); break;
    case EffectType::OnSkillTeamHealPct: desc = compact ? std::format("群療{}%HP", eff.value) : std::format("全隊回{}%HP", eff.value); break;
    case EffectType::DeathPrevention: desc = std::format("鎖血並無敵{}幀", eff.value); break;
    case EffectType::DeathMedical: desc = compact ? std::format("死療{}%", eff.value) : std::format("死亡醫療{}%HP", eff.value); break;
    case EffectType::ForcePullProtect: desc = compact? "保護挪移" : "一場一次隊友低血挪移并保護"; break;
    case EffectType::ForcePullExecute: desc = compact? "處決挪移" : "一場一次敵方殘血挪移"; break;
    case EffectType::ProjectileBounce: desc = compact ?  std::format("彈道彈射{}次", eff.value) : std::format("彈道{}半径内彈射{}次", eff.value2, eff.value); break;
    case EffectType::Execute: desc = std::format("斩殺<{}%", eff.value); break;
    case EffectType::MPBlock: desc = std::format("封内力回復{}幀", eff.value); break;
    case EffectType::CharmCDRDebuff: desc = std::format("{}%增敵CD{}%", eff.value, eff.value2); break;
    case EffectType::OffensiveCharm: desc = std::format("攻擊傾城{}%", eff.value); break;
    case EffectType::DeathAOE: desc = eff.value2 ? std::format("殉爆{}%眩{}幀", eff.value, eff.value2) : std::format("殉爆{}%", eff.value); break;
    case EffectType::ShieldExplosion: desc = "護盾解除"; break;
    case EffectType::TempFlatATK: desc = compact ? std::format("临时攻+{}", eff.value) : std::format("临时攻击+{}", eff.value); break;
    case EffectType::AutoUltimate: desc = "自動絕招"; break;
    case EffectType::MPRestore: desc = std::format("回内力+{}", eff.value); break;
    case EffectType::ShieldOnAllyDeath: desc = compact ? std::format("每{}友死獲盾", eff.value) : std::format("每{}友死獲盾", eff.value); break;
    case EffectType::DamageImmunityAfterFrames: desc = std::format("每{}幀免傷{}幀", eff.value, eff.value2); break;
    case EffectType::AutoUltimateAfterFrames: desc = std::format("每{}幀自動絕招", eff.value); break;
    case EffectType::UltimateExtraProjectiles: desc = compact ? std::format("絕招+{}彈", eff.value) : std::format("絕招額外投射物+{}", eff.value); break;
    case EffectType::BlockFirstHits: desc = compact ? std::format("格擋前{}次", eff.value) : std::format("格擋前{}次攻擊", eff.value); break;
    case EffectType::GoldCoefficient: desc = compact ? std::format("勝利+{}×最高星金", eff.value) : std::format("勝利獲得{}×最高星級金幣", eff.value); break;
    case EffectType::HurtInvincFrames: desc = std::format("受傷後無敵{}幀", eff.value); break;
    case EffectType::DashAttack: desc = "滑步攻擊"; break;
    case EffectType::DashChanceBoost: desc = std::format("滑步率+{}%", eff.value); break;
    case EffectType::MPRatioDmgBoost: desc = compact ? std::format("內力加傷≤{}%", eff.value) : std::format("當前內力比例加傷≤{}%", eff.value); break;
    case EffectType::DmgReduceDebuff: desc = compact ? std::format("降傷標記{}%/{}幀", eff.value, eff.value2) : std::format("攻擊{}%概率標記降傷{}幀", eff.value, eff.value2); break;
    default: desc = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + desc + durationSuffix() + countSuffix();
}

}    // namespace

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

int getRandomChessMusic()
{
    return pickRandomMusic(kChessMusicIds);
}

int getRandomBattleMusic()
{
    return pickRandomMusic(kBattleMusicIds);
}

bool isChessSceneMusic(int musicId)
{
    return containsMusicId(kChessMusicIds, musicId);
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
    return comboEffectLabel(eff, false);
}

std::string comboEffectCompactDesc(const ComboEffect& eff)
{
    return comboEffectLabel(eff, true);
}

std::vector<std::string> wrapDisplayText(const std::string& text, int maxWidth)
{
    if (text.empty() || maxWidth <= 0)
    {
        return {};
    }

    std::vector<DisplayGlyph> glyphs;
    for (size_t index = 0; index < text.size();)
    {
        auto glyph = readDisplayGlyph(text, index);
        index += glyph.text.size();
        glyphs.push_back(std::move(glyph));
    }

    std::vector<std::string> lines;
    size_t start = 0;
    while (start < glyphs.size())
    {
        int width = 0;
        size_t end = start;
        size_t preferredBreak = start;
        while (end < glyphs.size() && width + glyphs[end].width <= maxWidth)
        {
            width += glyphs[end].width;
            if (glyphs[end].preferredBreakAfter)
            {
                preferredBreak = end + 1;
            }
            ++end;
        }

        size_t lineEnd = end;
        if (end < glyphs.size() && preferredBreak > start)
        {
            lineEnd = preferredBreak;
        }
        if (lineEnd == start)
        {
            lineEnd = std::min(start + 1, glyphs.size());
        }

        std::string line;
        for (size_t i = start; i < lineEnd; ++i)
        {
            line += glyphs[i].text;
        }
        lines.push_back(trimLine(std::move(line)));

        start = lineEnd;
        while (start < glyphs.size() && glyphs[start].text == " ")
        {
            ++start;
        }
    }
    return lines;
}

}    // namespace KysChess
