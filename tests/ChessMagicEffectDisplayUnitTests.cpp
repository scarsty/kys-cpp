#include "ChessMagicEffectDisplay.h"
#include "ChessRoleDetailLayout.h"
#include "ChessBattleEffectTestHelpers.h"
#include "DisplayText.h"
#include "Types.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

using namespace KysChess;

TEST_CASE("ChessMagicEffectDisplay_InsertsCompactEffectRowsAfterUltimateSkill", "[chess][effects][magic]")
{
    const auto root = YAML::Load(R"(
絕招:
  - 武功: 26
    名稱: 降龍十八掌
    效果:
      - 類型: 命中眩暈
        持續幀數: 14
      - 類型: 出招全隊回內
        回復內力: 30
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(parseMagicEffects(root, definitions, "絕招顯示"));
    REQUIRE(definitions.size() == 1);
    REQUIRE(definitions[0].rules.size() == 2);

    Magic normal;
    normal.ID = 5;
    normal.Name = "寒冰綿掌";
    Magic ultimate;
    ultimate.ID = 26;
    ultimate.Name = "降龍十八掌";

    std::vector<const MagicSave*> magics{ &normal, &ultimate };
    const auto rows = buildChessMagicEffectDisplayRows(magics, definitions, ultimate.ID);

    REQUIRE(rows.size() >= 4);
    CHECK(rows[0].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[0].text == "寒冰綿掌");
    CHECK_FALSE(rows[0].ultimate);
    CHECK(rows[1].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[1].text == "降龍十八掌");
    CHECK(rows[1].ultimate);
    CHECK(rows[2].kind == ChessMagicEffectDisplayLineKind::Effect);
    CHECK(rows[2].text.starts_with("命中"));
    std::string effectText;
    for (std::size_t index = 2; index < rows.size(); ++index)
    {
        CHECK(rows[index].kind == ChessMagicEffectDisplayLineKind::Effect);
        effectText += rows[index].text;
    }
    CHECK(effectText.find("眩暈") != std::string::npos);
    CHECK(effectText.find("14幀") != std::string::npos);
    CHECK(effectText.find("全隊內力+30") != std::string::npos);
}

TEST_CASE("ChessMagicEffectDisplay_FitsWrappedEffectsInOneBoundedColumn",
          "[chess][effects][magic][layout]")
{
    constexpr int viewportWidth = 244;
    constexpr int viewportHeight = 133;
    Magic normal;
    normal.ID = 1;
    normal.Name = "普通武學";
    Magic ultimate;
    ultimate.ID = 2;
    ultimate.Name = "絕學";
    std::vector<ChessMagicEffectDisplayLine> rows{
        { ChessMagicEffectDisplayLineKind::Skill, &normal, normal.Name },
        { ChessMagicEffectDisplayLineKind::Skill, &ultimate, ultimate.Name, true },
        {
            .kind = ChessMagicEffectDisplayLineKind::Effect,
            .magic = &ultimate,
            .text = "全隊，防+66，100幀，額外長文字，再追加一段完整效果，且保留所有條件與結果",
            .ultimate = true,
            .breakBefore = EffectDescriptionSemanticBreak::Block,
        },
    };

    const auto layout = layoutChessMagicEffectDisplay(rows, viewportWidth, viewportHeight);
    REQUIRE(layout.lines.size() > rows.size());
    CHECK(layout.requiredHeight <= viewportHeight);
    int previousBottom = 0;
    std::string wrappedEffect;
    int effectPhysicalLineIndex{};
    for (const auto& line : layout.lines)
    {
        CHECK(line.x >= 0);
        CHECK(line.x + line.width <= viewportWidth);
        CHECK(line.y >= previousBottom);
        CHECK(line.y + line.height <= viewportHeight);
        previousBottom = line.y + line.height;
        if (line.content.kind == ChessMagicEffectDisplayLineKind::Effect)
        {
            CHECK(line.content.breakBefore
                == (effectPhysicalLineIndex == 0
                    ? EffectDescriptionSemanticBreak::Block
                    : EffectDescriptionSemanticBreak::None));
            ++effectPhysicalLineIndex;
            wrappedEffect += line.content.text;
        }
    }
    CHECK(previousBottom == layout.requiredHeight);
    CHECK(wrappedEffect == rows.back().text);
}

TEST_CASE("ChessMagicEffectDisplay_NormalPoolFitsTheNarrowSingleColumnViewport",
          "[chess][effects][magic][layout][content]")
{
    // 244×133 是一般商店面板扣除棋池最寬頭像後的內容區；
    // 196×133 則涵蓋二星升三星欄位使同版型面板進一步變窄的情況。
    constexpr std::array viewports{
        std::pair{244, 133},
        std::pair{196, 133},
    };
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    for (const auto [viewportWidth, viewportHeight] : viewports)
    {
        int minimumEffectFontSize = 100;
        for (const int roleId : content->poolRoleIds())
        {
            const auto* role = content->role(roleId);
            REQUIRE(role);
            for (int star = 1; star <= 3; ++star)
            {
                CAPTURE(viewportWidth, viewportHeight, roleId, role->Name, star);
                const auto selected = chessRoleMagicsForStar(*content, *role, star);
                REQUIRE(selected.size() <= 2);
                std::vector<const MagicSave*> magics;
                for (const auto& [magic, power] : selected)
                {
                    magics.push_back(magic);
                }
                const auto rows = buildChessMagicEffectDisplayRows(
                    magics,
                    content->magicEffects(),
                    selected.empty() ? -1 : selected.back().first->ID);
                const auto layout = layoutChessMagicEffectDisplay(
                    rows,
                    viewportWidth,
                    viewportHeight);
                minimumEffectFontSize = std::min(
                    minimumEffectFontSize,
                    layout.effectFontSize);
                CHECK(layout.effectFontSize >= 14);
                CHECK_FALSE(layout.scrollable);
                CHECK(layout.scrollable == (layout.requiredHeight > viewportHeight));
                int previousBottom = 0;
                for (const auto& line : layout.lines)
                {
                    CHECK(line.x >= 0);
                    CHECK(line.x + line.width <= viewportWidth);
                    CHECK(line.y >= previousBottom);
                    previousBottom = line.y + line.height;
                    if (line.content.kind == ChessMagicEffectDisplayLineKind::Skill)
                    {
                        CHECK(line.x + line.width <= layout.skillValueX);
                        const int valueWidth = displayTextWidth("9999 遠程")
                            * line.fontSize / 2;
                        CHECK(layout.skillValueX + valueWidth <= viewportWidth);
                    }
                }
                CHECK(previousBottom == layout.requiredHeight);
                if (layout.scrollable)
                {
                    REQUIRE_FALSE(layout.scrollStops.empty());
                    CHECK(layout.scrollStops.front() == 0);
                    CHECK(layout.scrollStops.back() == layout.maximumScrollOffset);
                    CHECK(clampChessMagicEffectDisplayScrollOffset(layout, -1) == 0);
                    CHECK(clampChessMagicEffectDisplayScrollOffset(
                        layout,
                        layout.maximumScrollOffset + 1) == layout.maximumScrollOffset);
                    std::vector<bool> reached(layout.lines.size());
                    for (const int offset : layout.scrollStops)
                    {
                        const auto visible = visibleChessMagicEffectDisplayLines(
                            layout,
                            offset);
                        for (const auto& line : visible)
                        {
                            CHECK(line.x >= 0);
                            CHECK(line.x + line.width <= viewportWidth);
                            CHECK(line.y >= 0);
                            CHECK(line.y + line.height
                                <= viewportHeight - layout.scrollIndicatorHeight);
                            for (std::size_t index = 0;
                                 index < layout.lines.size();
                                 ++index)
                            {
                                const auto& source = layout.lines[index];
                                if (source.y == line.y + offset
                                    && source.content.text == line.content.text
                                    && source.content.kind == line.content.kind)
                                {
                                    reached[index] = true;
                                }
                            }
                        }
                    }
                    CHECK(std::ranges::all_of(reached, std::identity{}));
                    int offset{};
                    while (offset < layout.maximumScrollOffset)
                    {
                        const int next = stepChessMagicEffectDisplayScrollOffset(
                            layout,
                            offset,
                            1);
                        REQUIRE(next > offset);
                        offset = next;
                    }
                    CHECK(offset == layout.maximumScrollOffset);
                    while (offset > 0)
                    {
                        const int previous = stepChessMagicEffectDisplayScrollOffset(
                            layout,
                            offset,
                            -1);
                        REQUIRE(previous < offset);
                        offset = previous;
                    }
                    CHECK(offset == 0);
                }
                else
                {
                    CHECK(layout.maximumScrollOffset == 0);
                    const auto visible = visibleChessMagicEffectDisplayLines(layout, 0);
                    REQUIRE(visible.size() == layout.lines.size());
                    for (std::size_t index = 0; index < visible.size(); ++index)
                    {
                        CHECK(visible[index].content.text == layout.lines[index].content.text);
                        CHECK(visible[index].y == layout.lines[index].y);
                    }
                    CHECK(std::ranges::all_of(visible, [viewportHeight](const auto& line)
                    {
                        return line.y >= 0 && line.y + line.height <= viewportHeight;
                    }));
                }
            }
        }
        CHECK(minimumEffectFontSize >= 14);
    }
}

TEST_CASE("Role detail keeps proficiencies and equipment within separate bounds", "[chess][ui][layout]")
{
    for (const int width : {560, 622, 725})
    for (const int height : {337, 365, 610})
    for (const int portraitWidth : {96, 128, 170})
    for (const int equipmentWidth : {200, 202, 242})
    {
        const PanelFrame panel{400, 55, width, height};
        const auto layout = SessionStatusLayout::build(nullptr, panel, portraitWidth, equipmentWidth);
        const int speedBottom = layout.topY + 4 * layout.lineHeight + layout.fontSize;
        CHECK(layout.skillTopY >= speedBottom + 14);
        CHECK(layout.skillTopY >= layout.avatar.y + layout.avatar.h + 14);
        CHECK(layout.skillTopY + layout.lineHeight + layout.fontSize < layout.sectionTitleY);
        CHECK(layout.combo.x + layout.combo.w + layout.gap == layout.equip.x);
        CHECK(layout.equip.x + layout.equip.w == panel.x + panel.w - layout.pad);
        // Two equipment rows: 28px stride and a 28px icon beginning 3px above the baseline.
        CHECK(layout.sectionContentY + 28 - 3 + 28 <= panel.y + panel.h - layout.pad);
        CHECK(layout.magic.x + layout.magic.w == panel.x + panel.w - layout.pad);
    }
}

TEST_CASE("Equipped character values and longest equipment names fit the role card", "[chess][ui][layout][content]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);
    int equipmentWidth = 200;
    std::array<int, 8> widestValues{};
    for (const auto& equipment : content->equipment())
    {
        const auto* item = content->item(equipment.itemId);
        REQUIRE(item);
        equipmentWidth = std::max(equipmentWidth, 82 + displayTextWidth(item->name) * 10);
    }
    for (const int roleId : content->poolRoleIds())
    for (int star = 1; star <= 3; ++star)
    for (const auto& weapon : content->equipment())
    {
        if (weapon.equipType != 0) continue;
        for (const auto& armor : content->equipment())
        {
            if (armor.equipType != 1) continue;
            auto stats = chessRoleStats(*content->role(roleId), content->balance(), star, 0);
            applyChessItemBaseStats(stats, content->item(weapon.itemId));
            applyChessItemBaseStats(stats, content->item(armor.itemId));
            const std::array values{stats.maxHp, stats.attack, stats.defence, stats.speed,
                stats.fist, stats.sword, stats.knife, stats.unusual};
            for (std::size_t i = 0; i < values.size(); ++i)
                widestValues[i] = std::max(widestValues[i], displayTextWidth(std::to_string(values[i])) * 11);
        }
    }
    const auto layout = SessionStatusLayout::build(nullptr, {0, 0, 560, 337}, 128, equipmentWidth);
    INFO("equipment column width: " << equipmentWidth);
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(layout.statsColumn.valueX + widestValues[i] + 14 <= layout.magic.x);
    for (std::size_t i = 4; i < widestValues.size(); ++i)
    {
        CHECK(layout.skillCol1.valueX + widestValues[i] + 14 <= layout.skillCol2.labelX);
        CHECK(layout.skillCol2.valueX + widestValues[i] + 14 <= layout.magic.x);
    }
    CHECK(layout.equip.x + equipmentWidth <= 560 - layout.pad);
}
