# KYS Chess UI Design Language

Apply when adding or changing chess screens, layouts, or interactions. Use preparation (整備) as the visual baseline: **warm, translucent, readable, with complete overviews.**

## Visuals

- Keep the **1280×720 logical canvas**. The existing presentation layer handles window scaling; do not introduce responsive logical dimensions.
- Reuse panels, text, icons, and control states from [ChessUiStyle.h](../src/ChessUiStyle.h). Warm charcoal panels reveal the scene beneath. Current alpha values: panels 180/255, full-screen scrim 48/255; avoid arbitrary darkening per screen.
- Use ivory for body text, muted taupe for secondary text, gold for emphasis and primary actions, cyan for contextual information, and green/red for positive/negative states. Preserve semantic colors for piece tiers, stars, gold, experience, and difficulty.
- Default type sizes: page title 24, section title 22, body 18, metadata 16; dense tables may use 14. Simplify columns and wording before shrinking text.
- Default spacing: outer margin 16, section gap 12, padding 24; panel/button corner radii 6/4. Dense reports may reduce padding while retaining alignment and consistent line spacing.

## Information and Interaction

- Keep the main list, board, or comparison visible. Hover previews; clicking pins the selection. Prefer expanding details within the same right-hand inspector (檢視器).
- Complex content may open an existing full view, such as the synergy overview. Back must restore the originating screen and selection; avoid duplicate subviews.
- Use readable, color-coded, single-screen tables for comparisons such as talents. Organize long guides and challenges with tab buttons and ample content space.
- Pair shared icons with short labels. Distinguish Back from Return to Start; show hover, selection, focus, and disabled states clearly.
- Expose frequent actions directly, using the bottom action bar for discoverable save/load controls. Avoid nested “More → menu → menu” flows. List/map formation returns to the main pre-battle formation menu.
- Show static content and a meaningful loading notice first; block dependent actions until ready. Do not assume background threads are available.
- Write in-game copy in Traditional Chinese and describe actual chess rules. Remove unused fields, empty-effect explanations, and redundant labels. Group related stats and keep values close to their names.

## Battle Overviews

- **Pre-battle:** preserve both complete rosters with names, stars, HP, attack, defense, speed (輕功), equipment, and synergy thresholds. Skill mechanics and rules can live in the inspector.
- **Post-battle:** make a shareable report with up to 13 units per side on one screen, one row per unit, including defeated units and clones. Retain damage dealt, DPS, damage taken, kills, damage canceled (抵消), remaining HP, equipment names, and the two main skills with their damage.
- Stack teams vertically with shared column boundaries and right-aligned numbers. Place skill damage beside its name. Keep equipment names alongside icons.
- Each team's right-hand summary shows its talent, around eight inner skills (內功), and synergies (羈絆) **that reach the first threshold**. Normally show icon, name, and threshold; dense sets may use multiple columns of icons and thresholds.
- Offline battles show both players' talents. Omit effect tutorials, duration in seconds, and screenshot-saving controls from the report. Open logs explicitly and return to the same report; clicking the report body must not dismiss it.

## References and Verification

- Preparation/inspector: [ChessPreparationScreen.cpp](../src/ChessPreparationScreen.cpp). Selection/tabs: [ChessSelectionScreen.cpp](../src/ChessSelectionScreen.cpp).
- Pre-battle: [ChessBattleOverview.cpp](../src/ChessBattleOverview.cpp). Post-battle: [BattleStatsView.cpp](../src/BattleStatsView.cpp). Reuse shared styles and data formatting rather than duplicating implementations.
- Verify actual 1280×720 rendering with dense content, long names, 13 units per side, eight inner skills, many synergies, and light/dark backgrounds. Check scaled hit targets, return navigation, and floating web controls for overlap. Follow the root AGENTS.md for builds and tests.
