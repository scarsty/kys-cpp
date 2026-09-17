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
    CHECK(effectText.find("全隊回30內") != std::string::npos);
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