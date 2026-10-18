#include "BattleSceneHades.h"
#include "BattlePieceInspector.h"
#include "ChessUiStyle.h"

#include <cmath>
#include <limits>

namespace
{
constexpr SDL_Rect InspectorFrame{900, 164, 364, 540};
constexpr SDL_Rect CloseFrame{1208, 176, 40, 36};
constexpr SDL_Rect PreviousFrame{916, 652, 72, 36};
constexpr SDL_Rect NextFrame{1176, 652, 72, 36};
constexpr int EffectTop = 374;
constexpr int EffectBottom = 642;
constexpr int EffectGap = 6;
}

BattleControlLayout BattleSceneHades::battleControlLayout() const
{
    auto layout = makeBattleControlLayout(Engine::getInstance()->getUIWidth());
    if (inspected_unit_id_ >= 0)
    {
        layout.set(BattleControlId::InspectorClose, CloseFrame);
        if (inspector_max_scroll_ > 0)
        {
            layout.set(BattleControlId::InspectorPrevious, PreviousFrame);
            layout.set(BattleControlId::InspectorNext, NextFrame);
        }
        layout.set(BattleControlId::InspectorBody, InspectorFrame);
    }
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
    if (selected < 0) return;
    inspected_unit_id_ = selected;
    inspector_pinned_ = true;
    inspector_scroll_ = 0;
}

void BattleSceneHades::drawPieceInspector()
{
    using namespace KysChess;
    using namespace KysChess::Battle;
    namespace Ui = ChessUiStyle;
    const auto* session = activeRuntimeSession();
    if (!session || formation_preview_runtime_) return;
    if (inspected_unit_id_ < 0)
    {
        Ui::panel(900, 660, 364, 44);
        Ui::controlLabel({900, 660, 364, 44}, "點選棋子查看屬性與狀態", 18, Ui::muted, Ui::Icon::Eye);
        return;
    }
    const auto& runtime = session->runtime();
    const auto& unit = runtime.units.requireCore(inspected_unit_id_);
    for (const auto& target : piece_hit_targets_)
    {
        if (target.unitId != inspected_unit_id_) continue;
        const auto& r = target.frame;
        Engine::getInstance()->drawRoundedRect(Ui::gold, int(r.x), int(r.y), int(r.w), int(r.h), 6);
    }
    Ui::panel(InspectorFrame.x, InspectorFrame.y, InspectorFrame.w, InspectorFrame.h);
    const auto pointer = PointerInput::instance().logicalPointerUiPosition();
    const auto layout = battleControlLayout();
    const auto control = [&](BattleControlId id, SDL_Rect frame, Ui::Icon icon, bool disabled)
    {
        const Ui::ControlState state{.hovered = layout.hitTest(pointer) == id, .disabled = disabled};
        Ui::controlSurface(frame, state);
        Ui::controlLabel(frame, {}, 18, Ui::controlText(state), icon, Ui::controlText(state));
    };
    control(BattleControlId::InspectorClose, CloseFrame, Ui::Icon::Close, false);
    Ui::label(unit.name, 22, 916, 180, Ui::gold, 280);
    Ui::label(std::format("{} · {}星{}{}", unit.team == 0 ? "我方" : "敵方", unit.star,
        unit.cloneSourceUnitId >= 0 ? " · 分身" : "", unit.alive ? "" : " · 已退場"),
        16, 916, 212, unit.alive ? Ui::context : Ui::negative, 332);
    const auto stat = [&](std::string text, int x, int y, Color color = Ui::ink)
    {
        Ui::label(text, 18, x, y, color, x == 916 ? 172 : 156);
    };
    stat(std::format("生命 {}/{}", unit.vitals.hp, unit.vitals.maxHp), 916, 244);
    stat(std::format("內力 {}/{}", unit.vitals.mp, unit.vitals.maxMp), 1092, 244);
    stat(std::format("攻擊 {}", effectAdjustedAttribute(runtime, unit.id, BattleAttribute::Attack, unit.stats.attack)), 916, 272);
    stat(std::format("防禦 {}", effectAdjustedAttribute(runtime, unit.id, BattleAttribute::Defence, unit.stats.defence)), 1092, 272);
    stat(std::format("輕功 {}", effectAndAreaAdjustedSpeed(runtime, unit.id, unit.stats.speed)), 916, 300);
    stat(std::format("護盾 {}", unit.shield), 1092, 300, Ui::context);
    Ui::label(std::format("冷卻 {} 幀", unit.animation.cooldown), 16, 916, 328, Ui::muted, 332);

    auto effects = battlePieceEffectRows(runtime, unit.id);
    std::stable_sort(effects.begin(), effects.end(), [](const auto& a, const auto& b) { return a.negative < b.negative; });
    const int negativeCount = int(std::count_if(effects.begin(), effects.end(), [](const auto& effect) { return effect.negative; }));
    const int positiveCount = int(effects.size()) - negativeCount;
    int headingX = 916;
    for (const auto& [count, negative] : {std::pair{positiveCount, false}, std::pair{negativeCount, true}})
    {
        if (count == 0) continue;
        Ui::label(std::format("{} {}", negative ? "減益" : "增益", count), 18, headingX, 352,
            negative ? Ui::negative : Ui::positive, 156);
        headingX += 168;
    }

    // 以完整效果卡片捲動，避免名稱、數值和來源被分到不同頁。
    struct Card { std::vector<std::string> lines; int height{}; };
    std::vector<Card> cards;
    for (const auto& effect : effects)
    {
        auto lines = Ui::wrapLines(effect.value, 32);
        const auto details = Ui::wrapLines(effect.detail, 32);
        lines.insert(lines.end(), details.begin(), details.end());
        const int height = 56 + int(lines.size()) * 22;
        cards.push_back({std::move(lines), height});
    }
    inspector_max_scroll_ = int(cards.size());
    int tailHeight = 0;
    while (inspector_max_scroll_ > 0)
    {
        const int height = cards[inspector_max_scroll_ - 1].height + EffectGap;
        if (tailHeight + height > EffectBottom - EffectTop) break;
        tailHeight += height;
        --inspector_max_scroll_;
    }
    inspector_scroll_ = std::clamp(inspector_scroll_, 0, inspector_max_scroll_);
    int y = EffectTop;
    int visible = 0;
    for (int row = inspector_scroll_; row < int(cards.size()); ++row)
    {
        const auto& card = cards[row];
        if (y + card.height > EffectBottom) break;
        const auto& effect = effects[row];
        const Color color = effect.negative ? Ui::negative : Ui::positive;
        Ui::panel(916, y, 332, card.height, Ui::quietFill);
        Engine::getInstance()->fillColor(color, 916, y + 8, 2, card.height - 16);
        Ui::asset("chess-status", static_cast<int>(effect.icon), {924, y + 10, 32, 32}, false, color);
        Ui::label(effect.name, 18, 966, y + 7, Ui::ink, 220);
        Ui::label(effect.quantity, 16, 1196, y + 9, color, 48);
        int lineY = y + 31;
        for (const auto& line : card.lines)
        {
            Ui::label(line, 16, 966, lineY, color, 270);
            lineY += 22;
        }
        Ui::label(effect.source.empty() ? "" : "來源 " + effect.source, 14, 966, lineY, Ui::muted, 152);
        Ui::label(effect.duration, 14, 1124, lineY, Ui::muted, 116);
        y += card.height + EffectGap;
        ++visible;
    }
    if (inspector_max_scroll_ > 0)
    {
        control(BattleControlId::InspectorPrevious, PreviousFrame, Ui::Icon::Up, inspector_scroll_ == 0);
        control(BattleControlId::InspectorNext, NextFrame, Ui::Icon::Down, inspector_scroll_ == inspector_max_scroll_);
        Ui::label(std::format("{}–{} / {}", inspector_scroll_ + 1, inspector_scroll_ + visible, effects.size()),
            16, 1000, 662, Ui::muted, 164);
    }
}
