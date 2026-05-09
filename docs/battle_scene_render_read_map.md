# Battle Scene Render Read Map

## Draw Source

Draw reads `BattleSceneHades::BattleSceneRenderUnit`, not `Role`.

## Render Unit Fields

- Identity/assets: `unitId`, `realRoleId`, `name`, `headId`, `fightFrames`
- Sprite pose: `position`, `velocity`, `acceleration`, `realTowards`, `actType`, `actFrame`
- State: `alive`, `team`, `shake`, `attention`
- Bars/text: `hp`, `maxHp`, `mp`, `maxMp`, `frozen`, `frozenMax`, `cooldown`, `cooldownMax`, `invincible`
- Combo render: `combo.shield`, `combo.blockFirstHitsRemaining`

## Remaining `Role` Uses

- Setup and initialization seed `Role` and `Magic` facts into runtime.
- Position swap still mutates pre-battle setup roles.
- Tracker logging still accepts `Role*`.
- Targeting highlight still calls legacy `inEffect(acting_role_, targetRole)`.
- Legacy non-runtime effects may still use `AttackEffect::FollowRole`; runtime visual effects use `FollowUnitId`.
