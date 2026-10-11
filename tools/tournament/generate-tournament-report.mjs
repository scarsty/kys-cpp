import fs from "node:fs";
import { createHash } from "node:crypto";
import path from "node:path";
import { fileURLToPath } from "node:url";

const [sourcePath, outputPath, saveDirectory, cliPath = "kys_chess_cli.exe", dataRootArg, configRootArg] = process.argv.slice(2);
if (!sourcePath || !outputPath || !saveDirectory) {
  throw new Error("Usage: node generate-tournament-report.mjs <results.json> <report.html> <save-directory> [cli-path] [data-root] [config-root]");
}

const source = JSON.parse(fs.readFileSync(sourcePath, "utf8"));
if (source.format !== "full_round_robin_mirrored") {
  throw new Error(`Unsupported tournament format: ${source.format}`);
}
const metadata = source.run_metadata ?? {};
const dataRoot = dataRootArg || metadata.data_root;
if (!dataRoot) throw new Error("Provide data-root so replay commands use the tournament's game data.");
const configRoot = configRootArg || metadata.config_root || path.join(dataRoot, "config");
const talents = new Map([
  ["divine_arms", "神兵"], ["backbone", "中堅"],
  ["late_bloomer", "晚成"], ["gambler", "賭徒"], ["alliance", "合縱"],
]);
const headers = [];
const submissions = source.submissions.map(entry => {
  if (path.basename(entry.source) !== entry.source) throw new Error(`Expected a save filename: ${entry.source}`);
  const bytes = fs.readFileSync(path.join(saveDirectory, entry.source));
  const hash = createHash("sha256").update(bytes).digest("hex");
  if (hash !== entry.payload_sha256) throw new Error(`Save SHA256 differs from tournament input: ${entry.source}`);
  const payload = JSON.parse(bytes.toString("utf8").replace(/^\uFEFF/, ""));
  const header = (payload.checkpoint ?? payload).replay.header;
  const talent = talents.get(header.talent);
  if (!talent) throw new Error(`Unknown talent in ${entry.source}: ${header.talent}`);
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
const serialized = JSON.stringify(reportData)
  .replaceAll("<", "\\u003c")
  .replaceAll("\u2028", "\\u2028")
  .replaceAll("\u2029", "\\u2029");
const html = template.replace("__TOURNAMENT_DATA__", serialized);
if (html === template) {
  throw new Error("Tournament data placeholder was not found in the template");
}
fs.writeFileSync(outputPath, html, "utf8");
console.log(outputPath);
