import {mountChessUsage} from './usage-view.mjs';
import {scriptJson, talentNames} from './report-common.mjs';

export function usageScript(data, integrated = false) {
  const mount = `(${mountChessUsage.toString()})(document.getElementById('usageReport'),${scriptJson(data)},${scriptJson({integrated,talentNames:Object.fromEntries(talentNames)})})`;
  if (!integrated) return `<script>${mount};</script>`;
  return `<script>(()=>{const usage=${mount};const tabs=document.getElementById('reportTabs');tabs.addEventListener('click',event=>{const button=event.target.closest('[data-report-page]');if(!button)return;const page=button.dataset.reportPage;document.getElementById('tournamentPanel').hidden=page!=='tournament';document.getElementById('usageReport').hidden=page==='tournament';tabs.querySelectorAll('button').forEach(b=>{b.classList.toggle('active',b===button);b.setAttribute('aria-current',b===button?'page':'false');});if(page!=='tournament')usage.showPage(page);});document.querySelectorAll('a[href="#standings"],a[href="#matchups"]').forEach(a=>a.addEventListener('click',()=>tabs.querySelector('[data-report-page="tournament"]').click()));})();</script>`;
}

export function usageTabs() {
  return `<style>[hidden]{display:none!important}.report-tabs{display:flex;gap:6px;flex-wrap:wrap;margin:22px 0}.report-tabs button{font:inherit;padding:9px 14px;background:var(--surface,#fffefa);color:var(--ink);border:1px solid var(--line);border-radius:7px}.report-tabs button.active{background:var(--ink);color:var(--bg)}</style><nav class="report-tabs" id="reportTabs" aria-label="報告分頁">${[['tournament','賽事戰績'],['overview','使用總覽'],['pieces','棋子使用'],['combos','羈絆使用'],['saves','存檔歷程'],['method','統計摘要']].map(([id,name]) => `<button data-report-page="${id}" class="${id==='tournament'?'active':''}" aria-current="${id==='tournament'?'page':'false'}">${name}</button>`).join('')}</nav>`;
}

export function standaloneUsageHtml(data) {
  return `<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>金群自走棋 · 使用率研究</title><style>body{margin:0;background:#f4f2ed}@media(prefers-color-scheme:dark){body{background:#172020}}</style></head><body><div id="usageReport"></div>${usageScript(data)}</body></html>`;
}
