import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import zlib from 'node:zlib';
import {spawn} from 'node:child_process';
import {pipeline} from 'node:stream/promises';
import {parseArgs} from 'node:util';
import {analyzeManifest, writeUsageFiles} from './usage-analysis.mjs';
import {sha256} from './report-common.mjs';

const {values, positionals} = parseArgs({allowPositionals: true, options: {
  cli: {type: 'string'}, 'data-root': {type: 'string'}, 'config-root': {type: 'string'},
  output: {type: 'string'}, jobs: {type: 'string', default: String(Math.max(1, Math.floor(os.availableParallelism() / 2)))},
  'trace-manifest': {type: 'string'}, 'source-commit': {type: 'string'},
}});
const [directoryArg] = positionals;
if (!directoryArg || positionals.length !== 1 || !values.output) {
  throw new Error('用法：node analyze-save-usage.mjs <存檔目錄> --output <usage.json> [--cli <CLI> --data-root <資料目錄> --config-root <配置目錄> --jobs N | --trace-manifest <traces.json>]');
}
const directory = path.resolve(directoryArg), output = path.resolve(values.output);
fs.mkdirSync(path.dirname(output), {recursive: true});
if (fs.realpathSync(directory) === fs.realpathSync(path.dirname(output))) {
  throw new Error('輸出須放在原始存檔目錄之外');
}
let manifestPath = values['trace-manifest'];
if (!manifestPath) {
  if (!values.cli || !values['data-root']) throw new Error('重播需要 --cli 與相符的 --data-root');
  const jobs = Number(values.jobs);
  if (!Number.isInteger(jobs) || jobs < 1) throw new Error('--jobs 必須是正整數');
  const cli = path.resolve(values.cli), dataRoot = path.resolve(values['data-root']);
  const configRoot = path.resolve(values['config-root'] ?? path.join(dataRoot, 'config'));
  const traces = path.join(path.dirname(output), 'usage-traces');
  // 每次重播使用新目錄，防止誤把舊快照當成本次的驗證結果。
  fs.mkdirSync(path.dirname(output), {recursive: true});
  fs.mkdirSync(traces);
  const files = fs.readdirSync(directory, {withFileTypes: true}).filter(e => e.isFile() && e.name.endsWith('.json')).map(e => e.name).sort();
  if (!files.length) throw new Error('存檔目錄沒有 JSON 檔案');
  const entries = files.map((file, index) => ({index, file, sha256: sha256(fs.readFileSync(path.join(directory, file)))}));
  let cursor = 0, completed = 0;
  async function worker() {
    while (cursor < entries.length) {
      const entry = entries[cursor++], prefix = String(entry.index).padStart(4, '0');
      const trace = path.join(traces, prefix + '.jsonl'), log = path.join(traces, prefix + '.log');
      const descriptor = fs.openSync(log, 'w');
      let exitCode;
      try {
        exitCode = await new Promise((resolve, reject) => {
          const child = spawn(cli, ['verify-pvp', path.join(directory, entry.file), '--trace-output', trace,
            '--data-root', dataRoot, '--config-root', configRoot], {stdio: ['ignore', descriptor, descriptor], windowsHide: true});
          child.on('error', reject); child.on('close', resolve);
        });
      } finally { fs.closeSync(descriptor); }
      entry.exit_code = exitCode;
      entry.unchanged = sha256(fs.readFileSync(path.join(directory, entry.file))) === entry.sha256;
      entry.valid = exitCode === 0 && entry.unchanged;
      entry.log = path.relative(path.dirname(output), log);
      if (!entry.valid) entry.error = fs.readFileSync(log, 'utf8').trim();
      if (fs.existsSync(trace)) {
        await pipeline(fs.createReadStream(trace), zlib.createGzip(), fs.createWriteStream(trace + '.gz'));
        fs.unlinkSync(trace); entry.trace = path.relative(path.dirname(output), trace + '.gz');
      }
      console.log(`[${++completed}/${entries.length}] ${entry.valid ? '通過' : '拒絕'} ${entry.file}`);
    }
  }
  await Promise.all(Array.from({length: Math.min(jobs, entries.length)}, worker));
  manifestPath = path.join(path.dirname(output), 'usage-traces.json');
  fs.writeFileSync(manifestPath, JSON.stringify({format: 'kys_chess_usage_traces', format_version: 1,
    source_directory: directory, engine_path: cli, engine_sha256: sha256(fs.readFileSync(cli)),
    data_root: dataRoot, config_root: configRoot, source_commit: values['source-commit'], workers: jobs,
    generated_at: new Date().toISOString(), entries}, null, 2) + '\n');
}
const data = await analyzeManifest(path.resolve(manifestPath), directory);
writeUsageFiles(output, data);
console.log(`${output}：${data.n} 份通過，${data.rejected.length} 份拒絕，${data.metadata.total_battles} 場戰鬥`);
