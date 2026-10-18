#include "ChessUiCommon.h"

#include "Audio.h"
#include "Engine.h"
#include "Font.h"
#include "TextBox.h"
#include "ChessUiStyle.h"
#include "ChessSelectionScreen.h"
#include "GameUtil.h"
#include "ChessPreparationModel.h"

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

}    // namespace

void showChessMessage(const std::string& text, int fontSize)
{
    class Message : public DismissibleTextBox
    {
        void draw() override
        {
            using namespace ChessUiStyle;
            auto* engine = Engine::getInstance();
            engine->fillColor({18, 17, 16, 160}, 0, 0, engine->getUIWidth(), engine->getUIHeight());
            const int size = std::min(font_size_, 26);
            const auto rows = wrapLines(text_, 64);
            const int height = static_cast<int>(rows.size()) * (size + 10) + 108;
            const int top = (engine->getUIHeight() - height) / 2;
            const int left = (engine->getUIWidth() - 920) / 2;
            panel(left, top, 920, height);
            int y = top + 32;
            for (const auto& row : rows)
            {
                Font::getInstance()->draw(row, size, left + 32, y, ink);
                y += size + 10;
            }
            engine->fillColor(line, left + 32, y + 8, 856, 1);
            Font::getInstance()->draw("輕觸或按確認繼續", 18, left + 32, y + 28, muted);
        }
    };
    auto box = std::make_shared<Message>();
    box->setText(text);
    box->setFontSize(fontSize);
    box->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

int runChessChoiceMenu(const std::string& title, const std::vector<std::string>& choices,
    ChessChoiceStyle style)
{
    std::vector<ChessSelectionItem> items;
    for (const auto& choice : choices)
        items.push_back({.title = choice});
    auto menu = std::make_shared<ChessSelectionScreen>(title, std::move(items));
    menu->setCommandMode(true);
    menu->setCompactChoices(choices.size() <= 4);
    if (style != ChessChoiceStyle::Options)
        menu->setConfirmation(style == ChessChoiceStyle::DestructiveConfirmation);
    menu->setActionLabel("確認");
    return menu->run();
}

bool confirmChessBan(const std::string& roleName)
{
    std::vector<ChessSelectionItem> items{{.title = "確認封禁"}, {.title = "取消"}};
    auto menu = std::make_shared<ChessSelectionScreen>(std::format("封禁「{}」？", roleName), std::move(items));
    menu->setCompactChoices(true);
    menu->setConfirmation(true);
    menu->setSelectedItem(1);
    menu->setHeaderNote("本局無法解除；後續商店不再抽到。保留已持有棋子與當輪商品。");
    return menu->run() == 0;
}

namespace
{
ChessSelectionItem comboOverviewItem(const ChessGameContent& content, const ChessComboMetadata& metadata,
    std::span<const int> deployedRoles, const std::string& bonusNote = {}, const ChessSessionState* state = nullptr)
{
    using namespace ChessUiStyle;
    const auto found = std::ranges::find(content.combos(), metadata.comboId, &ComboDef::id);
    assert(found != content.combos().end());
    const auto& combo = *found;
    ChessSelectionItem item{
        .title = combo.name, .subtitle = metadata.active ? "已啟動" : "未啟動",
        .badge = metadata.progressCount,
        .image = {"chess-synergies", combo.iconId}, .accent = metadata.active ? positive : muted,
    };
    item.openCombo = [&content, state](int id) {
        showChessComboOverview(content, state ? *state : ChessSessionState{}, id);
    };
    ChessOverviewColumn members{.width = 256};
    members.rows.push_back({"成員 · 出戰標記 ✓", gold, true});
    auto memberRoleIds = combo.memberRoleIds;
    std::ranges::stable_sort(memberRoleIds, {}, [&](int id) { return content.role(id)->Cost; });
    for (const int id : memberRoleIds)
    {
        const bool deployed = std::ranges::contains(deployedRoles, id);
        if (!std::ranges::contains(content.poolRoleIds(), id) && !deployed) continue;
        const auto* role = content.role(id);
        members.rows.push_back({.text = std::format("{} · {}費{}", role->Name, role->Cost, deployed ? " ✓" : ""),
            .color = chessPieceTierColor(role->Cost),
            .inspect = [&content, state, id] {
                return buildChessCharacterPresentation(content, state ? *state : ChessSessionState{}, id);
            }});
    }
    ChessOverviewColumn effects{.width = 664};
    if (combo.starSynergyBonus) effects.rows.push_back({"★ 星級計點 · 每多一星增加一點", gold, true});
    if (!bonusNote.empty()) effects.rows.push_back({bonusNote, context, true});
    for (const auto& threshold : metadata.thresholds)
    {
        effects.rows.push_back({std::format("{}點 · {} · {}", threshold.requiredCount, threshold.name,
            threshold.active ? "已啟動" : "未啟動"), threshold.active ? positive : gold, true});
        for (const auto& line : panelTextRowsForEffectDescription(threshold.effects))
            effects.rows.push_back({line.text, ink});
    }
    item.overview = {std::move(members), std::move(effects)};
    return item;
}

void runComboOverview(std::vector<ChessSelectionItem> items, int selected, const std::string& title)
{
    auto screen = std::make_shared<ChessSelectionScreen>(title, std::move(items));
    screen->setOverviewMode();
    screen->setBrowseOnly(true);
    screen->setSelectedItem(selected);
    screen->run();
}
}

void showChessComboOverview(const ChessGameContent& content, const ChessSessionState& state, int comboId)
{
    std::vector<ChessSelectionItem> items;
    std::vector<int> deployedRoles;
    for (const auto& [id, piece] : state.roster) if (piece.deployed) deployedRoles.push_back(piece.roleId);
    int selected{};
    for (const auto& row : chessPreparationCombos(state, content))
    {
        const auto& combo = *row.definition;
        if (std::ranges::none_of(combo.memberRoleIds, [&](int role) { return std::ranges::contains(content.poolRoleIds(), role); })) continue;
        if (combo.id == comboId) selected = static_cast<int>(items.size());
        const auto metadata = chessComboMetadata(content, combo, row.progress.physicalCount, row.progress.effectiveCount,
            row.progress.activeThresholdIndex, row.progress.nextThresholdIndex, row.progress.contributions,
            EffectDescriptionStyle::Full, row.progress.requiredCounts);
        const auto bonusNote = state.selectedTalentComboId == combo.id
            ? std::format("合縱 · 永久 +{}點", content.balance().talent(state.talent).selectedComboBonus) : std::string{};
        auto item = comboOverviewItem(content, metadata, deployedRoles, bonusNote, &state);
        items.push_back(std::move(item));
    }
    runComboOverview(std::move(items), selected, "羈絆一覽");
}

void showChessComboOverview(const ChessGameContent& content, std::span<const ChessComboMetadata> combos,
    std::span<const int> deployedRoles, int comboId, const std::string& title)
{
    std::vector<ChessSelectionItem> items;
    int selected{};
    for (const auto& combo : combos)
    {
        if (combo.comboId == comboId) selected = static_cast<int>(items.size());
        auto item = comboOverviewItem(content, combo, deployedRoles);
        items.push_back(std::move(item));
    }
    runComboOverview(std::move(items), selected, title);
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

}    // namespace KysChess
