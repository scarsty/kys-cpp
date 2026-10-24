import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {parseArgs} from 'node:util';
import {readJson, readSave, scriptJson, talentNames} from './report-common.mjs';
import {selectUsageForTournament} from './usage-analysis.mjs';
import {usageScript, usageTabs} from './usage-report.mjs';

const {values, positionals} = parseArgs({allowPositionals:true, options:{usage:{type:'string'}}});
const [sourcePath, outputPath, saveDirectory, cliPath = "kys_chess_cli.exe", dataRootArg, configRootArg] = positionals;
if (!sourcePath || !outputPath || !saveDirectory || positionals.length > 6) {
  throw new Error("用法：node generate-tournament-report.mjs <results.json> <report.html> <存檔目錄> [CLI] [資料目錄] [配置目錄] [--usage usage.json]");
}

const source = readJson(sourcePath);
if (source.format !== "full_round_robin_mirrored") {
  throw new Error(`Unsupported tournament format: ${source.format}`);
}
const metadata = source.run_metadata ?? {};
const dataRoot = dataRootArg || metadata.data_root;
if (!dataRoot) throw new Error("Provide data-root so replay commands use the tournament's game data.");
const configRoot = configRootArg || metadata.config_root || path.join(dataRoot, "config");
const headers = [];
const submissions = source.submissions.map(entry => {
  const {hash, header} = readSave(saveDirectory, entry.source, entry.payload_sha256);
  const talent = talentNames.get(header.talent);
  headers.push(header);
  return [entry.index, entry.name, entry.source, hash, talent, header.talent];
});
const difficulties = new Set(headers.map(header => header.difficulty));
const difficultyLabels = {hard: "困難模式", normal: "普通模式", easy: "簡單模式"};
const difficulty = difficulties.size === 1
  ? (difficultyLabels[[...difficulties][0]] ?? [...difficulties][0]) : "混合難度";
if (source.submissions.length < 2) throw new Error("A tournament report needs at least two entrants.");

const outcomeCode = {
  team_zero_victory: 0,
  team_one_victory: 1,
  draw: 2,
};

const reportData = {
  meta: {
    formatVersion: source.format_version,
    format: source.format,
    entrantOrder: source.entrant_order,
    sideAssignment: source.side_assignment,
    tournamentSeed: source.tournament_seed,
    saveDirectory: path.resolve(saveDirectory),
    cliPath,
    sourceJson: path.resolve(sourcePath),
    generatedAt: new Date().toISOString(),
    gameVersion: metadata.game_version || "引擎版本未記錄",
    difficulty,
    dataRoot: path.resolve(dataRoot),
    configRoot: path.resolve(configRoot),
  },
  submissions,
  seeds: source.battle_seeds.map((seed) => [
    seed.sequence_index,
    seed.battle_seed,
  ]),
  pairings: source.pairings.map((pairing) => [
    pairing.first_entrant_index,
    pairing.second_entrant_index,
    pairing.first_half_points,
    pairing.second_half_points,
  ]),
  battles: source.battles.map((battle) => [
    battle.pairing_index,
    battle.seed_sequence_index,
    battle.leg,
    battle.team_zero_entrant_index,
    battle.team_one_entrant_index,
    outcomeCode[battle.outcome],
    battle.end_frame,
    battle.digest_sha256,
    battle.engine_outcome,
  ]),
  standings: source.standings.map((standing) => [
    standing.rank,
    standing.entrant_index,
    standing.half_points,
    standing.points,
    standing.battle_wins,
    standing.battle_draws,
    standing.battle_losses,
  ]),
  champions: source.champion_entrant_indices,
};

const templatePath = fileURLToPath(new URL("./modern-tournament-template.html", import.meta.url));
const template = fs.readFileSync(templatePath, "utf8");
if (!template.includes('__TOURNAMENT_DATA__')) {
  throw new Error("Tournament data placeholder was not found in the template");
}
const usage = values.usage ? selectUsageForTournament(readJson(values.usage), source.submissions) : null;
const html = template.replace('__TOURNAMENT_DATA__', () => scriptJson(reportData))
  .replace('__REPORT_TABS__', () => usage ? usageTabs() : '')
  .replace('__USAGE_PANEL__', () => usage ? '<div id="usageReport" hidden></div>' : '')
  .replace('__USAGE_SCRIPT__', () => usage ? usageScript(usage, true) : '');
fs.mkdirSync(path.dirname(outputPath), {recursive:true});
fs.writeFileSync(outputPath, html, "utf8");
console.log(outputPath);
