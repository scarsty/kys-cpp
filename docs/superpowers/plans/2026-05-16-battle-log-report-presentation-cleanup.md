# Battle Log Report Presentation Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current battle log/tracker/UI chain with one canonical battle report archive, one presenter, and one ImGui battle-log view, with no compatibility aliases or old duplicate names left behind.

**Architecture:** Core battle runtime still emits ordered `KysChess::Battle::BattleLogEvent` facts. Scene code feeds those facts into `BattleReportBuilder`; `BattleLogPresenter` converts a completed `BattleReport` plus `BattlePostBattleSummary` into a `BattleLogViewModel`; `BattleLogImGuiView` renders that view model. `BattleStatsView` only coordinates post-battle summary flow and no longer formats battle log text, while `ImGuiLayer` only owns overlay orchestration and delegates battle-log rendering.

**Tech Stack:** C++20, Catch2, MSBuild via `.github/build-command.ps1`, existing SDL/ImGui overlay stack.

---

## Non-Negotiable Cleanup Rules

- Delete `src/BattleTracker.cpp`; do not leave a forwarding file.
- Remove `BattleTracker`, global `BattleLogEvent`, and global `BattleLogEventType` from `src/BattleStatsView.h`.
- Delete `src/BattleSceneTrackerPlayer.h` and `src/BattleSceneTrackerPlayer.cpp`; replace them with `BattleSceneReportPlayer`.
- Remove `BattleLogData`, `BattleLogLine`, `BattleLogRoleRow`, `BattleLogTone`, `BattleLogFieldTone`, and `BattleLogSegment` from `src/ImGuiLayer.h`; define final view-model types in `src/BattleLogViewModel.h`.
- Do not add type aliases such as `using BattleTracker = BattleReportBuilder`.
- Do not keep old method names and new method names side by side. Pick final names and update every call site.
- Keep `KysChess::Battle::BattleLogEvent` in `src/battle/BattlePresentation.h`; that is the core runtime event type and remains canonical for frame output.
- Use Traditional Chinese for new visible UI/log strings.

## Final Data Flow

```text
Battle runtime frame
  -> KysChess::Battle::BattlePresentationFrame::logEvents
  -> BattleSceneReportPlayer
  -> BattleReportBuilder
  -> BattleReport
  -> BattleLogPresenter
  -> BattleLogViewModel
  -> Engine::showBattleLogOverlay()
  -> ImGuiLayer
  -> BattleLogImGuiView
```

## File Structure

- Create `src/BattleReport.h`
  - Defines `BattleReportEventType`, `BattleReportEvent`, `BattleReportUnitStats`, `BattleReport`, and `BattleReportBuilder`.
- Create `src/BattleReport.cpp`
  - Owns stats aggregation and ordered report event storage.
  - Battle end does not suppress later already-ordered events; it only records end frame/result.
- Delete `src/BattleTracker.cpp`
  - Replaced by `BattleReport.cpp`.
- Create `src/BattleSceneReportPlayer.h`
  - Scene adapter from runtime log/projectile-cancel commands to `BattleReportBuilder`.
- Create `src/BattleSceneReportPlayer.cpp`
  - Moves current `BattleSceneTrackerPlayer` behavior to report terminology and final types.
- Delete `src/BattleSceneTrackerPlayer.h`
- Delete `src/BattleSceneTrackerPlayer.cpp`
- Create `src/BattleLogViewModel.h`
  - Defines the final UI-facing battle log data structures.
- Create `src/BattleLogPresenter.h`
  - Declares pure presenter APIs for report-to-view-model conversion and filtering.
- Create `src/BattleLogPresenter.cpp`
  - Moves log formatting and duplicate-name label logic out of `BattleStatsView.cpp`.
- Create `src/BattleLogImGuiView.h`
  - Defines `BattleLogWindowState` and `BattleLogImGuiView`.
- Create `src/BattleLogImGuiView.cpp`
  - Moves `ImGuiLayer::renderBattleLogWindow()` logic out of `ImGuiLayer.cpp`.
- Modify `src/BattleStatsView.h`
  - Include `BattleReport.h`; keep `BattleStatsView::RoleEntry` and `ComboEntry`; remove tracker/log model declarations.
- Modify `src/BattleStatsView.cpp`
  - Remove log formatting helpers; call `BattleLogPresenter`.
- Modify `src/BattleSceneHades.h/.cpp`
  - Replace `BattleTracker tracker_` with `BattleReportBuilder battle_report_`.
  - Replace `BattleSceneTrackerPlayer tracker_player_` with `BattleSceneReportPlayer report_player_`.
- Modify `src/BattleSceneUnitStore.h/.cpp`
  - `makePostBattleSummary` takes `const BattleReport&`.
- Modify `src/Engine.h/.cpp`
  - Replace `showBattleLogWindow`, `hideBattleLogWindow`, `isBattleLogWindowOpen` with `showBattleLogOverlay`, `hideBattleLogOverlay`, `isBattleLogOverlayOpen`.
- Modify `src/ImGuiLayer.h/.cpp`
  - Hold `BattleLogWindowState battle_log_` and delegate rendering to `BattleLogImGuiView`.
- Modify project files:
  - `src/kys.vcxproj`
  - `tests/kys_tests.vcxproj`
- Delete old tests:
  - `tests/BattleSceneTrackerPlayerUnitTests.cpp`
- Create/update tests:
  - `tests/BattleReportUnitTests.cpp`
  - `tests/BattleSceneReportPlayerUnitTests.cpp`
  - `tests/BattleLogPresenterUnitTests.cpp`
  - `tests/BattleLogImGuiViewUnitTests.cpp`
  - `tests/BattlePresentationUnitTests.cpp`

---

### Task 1: Replace BattleTracker With BattleReport

**Files:**
- Create: `src/BattleReport.h`
- Create: `src/BattleReport.cpp`
- Delete: `src/BattleTracker.cpp`
- Modify: `src/BattleStatsView.h`
- Modify: `tests/BattlePresentationUnitTests.cpp`
- Create: `tests/BattleReportUnitTests.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Write report model tests**

Create `tests/BattleReportUnitTests.cpp`:

```cpp
#include "BattleReport.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleReportBuilder_RecordsDamageStatsAndOrderedEvents", "[battle][report]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };

    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 152, "六脈神劍", 221, "連擊增傷 +42%（3層）");
    builder.recordBattleEnd(221, 0);

    const auto& report = builder.report();
    REQUIRE(report.stats().contains(101));
    CHECK(report.stats().at(101).damageDealt == 152);
    CHECK(report.stats().at(101).damagePerSkill.at("六脈神劍") == 152);
    REQUIRE(report.stats().contains(202));
    CHECK(report.stats().at(202).damageTaken == 152);
    CHECK(report.battleEndFrame() == 221);
    CHECK(report.battleResult() == 0);

    const auto& events = report.events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::Damage);
    CHECK(events[0].sourceName == "段譽");
    CHECK(events[0].targetName == "岳不群");
    CHECK(events[0].value == 152);
    CHECK(events[0].detailText == "連擊增傷 +42%（3層）");
    CHECK(events[1].type == BattleReportEventType::BattleEnd);
}

TEST_CASE("BattleReportBuilder_BattleEndDoesNotSuppressAlreadyOrderedLaterEvents", "[battle][report]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };

    BattleReportBuilder builder;
    builder.recordBattleEnd(221, 0);
    builder.recordDamage(&attacker, &defender, 1, "收尾", 221);

    const auto& events = builder.report().events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::BattleEnd);
    CHECK(events[1].type == BattleReportEventType::Damage);
}

TEST_CASE("BattleReportBuilder_RecordsLifecycleAndProjectileCancelStats", "[battle][report]")
{
    BattleUnitIdentity killer{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity victim{ 202, 2, 1, 12, "岳不群" };

    BattleReportBuilder builder;
    builder.recordKill(&killer, &victim, 20);
    builder.recordDeath(&victim, 20);
    builder.recordProjectileCancel(101, 77);
    builder.recordProjectileCancel(101, 23);

    const auto& report = builder.report();
    REQUIRE(report.stats().contains(101));
    CHECK(report.stats().at(101).kills == 1);
    CHECK(report.cancelDamageForUnit(101) == 100);
    CHECK(report.cancelDamageForUnit(202) == 0);

    const auto& events = report.events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleReportEventType::Kill);
    CHECK(events[1].type == BattleReportEventType::Death);
    CHECK(events[1].targetName == "岳不群");
}
```

- [ ] **Step 2: Add report types**

Create `src/BattleReport.h`:

```cpp
#pragma once

#include "BattlePostBattleSummary.h"

#include <map>
#include <string>
#include <vector>

struct BattleReportUnitStats
{
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    std::map<std::string, int> damagePerSkill;
    int firstDamageFrame = 0;
    int lastActiveFrame = 0;
};

enum class BattleReportEventType
{
    Damage,
    Heal,
    Status,
    Kill,
    Death,
    BattleEnd
};

struct BattleReportEvent
{
    BattleReportEventType type = BattleReportEventType::Damage;
    int frame = 0;
    int sourceId = -1;
    int targetId = -1;
    int sourceTeam = -1;
    int targetTeam = -1;
    int value = 0;
    std::string sourceName;
    std::string targetName;
    std::string skillName;
    std::string detailText;
};

class BattleReport
{
public:
    const std::map<int, BattleReportUnitStats>& stats() const { return stats_; }
    const std::vector<BattleReportEvent>& events() const { return events_; }
    int battleEndFrame() const { return battleEndFrame_; }
    int battleResult() const { return battleResult_; }
    int cancelDamageForUnit(int unitId) const;

private:
    friend class BattleReportBuilder;

    std::map<int, BattleReportUnitStats> stats_;
    std::map<int, int> cancelDamageByUnit_;
    std::vector<BattleReportEvent> events_;
    int battleEndFrame_ = 0;
    int battleResult_ = -1;
};

class BattleReportBuilder
{
public:
    void recordDamage(const BattleUnitIdentity* attacker, const BattleUnitIdentity* defender, int damage, const std::string& skillName, int frame, const std::string& detailText = "");
    void recordHeal(const BattleUnitIdentity* source, const BattleUnitIdentity* target, int amount, const std::string& reason, int frame);
    void recordStatus(const BattleUnitIdentity* source, const BattleUnitIdentity* target, const std::string& text, int frame);
    void recordKill(const BattleUnitIdentity* killer, const BattleUnitIdentity* victim, int frame);
    void recordDeath(const BattleUnitIdentity* unit, int frame);
    void recordProjectileCancel(int unitId, int damage);
    void recordBattleEnd(int frame, int battleResult);

    const BattleReport& report() const { return report_; }

private:
    BattleReport report_;
};
```

- [ ] **Step 3: Implement report builder**

Create `src/BattleReport.cpp` by moving the logic from `src/BattleTracker.cpp`, with these deliberate changes:

```cpp
#include "BattleReport.h"

#include <cassert>
#include <utility>

int BattleReport::cancelDamageForUnit(int unitId) const
{
    const auto it = cancelDamageByUnit_.find(unitId);
    return it == cancelDamageByUnit_.end() ? 0 : it->second;
}

void BattleReportBuilder::recordDamage(
    const BattleUnitIdentity* attacker,
    const BattleUnitIdentity* defender,
    int damage,
    const std::string& skillName,
    int frame,
    const std::string& detailText)
{
    if (attacker)
    {
        auto& stats = report_.stats_[attacker->battleId];
        stats.damageDealt += damage;
        if (stats.firstDamageFrame == 0)
        {
            stats.firstDamageFrame = frame;
        }
        stats.lastActiveFrame = frame;
        if (!skillName.empty())
        {
            stats.damagePerSkill[skillName] += damage;
        }
    }
    if (defender)
    {
        report_.stats_[defender->battleId].damageTaken += damage;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Damage;
    event.frame = frame;
    event.value = damage;
    if (attacker)
    {
        event.sourceId = attacker->battleId;
        event.sourceTeam = attacker->team;
        event.sourceName = attacker->name;
    }
    if (defender)
    {
        event.targetId = defender->battleId;
        event.targetTeam = defender->team;
        event.targetName = defender->name;
    }
    event.skillName = skillName;
    event.detailText = detailText;
    report_.events_.push_back(std::move(event));
}
```

Then port `recordHeal`, `recordStatus`, `recordKill`, `recordDeath`, `recordProjectileCancel`, and `recordBattleEnd` from `BattleTracker.cpp` using `BattleReportEvent` and `report_`. Do not keep any `if (battleResult_ != -1) return;` gates.

- [ ] **Step 4: Remove tracker declarations from `BattleStatsView.h`**

Replace:

```cpp
struct RoleBattleStats;
enum class BattleLogEventType;
struct BattleLogEvent;
class BattleTracker;
```

with:

```cpp
#include "BattleReport.h"
```

Change:

```cpp
void setupPostBattle(const BattlePostBattleSummary& summary, const BattleTracker& tracker);
std::vector<BattleLogEvent> battleLogEvents_;
```

to:

```cpp
void setupPostBattle(const BattlePostBattleSummary& summary, const BattleReport& report);
const BattleReport* battleReport_ = nullptr;
```

- [ ] **Step 5: Update project files**

In `src/kys.vcxproj`, replace:

```xml
<ClCompile Include="BattleTracker.cpp" />
```

with:

```xml
<ClCompile Include="BattleReport.cpp" />
```

In `tests/kys_tests.vcxproj`, replace:

```xml
<ClCompile Include="..\src\BattleTracker.cpp" />
```

with:

```xml
<ClCompile Include="..\src\BattleReport.cpp" />
```

Add:

```xml
<ClCompile Include="BattleReportUnitTests.cpp" />
```

- [ ] **Step 6: Delete old tracker tests from `BattlePresentationUnitTests.cpp`**

Remove the two test cases:

```cpp
TEST_CASE("BattleTracker_RecordsDamageFromRoleFreeIdentities", "[battle][presentation][unit]")
TEST_CASE("BattleTracker_RecordsLifecycleAndProjectileCancelFromIds", "[battle][presentation][unit]")
```

Those expectations now live in `tests/BattleReportUnitTests.cpp`.

- [ ] **Step 7: Run report tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][report]"
```

Expected:
- build succeeds;
- report tests pass.

---

### Task 2: Replace BattleSceneTrackerPlayer With BattleSceneReportPlayer

**Files:**
- Create: `src/BattleSceneReportPlayer.h`
- Create: `src/BattleSceneReportPlayer.cpp`
- Delete: `src/BattleSceneTrackerPlayer.h`
- Delete: `src/BattleSceneTrackerPlayer.cpp`
- Delete: `tests/BattleSceneTrackerPlayerUnitTests.cpp`
- Create: `tests/BattleSceneReportPlayerUnitTests.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Write report player tests**

Create `tests/BattleSceneReportPlayerUnitTests.cpp`:

```cpp
#include "BattleSceneReportPlayer.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{
BattleSceneUnit reportPlayerUnit(int unitId, int battleId, int team, std::string name)
{
    BattleSceneUnit unit;
    unit.unitId = unitId;
    unit.identity = { battleId, battleId, team, 0, std::move(name) };
    return unit;
}
}  // namespace

TEST_CASE("BattleSceneReportPlayer_RecordsDamageBeforeDeathAndBattleEnd", "[battle][scene_report]")
{
    BattleSceneUnitStore units;
    units.initialize({
        reportPlayerUnit(0, 101, 0, "段譽"),
        reportPlayerUnit(1, 202, 1, "岳不群"),
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleLogEvent> logs = {
        { KysChess::Battle::BattleLogEventType::Damage, 221, 0, 1, 152, "", "六脈神劍" },
        { KysChess::Battle::BattleLogEventType::UnitDied, 221, 0, 1 },
        { KysChess::Battle::BattleLogEventType::BattleEnded, 221, -1, -1, 0 },
    };

    player.playLogs(logs, { &builder, &units });

    const auto& events = builder.report().events();
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == BattleReportEventType::Damage);
    CHECK(events[0].sourceName == "段譽");
    CHECK(events[0].targetName == "岳不群");
    CHECK(events[0].value == 152);
    CHECK(events[1].type == BattleReportEventType::Kill);
    CHECK(events[2].type == BattleReportEventType::Death);
    CHECK(events[3].type == BattleReportEventType::BattleEnd);
}

TEST_CASE("BattleSceneReportPlayer_RecordsProjectileCancelStats", "[battle][scene_report]")
{
    BattleSceneUnitStore units;
    units.initialize({
        reportPlayerUnit(0, 101, 0, "段譽"),
        reportPlayerUnit(1, 202, 1, "岳不群"),
    });
    BattleReportBuilder builder;
    BattleSceneReportPlayer player;

    std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand> commands = {
        { 34, 29, 0, 1, 35, 20 },
    };

    player.playProjectileCancelDamageCommands(commands, { &builder, &units });

    CHECK(builder.report().cancelDamageForUnit(0) == 35);
    CHECK(builder.report().cancelDamageForUnit(1) == 20);
}
```

- [ ] **Step 2: Add report player header**

Create `src/BattleSceneReportPlayer.h`:

```cpp
#pragma once

#include "BattleReport.h"
#include "BattleSceneUnitStore.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattlePresentation.h"

#include <vector>

class BattleSceneReportPlayer
{
public:
    struct Bindings
    {
        BattleReportBuilder* report = nullptr;
        const BattleSceneUnitStore* units = nullptr;
    };

    void playLogs(
        const std::vector<KysChess::Battle::BattleLogEvent>& logEvents,
        const Bindings& bindings) const;

    void playProjectileCancelDamageCommands(
        const std::vector<KysChess::Battle::BattleProjectileCancelDamageCommand>& commands,
        const Bindings& bindings) const;

private:
    void recordLog(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordDamage(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordHeal(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordStatus(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordUnitDied(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
    void recordBattleEnded(const KysChess::Battle::BattleLogEvent& event, const Bindings& bindings) const;
};
```

- [ ] **Step 3: Add report player implementation**

Create `src/BattleSceneReportPlayer.cpp` by moving `BattleSceneTrackerPlayer.cpp` and changing:

```cpp
BattleSceneTrackerPlayer -> BattleSceneReportPlayer
bindings.tracker -> bindings.report
recordProjectileCancel -> BattleReportBuilder::recordProjectileCancel
recordDamage/recordHeal/recordStatus/recordKill/recordDeath/recordBattleEnd -> BattleReportBuilder methods
```

Keep `resolveIdentity` as a local helper using `bindings.units->requireUnit(unitId).identity`.

- [ ] **Step 4: Update Hades ownership**

In `src/BattleSceneHades.h`, replace:

```cpp
#include "BattleStatsView.h"
#include "BattleSceneTrackerPlayer.h"
BattleTracker tracker_;
BattleSceneTrackerPlayer tracker_player_;
BattleTracker& getTracker() { return tracker_; }
```

with:

```cpp
#include "BattleReport.h"
#include "BattleSceneReportPlayer.h"
BattleReportBuilder battle_report_;
BattleSceneReportPlayer report_player_;
const BattleReport& getBattleReport() const { return battle_report_.report(); }
```

In `src/BattleSceneHades.cpp`, replace:

```cpp
tracker_player_.playLogs(... { &tracker_, &scene_units_ });
tracker_player_.playProjectileCancelDamageCommands(... { &tracker_, &scene_units_ });
scene_units_.makePostBattleSummary(tracker_, result_);
```

with:

```cpp
report_player_.playLogs(... { &battle_report_, &scene_units_ });
report_player_.playProjectileCancelDamageCommands(... { &battle_report_, &scene_units_ });
scene_units_.makePostBattleSummary(battle_report_.report(), result_);
```

- [ ] **Step 5: Update project files and delete old files**

In `src/kys.vcxproj`, replace:

```xml
<ClCompile Include="BattleSceneTrackerPlayer.cpp" />
```

with:

```xml
<ClCompile Include="BattleSceneReportPlayer.cpp" />
```

In `tests/kys_tests.vcxproj`, replace:

```xml
<ClCompile Include="BattleSceneTrackerPlayerUnitTests.cpp" />
<ClCompile Include="..\src\BattleSceneTrackerPlayer.cpp" />
```

with:

```xml
<ClCompile Include="BattleSceneReportPlayerUnitTests.cpp" />
<ClCompile Include="..\src\BattleSceneReportPlayer.cpp" />
```

Delete:

```text
src/BattleSceneTrackerPlayer.h
src/BattleSceneTrackerPlayer.cpp
tests/BattleSceneTrackerPlayerUnitTests.cpp
```

- [ ] **Step 6: Run report player tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_report]"
```

Expected:
- build succeeds;
- report player tests pass.

---

### Task 3: Introduce BattleLogViewModel And BattleLogPresenter

**Files:**
- Create: `src/BattleLogViewModel.h`
- Create: `src/BattleLogPresenter.h`
- Create: `src/BattleLogPresenter.cpp`
- Create: `tests/BattleLogPresenterUnitTests.cpp`
- Modify: `src/BattleStatsView.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add presenter tests**

Create `tests/BattleLogPresenterUnitTests.cpp`:

```cpp
#include "BattleLogPresenter.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
BattlePostBattleUnitSummary summaryUnit(int battleId, int team, std::string name)
{
    BattlePostBattleUnitSummary unit;
    unit.identity = { battleId, battleId, team, 0, std::move(name) };
    unit.maxHpRemaining = 100;
    unit.hpRemaining = 80;
    return unit;
}
}

TEST_CASE("BattleLogPresenter_FormatsDamageDeathAndBattleEndInOrder", "[battle][log_presenter]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 152, "六脈神劍", 221, "連擊增傷 +42%（3層）");
    builder.recordKill(&attacker, &defender, 221);
    builder.recordDeath(&defender, 221);
    builder.recordBattleEnd(221, 0);

    BattlePostBattleSummary summary;
    summary.battleResult = 0;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    REQUIRE(model.entries.size() == 4);
    CHECK(model.entries[0].segments[0].text == "[ 221F] ");
    CHECK(model.entries[0].plainText() == "[ 221F] 段譽 施放 六脈神劍 命中 岳不群，造成 152 點傷害（連擊增傷 +42%（3層））");
    CHECK(model.entries[1].plainText() == "[ 221F] 段譽 擊殺了 岳不群");
    CHECK(model.entries[2].plainText() == "[ 221F] 岳不群 倒下");
    CHECK(model.entries[3].plainText() == "[ 221F] 戰鬥勝利");
}

TEST_CASE("BattleLogPresenter_DisambiguatesDuplicateNames", "[battle][log_presenter]")
{
    BattleUnitIdentity first{ 201, 2, 1, 12, "弟子" };
    BattleUnitIdentity second{ 202, 3, 1, 13, "弟子" };
    BattleReportBuilder builder;
    builder.recordStatus(&first, &second, "測試狀態", 10);

    BattlePostBattleSummary summary;
    summary.battleResult = 0;
    summary.enemies.push_back(summaryUnit(201, 1, "弟子"));
    summary.enemies.push_back(summaryUnit(202, 1, "弟子"));

    auto model = BattleLogPresenter().present(summary, builder.report());

    REQUIRE(model.entries.size() == 1);
    CHECK(model.entries[0].plainText() == "[  10F] 弟子 [1] 對 弟子 [2]：測試狀態");
}

TEST_CASE("BattleLogPresenter_FiltersByParticipantAndCategory", "[battle][log_presenter]")
{
    BattleUnitIdentity attacker{ 101, 1, 0, 11, "段譽" };
    BattleUnitIdentity defender{ 202, 2, 1, 12, "岳不群" };
    BattleReportBuilder builder;
    builder.recordDamage(&attacker, &defender, 20, "六脈神劍", 12);
    builder.recordStatus(&attacker, &defender, "眩暈（12幀）", 13);

    BattlePostBattleSummary summary;
    summary.allies.push_back(summaryUnit(101, 0, "段譽"));
    summary.enemies.push_back(summaryUnit(202, 1, "岳不群"));
    auto model = BattleLogPresenter().present(summary, builder.report());

    BattleLogFilter filter;
    filter.allyUnitId = 101;
    filter.category = BattleLogEntryCategory::Damage;

    REQUIRE(countVisibleBattleLogEntries(model, filter) == 1);
    CHECK(battleLogEntryMatchesFilter(model.entries[0], filter));
    CHECK_FALSE(battleLogEntryMatchesFilter(model.entries[1], filter));
}
```

- [ ] **Step 2: Add view model types**

Create `src/BattleLogViewModel.h`:

```cpp
#pragma once

#include <string>
#include <vector>

enum class BattleLogEntryTone
{
    Neutral,
    Ally,
    Enemy,
    System
};

enum class BattleLogFieldTone
{
    Default,
    AllyName,
    EnemyName,
    SkillName,
    DamageValue,
    SystemAccent
};

enum class BattleLogEntryCategory
{
    Any,
    Damage,
    Heal,
    Status,
    Kill,
    Death,
    BattleEnd
};

struct BattleLogSegment
{
    std::string text;
    BattleLogFieldTone tone = BattleLogFieldTone::Default;
};

struct BattleLogEntryView
{
    BattleLogEntryTone tone = BattleLogEntryTone::Neutral;
    BattleLogEntryCategory category = BattleLogEntryCategory::Status;
    int sourceId = -1;
    int targetId = -1;
    int sourceTeam = -1;
    int targetTeam = -1;
    std::vector<BattleLogSegment> segments;

    std::string plainText() const;
};

struct BattleLogRoleFilterRow
{
    int id = -1;
    std::string name;
    int team = 0;
    int damageDealt = 0;
    int damageTaken = 0;
    int kills = 0;
    int cancelDmg = 0;
    int hpRemaining = 0;
    int maxHp = 0;
    bool dead = false;
};

struct BattleLogViewModel
{
    bool open = false;
    std::string title;
    std::string resultText;
    int totalFrames = 0;
    int omittedEntries = 0;
    std::vector<BattleLogRoleFilterRow> allies;
    std::vector<BattleLogRoleFilterRow> enemies;
    std::vector<BattleLogEntryView> entries;
};

struct BattleLogFilter
{
    int allyUnitId = -1;
    int enemyUnitId = -1;
    BattleLogEntryCategory category = BattleLogEntryCategory::Any;
};
```

- [ ] **Step 3: Add presenter header**

Create `src/BattleLogPresenter.h`:

```cpp
#pragma once

#include "BattleLogViewModel.h"
#include "BattlePostBattleSummary.h"
#include "BattleReport.h"

class BattleLogPresenter
{
public:
    BattleLogViewModel present(const BattlePostBattleSummary& summary, const BattleReport& report) const;
};

bool battleLogEntryMatchesFilter(const BattleLogEntryView& entry, const BattleLogFilter& filter);
int countVisibleBattleLogEntries(const BattleLogViewModel& model, const BattleLogFilter& filter);
```

- [ ] **Step 4: Implement presenter**

Create `src/BattleLogPresenter.cpp` by moving these functions from `BattleStatsView.cpp`:

```cpp
teamToBattleLogTone
teamToFieldTone
appendBattleLogSegment
battleResultText
buildDistinctRoleLabels
setLineParticipants
buildBattleLogLine
```

Rename types while moving:

```text
BattleLogTone -> BattleLogEntryTone
BattleLogLine -> BattleLogEntryView
BattleLogRoleRow -> BattleLogRoleFilterRow
BattleLogEvent -> BattleReportEvent
BattleLogEventType -> BattleReportEventType
```

Add the plain-text helper:

```cpp
std::string BattleLogEntryView::plainText() const
{
    std::string text;
    for (const auto& segment : segments)
    {
        text += segment.text;
    }
    return text;
}
```

In `BattleLogPresenter::present`, port `BattleStatsView::showPostBattleLog` data construction but return `BattleLogViewModel` instead of calling `Engine`.

Use these final strings:

```cpp
data.title = "本場戰鬥日誌";
emptyLine: "本場戰鬥沒有產生可記錄事件。"
omittedLine: std::format("前 {} 條記錄已省略", omitted)
```

Use this filter implementation:

```cpp
bool battleLogEntryMatchesFilter(const BattleLogEntryView& entry, const BattleLogFilter& filter)
{
    if (filter.category != BattleLogEntryCategory::Any && entry.category != filter.category)
    {
        return false;
    }
    if (entry.sourceId < 0 && entry.targetId < 0)
    {
        return true;
    }

    auto matchesTeamFilter = [&](int filterId, int team)
    {
        if (filterId < 0)
        {
            return true;
        }
        return (entry.sourceTeam == team && entry.sourceId == filterId)
            || (entry.targetTeam == team && entry.targetId == filterId);
    };

    return matchesTeamFilter(filter.allyUnitId, 0)
        && matchesTeamFilter(filter.enemyUnitId, 1);
}

int countVisibleBattleLogEntries(const BattleLogViewModel& model, const BattleLogFilter& filter)
{
    int count = 0;
    for (const auto& entry : model.entries)
    {
        if (battleLogEntryMatchesFilter(entry, filter))
        {
            ++count;
        }
    }
    return count;
}
```

- [ ] **Step 5: Update project files**

In `src/kys.vcxproj`, add:

```xml
<ClCompile Include="BattleLogPresenter.cpp" />
```

In `tests/kys_tests.vcxproj`, add:

```xml
<ClCompile Include="BattleLogPresenterUnitTests.cpp" />
<ClCompile Include="..\src\BattleLogPresenter.cpp" />
```

- [ ] **Step 6: Run presenter tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][log_presenter]"
```

Expected:
- build succeeds;
- presenter tests pass.

---

### Task 4: Slim BattleStatsView To Summary Coordination

**Files:**
- Modify: `src/BattleStatsView.h`
- Modify: `src/BattleStatsView.cpp`
- Modify: `src/BattleSceneUnitStore.h`
- Modify: `src/BattleSceneUnitStore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/ChessBattleFlow.cpp`

- [ ] **Step 1: Remove battle-log formatting from `BattleStatsView.cpp`**

Delete the anonymous-namespace helpers moved to `BattleLogPresenter.cpp`:

```text
kMaxBattleLogEntries
teamToBattleLogTone
teamToFieldTone
appendBattleLogSegment
battleResultText
buildDistinctRoleLabels
setLineParticipants
buildBattleLogLine
```

Keep post-battle table drawing and pre-battle asset preload in `BattleStatsView`.

- [ ] **Step 2: Store report pointer instead of copied events**

In `BattleStatsView::setupPostBattle`, replace:

```cpp
void BattleStatsView::setupPostBattle(const BattlePostBattleSummary& summary, const BattleTracker& tracker)
```

with:

```cpp
void BattleStatsView::setupPostBattle(const BattlePostBattleSummary& summary, const BattleReport& report)
```

Replace:

```cpp
battleLogEvents_ = tracker.getEvents();
auto& stats = tracker.getStats();
int endFrame = tracker.getBattleEndFrame();
```

with:

```cpp
battleReport_ = &report;
const auto& stats = report.stats();
int endFrame = report.battleEndFrame();
```

In `setupPreBattle`, clear the pointer:

```cpp
battleReport_ = nullptr;
```

- [ ] **Step 3: Rebuild post-battle log through presenter**

Replace the body of `BattleStatsView::showPostBattleLog()` with:

```cpp
assert(battleReport_);
BattlePostBattleSummary summary;
summary.battleResult = battleResult_;

auto append = [](const RoleEntry& entry, std::vector<BattlePostBattleUnitSummary>& target)
{
    BattlePostBattleUnitSummary unit;
    unit.identity = entry.identity;
    unit.star = entry.star;
    unit.chessInstanceId = entry.chessInstanceId;
    unit.hp = entry.hp;
    unit.maxHp = entry.hp;
    unit.attack = entry.atk;
    unit.defence = entry.def;
    unit.speed = entry.spd;
    unit.weaponId = entry.weaponId;
    unit.armorId = entry.armorId;
    unit.skillNames = entry.skillNames;
    unit.hpRemaining = entry.hpRemaining;
    unit.maxHpRemaining = entry.maxHpRemaining;
    unit.dead = entry.dead;
    unit.cancelDmg = entry.cancelDmg;
    target.push_back(std::move(unit));
};

for (const auto& entry : allies_)
{
    append(entry, summary.allies);
}
for (const auto& entry : enemies_)
{
    append(entry, summary.enemies);
}

Engine::getInstance()->showBattleLogOverlay(BattleLogPresenter().present(summary, *battleReport_));
```

Include:

```cpp
#include "BattleLogPresenter.h"
```

- [ ] **Step 4: Update unit store summary**

In `src/BattleSceneUnitStore.h`, replace:

```cpp
class BattleTracker;
BattlePostBattleSummary makePostBattleSummary(const BattleTracker& tracker, int battleResult) const;
```

with:

```cpp
class BattleReport;
BattlePostBattleSummary makePostBattleSummary(const BattleReport& report, int battleResult) const;
```

In `src/BattleSceneUnitStore.cpp`, replace:

```cpp
const BattleTracker& tracker
unit.cancelDmg = tracker.cancelDamageForUnit(source.unitId);
```

with:

```cpp
const BattleReport& report
unit.cancelDmg = report.cancelDamageForUnit(source.unitId);
```

Include `BattleReport.h` instead of `BattleStatsView.h`.

- [ ] **Step 5: Update post-battle summary call sites**

In `BattleSceneHades::makePostBattleSummary`, call:

```cpp
return scene_units_.makePostBattleSummary(battle_report_.report(), result_);
```

In `src/ChessBattleFlow.cpp`, replace:

```cpp
view->setupPostBattle(battle->makePostBattleSummary(), battle->getTracker());
```

with:

```cpp
view->setupPostBattle(battle->makePostBattleSummary(), battle->getBattleReport());
```

- [ ] **Step 6: Run post-battle related tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][scene_unit_store]"
.\x64\Debug\kys_tests.exe "[battle][log_presenter]"
```

Expected:
- build succeeds;
- unit store and presenter tests pass.

---

### Task 5: Extract BattleLogImGuiView

**Files:**
- Create: `src/BattleLogImGuiView.h`
- Create: `src/BattleLogImGuiView.cpp`
- Create: `tests/BattleLogImGuiViewUnitTests.cpp`
- Modify: `src/ImGuiLayer.h`
- Modify: `src/ImGuiLayer.cpp`
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add lightweight state tests**

Create `tests/BattleLogImGuiViewUnitTests.cpp`:

```cpp
#include "BattleLogImGuiView.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleLogImGuiViewState_ShowResetsFiltersAndOpens", "[battle][log_imgui]")
{
    BattleLogWindowState state;
    state.filter.allyUnitId = 101;
    state.filter.enemyUnitId = 202;
    state.dragging = true;

    BattleLogViewModel model;
    model.title = "本場戰鬥日誌";

    BattleLogImGuiView().show(state, model);

    CHECK(state.model.open);
    CHECK(state.model.title == "本場戰鬥日誌");
    CHECK(state.inputGuardFrames == 10);
    CHECK(state.hoverGuard);
    CHECK_FALSE(state.dragging);
    CHECK(state.filter.allyUnitId == -1);
    CHECK(state.filter.enemyUnitId == -1);
    CHECK(state.filter.category == BattleLogEntryCategory::Any);
}

TEST_CASE("BattleLogImGuiViewState_HideClosesAndClearsInteractionState", "[battle][log_imgui]")
{
    BattleLogWindowState state;
    state.model.open = true;
    state.inputGuardFrames = 5;
    state.hoverGuard = true;
    state.dragging = true;

    BattleLogImGuiView().hide(state);

    CHECK_FALSE(state.model.open);
    CHECK(state.inputGuardFrames == 0);
    CHECK_FALSE(state.hoverGuard);
    CHECK_FALSE(state.dragging);
}
```

- [ ] **Step 2: Add ImGui view header**

Create `src/BattleLogImGuiView.h`:

```cpp
#pragma once

#include "BattleLogViewModel.h"

struct BattleLogWindowState
{
    BattleLogViewModel model;
    int inputGuardFrames = 0;
    bool hoverGuard = false;
    bool dragging = false;
    int childFlip = 0;
    BattleLogFilter filter;
};

class BattleLogImGuiView
{
public:
    void show(BattleLogWindowState& state, const BattleLogViewModel& model) const;
    void hide(BattleLogWindowState& state) const;
    bool isOpen(const BattleLogWindowState& state) const;
    void render(BattleLogWindowState& state) const;
};
```

- [ ] **Step 3: Move battle log rendering**

Create `src/BattleLogImGuiView.cpp`.

Move the current body of `ImGuiLayer::renderBattleLogWindow()` into:

```cpp
void BattleLogImGuiView::render(BattleLogWindowState& state) const
```

Rename local/member references:

```text
battle_log_ -> state.model
battle_log_input_guard_frames_ -> state.inputGuardFrames
battle_log_hover_guard_ -> state.hoverGuard
battle_log_dragging_ -> state.dragging
battle_log_child_flip_ -> state.childFlip
battle_log_ally_filter_id_ -> state.filter.allyUnitId
battle_log_enemy_filter_id_ -> state.filter.enemyUnitId
matchesFilter(entry) -> battleLogEntryMatchesFilter(entry, state.filter)
visibleEntryCount() -> countVisibleBattleLogEntries(state.model, state.filter)
BattleLogTone -> BattleLogEntryTone
BattleLogLine -> BattleLogEntryView
BattleLogRoleRow -> BattleLogRoleFilterRow
```

Add the state methods:

```cpp
void BattleLogImGuiView::show(BattleLogWindowState& state, const BattleLogViewModel& model) const
{
    state.model = model;
    state.model.open = true;
    state.inputGuardFrames = 10;
    state.hoverGuard = true;
    state.dragging = false;
    state.childFlip ^= 1;
    state.filter = {};
}

void BattleLogImGuiView::hide(BattleLogWindowState& state) const
{
    state.model.open = false;
    state.inputGuardFrames = 0;
    state.hoverGuard = false;
    state.dragging = false;
}

bool BattleLogImGuiView::isOpen(const BattleLogWindowState& state) const
{
    return state.model.open;
}
```

- [ ] **Step 4: Slim ImGuiLayer**

In `src/ImGuiLayer.h`, remove battle-log view model structs and include:

```cpp
#include "BattleLogImGuiView.h"
```

Replace old battle-log fields:

```cpp
int battle_log_input_guard_frames_;
bool battle_log_hover_guard_;
bool battle_log_dragging_;
int battle_log_child_flip_;
int battle_log_ally_filter_id_;
int battle_log_enemy_filter_id_;
BattleLogData battle_log_;
```

with:

```cpp
BattleLogWindowState battle_log_;
BattleLogImGuiView battle_log_view_;
```

In `src/ImGuiLayer.cpp`, replace:

```cpp
void ImGuiLayer::showBattleLog(const BattleLogData& data)
void ImGuiLayer::hideBattleLog()
bool ImGuiLayer::isBattleLogOpen() const
void ImGuiLayer::renderBattleLogWindow()
```

with:

```cpp
void ImGuiLayer::showBattleLog(const BattleLogViewModel& model)
{
    battle_log_view_.show(battle_log_, model);
}

void ImGuiLayer::hideBattleLog()
{
    battle_log_view_.hide(battle_log_);
}

bool ImGuiLayer::isBattleLogOpen() const
{
    return battle_log_view_.isOpen(battle_log_);
}

void ImGuiLayer::renderBattleLogWindow()
{
    battle_log_view_.render(battle_log_);
}
```

Update event checks:

```cpp
battle_log_.model.open
battle_log_.inputGuardFrames
```

- [ ] **Step 5: Rename Engine battle-log API**

In `src/Engine.h`, replace:

```cpp
void showBattleLogWindow(const BattleLogData& data) const;
void hideBattleLogWindow() const;
bool isBattleLogWindowOpen() const;
```

with:

```cpp
void showBattleLogOverlay(const BattleLogViewModel& model) const;
void hideBattleLogOverlay() const;
bool isBattleLogOverlayOpen() const;
```

In `src/Engine.cpp`, rename the implementations and keep the settings gate:

```cpp
void Engine::showBattleLogOverlay(const BattleLogViewModel& model) const
{
    if (!imgui_)
    {
        return;
    }
    if (!SystemSettings::getInstance()->data().showBattleLog)
    {
        return;
    }
    imgui_->showBattleLog(model);
}
```

Update all call sites:

```text
BattleStatsView.cpp: showBattleLogOverlay, isBattleLogOverlayOpen
BattleSceneHades.cpp comment or call: hideBattleLogOverlay if still present
```

- [ ] **Step 6: Update project files**

In `src/kys.vcxproj`, add:

```xml
<ClCompile Include="BattleLogImGuiView.cpp" />
```

In `tests/kys_tests.vcxproj`, add:

```xml
<ClCompile Include="BattleLogImGuiViewUnitTests.cpp" />
<ClCompile Include="..\src\BattleLogImGuiView.cpp" />
```

- [ ] **Step 7: Run ImGui view tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
.\x64\Debug\kys_tests.exe "[battle][log_imgui]"
```

Expected:
- build succeeds;
- ImGui view state tests pass.

---

### Task 6: Remove Old Names And Project Debris

**Files:**
- Modify/delete all files found by the scans below.
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Verify old tracker/report names are gone**

Run:

```powershell
rg "\bBattleTracker\b|BattleSceneTrackerPlayer|RoleBattleStats|::BattleLogEventType|global BattleLogEvent" src tests
```

Expected:
- no `BattleTracker`;
- no `BattleSceneTrackerPlayer`;
- no `RoleBattleStats`;
- no global `::BattleLogEventType` references.

If the scan finds legitimate runtime `KysChess::Battle::BattleLogEventType` references, leave those in place.

- [ ] **Step 2: Verify old ImGui view-model names are gone**

Run:

```powershell
rg "\bBattleLogData\b|\bBattleLogLine\b|\bBattleLogRoleRow\b|\bBattleLogTone\b" src tests
```

Expected: no output.

- [ ] **Step 3: Verify old Engine method names are gone**

Run:

```powershell
rg "showBattleLogWindow|hideBattleLogWindow|isBattleLogWindowOpen" src tests
```

Expected: no output.

- [ ] **Step 4: Verify project files reference only final files**

Run:

```powershell
rg "BattleTracker.cpp|BattleSceneTrackerPlayer|BattleLogPresenter.cpp|BattleLogImGuiView.cpp|BattleReport.cpp|BattleSceneReportPlayer.cpp" src/kys.vcxproj tests/kys_tests.vcxproj
```

Expected:
- no `BattleTracker.cpp`;
- no `BattleSceneTrackerPlayer`;
- yes `BattleReport.cpp`;
- yes `BattleSceneReportPlayer.cpp`;
- yes `BattleLogPresenter.cpp`;
- yes `BattleLogImGuiView.cpp`.

---

### Task 7: Full Verification

**Files:**
- No new files.
- Fix only files implicated by failed verification.

- [ ] **Step 1: Run targeted tests**

Run:

```powershell
.\x64\Debug\kys_tests.exe "[battle][report]"
.\x64\Debug\kys_tests.exe "[battle][scene_report]"
.\x64\Debug\kys_tests.exe "[battle][log_presenter]"
.\x64\Debug\kys_tests.exe "[battle][log_imgui]"
```

Expected: all targeted tests pass.

- [ ] **Step 2: Run full unit tests**

Run:

```powershell
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 3: Run full Debug build**

Run:

```powershell
.\.github\build-command.ps1 -Configuration Debug -Platform x64 -Verbosity minimal -NoLogo
```

Expected:
- build succeeds;
- existing warnings are acceptable;
- if final link fails only because `x64\Debug\kys.exe` is locked, report the lock and treat compile as verified per repo instructions.

## Self-Review

- Spec coverage: tracker/report split, presenter extraction, ImGui view extraction, filter structuring, and old-name deletion are all covered.
- No migration artifacts: old files and old public type names are deleted, not wrapped.
- Runtime event source remains canonical: `KysChess::Battle::BattleLogEvent` remains in core battle output; report/presenter/UI types have separate names.
- Type consistency: `BattleReportBuilder` writes `BattleReport`; `BattleSceneReportPlayer` writes `BattleReportBuilder`; `BattleLogPresenter` reads `BattleReport`; `BattleLogImGuiView` renders `BattleLogViewModel`.
