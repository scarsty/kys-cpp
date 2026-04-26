from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
JSG_ROOT = Path(r"D:\games\金书群侠传1.07V25")
JSG_DATA_DIR = JSG_ROOT / "data"
JSG_FIGHT_DIR = JSG_DATA_DIR / "fight"
JSG_RANGER_IDX = JSG_DATA_DIR / "ranger.idx"
JSG_RANGER_GRP = JSG_DATA_DIR / "ranger.grp"

JSG_ROLE_RECORD_SIZE = 1278
JSG_ROLE_ID_OFFSET = 0
JSG_ROLE_HEAD_OFFSET = 2
JSG_ROLE_NAME_OFFSET = 8
JSG_ROLE_NAME_SIZE = 10
JSG_ROLE_NICK_OFFSET = 18
JSG_ROLE_NICK_SIZE = 10
JSG_ROLE_FRAME_OFFSET = 50
JSG_ROLE_FRAME_DELAY_OFFSET = 60
JSG_ROLE_SOUND_DELAY_OFFSET = 70


def to_simplified(text: str) -> str:
    t2s = str.maketrans({
        "張": "张", "無": "无", "忌": "忌", "楊": "杨", "逍": "逍", "謝": "谢", "遜": "逊",
        "韋": "韦", "東": "东", "敗": "败", "黃": "黄", "裳": "裳", "豐": "丰", "絕": "绝",
        "師": "师", "趙": "赵", "書": "书", "靈": "灵", "瑤": "瑶", "遠": "远", "橋": "桥",
        "蓮": "莲", "舟": "舟", "岱": "岱", "巖": "岩", "聲": "声", "禪": "禅", "馬": "马",
        "鈺": "钰", "處": "处", "譚": "谭", "劉": "刘", "閻": "阎", "歸": "归", "農": "农",
        "慕": "慕", "復": "复", "歐": "欧", "陽": "阳", "輪": "轮", "榮": "荣", "鳩": "鸠",
        "掃": "扫", "蘇": "苏", "遙": "遥", "臺": "台", "俠": "侠", "麗": "丽", "綺": "绮",
        "絲": "丝", "黛": "黛", "碧": "碧", "蓉": "蓉", "藥": "药", "燈": "灯", "聰": "聪",
        "韓": "韩", "瑩": "莹", "達": "达", "爾": "尔", "壩": "坝", "虛": "虚", "喬": "乔",
        "蕭": "萧", "譽": "誉", "龍": "龙", "滅": "灭", "綠": "绿", "風": "风", "雲": "云",
        "嶽": "岳", "葉": "叶", "後": "后", "國": "国", "實": "实", "兒": "儿", "雙": "双",
    })
    return text.translate(t2s)


def decode_cp950(raw: bytes) -> str:
    return raw.split(b"\x00", 1)[0].decode("cp950", errors="ignore").strip()


def read_i16_values(raw: bytes, offset: int, count: int = 5) -> list[int]:
    return [
        int.from_bytes(raw[offset + 2 * index: offset + 2 * index + 2], "little", signed=True)
        for index in range(count)
    ]


def fightframe_text_from_values(values: list[int]) -> str:
    lines = [f"{index}, {value}" for index, value in enumerate(values) if value > 0]
    return "\n".join(lines) + ("\n" if lines else "")


def fightframe_hash_from_values(values: list[int]) -> str:
    h = hashlib.sha256()
    h.update(fightframe_text_from_values(values).encode("utf-8"))
    return h.hexdigest()


def role_blob_offsets(idx_bytes: bytes) -> tuple[int, int]:
    offsets = [int.from_bytes(idx_bytes[i:i + 4], "little") for i in range(0, len(idx_bytes), 4)]
    if len(offsets) < 2:
        raise RuntimeError(f"Unexpected ranger.idx entry count: {len(offsets)}")
    return offsets[0], offsets[1]


def iter_jsg_rows() -> list[dict[str, Any]]:
    role_start, role_end = role_blob_offsets(JSG_RANGER_IDX.read_bytes())
    role_blob = JSG_RANGER_GRP.read_bytes()[role_start:role_end]
    if len(role_blob) % JSG_ROLE_RECORD_SIZE != 0:
        raise RuntimeError(f"Unexpected ranger role blob size: {len(role_blob)}")

    rows: list[dict[str, Any]] = []
    for start in range(0, len(role_blob), JSG_ROLE_RECORD_SIZE):
        record_id = int.from_bytes(role_blob[start + JSG_ROLE_ID_OFFSET:start + JSG_ROLE_ID_OFFSET + 2], "little", signed=True)
        head_id = int.from_bytes(role_blob[start + JSG_ROLE_HEAD_OFFSET:start + JSG_ROLE_HEAD_OFFSET + 2], "little", signed=True)
        name = decode_cp950(role_blob[start + JSG_ROLE_NAME_OFFSET:start + JSG_ROLE_NAME_OFFSET + JSG_ROLE_NAME_SIZE])
        nickname = decode_cp950(role_blob[start + JSG_ROLE_NICK_OFFSET:start + JSG_ROLE_NICK_OFFSET + JSG_ROLE_NICK_SIZE])
        if not name:
            continue
        frame_num = read_i16_values(role_blob, start + JSG_ROLE_FRAME_OFFSET)
        frame_delay = read_i16_values(role_blob, start + JSG_ROLE_FRAME_DELAY_OFFSET)
        sound_delay = read_i16_values(role_blob, start + JSG_ROLE_SOUND_DELAY_OFFSET)
        rows.append(
            {
                "record_id": record_id,
                "head_id": head_id,
                "name": name,
                "nickname": nickname,
                "frame_num": frame_num,
                "frame_delay": frame_delay,
                "sound_delay": sound_delay,
                "fightframe_text": fightframe_text_from_values(frame_num),
                "fightframe_hash": fightframe_hash_from_values(frame_num),
                "attack_frame": frame_delay[2] if len(frame_delay) > 2 else 0,
                "attack_sound_frame": sound_delay[2] if len(sound_delay) > 2 else 0,
            }
        )
    return rows


def dedupe_rows(rows: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    deduped: list[dict[str, Any]] = []
    seen: set[tuple[int, int]] = set()
    for row in rows:
        key = (int(row["record_id"]), int(row["head_id"]))
        if key in seen:
            continue
        seen.add(key)
        deduped.append(row)
    return deduped


def load_jsg_rows() -> dict[str, list[dict[str, Any]]]:
    by_name: dict[str, list[dict[str, Any]]] = {}
    for row in dedupe_rows(iter_jsg_rows()):
        for key in {row["name"], to_simplified(row["name"])}:
            bucket = by_name.setdefault(key, [])
            if row not in bucket:
                bucket.append(row)
    return by_name


def load_jsg_rows_by_head() -> dict[int, list[dict[str, Any]]]:
    by_head: dict[int, list[dict[str, Any]]] = {}
    for row in dedupe_rows(iter_jsg_rows()):
        by_head.setdefault(int(row["head_id"]), []).append(row)
    return by_head


def find_fight_path(fight_id: int, suffix: str) -> Path | None:
    for stem in (f"fight{fight_id}", f"fight{fight_id:03}", f"fight{fight_id:04}"):
        candidate = JSG_FIGHT_DIR / f"{stem}{suffix}"
        if candidate.exists():
            return candidate
    return None


def require_fight_path(fight_id: int, suffix: str) -> Path:
    path = find_fight_path(fight_id, suffix)
    if path is None:
        raise FileNotFoundError(f"Missing {suffix} for fight id {fight_id} under {JSG_FIGHT_DIR}")
    return path