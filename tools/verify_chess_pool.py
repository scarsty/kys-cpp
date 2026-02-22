# -*- coding: utf-8 -*-
import sqlite3, sys
sys.stdout.reconfigure(encoding='utf-8')

conn = sqlite3.connect('d:/projects/kys-cpp/kys-cpp/work/game-dev/save/0.db')
cur = conn.cursor()

# Hardcoded from ChessPool.cpp
pool = {
    1: [(130, "柯镇恶"), (131, "朱聪"), (132, "韩宝驹"), (133, "南希仁"),
        (134, "张阿生"), (135, "全金发"), (136, "韩小莹"), (63, "程英"),
        (84, "霍都"), (160, "达尔巴"), (161, "李莫愁"), (45, "薛慕华"),
        (47, "阿紫"), (104, "阿朱"), (105, "阿碧")],
    2: [(54, "袁承志"), (37, "狄云"), (97, "血刀老祖"), (56, "黄蓉"),
        (44, "岳老三"), (48, "游坦之"), (99, "叶二娘"), (100, "云中鹤"),
        (102, "枯荣"), (115, "苏星河")],
    3: [(55, "郭靖"), (67, "裘千仞"), (68, "丘处机"), (59, "小龙女"),
        (46, "丁春秋"), (51, "慕容复"), (53, "段誉"), (70, "玄慈"),
        (98, "段延庆"), (103, "鸠摩智"), (112, "萧远山"), (113, "慕容博")],
    4: [(57, "黄药师"), (60, "欧阳锋"), (64, "周伯通"), (65, "一灯"),
        (69, "洪七公"), (58, "杨过"), (62, "金轮法王"), (49, "虚竹"),
        (50, "乔峰"), (117, "天山童姥"), (118, "李秋水")],
    5: [(129, "王重阳"), (114, "扫地老僧"), (116, "无崖子")],
}

errors = 0
for tier, entries in pool.items():
    print(f"\n=== Tier {tier} ===")
    for role_id, expected_name in entries:
        cur.execute('SELECT 名字 FROM role WHERE 编号 = ?', (role_id,))
        row = cur.fetchone()
        if row is None:
            print(f"  ERROR: ID {role_id} not found in DB (expected: {expected_name})")
            errors += 1
        else:
            actual = row[0].strip()
            if actual == expected_name:
                print(f"  OK: {role_id} = {actual}")
            else:
                print(f"  MISMATCH: ID {role_id} expected '{expected_name}' but DB has '{actual}'")
                errors += 1

print(f"\n{'ALL GOOD' if errors == 0 else f'{errors} ERRORS FOUND'}!")
conn.close()
