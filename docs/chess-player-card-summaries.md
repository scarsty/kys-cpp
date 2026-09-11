# Player card summaries

The shopping card answers “what does this do, and why would I pick it?” A lossless
description of every trigger, propagation rule and stacking policy cannot also be
a short card. The card now has an editorial summary of the combined effect.
The generated Full/Detailed descriptions remain the rules reference, accessible
through the right-click menu's **棋局總覽 → 效果全覽** and **查看羈絆**.

## Authoring

Add `卡片摘要` beside `效果` on an ultimate or a combo threshold. Write one or two
short sentences in Traditional Chinese. Each resolved sentence is limited to 80
display units (40 full-width characters). Sentences wrap at the actual card width;
this is a content budget, not permission to truncate a description.

```yaml
卡片摘要:
  - "追加${效果/0/修改攻擊/數量}道側翼劍氣，每道${效果/0/修改攻擊/傷害倍率}%傷害。"
  - "本次命中${效果/1/條件/0/不同目標數至少}名敵人，全隊速度+${效果/1/動作/0/屬性修正/數值}%，${效果/1/動作/0/屬性修正/持續幀數}幀。"
```

References use slash-separated YAML keys and zero-based sequence indices,
starting at the same container's `效果` or `管理規則`. They resolve when content loads, without
modifying the YAML or combat rules. Missing fields, invalid indices, non-scalar
references, malformed placeholders and over-budget summaries reject the content
with a diagnostic. Numerical balance changes therefore update the card without
duplicating those values in prose. No expression language or extra numeric config
is introduced.

Choose the trigger, target, defining benefit, material limitation and most useful
comparison numbers. Explain state names when they are not self-explanatory. Use a
single sentence for a simple effect. Keep frame-by-frame bookkeeping, rounding,
stack replacement policies and propagation details in the full reference unless
they define the ability's tradeoff. A summary is intentionally selective; it must
not claim to contain every rule.

References do not prove that prose still describes a changed mechanic. When
changing a target, condition, operation or rule order, review the summary alongside
the effect. Reordering two valid numeric fields can change the meaning of a path
without making it invalid. Avoid hard-coded balance numbers in prose.

## Punctuation and line breaks

Card text is prose. The author writes its punctuation; the renderer does not add
colons, strip commas or turn introductory clauses into headings.

- Use `，` to connect a trigger or condition to its result: `命中時，有30%機率彈射。`
- Use `、` for short lists and `；` when parallel clauses need a stronger separation.
- End each summary entry with `。`. Do not write colon headings such as `出手：`
  or `觸發時：`; both the loader and generated schema reject them.
- Each YAML list entry starts a new paragraph for a separate gameplay thought.
  A comma, semicolon or full stop by itself never inserts a paragraph break.
- The panel adds physical line wraps only to fit its width. Prose wrapping prefers
  punctuation only when the preceding line fills at least two-thirds of the width.
  It keeps closing punctuation with a preceding character and keeps numeric runs
  together when they fit. Wrapped continuation lines keep the same indentation.

The old generated reference has different needs: trigger, condition and chance
prefixes can each introduce a colon, semantic rows can be split at a fixed width,
and the panel then wraps those rows again. That path remains available for complete
rules, but none of the shipped ultimate or synergy cards falls back to it.

## Rendering and coverage

The content definitions hold resolved `cardSummary` sentences. The description
document copies them as presentation metadata. Only `Compact` with the explicit
`PlayerCard` policy uses them. Full, Detailed and regular Compact still render the
semantic rules and retain their existing coverage guarantees. This also keeps the
CLI's compact rules reference complete.

All 59 ultimates and all 85 thresholds across 42 synergies have summaries, including
management-only synergies such as 丐幫. Content tests require this coverage and check
the punctuation and length of every entry. Equipment and neigong are outside this
card surface. Ad hoc rules without summary metadata still use the generated renderer.

The player-card tests check the actual content, summary limits, dynamic numeric
references, unchanged complete descriptions and the narrow shop layouts. Summary
wording is content, not another C++ shape matcher for each named ability.
