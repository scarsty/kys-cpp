import fs from 'node:fs';
import path from 'node:path';
import {readJson} from './report-common.mjs';
import {standaloneUsageHtml} from './usage-report.mjs';

const [source, output] = process.argv.slice(2);
if (!source || !output) throw new Error('用法：node generate-usage-report.mjs <usage.json> <report.html>');
const data = readJson(source);
if (data.format !== 'kys_chess_usage' || data.format_version !== 1) throw new Error('不支援的使用率報告格式');
fs.mkdirSync(path.dirname(output), {recursive:true});
fs.writeFileSync(output, standaloneUsageHtml(data));
console.log(output);
