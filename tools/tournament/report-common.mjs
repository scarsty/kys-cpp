import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';

export const talentNames = new Map([
  ['divine_arms', '神兵'], ['backbone', '中堅'], ['late_bloomer', '晚成'],
  ['gambler', '賭徒'], ['alliance', '合縱'],
]);

export const readJson = file => JSON.parse(fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, ''));
export const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
export const scriptJson = value => JSON.stringify(value).replaceAll('<', '\\u003c')
  .replaceAll('\u2028', '\\u2028').replaceAll('\u2029', '\\u2029');

export function readSave(directory, filename, expectedHash) {
  if (path.basename(filename) !== filename) throw new Error(`存檔必須是檔名：${filename}`);
  const bytes = fs.readFileSync(path.join(directory, filename));
  const hash = sha256(bytes);
  if (expectedHash && hash !== expectedHash) throw new Error(`存檔 SHA-256 不符：${filename}`);
  const payload = JSON.parse(bytes.toString('utf8').replace(/^\uFEFF/, ''));
  const checkpoint = payload.checkpoint ?? payload;
  const header = checkpoint.replay.header;
  if (!talentNames.has(header.talent)) throw new Error(`未知天賦：${header.talent}`);
  return { hash, checkpoint, header };
}
