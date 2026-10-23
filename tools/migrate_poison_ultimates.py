"""遷移毒宗絕招；保留既有角色數值與一般攻擊。"""

from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHILIAN_ID = 137
ULTIMATES = {161: CHILIAN_ID, 47: 21, 48: 21, 46: 87}


def migrate(database: Path) -> None:
    with sqlite3.connect(database.resolve().as_uri() + "?mode=rw", uri=True) as connection:
        connection.row_factory = sqlite3.Row
        existing = connection.execute(
            'SELECT "名称" FROM magic WHERE "编号" = ?', (CHILIAN_ID,)
        ).fetchone()
        if existing is not None and existing[0] != "赤練神掌":
            raise ValueError(f"武功 {CHILIAN_ID} 已由其他武功使用")

        magic = dict(connection.execute('SELECT * FROM magic WHERE "编号" = 21').fetchone())
        magic.update({"编号": CHILIAN_ID, "名称": "赤練神掌", "武功动画": 53})
        columns = ", ".join(f'"{column}"' for column in magic)
        placeholders = ", ".join("?" for _ in magic)
        connection.execute(
            f"INSERT OR REPLACE INTO magic ({columns}) VALUES ({placeholders})",
            tuple(magic.values()),
        )

        # 扣內由化功狀態的中毒跳傷處理；取消舊武功的立即內力傷害。
        connection.execute(
            'UPDATE magic SET "武功类型" = 1, "伤害类型" = 0, '
            '"攻击范围类型" = 0, "消耗内力" = 50, "移动范围" = 5, '
            '"杀伤范围" = 0, "杀伤内力" = 0 WHERE "编号" = 87'
        )
        for role_id, magic_id in ULTIMATES.items():
            role = connection.execute(
                'SELECT * FROM role WHERE "编号" = ?', (role_id,)
            ).fetchone()
            for star in ("一", "二", "三"):
                slot = max((1, 2), key=lambda index: role[f"{star}星威力{index}"])
                connection.execute(
                    f'UPDATE role SET "{star}星武功{slot}" = ? WHERE "编号" = ?',
                    (magic_id, role_id),
                )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", nargs="?", type=Path, default=ROOT / "work/game-dev/save/game.db")
    args = parser.parse_args()
    migrate(args.database)
    print(f"Updated poison ultimates: {args.database}")
