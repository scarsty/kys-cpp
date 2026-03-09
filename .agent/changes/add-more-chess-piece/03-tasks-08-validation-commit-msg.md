# Task 8: Final Validation and Commit Message

**Depends on**: All previous tasks (1-7)
**Estimated complexity**: Low
**Type**: Documentation

## Objective

Perform final integration validation, update any documentation, and generate the commit message for the entire feature.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

## Files to Create
- `.agent/changes/add-more-chess-piece/04-commit-msg.md`

## Detailed Steps

### 1. Full build verification
Build the project on the primary platform (Windows with `kys.sln`). Fix any remaining compilation errors.

### 2. Run verification tools
- Run `tools/verify_chess_pool.py` against the updated `chess_pool.yaml` (if it exists and validates pool integrity)
- Run `tools/add_chess_characters.py` to ensure it still works idempotently
- Verify all YAML files parse without errors (load the game and check console for "无法识别" warnings — there should be none after Task 3)

### 3. Smoke test checklist
Manually verify in-game:
- [ ] New characters appear in shop in Normal mode
- [ ] Shop shows 6 slots in Normal, 5 in Easy
- [ ] At least one new synergy activates correctly (e.g., 刀客 with 3 members)
- [ ] Banning a character removes it from subsequent shop rolls
- [ ] A reworked synergy (剑客 3/6/9) shows correct thresholds in UI
- [ ] Easy mode uses curated pool
- [ ] Save/load works with new GameDataStore fields (seenRoleIds, bannedRoleIds)
- [ ] Old saves load without crash (backward compatibility)

### 4. Review git history
- Identify all commits made during this implementation (from Task 1 through Task 7)
- Read their commit messages and at least skim the diffs
- Understand the full scope of changes made

### 5. Generate `04-commit-msg.md`

Create the file with a conventional commit message summarizing the entire feature, following this template:

```markdown
feat(chess): expand pool to 88 characters with 13 new synergies

<body focusing on what changed for players, wrapped at 100 chars>

- Key bullet points of behavioral changes
- New mechanics available

Example:
`<code example if applicable>`
```

The commit message should:
- Focus on WHAT changed and WHY (player impact), not HOW
- Lines wrapped to 100 characters
- Not describe files changed or tests (CI handles that)
- Highlight behavioral changes for users
- Use simple markdown for inline code or examples
- Be concise

### 6. Final commit: `chore: add final validation and commit message for chess pool expansion`

## Acceptance Criteria
- [ ] Project compiles without errors
- [ ] No YAML parsing warnings in console
- [ ] `04-commit-msg.md` exists with proper conventional commit format
- [ ] Smoke test checklist items verified

## Notes
- This task is the wrap-up. It should be fast if all previous tasks are done correctly.
- The commit message in `04-commit-msg.md` is for the squash/merge commit of the entire feature, not individual task commits.
