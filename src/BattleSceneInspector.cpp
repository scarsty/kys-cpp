#include "BattleSceneHades.h"
#include "BattlePieceInspector.h"
#include "ChessGameSession.h"
#include "ChessUiStyle.h"

#include <cmath>
#include <limits>

namespace
{
namespace Ui = KysChess::ChessUiStyle;

void drawResourceBar(SDL_Rect frame, int value, int maximum, Color color, const char* name)
{
    auto* engine = Engine::getInstance();
    Ui::label(name, Ui::Type::metadata, frame.x, frame.y, Ui::muted, 56);
    const auto amount = std::format("{} / {}", value, maximum);
    const int width = Font::getTextDrawSize(amount) * Ui::Type::body / 2;
    Ui::label(amount, Ui::Type::body, frame.x + frame.w - width, frame.y, Ui::ink, width);
    const int barY = frame.y + 24;
    engine->fillRoundedRect(Ui::quietFill, frame.x, barY, frame.w, 12, 3);
    // 無內力上限的棋子仍保留空的內力條與數值。
    const int fill = maximum > 0 ? int(std::int64_t(frame.w) * std::clamp(value, 0, maximum) / maximum) : 0;
    if (fill > 0) engine->fillRoundedRect(color, frame.x, barY, fill, 12, 3);
}

const char* effectLifetimeLabel(KysChess::BattleEffectLifetime lifetime)
{
    using KysChess::BattleEffectLifetime;
    switch (lifetime)
    {
    case BattleEffectLifetime::Timed: return "限時";
    case BattleEffectLifetime::Consumable: return "可消耗";
    case BattleEffectLifetime::Ongoing: return "常駐";
    }
    std::unreachable();
}

struct EffectCard
{
    std::vector<std::string> lines;
    std::vector<std::string> metadata;
    int height{};
};
}

BattleControlLayout BattleSceneHades::battleControlLayout() const
{
    auto layout = makeBattleControlLayout(Engine::getInstance()->getUIWidth());
    if (inspected_unit_id_ >= 0 && !formation_preview_runtime_)
        addBattleInspectorControls(layout, inspector_layout_);
    return layout;
}

int BattleSceneHades::pieceAtPoint(SDL_FPoint point) const
{
    // 擁擠時選擇最靠近游標的棋子中心，避免以單位編號決定命中。
    int selected = -1;
    float nearest = std::numeric_limits<float>::max();
    for (const auto& target : piece_hit_targets_)
    {
        const auto& rect = target.frame;
        if (point.x < rect.x || point.x >= rect.x + rect.w || point.y < rect.y || point.y >= rect.y + rect.h) continue;
        const float distance = std::hypot(point.x - rect.x - rect.w / 2, point.y - rect.y - rect.h / 2);
        if (distance < nearest)
        {
            selected = target.unitId;
            nearest = distance;
        }
    }
    return selected;
}

void BattleSceneHades::selectPieceAtPoint(SDL_FPoint point)
{
    const int selected = pieceAtPoint(point);
    if (selected < 0) { dismissPieceInspector(); return; }
    inspected_unit_id_ = selected;
    inspector_pinned_ = true;
    inspector_layout_.page = 0;
}

void BattleSceneHades::dismissPieceInspector()
{
    inspected_unit_id_ = -1;
    inspector_pinned_ = false;
    inspector_layout_.page = 0;
}

void BattleSceneHades::drawPieceInspector()
{
    using namespace KysChess;
    using namespace KysChess::Battle;
    const auto* session = activeRuntimeSession();
    if (!session || formation_preview_runtime_) return;
    if (inspected_unit_id_ < 0)
    {
        Ui::panel(440, 660, 400, 44);
        Ui::controlLabel({440, 660, 400, 44}, "點選棋子查看屬性與狀態", 18, Ui::muted, Ui::Icon::Eye);
        return;
    }
    const auto& runtime = session->runtime();
    const auto& unit = runtime.units.requireCore(inspected_unit_id_);
    auto* engine = Engine::getInstance();
    for (const auto& target : piece_hit_targets_)
    {
        if (target.unitId != inspected_unit_id_) continue;
        const auto& r = target.frame;
        engine->drawRoundedRect(Ui::gold, int(r.x), int(r.y), int(r.w), int(r.h), 6);
    }

    const auto effects = battlePieceEffectRows(runtime, unit.id);
    std::vector<EffectCard> cards;
    std::vector<int> heights;
    for (const auto& effect : effects)
    {
        std::string value = effect.value;
        if (!effect.quantity.empty()) value += (value.empty() ? "" : " · ") + effect.quantity;
        auto lines = Ui::wrapLines(value, 28);
        const auto details = Ui::wrapLines(effect.detail, 28);
        lines.insert(lines.end(), details.begin(), details.end());
        std::string metadata = effect.source.empty() ? "" : "來源 " + effect.source;
        if (effect.lifetime == BattleEffectLifetime::Timed)
            metadata += (metadata.empty() ? "" : " · ") + effect.duration;
        auto metadataLines = Ui::wrapLines(metadata, 32);
        const int height = 36 + int(lines.size()) * 20 + int(metadataLines.size()) * 18;
        heights.push_back(height);
        cards.push_back({std::move(lines), std::move(metadataLines), height});
    }
    inspector_layout_ = makeBattleInspectorLayout(heights, inspector_layout_.page);
    const auto controls = battleControlLayout();
    const auto pointer = PointerInput::instance().logicalPointerUiPosition();
    const auto control = [&](BattleControlId id, SDL_Rect frame, Ui::Icon icon, bool disabled)
    {
        const Ui::ControlState state{.hovered = controls.hitTest(pointer) == id, .disabled = disabled};
        Ui::controlSurface(frame, state);
        Ui::controlLabel(frame, {}, 18, Ui::controlText(state), icon, Ui::controlText(state));
    };

    const auto summary = BattleInspectorLayout::summary;
    Ui::panel(summary.x, summary.y, summary.w, summary.h);
    // 共用面板與分隔線；資源、屬性與戰報數字各自對齊，避免多種卡片樣式並列。
    engine->fillColor(Ui::line, 624, 576, 1, 112);
    engine->fillColor(Ui::line, 936, 576, 1, 112);
    Ui::asset("head", unit.headId, {320, 576, 64, 64});
    Ui::label(unit.name, Ui::Type::sectionTitle, 396, 576, Ui::gold, 216);
    Ui::label(std::format("{} · {}星{}{}", unit.team == 0 ? "我方" : "敵方", unit.star,
        unit.cloneSourceUnitId >= 0 ? " · 分身" : "", unit.alive ? "" : " · 已退場"),
        Ui::Type::metadata, 396, 608, unit.alive ? Ui::muted : Ui::negative, 216);
    Ui::label(unit.skillNames, Ui::Type::metadata, 320, 646, Ui::ink, 296);

    const auto& content = session_transition_source_->content();
    const auto equipment = [&](int id, Ui::Icon symbol, int x, const char* empty)
    {
        if (id >= 0)
        {
            Ui::asset("item", id, {x, 676, 20, 20});
            Ui::label(content.items().at(id).name, Ui::Type::metadata, x + 28, 678, Ui::ink, 116);
        }
        else
        {
            Ui::asset("chess-preparation", Ui::iconId(symbol), {x, 676, 20, 20}, false, Ui::muted);
            Ui::label(empty, Ui::Type::metadata, x + 28, 678, Ui::muted, 116);
        }
    };
    equipment(unit.weaponId, Ui::Icon::Sword, 320, "未裝備武器");
    equipment(unit.armorId, Ui::Icon::Armor, 472, "未裝備防具");

    drawResourceBar({644, 576, 272, 36}, unit.vitals.hp, unit.vitals.maxHp, {38, 170, 76, 255}, "生命");
    drawResourceBar({644, 624, 272, 36}, unit.vitals.mp, unit.vitals.maxMp, {38, 112, 224, 255}, "內力");
    Ui::label(!unit.alive ? "已退場" : unit.animation.cooldown > 0
            ? std::format("冷卻 {} 幀", unit.animation.cooldown) : "冷卻 就緒",
        Ui::Type::metadata, 644, 678, Ui::muted, 152);
    Ui::asset("chess-status", int(BattleEffectIcon::Shield), {808, 676, 20, 20}, false, Ui::context);
    Ui::label(std::to_string(unit.shield), Ui::Type::metadata, 836, 678, Ui::context, 80);

    const auto stat = [&](const char* name, int value, int x, int y, int width, Color color)
    {
        Ui::label(name, Ui::Type::metadata, x, y, Ui::muted, width);
        Ui::label(std::to_string(value), Ui::Type::sectionTitle, x, y + 24, color, width);
    };
    stat("攻擊", effectAdjustedAttribute(runtime, unit.id, BattleAttribute::Attack, unit.stats.attack), 956, 576, 88, Ui::ink);
    stat("防禦", effectAdjustedAttribute(runtime, unit.id, BattleAttribute::Defence, unit.stats.defence), 1056, 576, 88, Ui::ink);
    stat("輕功", effectAndAreaAdjustedSpeed(runtime, unit.id, unit.stats.speed), 1156, 576, 92, Ui::ink);
    const auto& report = getBattleReport();
    const auto found = report.stats().find(unit.id);
    // 尚未造成或承受傷害的棋子還沒有戰報項目。
    const int dealt = found == report.stats().end() ? 0 : found->second.damageDealt;
    const int taken = found == report.stats().end() ? 0 : found->second.damageTaken;
    stat("造成傷害", dealt, 956, 648, 140, Ui::gold);
    stat("承受傷害", taken, 1108, 648, 140, Ui::negative);

    const auto frame = inspector_layout_.effects;
    Ui::panel(frame.x, frame.y, frame.w, frame.h);
    const int negativeCount = int(std::count_if(effects.begin(), effects.end(), [](const auto& effect) { return effect.negative; }));
    Ui::label(std::format("增益 {}", int(effects.size()) - negativeCount), 18, 28, frame.y + 10, Ui::positive, 120);
    Ui::label(std::format("減益 {}", negativeCount), 18, 160, frame.y + 10, Ui::negative, 120);
    if (effects.empty())
        Ui::controlLabel({28, frame.y + BattleInspectorLayout::effectHeader, 252, 28}, "目前無增益或減益", 16, Ui::muted);

    const auto& page = inspector_layout_.pages[inspector_layout_.page];
    int y = frame.y + BattleInspectorLayout::effectHeader;
    for (int row = page.firstRow; row < page.firstRow + page.rowCount; ++row)
    {
        const auto& card = cards[row];
        const auto& effect = effects[row];
        const Color color = effect.negative ? Ui::negative : Ui::positive;
        Ui::panel(28, y, 252, card.height, Ui::quietFill);
        engine->fillColor(color, 28, y + 8, 2, card.height - 16);
        Ui::asset("chess-status", int(effect.icon), {36, y + 6, 24, 24}, false, color);
        Ui::label(effect.name, 18, 68, y + 6, Ui::ink, 144);
        const Color lifetimeColor = effect.lifetime == BattleEffectLifetime::Timed ? Ui::context : Ui::muted;
        Ui::label(effectLifetimeLabel(effect.lifetime), 14, 224, y + 8, lifetimeColor, 48);
        int lineY = y + 30;
        for (const auto& line : card.lines)
        {
            Ui::label(line, 16, 40, lineY, color, 228);
            lineY += 20;
        }
        for (const auto& line : card.metadata)
        {
            Ui::label(line, 14, 40, lineY, Ui::muted, 228);
            lineY += 18;
        }
        y += card.height + BattleInspectorLayout::effectGap;
    }
    if (inspector_layout_.pages.size() > 1)
    {
        control(BattleControlId::InspectorPrevious, BattleInspectorLayout::previous, Ui::Icon::Up, inspector_layout_.page == 0);
        control(BattleControlId::InspectorNext, BattleInspectorLayout::next, Ui::Icon::Down,
            inspector_layout_.page + 1 == int(inspector_layout_.pages.size()));
        Ui::controlLabel({144, 656, 96, 36}, std::format("{} / {} 頁", inspector_layout_.page + 1, inspector_layout_.pages.size()),
            16, Ui::muted);
    }
}
