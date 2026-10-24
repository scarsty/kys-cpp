// 此函式同時嵌入獨立使用率報告與賽事報告；不依賴外部資源或全域事件函式。
export function mountChessUsage(root, D, options = {}) {
  const view = root.attachShadow({mode: 'open'});
  view.innerHTML = `<style>
  :host{display:block;--bg:#f4f2ed;--card:#fffefa;--ink:#202d31;--muted:#647276;--line:#dbdfd9;--green:#157d70;--gold:#bd7c2f;--soft:#e9f1eb;--red:#ab4a3e;color:var(--ink);background:var(--bg);font:15px/1.65 system-ui,"Microsoft JhengHei",sans-serif}*{box-sizing:border-box}[hidden]{display:none!important}header{padding:34px 26px;background:#203b3c;color:#fff;border-bottom:5px solid #b8955d}.eyebrow{font-size:12px;letter-spacing:2px;color:#b8d2cd}h1{font-size:clamp(26px,3.2vw,40px);line-height:1.2;margin:10px 0}header p{color:#cbdad7;max-width:850px;margin:12px 0 0}main{max-width:1390px;margin:auto;padding:22px 25px 45px}nav{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:18px}button,select,input{font:inherit;border:1px solid var(--line);background:var(--card);border-radius:7px;padding:8px 12px;color:inherit}button{cursor:pointer}button:hover{border-color:var(--green)}:focus-visible{outline:3px solid var(--green);outline-offset:2px}nav button.active{background:var(--ink);color:var(--card);border-color:var(--ink)}.filters{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-bottom:20px}.filters label{color:var(--muted);font-size:13px}select,input{margin-left:5px}.cards{display:grid;grid-template-columns:repeat(4,1fr);gap:14px}.card,.panel{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:20px}.card small{color:var(--muted)}.card strong{display:block;font-size:32px;line-height:1.3}.grid{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin-top:18px}h2{font-size:21px;line-height:1.35;margin:0 0 12px}h3{font-size:16px;margin:18px 0 9px}.note{font-size:13px;color:var(--muted)}.signal{padding:12px 0;border-bottom:1px solid var(--line)}.signal b{color:var(--red)}.chip{display:inline-block;font-size:12px;padding:2px 8px;background:var(--soft);border-radius:5px;margin:2px}.tablewrap{overflow:auto;max-height:71vh}table{border-collapse:collapse;width:100%;white-space:nowrap;font-size:14px}th{text-align:right;font-size:12px;color:var(--muted);font-weight:600;background:var(--card);position:sticky;top:0;z-index:1;border-bottom:2px solid var(--line);padding:12px 10px}th button{border:0;background:none;padding:0;font-size:inherit;font-weight:inherit}td{text-align:right;padding:10px;border-bottom:1px solid var(--line);font-variant-numeric:tabular-nums}td:first-child,th:first-child{text-align:left}tbody tr:hover{background:var(--soft)}.pick{cursor:pointer}td.name{font-weight:600;color:var(--green)}td.name button{background:none;border:0;color:inherit;font-weight:inherit;padding:0}.rare{color:var(--red);font-weight:600}.barrow{display:grid;grid-template-columns:100px 1fr 85px;align-items:center;gap:10px;margin:12px 0;font-size:13px}.bars{display:grid;gap:3px}.bar{height:8px;border-radius:3px;background:var(--green)}.bar.final{background:var(--gold)}.legend{display:flex;gap:18px;font-size:12px;color:var(--muted)}.dot{display:inline-block;width:10px;height:10px;margin-right:5px;background:var(--green);border-radius:2px}.dot.final{background:var(--gold)}.detail{margin-top:20px;border-top:3px solid var(--green)}.close{float:right}.tier{font-size:12px}.tier span{display:inline-block;padding:2px 6px;border:1px solid var(--line);border-radius:4px;margin:2px}.summaryline{display:flex;justify-content:space-between;gap:20px;flex-wrap:wrap}.fact{background:var(--soft);padding:14px 16px;border-radius:7px;margin:12px 0}.method{max-width:1050px}.method p{margin:11px 0}code{overflow-wrap:anywhere;white-space:normal}footer{font-size:12px;color:var(--muted);margin-top:25px}.detail td.wrap{white-space:normal;text-align:left;min-width:240px}@media(max-width:850px){.cards{grid-template-columns:1fr 1fr}.grid{grid-template-columns:1fr}main{padding:18px 13px}header{padding:26px 18px}.panel{padding:15px}.barrow{grid-template-columns:85px 1fr 72px}.filters{gap:8px}}@media(prefers-color-scheme:dark){:host{--bg:#172020;--card:#202c2b;--ink:#e1e9e3;--muted:#a7b6b0;--line:#3c4d46;--green:#6ac6ad;--gold:#d6ae68;--soft:#2b3c35;--red:#ed9e89}}
  </style><header ${options.integrated ? 'hidden' : ''}><div class="eyebrow">金群自走棋 · 平衡研究</div><h1>通關路上的棋子與羈絆</h1><p>從首次購買、曾經入陣，到各階段實際參戰與最終陣容，查看被忽略的過渡角色與低使用組合。</p></header><main><nav id="nav" aria-label="使用率報告分頁" ${options.integrated ? 'hidden' : ''}></nav><div class="filters"><label>棋手天賦<select id="talent"></select></label><span class="note" id="cohort" aria-live="polite"></span></div><div id="content"></div><div id="detail" aria-live="polite"></div><footer>離線報告 · 原生完整重播驗證 · 以已驗證決策重建使用歷程</footer></main>`;
  const T = options.talentNames;
  const E = s => String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  let page = 'overview', talent = 'all', query = '', cost = 'all', subset = 'all', sort = 'fielded_runs', ascending = false;
  const $ = id => view.getElementById(id), group = () => talent === 'all' ? D : D.by_talent[talent];
  const runs = () => D.runs.filter(r => talent === 'all' || r.talent === talent), num = n => n.toLocaleString('zh-TW');
  const roleNames = new Map(D.catalog.roles.map(r => [r.id, r.name]));
  const comboNames = new Map(D.catalog.combos.map(r => [r.id, r.name]));
  const pages = [['overview','使用總覽'],['pieces','棋子使用'],['combos','羈絆使用'],['saves','存檔歷程'],['method','統計摘要']];
  $('talent').innerHTML = `<option value="all">全部天賦 · ${D.n} 份</option>` + Object.entries(D.by_talent).map(([k,g]) => `<option value="${E(k)}">${E(T[k] ?? k)} · ${g.n} 份</option>`).join('');
  $('talent').onchange = e => { talent = e.target.value; render(); };
  view.addEventListener('click', e => {
    const target = e.target.closest('[data-page],[data-piece],[data-combo],[data-save],[data-close],[data-sort]');
    if (!target) return;
    if (target.hasAttribute('data-page')) showPage(target.dataset.page);
    else if (target.hasAttribute('data-piece')) pieceDetail(+target.dataset.piece);
    else if (target.hasAttribute('data-combo')) comboDetail(+target.dataset.combo);
    else if (target.hasAttribute('data-save')) saveDetail(+target.dataset.save);
    else if (target.hasAttribute('data-close')) $('detail').innerHTML = '';
    else { ascending = sort === target.dataset.sort ? !ascending : false; sort = target.dataset.sort; renderRows(); }
  });
  function showPage(next) { page = next; query = ''; subset = 'all'; sort = 'fielded_runs'; ascending = false; render(); }
  function render() {
    $('nav').innerHTML = pages.map(([id,label]) => `<button data-page="${id}" class="${page === id ? 'active' : ''}" aria-current="${page === id ? 'page' : 'false'}">${label}</button>`).join('');
    $('detail').innerHTML = '';
    const g = group(), cleared = runs().filter(r => r.campaign_complete).length;
    $('cohort').textContent = `${g.n} 份存檔 · ${g.seeds} 個種子家族 · ${cleared} 份完成主線`;
    if (page === 'overview') overview(); else if (page === 'pieces' || page === 'combos') listing(); else if (page === 'saves') saveList(); else method();
  }
  const cards = items => `<div class="cards">${items.map(([value,label,note]) => `<div class="card"><small>${label}</small><strong>${value}</strong><span class="note">${note}</span></div>`).join('')}</div>`;
  const legend = (a='曾經參戰', b='最終入陣') => `<div class="legend"><span><i class="dot"></i>${a}</span><span><i class="dot final"></i>${b}</span></div>`;
  const bars = rows => rows.map(r => `<div class="barrow"><b>${E(r.name)}</b><div class="bars"><div class="bar" style="width:${100*r.fielded_runs/group().n}%"></div><div class="bar final" style="width:${100*r.final_runs/group().n}%"></div></div><span>${r.fielded_runs} → ${r.final_runs}</span></div>`).join('');
  const link = (kind,id,name) => `<button data-${kind}="${id}">${E(name)}</button>`;
  function overview() {
    const g = group(), never = g.roles.filter(r => !r.purchased_runs), unfielded = g.roles.filter(r => !r.fielded_runs), unused = g.combos.filter(r => !r.fielded_runs);
    const gaps = g.roles.filter(r => !r.purchased_runs || !r.fielded_runs);
    const hidden = [...g.roles].sort((a,b) => b.history_only_runs-a.history_only_runs).slice(0,8);
    const low = g.roles.filter(r => r.fielded_runs <= 2).sort((a,b) => a.fielded_runs-b.fielded_runs || a.purchased_runs-b.purchased_runs);
    $('content').innerHTML = cards([[g.n,'通過完整驗證',`${g.seeds} 個種子家族；上傳數不等於玩家數`],[`${g.roles.length-unfielded.length}/${g.roles.length}`,'曾經參戰的棋子',`${unfielded.length} 種未參戰；${never.length} 種未購買`],[`${g.combos.length-unused.length}/${g.combos.length}`,'戰鬥中啟動的羈絆',`${unused.length} 種未在戰鬥中啟動`],[num(runs().reduce((s,r) => s+r.battles.length,0)),'重建戰鬥場次','包含主線與遠征的重複嘗試']]) +
      `<div class="grid"><section class="panel"><h2>優先檢查的使用缺口</h2>
      <h3>未購買／未參戰棋子 · ${gaps.length}</h3><div class="tablewrap"><table><thead><tr><th>棋子</th><th>費用</th><th>購買</th><th>入陣</th><th>參戰</th><th>刷出</th><th>禁用</th></tr></thead><tbody>${gaps.map(r => `<tr data-piece="${r.id}" class="pick"><td class="name">${link('piece',r.id,r.name)}</td><td>${r.cost}</td><td>${r.purchased_runs}</td><td>${r.enlisted_runs}</td><td>${r.fielded_runs}</td><td>${r.offered_runs}</td><td>${r.banned_runs}</td></tr>`).join('') || '<tr><td colspan="7">—</td></tr>'}</tbody></table></div>
      <h3>未參戰羈絆 · ${unused.length}</h3><div class="tablewrap"><table><thead><tr><th>羈絆</th><th>管理啟動</th><th>參戰</th><th>最終</th></tr></thead><tbody>${unused.map(r => `<tr data-combo="${r.id}" class="pick"><td class="name">${link('combo',r.id,r.name)}</td><td>${r.ever_active_runs}</td><td>${r.fielded_runs}</td><td>${r.final_runs}</td></tr>`).join('') || '<tr><td colspan="4">—</td></tr>'}</tbody></table></div>
      </section><section class="panel"><h2>最終陣容隱藏了什麼</h2>${legend()}${bars(hidden)}<p class="note">數字是使用過該棋子的存檔數；每份存檔至多計一次。</p></section></div><div class="grid"><section class="panel"><h2>最常啟動的戰鬥羈絆</h2>${legend('戰鬥中曾啟動','最終啟動')}${bars([...g.combos].sort((a,b) => b.fielded_runs-a.fielded_runs).slice(0,8))}</section><section class="panel"><h2>少見的參戰棋子</h2><p class="note">本群組至多 2 份存檔使用。建議與同費棋子比較，並考慮解鎖時機及禁用習慣。</p><div class="tablewrap"><table><thead><tr><th>棋子</th><th>費用</th><th>購買</th><th>參戰</th><th>最終</th><th>禁用</th></tr></thead><tbody>${low.map(r => `<tr data-piece="${r.id}" class="pick"><td class="name">${link('piece',r.id,r.name)}</td><td>${r.cost}</td><td>${r.purchased_runs}</td><td>${r.fielded_runs}</td><td>${r.final_runs}</td><td>${r.banned_runs}</td></tr>`).join('')}</tbody></table></div></section></div>`;
  }
  function listing() {
    const combo = page === 'combos';
    $('content').innerHTML = `<section class="panel"><h2>${combo ? '羈絆啟動歷程' : '整段流程的棋子使用'}</h2><p class="note">${combo ? '依原生規則計算裝備附加成員、星級點數與合縱門檻調整。階數顯示配置中的名義門檻。「裝備參與」表示啟動時有裝備附加成員。' : '購買＝曾在商店買入；入陣＝曾放入陣容，包含短暫擺放；參戰＝實際進入戰鬥。前／中／後期為主線第 1–8／9–16／17 輪起，遠征另計。最終＝匯出時入陣；種子加權讓每個種子家族總權重相同。'} 點選名稱可查看存檔證據。</p><div class="filters"><input id="search" type="search" aria-label="搜尋名稱" placeholder="搜尋${combo ? '羈絆' : '棋子或羈絆'}" value="${E(query)}">${combo ? '' : `<label>費用<select id="cost"><option value="all">全部費用</option>${[...new Set(group().roles.map(r => r.cost))].sort((a,b)=>a-b).map(c => `<option value="${c}" ${cost == c ? 'selected' : ''}>${c} 費</option>`).join('')}</select></label>`}<label>顯示<select id="subset"><option value="all">全部</option><option value="unused" ${subset === 'unused' ? 'selected' : ''}>從未${combo ? '啟動' : '參戰'}</option><option value="rare" ${subset === 'rare' ? 'selected' : ''}>至多 2 份使用</option><option value="absent" ${subset === 'absent' ? 'selected' : ''}>最終未出現</option></select></label></div><div class="summaryline"><span class="note" id="rowscount"></span><span class="note">點選欄名排序</span></div><div class="tablewrap"><table><thead id="thead"></thead><tbody id="tbody"></tbody></table></div></section>`;
    $('search').oninput = e => { query = e.target.value; renderRows(); };
    if ($('cost')) $('cost').onchange = e => { cost = e.target.value; renderRows(); };
    $('subset').onchange = e => { subset = e.target.value; renderRows(); }; renderRows();
  }
  function renderRows() {
    const combo = page === 'combos', g = group();
    const rows = (combo ? g.combos : g.roles).filter(r => (!query || [r.name,...(r.natural_combos || [])].join(' ').includes(query)) && (combo || cost === 'all' || r.cost == cost) && (subset === 'all' || subset === 'unused' && !r.fielded_runs || subset === 'rare' && r.fielded_runs <= 2 || subset === 'absent' && !r.final_runs));
    rows.sort((a,b) => (typeof a[sort] === 'string' ? a[sort].localeCompare(b[sort],'zh-Hant') : a[sort]-b[sort]) * (ascending ? 1 : -1) || a.name.localeCompare(b.name,'zh-Hant'));
    const cols = combo ? [['name','羈絆'],['fielded_runs','參戰存檔'],['early_runs','前期'],['mid_runs','中期'],['late_runs','後期'],['expedition_runs','遠征'],['final_runs','最終'],['fielded_seed_weighted_pct','種子加權 %'],['tiers','達成階數 ≥'],['equipment_runs','裝備參與']] : [['name','棋子'],['cost','費用'],['purchased_runs','購買'],['enlisted_runs','入陣'],['fielded_runs','參戰'],['early_runs','前期'],['mid_runs','中期'],['late_runs','後期'],['expedition_runs','遠征'],['final_runs','最終'],['banned_runs','禁用'],['fielded_seed_weighted_pct','種子加權 %'],['buys','購買張數']];
    $('thead').innerHTML = '<tr>' + cols.map(([key,label]) => `<th ${key !== 'tiers' ? `aria-sort="${key === sort ? ascending ? 'ascending' : 'descending' : 'none'}"` : ''}>${key === 'tiers' ? label : `<button data-sort="${key}">${label}${key === sort ? ascending ? ' ↑' : ' ↓' : ''}</button>`}</th>`).join('') + '</tr>';
    $('tbody').innerHTML = rows.map(r => `<tr class="pick" data-${combo ? 'combo' : 'piece'}="${r.id}">` + cols.map(([key]) => `<td class="${key === 'name' ? 'name' : key === 'fielded_runs' && r[key] <= 2 ? 'rare' : ''}" ${key.endsWith('_runs') ? `title="${(100*r[key]/g.n).toFixed(1)}%；分母 ${g.n} 份"` : ''}>${key === 'tiers' ? `<div class="tier">${Object.entries(r.threshold_runs).map(([t,n]) => `<span>${t}：${n} 份</span>`).join('')}</div>` : key === 'name' ? link(combo ? 'combo' : 'piece',r.id,r.name) : key === 'fielded_seed_weighted_pct' ? r[key].toFixed(1)+'%' : r[key]}</td>`).join('') + '</tr>').join('');
    $('rowscount').textContent = `${rows.length} 種${combo ? '羈絆' : '棋子'} · 分母 ${g.n} 份存檔`;
  }
  function showDetail(html) { $('detail').innerHTML = `<section class="panel detail"><button class="close" data-close>關閉</button>${html}</section>`; $('detail').scrollIntoView({behavior:'smooth',block:'start'}); }
  function pieceDetail(id) {
    const r = group().roles.find(x => x.id === id), evidence = runs().filter(x => x.roles[id]?.owned || x.roles[id]?.banned);
    showDetail(`<h2>${E(r.name)} <span class="chip">${r.cost} 費</span></h2><p>${r.natural_combos.map(x => `<span class="chip">${E(x)}</span>`).join('')}</p><p>${r.purchased_runs}/${group().n} 份曾購買、${r.enlisted_runs} 份曾入陣、${r.fielded_runs} 份曾參戰、${r.final_runs} 份保留於最終陣容。${r.offered_runs} 份曾刷出，${r.banned_runs} 份曾禁用。${r.history_only_runs} 份曾使用它戰鬥，之後從最終陣容移除。</p><div class="tablewrap"><table><thead><tr><th>存檔</th><th>購買張數</th><th>入陣</th><th>戰鬥</th><th>前期</th><th>最終</th><th>禁用</th></tr></thead><tbody>${evidence.map(run => { const x = run.roles[id] || {}; return `<tr data-save="${run.index}" class="pick"><td class="name">${link('save',run.index,run.name)}</td><td>${x.buys || 0}</td><td>${x.enlisted ? '✓' : '—'}</td><td>${x.battles || 0}</td><td>${x.early ? '✓' : '—'}</td><td>${x.final ? '✓' : '—'}</td><td>${x.banned ? '✓' : '—'}</td></tr>`; }).join('')}</tbody></table></div><p class="note">獎勵也可能帶來未經商店購買的棋子。禁用為累積紀錄，可能代表玩家移除不屬於所選流派的棋子。</p>`);
  }
  function comboDetail(id) {
    const r = group().combos.find(x => x.id === id), evidence = runs().filter(x => x.combos[id]?.ever_active);
    showDetail(`<h2>${E(r.name)}</h2><p>原生成員：${r.members.map(id => E(roleNames.get(id) ?? id)).join('、')}</p><p>${r.fielded_runs}/${group().n} 份曾在戰鬥啟動；最終 ${r.final_runs} 份啟動。管理局面曾啟動 ${r.ever_active_runs} 份。最高名義戰鬥階數：${r.max_threshold || '未達成'}。</p><p>${r.thresholds.map(t => `<span class="chip">${t} 階以上：參戰 ${r.threshold_runs[t]} · 最終 ${r.final_threshold_runs[t]}</span>`).join('')}</p><div class="tablewrap"><table><thead><tr><th>存檔</th><th>啟動戰鬥數</th><th>前期</th><th>最高階數</th><th>最終階數</th><th>裝備參與戰鬥</th></tr></thead><tbody>${evidence.map(run => { const c = run.combos[id]; return `<tr data-save="${run.index}" class="pick"><td class="name">${link('save',run.index,run.name)}</td><td>${c.battles}</td><td>${c.early ? '✓' : '—'}</td><td>${c.max_threshold || '—'}</td><td>${c.final_threshold || '—'}</td><td>${c.equipment_battles}</td></tr>`; }).join('')}</tbody></table></div>`);
  }
  function saveList() {
    $('content').innerHTML = `<section class="panel"><h2>已驗證存檔的使用歷程</h2><p class="note">點選存檔可檢查最終核心與每場戰鬥的陣容。相同種子視為相關家族，並不代表同一位玩家。</p><div class="tablewrap"><table><thead><tr><th>存檔</th><th>天賦</th><th>操作</th><th>主線戰鬥</th><th>遠征戰鬥</th><th>已完成遠征</th></tr></thead><tbody>${runs().map(r => `<tr data-save="${r.index}" class="pick"><td class="name">${link('save',r.index,r.name)}</td><td>${E(T[r.talent] ?? r.talent)}</td><td>${r.decisions}</td><td>${r.main_battles}</td><td>${r.challenge_battles}</td><td>${r.challenges}</td></tr>`).join('')}</tbody></table></div></section>`;
  }
  function saveDetail(index) {
    const r = D.runs.find(x => x.index === index), final = Object.entries(r.roles).filter(([,p]) => p.final).map(([id]) => E(roleNames.get(+id))).join('、');
    showDetail(`<h2>${E(r.name)}</h2><p>${E(T[r.talent] ?? r.talent)} · 種子 <code>${E(r.seed)}</code> · ${r.decisions} 次已驗證操作</p><p><b>最終入陣：</b>${final || '無'}</p><div class="tablewrap"><table><thead><tr><th>戰鬥／操作序號</th><th>關卡</th><th>入陣棋子與星級</th><th>啟動羈絆與階數</th></tr></thead><tbody>${r.battles.map((b,i) => `<tr><td>${i+1} / ${b.sequence}</td><td>${E(b.challenge || '主線第 '+b.fight+' 輪')}</td><td class="wrap">${b.pieces.map(p => `${E(roleNames.get(p.role_id))} ${p.star}★`).join(' · ')}</td><td class="wrap">${Object.entries(b.combos).map(([id,c]) => `${E(comboNames.get(+id))} ${c.threshold}`).join(' · ') || '無'}</td></tr>`).join('')}</tbody></table></div><p class="note">存檔 SHA-256：<code>${E(r.sha256)}</code></p>`);
  }
  function method() {
    const g = group(), selected = runs(), battles = selected.flatMap(r => r.battles);
    const total = key => selected.reduce((sum, r) => sum + r[key], 0);
    const table = (columns, rows) => `<div class="tablewrap"><table><thead><tr>${columns.map(c => `<th>${E(c)}</th>`).join('')}</tr></thead><tbody>${rows.map(row => `<tr>${row.map(c => `<td>${E(c)}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
    const coverage = (rows, key) => `${rows.filter(r => r[key] > 0).length} / ${rows.length}`;
    const stages = [['early','前期 · 1–8 輪'],['mid','中期 · 9–16 輪'],['late','後期 · 17 輪起'],['expedition','遠征']];
    $('content').innerHTML = cards([[g.n,'存檔',`${g.seeds} 個種子家族`],[num(total('decisions')),'操作次數',''],[num(g.roles.reduce((sum,r) => sum+r.buys,0)),'購買張數',''],[num(battles.length),'戰鬥場次','']]) +
      `<div class="grid"><section class="panel"><h2>戰鬥場次</h2>${table(['階段','場次'],stages.map(([key,label]) => [label,num(battles.filter(b => b.stage === key).length)]).concat([['主線合計',num(total('main_battles'))],['全部合計',num(battles.length)]]))}</section>
      <section class="panel"><h2>使用覆蓋</h2>${table(['項目','棋子','羈絆'],[
        ['購買',coverage(g.roles,'purchased_runs'),'—'],['持有',coverage(g.roles,'owned_runs'),'—'],
        ['入陣／管理啟動',coverage(g.roles,'enlisted_runs'),coverage(g.combos,'ever_active_runs')],
        ['參戰',coverage(g.roles,'fielded_runs'),coverage(g.combos,'fielded_runs')],
        ['最終',coverage(g.roles,'final_runs'),coverage(g.combos,'final_runs')],
      ])}</section><section class="panel"><h2>天賦分布</h2>${table(['天賦','存檔','比例'],Object.entries(D.by_talent).filter(([key]) => talent === 'all' || key === talent).map(([key,data]) => [T[key] ?? key,data.n,(100*data.n/g.n).toFixed(1)+'%']))}</section>
      <section class="panel"><h2>通關與驗證</h2>${table(['項目','存檔'],[
        ['主線通關',selected.filter(r => r.campaign_complete).length],
        ...[...new Set(selected.map(r => r.challenges))].sort((a,b) => b-a).map(n => [`完成遠征 · ${n} 項`,selected.filter(r => r.challenges === n).length]),
        ['驗證通過',g.n],['全體未通過',D.rejected.length],
      ])}</section></div>`;
  }
  render();
  return {showPage};
}
