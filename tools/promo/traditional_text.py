"""共用的繁體中文正規化規則。"""


# 這些字在一般詞彙中可以轉換，但在人名或武功名中必須保留原字。
PRESERVED_PHRASES = (
    "張三丰",
    "岳不群",
    "余滄海",
    "岳老三",
    "游坦之",
    "岳靈珊",
    "鮮于通",
    "游驥",
    "游駒",
    "桃干仙",
    "馮錫范",
    "范一飛",
    "板垣征四",
    "松井石根",
    "松風劍法",
    "斗轉星移",
    "黃沙萬里鞭",
)

# 與遊戲目前採用的人名和異體字保持一致。
GAME_TRADITIONAL_REPLACEMENTS = (
    ("羣", "群"),
    ("範遙", "范遙"),
    ("二孃", "二娘"),
    ("峯", "峰"),
    ("張三豐", "張三丰"),
    ("祕", "秘"),
    ("爲", "為"),
    ("裏", "裡"),
    ("纔", "才"),
    ("喫", "吃"),
)

PROMO_TRADITIONAL_REPLACEMENTS = GAME_TRADITIONAL_REPLACEMENTS + (
    ("遠徵", "遠征"),
    ("幹擾", "干擾"),
    ("揹包", "背包"),
    ("覆盤", "復盤"),
    ("進賬", "進帳"),
    ("着", "著"),
    ("隻", "只"),
)


def to_traditional(text: str, converter, replacements=GAME_TRADITIONAL_REPLACEMENTS) -> str:
    protected = text
    phrases = {}
    for index, phrase in enumerate(PRESERVED_PHRASES):
        if phrase not in protected:
            continue
        token = f"__KYS_PRESERVED_{index}__"
        protected = protected.replace(phrase, token)
        phrases[token] = phrase

    converted = converter.convert(protected)
    for old, new in replacements:
        converted = converted.replace(old, new)
    for token, phrase in phrases.items():
        converted = converted.replace(token, phrase)
    return converted
