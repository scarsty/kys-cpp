# KYS Chess UI Design Language

Apply when adding or changing chess screens, layouts, or interactions. Use preparation (整備) as the visual baseline: **warm, translucent, readable, with complete overviews.**

This document defines shared design principles and visual conventions. Keep feature rules, settings defaults, feature-specific gesture mappings, timing constants, and implementation details in their owning code or feature documentation.

## Visuals

- Keep the **1280×720 logical canvas**. All game UI uses this fixed coordinate system, regardless of physical window or device size. The presentation layer scales the complete canvas and maps pointer coordinates; do not add alternate layouts, font sizes, column counts, or pagination based on viewport dimensions. ImGui tooling is exempt.
- Reuse panels, text, icons, and control states from [ChessUiStyle.h](../src/ChessUiStyle.h). Warm charcoal panels reveal the scene beneath. Use the shared panel and scrim opacity values; avoid arbitrary darkening per screen.
- Use ivory for body text, muted taupe for secondary text, gold for emphasis and primary actions, cyan for contextual information, and green/red for positive/negative states. Preserve semantic colors for piece tiers, stars, gold, experience, and difficulty.
- Default type sizes: page title 24, section title 22, body 18, metadata 16; dense tables may use 14. Simplify columns and wording before shrinking text.
- Preserve a clear type hierarchy across screens. Keep names readable over artwork with a subtle shaded backing where needed. Fit artwork consistently within its controls, preserving aspect ratio and accounting for transparent margins.
- Default spacing: outer margin 16, section gap 12, padding 24; panel/button corner radii 6/4. Dense reports may reduce padding while retaining alignment and consistent line spacing.

## Information and Interaction

- Keep the main list, board, or comparison visible. Hover previews; clicking pins the selection. Prefer expanding details within the same right-hand inspector (檢視器).
- Complex content may open an existing full view, such as the synergy overview. Returning or refreshing after an action should preserve the user's place: selection, page, filters, and keyboard focus. Avoid duplicate subviews.
- Use readable, color-coded, single-screen tables for comparisons such as talents. Organize long guides and challenges with tab buttons and ample content space.
- Pair shared icons with short labels. Distinguish Back from Return to Start; show hover, selection, focus, and disabled states clearly.
- Make contextual help available to both pointer and touch users. Previews should be distinct from actions, and opening or dismissing help should not accidentally activate the underlying control.
- Use brief, purposeful motion to show where content moves. Preserve spatial continuity when state changes; prefer updating the affected card, count, or status in place over interrupting the user with a success popup.
- Expose frequent actions directly in a consistent action bar. Avoid nested “More → menu → menu” flows. Make active modes visible and explain unavailable actions close to the relevant control.
- Anchor guided help to the actual controls, use cyan highlights, and keep instructions clear of their targets. Let users navigate and dismiss guidance without requiring a resource-spending action.
- During loading, show the available content, a readable current stage, and meaningful progress. Use cyan for waiting information and make dependent actions visibly unavailable until ready.
- Action sections over scenery use one shallow translucent panel behind the whole group: hints, navigation and confirmation actions. This includes selection footers, tab rows, formation headers, and pre-/post-battle footers. Reuse an existing enclosing panel where present; do not add backgrounds or change colors on individual buttons.
- Reserve space for action bars in the content layout. Keep a 12 px gap from adjacent panels, center controls vertically, and leave padding between their focus outlines and the bar edges.
- Write in-game copy in Traditional Chinese and describe actual chess rules. Remove unused fields, empty-effect explanations, and redundant labels. Group related stats and keep values close to their names.

## Dense Information and Comparisons

- Keep complete rosters and comparable results visible together. Use one row per unit and a consistent information hierarchy for both sides; move detailed mechanics and rules into the inspector.
- Stack team tables vertically with shared column boundaries and right-aligned numbers. Keep labels, values, and related icons together; place skill damage beside its name and equipment names alongside icons.
- Group inspector content into identity/equipment, resources, and combat stats, separated by subtle shared dividers. Use slim, flat green HP and blue MP bars with labels and values above the fills; keep supporting metadata visually quiet.
- Present status effects as complete, readable cards with a clear distinction between timed, consumable, and ongoing effects. Preserve positive/negative semantic colors and paginate between cards rather than splitting them.
- Use compact summaries for talents, inner skills (內功), and synergies (羈絆). Keep icons, names, and thresholds legible; dense sets may use aligned columns without losing their grouping.
- Make reports readable as standalone, shareable views. Keep secondary details such as logs behind explicit controls, and require an explicit navigation action to leave the report. Inspector and overlay interactions must not activate the scene beneath them.

## References and Verification

- Preparation/inspector: [ChessPreparationScreen.cpp](../src/ChessPreparationScreen.cpp). Selection/tabs: [ChessSelectionScreen.cpp](../src/ChessSelectionScreen.cpp).
- Pre-battle: [ChessBattleOverview.cpp](../src/ChessBattleOverview.cpp). Post-battle: [BattleStatsView.cpp](../src/BattleStatsView.cpp). Reuse shared styles and data formatting rather than duplicating implementations.
- Verify actual 1280×720 rendering with dense content, long names, full rosters, many inner skills and synergies, and light/dark backgrounds. Check readability, hit targets, control states, return navigation, and overlay overlap.
- Use browser-use tools to inspect the running web UI and exercise interactions, with 1280×720 layout screenshots. Check floating web controls for overlap; smaller physical viewports scale the same logical canvas rather than define another layout.
- Follow the root AGENTS.md for builds and tests.
