import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import readline from 'node:readline';
import { isDeepStrictEqual } from 'node:util';
import { readJson, readSave, sha256 } from './report-common.mjs';

const roleFlags = ['purchased', 'owned', 'enlisted', 'fielded', 'early', 'mid', 'late', 'campaign', 'expedition', 'final_owned', 'final', 'banned', 'offered'];
const comboFlags = ['ever_active', 'fielded', 'early', 'mid', 'late', 'campaign', 'expedition', 'final'];
const roleCounters = ['buys', 'sales', 'battles', 'campaign_battles', 'expedition_battles', 'shop_slots'];
const emptyRole = () => Object.fromEntries([...roleFlags, ...roleCounters].map(k => [k, 0]));
const emptyCombo = () => ({...Object.fromEntries([...comboFlags, 'battles', 'max_threshold', 'max_count', 'final_threshold', 'final_count', 'equipment_battles'].map(k => [k, 0])), thresholds: {}, first_fight: null});
const ensure = (condition, message) => { if (!condition) throw new Error(message); };
const sum = values => values.reduce((a, b) => a + b, 0);
const round = n => Math.round((n + Number.EPSILON) * 100) / 100;

async function* records(file) {
  const input = fs.createReadStream(file);
  const stream = file.endsWith('.gz') ? input.pipe(zlib.createGunzip()) : input;
  if (stream !== input) input.on('error', error => stream.destroy(error));
  const lines = readline.createInterface({input: stream, crlfDelay: Infinity});
  try { for await (const line of lines) if (line.trim()) yield JSON.parse(line); }
  finally { lines.close(); input.destroy(); stream.destroy(); }
}

export async function analyzeRun(directory, entry, manifestDirectory, expectedCatalog) {
  const {checkpoint, header} = readSave(directory, entry.file, entry.sha256);
  ensure(entry.valid && entry.unchanged, `存檔未通過完整驗證：${entry.file}`);
  const decisions = checkpoint.replay.decisions;
  const iterator = records(path.resolve(manifestDirectory, entry.trace))[Symbol.asyncIterator]();
  try {
    const next = async () => { const r = await iterator.next(); ensure(!r.done, `決策快照不完整：${entry.file}`); return r.value; };
    const catalog = await next();
    ensure(catalog.format === 'kys_chess_usage_trace' && catalog.format_version === 1, '不支援的決策快照格式');
    ensure(catalog.content_fingerprint === header.content_fingerprint, `規則指紋不符：${entry.file}`);
    if (expectedCatalog) ensure(isDeepStrictEqual(catalog, expectedCatalog), '不可合併不同規則內容的使用率');
    const roles = {}, combos = {}, battles = [];
    const role = id => roles[id] ??= emptyRole();
    const combo = id => combos[id] ??= emptyCombo();
    const mark = state => {
      ensure(state.combos.length === catalog.combos.length, '羈絆快照與目錄不符');
      for (const p of Object.values(state.roster)) { role(p.roleId).owned = 1; if (p.deployed) role(p.roleId).enlisted = 1; }
      for (const id of state.bans) role(id).banned = 1;
      for (const s of state.shop) if (s.roleId >= 0) role(s.roleId).offered = 1;
      state.combos.forEach((p, i) => { if (p.active) combo(catalog.combos[i].id).ever_active = 1; });
    };
    let previous = await next(), challenge = null, mainBattles = 0, challengeBattles = 0, purchaseCount = 0;
    ensure(previous.sequence === 0, '缺少初始決策局面');
    mark(previous);
    const countShop = state => { for (const s of state.shop) if (s.roleId >= 0) ++role(s.roleId).shop_slots; };
    countShop(previous);
    for (let i = 0; i < decisions.length; ++i) {
      const current = await next(), action = decisions[i].action, kind = action.type;
      ensure(current.sequence === i + 1, `決策序號不連續：${entry.file}`);
      if (kind === 'buy_shop_slot') {
        const id = previous.shop[action.slot].roleId;
        ensure(id >= 0, '購買紀錄指向空商店欄位');
        role(id).purchased = 1; ++role(id).buys; ++purchaseCount;
      } else if (kind === 'sell_chess') ++role(previous.roster[action.chess_instance_id].roleId).sales;
      else if (kind === 'prepare_battle') challenge = null;
      else if (kind === 'start_challenge') challenge = action.challenge_name;
      else if (kind === 'start_battle') {
        const pieces = Object.values(previous.roster).filter(p => p.deployed);
        const deployed = [...new Set(pieces.map(p => p.roleId))].sort((a, b) => a - b);
        const stage = challenge !== null ? 'expedition' : previous.fight < 8 ? 'early' : previous.fight < 16 ? 'mid' : 'late';
        const category = challenge !== null ? 'expedition' : 'campaign';
        if (challenge !== null) ++challengeBattles; else ++mainBattles;
        for (const id of deployed) {
          const r = role(id); r.fielded = r[stage] = r[category] = 1; ++r.battles; ++r[category + '_battles'];
        }
        const active = {};
        previous.combos.forEach((progress, index) => {
          if (!progress.active) return;
          const definition = catalog.combos[index], c = combo(definition.id);
          c.fielded = c[stage] = c[category] = 1; ++c.battles;
          const threshold = definition.thresholds[progress.activeThresholdIndex];
          c.max_threshold = Math.max(c.max_threshold, threshold); c.max_count = Math.max(c.max_count, progress.effectiveCount);
          c.thresholds[threshold] = (c.thresholds[threshold] ?? 0) + 1;
          c.first_fight ??= previous.fight + 1;
          const equipment = progress.contributions.some(x => !x.naturalMember);
          if (equipment) ++c.equipment_battles;
          active[definition.id] = {threshold, count: progress.effectiveCount, equipment};
        });
        battles.push({sequence: i + 1, fight: previous.fight + 1, stage, challenge, roles: deployed, combos: active,
          outcome: current.last_outcome, pieces: pieces.map(p => ({id: p.instanceId, role_id: p.roleId, star: p.star, weapon: p.weaponInstanceId, armor: p.armorInstanceId}))});
      }
      mark(current);
      if (kind === 'refresh_shop' || (kind === 'start_battle' && !isDeepStrictEqual(current.shop, previous.shop))) countShop(current);
      previous = current;
    }
    const terminal = await next();
    ensure(terminal.validation?.valid && terminal.validation.sequence === decisions.length, `缺少完整驗證成功記錄：${entry.file}`);
    ensure((await iterator.next()).done, '決策快照尾端有額外資料');
    ensure(isDeepStrictEqual(previous.roster, checkpoint.state.roster), `最終棋子快照不符：${entry.file}`);
    for (const p of Object.values(previous.roster)) { role(p.roleId).final_owned = 1; if (p.deployed) role(p.roleId).final = 1; }
    previous.combos.forEach((p, i) => {
      if (!p.active) return;
      const definition = catalog.combos[i], c = combo(definition.id);
      c.final = 1; c.final_threshold = definition.thresholds[p.activeThresholdIndex]; c.final_count = p.effectiveCount;
    });
    for (const r of Object.values(roles)) {
      ensure(r.final <= r.final_owned && r.final_owned <= r.owned, '最終持有統計不一致');
      ensure(r.fielded <= r.enlisted && r.enlisted <= r.owned && r.purchased <= r.owned, '使用歷程統計不一致');
    }
    ensure(sum(Object.values(roles).map(r => r.buys)) === purchaseCount, '採購統計不一致');
    return {catalog, run: {index: entry.index, name: path.parse(entry.file).name, file: entry.file, sha256: entry.sha256,
      seed: header.root_seed, talent: header.talent, decisions: decisions.length, campaign_complete: checkpoint.state.campaignComplete,
      challenges: checkpoint.state.completedChallengeNames.length, main_battles: mainBattles, challenge_battles: challengeBattles,
      roles, combos, battles}};
  } finally { await iterator.return(); }
}

export function aggregateUsageRuns(runs, catalog) {
  ensure(runs.length > 0, '沒有可統計的有效存檔');
  const families = new Map();
  for (const r of runs) families.set(r.seed, (families.get(r.seed) ?? 0) + 1);
  function collect(definition, kind, flags) {
    const id = definition.id, row = {...definition};
    for (const flag of flags) {
      const count = sum(runs.map(r => r[kind][id]?.[flag] ?? 0));
      row[flag + '_runs'] = count; row[flag + '_pct'] = round(100 * count / runs.length);
      row[flag + '_seed_weighted_pct'] = round(100 * sum(runs.map(r => (r[kind][id]?.[flag] ?? 0) / families.get(r.seed))) / families.size);
    }
    row.battles = sum(runs.map(r => r[kind][id]?.battles ?? 0));
    row.fielded_seed_count = new Set(runs.filter(r => r[kind][id]?.fielded).map(r => r.seed)).size;
    return row;
  }
  const roles = catalog.roles.filter(r => r.in_pool).map(definition => {
    const r = collect(definition, 'roles', roleFlags), id = definition.id;
    for (const key of roleCounters) r[key] = sum(runs.map(run => run.roles[id]?.[key] ?? 0));
    r.history_only_runs = runs.filter(run => run.roles[id]?.fielded && !run.roles[id]?.final).length;
    const retained = runs.filter(run => run.roles[id]?.fielded && run.roles[id]?.final).length;
    r.final_retention_pct = r.fielded_runs ? round(100 * retained / r.fielded_runs) : null;
    r.natural_combos = catalog.combos.filter(c => c.members.includes(id) && !c.npc_only).map(c => c.name);
    return r;
  });
  for (const r of roles) r.cost_fielded_rank = 1 + roles.filter(x => x.cost === r.cost && x.fielded_runs > r.fielded_runs).length;
  const combos = catalog.combos.filter(c => !c.npc_only).map(definition => {
    const r = collect(definition, 'combos', comboFlags), id = definition.id;
    r.max_threshold = Math.max(...runs.map(run => run.combos[id]?.max_threshold ?? 0));
    r.equipment_runs = runs.filter(run => run.combos[id]?.equipment_battles > 0).length;
    r.threshold_runs = Object.fromEntries(definition.thresholds.map(t => [t, runs.filter(run => (run.combos[id]?.max_threshold ?? 0) >= t).length]));
    r.final_threshold_runs = Object.fromEntries(definition.thresholds.map(t => [t, runs.filter(run => (run.combos[id]?.final_threshold ?? 0) >= t).length]));
    return r;
  });
  return {n: runs.length, seeds: families.size, roles, combos};
}

export function buildUsageReport(runs, catalog, metadata = {}, rejected = []) {
  const data = aggregateUsageRuns(runs, catalog);
  data.format = 'kys_chess_usage'; data.format_version = 1;
  data.by_talent = Object.fromEntries([...new Set(runs.map(r => r.talent))].sort().map(t => [t, aggregateUsageRuns(runs.filter(r => r.talent === t), catalog)]));
  data.metadata = {...metadata, engine_version: catalog.engine_version, content_fingerprint: catalog.content_fingerprint,
    total_decisions: sum(runs.map(r => r.decisions)), total_purchases: sum(Object.values(data.roles).map(r => r.buys)),
    total_battles: sum(runs.map(r => r.battles.length)), campaign_battles: sum(runs.map(r => r.main_battles)), challenge_battles: sum(runs.map(r => r.challenge_battles)),
    early: '主線第 1–8 輪', mid: '主線第 9–16 輪', late: '主線第 17 輪起',
    seed_weighting: '每個種子家族總權重為 1，由同種子的上傳變體平分；種子不代表玩家身分。'};
  data.runs = runs; data.catalog = catalog; data.rejected = rejected;
  return data;
}

export async function analyzeManifest(manifestPath, directoryOverride) {
  const manifest = readJson(manifestPath);
  ensure(manifest.format === 'kys_chess_usage_traces' && manifest.format_version === 1, '不支援的決策快照清單格式');
  const directory = directoryOverride ?? manifest.source_directory;
  let catalog;
  const runs = [], rejected = [];
  for (const entry of manifest.entries) {
    ensure(path.basename(entry.file) === entry.file, '存檔必須是檔名');
    ensure(sha256(fs.readFileSync(path.join(directory, entry.file))) === entry.sha256, `存檔 SHA-256 不符：${entry.file}`);
    if (!entry.valid || !entry.unchanged) { rejected.push(entry); continue; }
    const result = await analyzeRun(directory, entry, path.dirname(manifestPath), catalog);
    catalog = result.catalog; runs.push(result.run);
  }
  return buildUsageReport(runs, catalog, {source_directory: path.resolve(directory), trace_manifest: path.resolve(manifestPath),
    engine_sha256: manifest.engine_sha256, engine_path: manifest.engine_path, data_root: manifest.data_root,
    config_root: manifest.config_root, source_commit: manifest.source_commit, migration: manifest.migration}, rejected);
}

export function selectUsageForTournament(usage, submissions) {
  ensure(usage.format === 'kys_chess_usage' && usage.format_version === 1, '不支援的使用率報告格式');
  const selected = submissions.map(s => {
    const run = usage.runs.find(r => r.file === s.source && r.sha256 === s.payload_sha256);
    ensure(run, `賽事存檔沒有相符的使用歷程：${s.source}`);
    return run;
  });
  return buildUsageReport(selected, usage.catalog, usage.metadata);
}

export function writeUsageFiles(outputPath, data) {
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, JSON.stringify(data, null, 2) + '\n');
  const quote = value => '"' + String(value ?? '').replaceAll('"', '""') + '"';
  function csv(name, rows) {
    const columns = Object.keys(rows[0]);
    const cell = value => quote(value && typeof value === 'object' ? JSON.stringify(value) : value);
    const text = [columns.map(quote).join(','), ...rows.map(r => columns.map(k => cell(r[k])).join(','))].join('\r\n');
    fs.writeFileSync(path.join(path.dirname(outputPath), name), '\uFEFF' + text + '\r\n');
  }
  csv('pieces.csv', data.roles); csv('synergies.csv', data.combos);
  csv('runs.csv', data.runs.map(r => Object.fromEntries(['name', 'seed', 'talent', 'decisions', 'main_battles', 'challenge_battles', 'challenges'].map(k => [k, r[k]]))));
}
