#include "ChessBattleOverview.h"

#include "Audio.h"
#include "ChessBattleOverviewLayout.h"
#include "ChessEftIds.h"
#include "ChessGameSession.h"
#include "ChessGuiBattleFlow.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessPresentationHelpers.h"
#include "ChessPvp.h"
#include "ChessUiCommon.h"
#include "ChessUiStyle.h"
#include "RunNode.h"
#include "ScenePreloader.h"
#include "SystemSettings.h"
#include "battle/BattleCombatIntent.h"
#include "battle/BattlePresentationVisuals.h"

#include <array>
#include <chrono>
#include <format>
#include <set>

namespace KysChess
{
namespace
{
using namespace ChessUiStyle;
namespace Layout = ChessBattleOverviewLayout;

bool contains(SDL_Rect frame, SDL_FPoint point)
{
    return point.x >= frame.x && point.x < frame.x + frame.w
        && point.y >= frame.y && point.y < frame.y + frame.h;
}

struct TextureLoadBatch
{
    explicit TextureLoadBatch(std::vector<TextureWarpper*> textures)
        : pending(std::move(textures)), total(pending.size())
    {
        std::erase_if(pending, [](const auto* texture) { return !texture || texture->loaded; });
    }

    ScenePreloader::LoadProgress progress() const { return {total - pending.size() + index, total}; }

    ScenePreloader::LoadProgress advance()
    {
        pending[index++]->load();
        return progress();
    }

    std::vector<TextureWarpper*> pending;
    std::size_t total{};
    std::size_t index{};
};

bool loadedAsset(const std::string& group, int id, SDL_Rect frame)
{
    const auto* texture = TextureManager::getInstance()->findTexture(group, id);
    if (!texture || !texture->loaded || !texture->tex[0]) return false;
    asset(group, id, frame);
    return true;
}

class BattleOverview : public RunNode
{
public:
    explicit BattleOverview(const ChessGameSession& session)
        : session_(session), analysis_(analyzePreparedChessBattle(*session.state().preparedBattle,
              session.content(), session.state().options.battleFrameLimit))
    {
        assert(analysis_.combatInitialized);
        setSize(1280, 720);
        for (const auto& unit : analysis_.units) teams_[unit.team].push_back(&unit);
        for (const auto& team : teams_) assert(team.size() <= Layout::maximumUnits);
        prepareLoads();
    }

    void draw() override
    {
        if (!isCurrentRunOwner()) return;
        controls_.clear();
        Engine::getInstance()->fillColor(canvasScrim, 0, 0, 1280, 720);
        panel(16, 16, 1248, 52);
        label("戰前總覽", 24, 32, 29, gold, 180);
        label(analysis_.identity.chosenMapName, 18, 230, 33, muted, 500);
        label("開戰屬性 · 已計入羈絆、內功與裝備", 16, 822, 34, context, 420);
        drawTeam(0);
        drawTeam(1);
        drawDetails();
        panel(16, 652, 1248, 60);
        // 左下角留給網頁與手機的平台按鈕。
        constexpr int statusX = 96;
        constexpr int statusWidth = 876;
        if (!flow_.assetsLoaded())
        {
            label("正在載入戰鬥 · 請稍候", Type::pageTitle, statusX, 657, context, statusWidth);
            label(std::format("{} · 載入進度 {}%", loads_.complete() ? "資源已就緒" : loads_.currentLabel(),
                    static_cast<int>(loads_.fraction() * 100)),
                Type::metadata, statusX, 685, ink, statusWidth);
            Engine::getInstance()->fillColor(activeFill, statusX, 705, statusWidth, 3);
            Engine::getInstance()->fillColor(context, statusX, 705,
                static_cast<int>(statusWidth * loads_.fraction()), 3);
        }
        else label(tapAnywhereEnabled()
            ? (session_.state().options.positionSwapEnabled ? "載入完成 · 點擊任意處進入佈陣 · Enter 繼續"
                                                          : "載入完成 · 點擊任意處開始戰鬥 · Enter 繼續")
            : "懸停預覽 · 點擊棋子固定／羈絆展開 · Enter 繼續", 17, statusX, 675, muted, statusWidth);
        controlSurface(Layout::proceed, {.disabled = !flow_.assetsLoaded(), .primary = flow_.assetsLoaded()});
        controlLabel(Layout::proceed,
            flow_.assetsLoaded() ? (session_.state().options.positionSwapEnabled ? "進入佈陣" : "開始戰鬥") : "準備中…",
            Type::sectionTitle, flow_.assetsLoaded() ? darkLabel : context);
        controls_.push_back({Layout::proceed, {}, [this] { setExit(true); }});
        flow_.markFramePresented();
    }

    void dealEvent(EngineEvent& event) override
    {
        if (flow_.runLoadBatch([this] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6);
            while (!loads_.complete())
            {
                loads_.advance();
                if (std::chrono::steady_clock::now() >= deadline) break;
            }
            return loads_.complete();
        }))
        {
#ifdef __EMSCRIPTEN__
            // 讓瀏覽器呈現上一幀，並收集同步 I/O 期間累積的輸入。
            emscripten_sleep(0);
#endif
            auto& input = PointerInput::instance();
            input.pumpSdlEvents();
            input.discardPendingUserInput();
            onPointerInputReset();
            invalidatePointerOwnership();
            event = {};
            return;
        }
        if (!flow_.interactive()) { event = {}; return; }
        if (event.type != EVENT_KEY_UP) return;
        if (event.key.key == SDLK_PAGEUP) changeDetailPage(-1);
        if (event.key.key == SDLK_PAGEDOWN) changeDetailPage(1);
        if (event.key.key == K_LEFT || event.key.key == K_RIGHT || event.key.key == K_UP || event.key.key == K_DOWN)
        {
            int side = pinned_ ? pinned_->side : 0;
            int index = pinned_ && pinned_->kind != Kind::Combo ? pinned_->index : 0;
            if (event.key.key == K_LEFT) side = 0;
            if (event.key.key == K_RIGHT) side = 1;
            if (event.key.key == K_UP) --index;
            if (event.key.key == K_DOWN) ++index;
            if (!teams_[side].empty()) pin({Kind::Unit, side, std::clamp(index, 0, static_cast<int>(teams_[side].size()) - 1)});
        }
    }

    PointerResult onPointerEvent(const PointerEvent& event) override
    {
        if (!flow_.interactive()) return PointerResult::Handled;
        // 交由 RunNode 的共用按下／放開序列處理滑鼠與主觸控，避免放開舊觸控就繼續。
        if (tapAnywhereEnabled()) return PointerResult::Ignored;
        const int target = hit(event.uiPosition);
        if (event.phase == PointerPhase::Motion)
        {
            if (!pinned_ && target >= 0 && controls_[target].target) select(controls_[target].target);
            return PointerResult::Handled;
        }
        if (event.button != SDL_BUTTON_LEFT) return PointerResult::Handled;
        if (event.phase == PointerPhase::ButtonDown)
        {
            pressed_ = target;
            return PointerResult::Captured;
        }
        if (event.phase == PointerPhase::ButtonUp)
        {
            if (target >= 0 && target == pressed_)
            {
                auto control = controls_[target];
                if (control.target)
                {
                    if (control.target->kind == Kind::Combo)
                    {
                        const auto target = *control.target;
                        std::vector<int> roles;
                        for (const auto* unit : teams_[target.side]) roles.push_back(unit->roleId);
                        showChessComboOverview(session_.content(), synergies(target.side), roles,
                            synergies(target.side)[target.index].comboId, target.side == 0 ? "我方羈絆" : "敵方羈絆");
                    }
                    else pin(*control.target);
                }
                else control.activate();
            }
            pressed_ = -1;
        }
        if (event.phase == PointerPhase::Cancel) pressed_ = -1;
        return PointerResult::Handled;
    }

    void onPointerInputReset() override { pressed_ = -1; }
    PointerActivationScope pointerActivationScope() const override
    {
        return flow_.interactive() && tapAnywhereEnabled()
            ? PointerActivationScope::Anywhere : PointerActivationScope::HitTargetOnly;
    }
    void onPressedOK() override { if (flow_.interactive()) setExit(true); }
    void onPressedCancel() override { pinned_.reset(); select({}); }

private:
    enum class Kind { Unit, Equipment, Combo };
    struct Target
    {
        Kind kind{};
        int side{};
        int index{};
        int slot{};
        bool operator==(const Target&) const = default;
    };
    struct Control
    {
        SDL_Rect frame{};
        std::optional<Target> target;
        std::function<void()> activate;
    };
    const ChessGameSession& session_;
    ChessPreparedBattleAnalysis analysis_;
    std::array<std::vector<const ChessPreparedBattleUnitAnalysis*>, 2> teams_;
    std::array<int, 2> synergyPage_{};
    ScenePreloader::LoadQueue loads_;
    ChessGuiPreBattleFlow flow_;
    std::vector<Control> controls_;
    int pressed_ = -1;
    std::optional<Target> selected_;
    std::optional<Target> pinned_;
    int detailPage_{};
    std::string detailTitle_;
    std::string detailContext_;
    std::vector<std::string> detailLines_;

    bool tapAnywhereEnabled() const { return SystemSettings::getInstance()->data().preBattleTapAnywhere; }

    const std::vector<ChessComboMetadata>& synergies(int side) const
    {
        return side == 0 ? analysis_.allySynergies : analysis_.enemySynergies;
    }

    void prepareLoads()
    {
        std::set<std::pair<std::string, int>> portraits;
        std::set<std::string> groups;
        for (const auto& unit : analysis_.units)
        {
            portraits.emplace("head", unit.headId);
            if (unit.weaponItemId >= 0) portraits.emplace("item", unit.weaponItemId);
            if (unit.armorItemId >= 0) portraits.emplace("item", unit.armorItemId);
            groups.insert(std::format("fight/fight{:03}", unit.headId));
        }
        for (int side = 0; side < 2; ++side)
            for (const auto& combo : synergies(side))
            {
                const auto definition = std::ranges::find(session_.content().combos(), combo.comboId, &ComboDef::id);
                assert(definition != session_.content().combos().end());
                portraits.emplace("chess-synergies", definition->iconId);
            }
        for (const auto& [group, id] : portraits) textureLoad("棋子與裝備圖示", group, id);
        for (const int effect : analysis_.presentationAssets.effectIds)
            if (effect >= 0) groups.insert(std::format("eft/eft{:03}", effect));
        for (const auto effect : EFT_ALL) groups.insert(std::format("eft/eft{:03}", std::to_underlying(effect)));
        for (const std::string_view path : Battle::BattleEffectVisualPaths) groups.emplace(path);
        for (const auto& path : groups) textureGroupLoad(path.starts_with("fight/") ? "棋子動作" : "武學特效", path);
        for (int effect = 0; effect < 5; ++effect) textureLoad("受擊特效", std::format("eft/bld{:03}", effect), 0);
        const auto map = session_.content().battleMaps().find(analysis_.identity.chosenMapId);
        assert(map != session_.content().battleMaps().end());
        const int battlefield = session_.state().preparedBattle->layout == PreparedChessBattleLayout::PvpArena
            ? ChessPvpMapLayout::BattlefieldId : map->second.battlefieldId;
        loads_.add("戰場地形", [battlefield, batch = std::optional<TextureLoadBatch>{}]() mutable {
            if (!batch)
            {
                std::vector<TextureWarpper*> textures;
                for (const auto& [group, id] : ScenePreloader::battlefieldAssets(battlefield))
                    textures.push_back(TextureManager::getInstance()->getTexture(group, id));
                batch.emplace(std::move(textures));
                return batch->progress();
            }
            return batch->advance();
        });
        audioLoad("戰場音樂", map->second.musicId, {}, {});
        for (const int id : analysis_.presentationAssets.attackSoundIds)
            audioLoad("攻擊音效", -1, {id}, {});
        for (const int id : analysis_.presentationAssets.effectIds)
            audioLoad("武學音效", -1, {}, {id});
    }

    void textureLoad(const std::string& title, const std::string& group, int id)
    {
        const auto* cached = TextureManager::getInstance()->findTexture(group, id);
        loads_.add(title, [group, id] {
            if (auto* texture = TextureManager::getInstance()->getTexture(group, id)) texture->load();
            return ScenePreloader::LoadProgress{1, 1};
        }, {cached && cached->loaded ? 1u : 0u, 1});
    }

    void textureGroupLoad(const std::string& title, const std::string& path)
    {
        std::optional<TextureLoadBatch> batch;
        if (const auto* group = TextureManager::getInstance()->findTextureGroup(path)) batch.emplace(group->group_);
        const auto progress = batch ? batch->progress() : ScenePreloader::LoadProgress{};
        loads_.add(title, [path, batch = std::move(batch)]() mutable {
            if (!batch)
            {
                batch.emplace(TextureManager::getInstance()->getTextureGroup(path)->group_);
                return batch->progress();
            }
            return batch->advance();
        }, progress);
    }

    void audioLoad(const std::string& title, int music, std::vector<int> attacks, std::vector<int> effects)
    {
        const bool cached = Audio::getInstance()->isBattleAudioPreloaded(music, attacks, effects);
        loads_.add(title, [music, attacks = std::move(attacks), effects = std::move(effects)] {
            Audio::getInstance()->preloadBattleAudio(music, attacks, effects);
            return ScenePreloader::LoadProgress{1, 1};
        }, {cached ? 1u : 0u, 1});
    }

    void drawTeam(int side)
    {
        const auto frame = Layout::team(side);
        const int x = frame.x + 12;
        panel(frame.x, frame.y, frame.w, frame.h);
        label(std::format("{} · {} 名", side == 0 ? "我方" : "敵方", teams_[side].size()), 20, x, 88, side == 0 ? positive : negative, 220);
        const std::array<std::pair<const char*, int>, 7> headers{{{"棋子", 32}, {"星", 170}, {"生命", 218},
            {"攻擊", 302}, {"防禦", 376}, {"輕功", 450}, {"裝備", 524}}};
        for (const auto& [title, offset] : headers) label(title, 16, x + offset, 115, muted, 68);
        const auto pointer = PointerInput::instance().logicalPointerUiPosition();
        for (int i = 0; i < static_cast<int>(teams_[side].size()); ++i)
        {
            const auto& unit = *teams_[side][i];
            const auto& stats = *unit.initializedCombatStats;
            const auto row = Layout::row(side, i);
            const bool selected = selected_ && selected_->kind != Kind::Combo && selected_->side == side && selected_->index == i;
            if (selected || (flow_.interactive() && !tapAnywhereEnabled() && contains(row, pointer))) panel(row.x, row.y, row.w, row.h, activeFill);
            else if (i % 2 == 0) panel(row.x, row.y, row.w, row.h, quietFill);
            controls_.push_back({row, Target{Kind::Unit, side, i}, {}});
            loadedAsset("head", unit.headId, {x, row.y + 1, 24, 24});
            const auto* role = session_.content().role(unit.roleId);
            label(unit.name, 18, x + 32, row.y + 3, chessPieceTierColor(role->Cost), 132);
            label(std::format("★{}", unit.star), 17, x + 170, row.y + 4, gold, 44);
            label(std::to_string(stats.maxHp), 18, x + 218, row.y + 3, positive, 78);
            label(std::to_string(stats.attack), 18, x + 302, row.y + 3, ink, 68);
            label(std::to_string(stats.defence), 18, x + 376, row.y + 3, ink, 68);
            label(std::to_string(stats.speed), 18, x + 450, row.y + 3, context, 68);
            for (int slot = 0; slot < 2; ++slot)
            {
                const int item = slot == 0 ? unit.weaponItemId : unit.armorItemId;
                const auto rect = Layout::equipment(side, i, slot);
                if (item >= 0)
                {
                    if (!loadedAsset("item", item, rect)) label("◇", 16, rect.x + 4, rect.y + 2, gold, 24);
                    controls_.push_back({rect, Target{Kind::Equipment, side, i, slot}, {}});
                }
                else label("—", 14, rect.x + 4, rect.y + 4, muted, 24);
            }
        }
        const auto& combos = synergies(side);
        const int start = synergyPage_[side] * Layout::synergiesPerPage;
        for (int i = 0; i < Layout::synergiesPerPage && start + i < static_cast<int>(combos.size()); ++i)
        {
            const auto& combo = combos[start + i];
            const auto rect = Layout::synergy(side, i);
            const auto definition = std::ranges::find(session_.content().combos(), combo.comboId, &ComboDef::id);
            loadedAsset("chess-synergies", definition->iconId, {rect.x, rect.y, 24, 24});
            const int threshold = combo.nextThresholdIndex >= 0 ? combo.nextThresholdIndex : static_cast<int>(combo.thresholds.size()) - 1;
            label(std::format("{}/{}", combo.effectiveCount, combo.thresholds[threshold].requiredCount), 15,
                rect.x + 26, rect.y + 4, positive, 42);
            controls_.push_back({rect, Target{Kind::Combo, side, start + i}, {}});
        }
        if (combos.empty()) label("尚無羈絆啟動", 16, x, Layout::synergyTop + 7, muted, 300);
        if (combos.size() > Layout::synergiesPerPage)
        {
            const SDL_Rect next{x + 562, Layout::synergyTop, 30, 48};
            label("›", 22, next.x + 5, next.y + 12, gold, 24);
            controls_.push_back({next, {}, [this, side, count = combos.size()] {
                synergyPage_[side] = (synergyPage_[side] + 1) % ((count + Layout::synergiesPerPage - 1) / Layout::synergiesPerPage);
                pressed_ = -1;
            }});
        }
    }

    void pin(Target target) { pinned_ = target; select(target); }
    void select(std::optional<Target> target)
    {
        if (selected_ == target) return;
        selected_ = target;
        detailPage_ = 0;
        detailLines_.clear();
        if (!target) return;
        std::string description;
        detailContext_ = target->side == 0 ? "我方" : "敵方";
        if (target->kind == Kind::Combo)
        {
            const auto& combo = synergies(target->side)[target->index];
            detailTitle_ = combo.name;
            const auto& threshold = combo.thresholds[combo.activeThresholdIndex];
            description = std::format("{}點 · {} · 已啟動\n{}", threshold.requiredCount, threshold.name,
                joinEffectDescriptionRows(threshold.effects, " · "));
            detailContext_ += " · 點擊查看完整羈絆";
        }
        else
        {
            const auto& unit = *teams_[target->side][target->index];
            detailTitle_ = unit.name + std::format(" · {}星", unit.star);
            if (target->kind == Kind::Equipment)
            {
                const int id = target->slot == 0 ? unit.weaponItemId : unit.armorItemId;
                const auto* item = session_.content().item(id);
                detailTitle_ = item->name;
                detailContext_ += " · " + unit.name;
                if (item->addMaxHP) description += std::format("生命 {:+}  ", item->addMaxHP);
                if (item->addAttack) description += std::format("攻擊 {:+}  ", item->addAttack);
                if (item->addDefence) description += std::format("防禦 {:+}  ", item->addDefence);
                if (item->addSpeed) description += std::format("輕功 {:+}  ", item->addSpeed);
                if (!description.empty()) description += '\n';
                for (const auto& row : panelTextRowsForEquipment(chessEquipmentMetadata(session_.content(), id), unit.roleId))
                    description += row.text + '\n';
            }
            else
            {
                for (std::size_t i = 0; i < unit.abilities.size(); ++i)
                {
                    const auto& ability = unit.abilities[i];
                    const auto* magic = session_.content().magic(ability.magicId);
                    const auto operation = Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(magic->AttackAreaType);
                    const char* style = operation == Battle::BattleOperationType::Melee ? "輕擊" :
                        operation == Battle::BattleOperationType::TrackingProjectile ? "重擊" : "遠程";
                    const bool ultimate = i + 1 == unit.abilities.size();
                    description += std::format("{} · {} · {} · 威力 {}\n", ultimate ? "絕招" : "普攻", ability.name, style, ability.powerByStar.front().power);
                    if (ultimate) description += joinEffectDescriptionRows(ability.effects) + '\n';
                }
                description += std::format("武器：{}　防具：{}\n{}", unit.weaponName.empty() ? "無" : unit.weaponName,
                    unit.armorName.empty() ? "無" : unit.armorName, analysis_.initializedStatsNote);
            }
        }
        detailLines_ = wrapLines(description, 100);
    }

    void changeDetailPage(int delta)
    {
        if (!selected_) return;
        pinned_ = selected_;
        detailPage_ = std::clamp(detailPage_ + delta, 0, std::max(0, (static_cast<int>(detailLines_.size()) - 1) / 3));
    }

    void drawDetails()
    {
        const auto frame = Layout::details;
        panel(frame.x, frame.y, frame.w, frame.h);
        if (!flow_.assetsLoaded() || !selected_)
        {
            label(flow_.assetsLoaded() ? (tapAnywhereEnabled() ? "快速繼續已開啟" : "檢視器") : "正在準備戰鬥資源", 22, 32, 563, gold, 280);
            label(flow_.assetsLoaded() ? (tapAnywhereEnabled()
                ? "點擊任意處即可繼續；若要點選棋子、裝備或羈絆查看詳情，請關閉系統設定中的「戰前點擊任意處繼續」"
                : "選取棋子查看武學；選取裝備或羈絆圖示查看效果；雙方陣容始終保留在上方")
                : "正在載入地圖、棋子與武學特效，完成後即可檢視陣容並繼續", 18, 32, 604,
                flow_.assetsLoaded() ? muted : ink, 1190);
            return;
        }
        label(detailTitle_, 20, 32, 560, gold, 250);
        label(detailContext_ + (selected_->kind == Kind::Combo ? "" : pinned_ ? " · 已固定" : " · 點擊固定"), 15, 32, 588, muted, 250);
        if (pinned_)
        {
            label("取消固定", 15, 32, 617, context, 120);
            controls_.push_back({{28, 611, 150, 30}, {}, [this] { pinned_.reset(); }});
        }
        for (int i = 0; i < 3 && detailPage_ * 3 + i < static_cast<int>(detailLines_.size()); ++i)
            label(detailLines_[detailPage_ * 3 + i], 18, 292, 560 + i * 26, ink, 900);
        if (detailLines_.size() > 3)
        {
            const int pages = (static_cast<int>(detailLines_.size()) + 2) / 3;
            label(std::format("{}/{}", detailPage_ + 1, pages), 14, 1196, 588, muted, 54);
            asset("chess-preparation", iconId(Icon::Up), {1202, 553, 28, 24});
            asset("chess-preparation", iconId(Icon::Down), {1202, 613, 28, 24});
            controls_.push_back({{1194, 551, 58, 30}, {}, [this] { changeDetailPage(-1); }});
            controls_.push_back({{1194, 611, 58, 32}, {}, [this] { changeDetailPage(1); }});
        }
    }

    int hit(SDL_FPoint point) const
    {
        for (int i = static_cast<int>(controls_.size()) - 1; i >= 0; --i)
            if (contains(controls_[i].frame, point)) return i;
        return -1;
    }
};
}

void showChessBattleOverview(const ChessGameSession& session)
{
    std::make_shared<BattleOverview>(session)->run();
}
}
