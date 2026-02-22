"""Migrate YAML configs from 0-based to 1-based star/tier indexing."""
import re, os

os.chdir(os.path.join(os.path.dirname(__file__), '..', 'config'))

def bump_pair(m):
    return f'[{int(m.group(1))+1}, {int(m.group(2))+1}]'

# chess_balance.yaml: enemy table [tier, star] -> [tier+1, star+1]
with open('chess_balance.yaml', 'r', encoding='utf-8') as f:
    lines = f.readlines()

in_enemy = False
out = []
for line in lines:
    if line.startswith('敌人表:'):
        in_enemy = True
    elif in_enemy and line and not line[0].isspace():
        in_enemy = False
    if in_enemy and '[' in line and not line.startswith('敌人表:'):
        line = re.sub(r'\[(\d+),\s*(\d+)\]', bump_pair, line)
    out.append(line)

with open('chess_balance.yaml', 'w', encoding='utf-8') as f:
    f.writelines(out)
print("Updated chess_balance.yaml")

# chess_challenge.yaml
with open('chess_challenge.yaml', 'r', encoding='utf-8') as f:
    lines = f.readlines()

def bump_star_only(m):
    return f'[{m.group(1)}, {int(m.group(2))+1}]'

out = []
for line in lines:
    if '敌人:' in line and '[[' in line:
        line = re.sub(r'\[(\d+),\s*(\d+)\]', bump_star_only, line)
    elif '最高费用:' in line:
        line = re.sub(r'(最高费用:\s*)(\d+)', lambda m: f'{m.group(1)}{int(m.group(2))+1}', line)
    elif '最高层级:' in line:
        line = re.sub(r'(最高层级:\s*)(\d+)', lambda m: f'{m.group(1)}{int(m.group(2))+1}', line)
    # Order matters: 升星1到2 before 升星0到1
    if '升星1到2' in line:
        line = line.replace('升星1到2', '升星2到3')
    elif '升星0到1' in line:
        line = line.replace('升星0到1', '升星1到2')
    out.append(line)

with open('chess_challenge.yaml', 'w', encoding='utf-8') as f:
    f.writelines(out)
print("Updated chess_challenge.yaml")

# chess_neigong.yaml
with open('chess_neigong.yaml', 'r', encoding='utf-8') as f:
    lines = f.readlines()

in_boss, in_tier = False, False
out = []
for line in lines:
    if 'Boss可选层级' in line:
        in_boss, in_tier = True, False
    elif '层级分配' in line:
        in_boss, in_tier = False, True
    elif line and not line[0].isspace() and ':' in line:
        in_boss = in_tier = False

    if in_boss and re.match(r'\s+\d+:\s*\[', line):
        key, rest = line.split('[', 1)
        rest = re.sub(r'\d+', lambda m: str(int(m.group(0))+1), rest)
        line = key + '[' + rest
    elif in_tier and '层级:' in line:
        line = re.sub(r'(层级:\s*)(\d+)', lambda m: f'{m.group(1)}{int(m.group(2))+1}', line)
    out.append(line)

with open('chess_neigong.yaml', 'w', encoding='utf-8') as f:
    f.writelines(out)
print("Updated chess_neigong.yaml")

print("\nDone! All configs migrated to 1-based indexing.")
