# Task 2: Config — Update Pool, Combos, and Balance YAML

**Depends on**: Task 1 (need role IDs from DB script)
**Estimated complexity**: High
**Type**: Feature

## Objective

Update all YAML configuration files: add new characters to pool, define 13 new synergies, disband 三大吸功, rework 4 existing synergies, redistribute members, and add balance config fields.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically "New Synergies", "Disbanded Synergy", "Reworked Synergies", and "Config File Changes" sections.

## Files to Modify
- `config/chess_pool.yaml`
- `config/chess_combos.yaml`
- `config/chess_balance_normal.yaml`
- `config/chess_balance_easy.yaml`

## Files to Create
- `config/chess_pool_easy.yaml`

## Reference Files to Read
- `config/chess_combos.yaml` — Existing combo format and effect type names
- `config/chess_pool.yaml` — Existing pool structure
- `src/ChessBattleEffects.cpp` — `effectTypeMap` for existing Chinese effect type names
- Output of Task 1 script — to get the actual role IDs assigned to new characters

## Detailed Steps

### 1. Update `config/chess_pool.yaml`
- Add all ~37 new role IDs to their correct tier sections
- Use format: `    - <ID>  # <Name>` matching existing style
- Tier distribution per spec:
  - Tier 1 (1费): 田归农, 阎基, 王元霸, 赵敏, 俞岱岩, 程灵素, 马钰, 谭处端 (+8)
  - Tier 2 (2费): 田伯光, 宋青书, 张翠山, 黛绮丝, 殷梨亭, 莫声谷, 平一指, 胡青牛, 王处一, 刘处玄 (+10)
  - Tier 3 (3费): 韦一笑, 周芷若, 杨逍, 灭绝师太, 范瑶, 张松溪, 俞莲舟, 胡斐, 成昆, 岳不群 (+10)
  - Tier 4 (4费): 胡一刀, 张无忌, 谢逊, 殷天正, 宋远桥, 无色禅师, 郭襄 (+7)
  - Tier 5 (5费): 张三丰, 东方不败, 黄裳 (+3)

### 2. Update `config/chess_combos.yaml` — Existing Synergies

#### a. Remove 三大吸功 (北冥神功)
- Delete the entire `三大吸功` synergy entry (currently `ComboId::BeiMingShenGong`)

#### b. Redistribute members to existing synergies:
- **独行**: Add 田伯光, 东方不败, 黄裳, 无崖子 to member list
- **阴险** (武林外道): Add 丁春秋 to member list (if 武林外道 is this synergy — verify by checking current members)
- **拳师**: Add 周芷若, 殷天正, 宋远桥, 无色禅师, 成昆, 黄裳, 王处一, 虚竹. Remove 俞岱岩, 莫声谷, 马钰 (moved to 龙套)
- **红颜**: Add 周芷若, 赵敏, 黛绮丝, 程灵素, 韩小莺
- **剑客**: Add 田归农, 灭绝师太, 张松溪, 俞莲舟, 殷梨亭, 岳不群, 东方不败, 郭襄. Remove 谭处端 if she was there (she wasn't in existing config)
- **少林寺**: Add 无色禅师

#### c. Rework 剑客 thresholds (3/6 → 3/6/9):
- 3-piece: Replace effects with `攻击百分比: 20`, `穿甲` (OnHit trigger, value=100 chance, value2=20 pen%)
- 6-piece: Replace with `攻击百分比: 40`, `穿甲` (OnHit trigger, value=100 chance, value2=50 pen%)
- Add 9-piece: `攻击百分比: 60`, `穿甲` (OnHit trigger, value=100 chance, value2=80 pen%), `斩杀` (OnHit trigger, value=10 chance, value2=10 threshold%)

#### d. Rework 拳师 thresholds (3/6 → 3/6/9):
- 3-piece: Add `眩晕` (OnHit trigger, triggerValue=20 chance, value=30 duration)
- 6-piece: Add stun + knockback
- Add 9-piece: stun + knockback + `破罡` (OnHit trigger, value=10, value2=50)

#### e. Rework 红颜 thresholds (2/4 → 3/6/9):
- Change thresholds from 2/4 to 3/6/9
- Each tier gets `倾国倾城` effect with different chance/amount:
  - 3-piece: Speed +15, DEF +20%, Heal aura 10/120f, 倾国倾城 15%/50%
  - 6-piece: Speed +25, DEF +35%, Heal aura 20/120f, Healed ATK/SPD +30%, 倾国倾城 30%/100%
  - 9-piece: Speed +35, DEF +50%, Heal aura 30/120f, Healed ATK/SPD +50%, 倾国倾城 60%/150%, 攻击倾城 10%

Note: The Chinese effect type names (倾国倾城, 攻击倾城, 斩杀, 破罡, etc.) are NEW and will be registered in Task 3. The YAML will log "无法识别" warnings until then — this is expected.

#### f. Rework 全真教 thresholds (3 → 3/5/7):
- Expand from 3-piece only to 3/5/7 thresholds
- Each tier gets shield + 护盾爆炸 effect
- 7-piece adds 护盾重获 (re-shield on ally death)

### 3. Add 13 new synergies to `config/chess_combos.yaml`
Add entries for: 刀客, 踏雪无痕, 阴险, 宗师, 四大法王, 真武七截阵, 逍遥二仙, 九阳传人, 倚天屠龙, 妙手仁心, 先天神照, 乾坤挪移, 龙套.

Each follows the YAML format:
```yaml
  - 名称: <name>
    成员:
      - <id>  # <character_name>
    阈值:
      - 人数: <N>
        名称: <threshold_name>
        效果:
          - 类型: <effect_type_chinese>
            数值: <value>
```

Use new Chinese effect type names that will be registered in Task 3:
- `流血` → BleedChance
- `流血持续` → BleedPersist
- `踏雪` → PostSkillDash
- `敌方削弱` → EnemyTopDebuff
- `闪击` → BlinkAttack
- `同袍之死` → AllyDeathStatBoost
- `七截分身` → CloneSummon
- `弹反` → ProjectileReflect
- `无视防御` → IgnoreDefense
- `群体施治` → OnSkillTeamHeal
- `死亡庇护` → DeathPrevention
- `保护挪移` → ForcePullProtect
- `处决挪移` → ForcePullExecute
- `斩杀` → Execute
- `破罡` → MPBlock
- `倾国倾城` → CharmCDRDebuff
- `攻击倾城` → OffensiveCharm
- `殉爆` → DeathAOE
- `护盾爆炸` → ShieldExplosion
- `护盾重获` → ShieldOnAllyDeath

For synergies with special flags:
- `独行` has `反向羁绊: true` — other new anti-combos don't need this
- `宗师` should have `星级羁绊: true` (`starSynergyBonus: true`) if applicable

### 4. Update balance configs
In `config/chess_balance_normal.yaml`:
```yaml
商店数量: 6
最大禁棋数: 15
```

In `config/chess_balance_easy.yaml`:
```yaml
商店数量: 5
最大禁棋数: 0
```

### 5. Create `config/chess_pool_easy.yaml`
- Select ~50-55 characters from the full pool
- For every synergy defined in `chess_combos.yaml`, either include 0 members or ALL members (enough for max threshold)
- Annotate with name comments
- Verify synergy completeness manually

### 6. Commit: `feat: add 13 new synergies, rework 4 existing, update pool and balance configs`

## Acceptance Criteria
- [ ] `chess_pool.yaml` has ~88 character IDs across 5 tiers
- [ ] `chess_combos.yaml` has all 13 new synergies with correct members and thresholds
- [ ] 三大吸功 is removed from `chess_combos.yaml`
- [ ] 剑客, 拳师, 红颜, 全真教 synergies have updated thresholds/effects
- [ ] Member redistribution complete (独行+无崖子, 阴险+丁春秋, 拳师+虚竹/周芷若/etc., 红颜+韩小莺/etc.)
- [ ] Balance configs have `商店数量` and `最大禁棋数` fields
- [ ] `chess_pool_easy.yaml` exists with synergy-complete subset
- [ ] All YAML is valid (no syntax errors)

## Notes
- New Chinese effect type names will cause log warnings until Task 3 registers them — this is expected and harmless.
- The spec's 阴险 synergy needs to be distinguished from the existing 武林外道 synergy — verify whether they're the same or separate. If they share members (成昆, 岳不群 are new adds), 阴险 may BE the renamed 武林外道, or be a new separate synergy. Check the spec carefully.
- 宋青书 is both in 真武七截 and 阴险 — 2 synergies, within the 3-max limit.
