import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {analyzeRun, analyzeManifest, buildUsageReport, selectUsageForTournament} from './usage-analysis.mjs';
import {sha256, scriptJson} from './report-common.mjs';
import {standaloneUsageHtml} from './usage-report.mjs';

const catalog = {format:'kys_chess_usage_trace',format_version:1,engine_version:'test-engine',content_fingerprint:'rules',
  roles:[{id:0,name:'過渡棋子',cost:1,in_pool:true},{id:1,name:'短暫入陣',cost:2,in_pool:true},{id:2,name:'最終棋子',cost:5,in_pool:true}],
  combos:[{id:8,name:'測試羈絆',members:[0],thresholds:[1,3],npc_only:false}]};
function fixture(t) {
  const parent = fileURLToPath(new URL('../../tmp/',import.meta.url));
  fs.mkdirSync(parent,{recursive:true});
  const directory = fs.mkdtempSync(path.join(parent,'usage-test-'));
  t.after(() => fs.rmSync(directory,{recursive:true}));
  const snapshots = [{sequence:0,fight:0,campaign_complete:false,level:1,roster:{},shop:[{roleId:0},{roleId:1},{roleId:2}],equipment:{},combos:[],bans:[],seen:[],last_outcome:0}];
  const decisions = [];
  function push(action, edit = () => {}) {
    const state = structuredClone(snapshots.at(-1));
    ++state.sequence; edit(state); snapshots.push(state); decisions.push({action});
  }
  const piece = (instanceId,roleId) => ({instanceId,roleId,star:1,deployed:false,weaponInstanceId:-1,armorInstanceId:-1});
  push({type:'buy_shop_slot',slot:0},s=>{s.roster[1]=piece(1,0);s.shop[0].roleId=-1;});
  push({type:'set_deployment'},s=>s.roster[1].deployed=true);
  push({type:'start_battle'},s=>++s.fight);
  push({type:'sell_chess',chess_instance_id:1},s=>delete s.roster[1]);
  push({type:'buy_shop_slot',slot:1},s=>{s.roster[2]=piece(2,1);s.shop[1].roleId=-1;});
  push({type:'set_deployment'},s=>s.roster[2].deployed=true);
  push({type:'set_deployment'},s=>s.roster[2].deployed=false);
  push({type:'buy_shop_slot',slot:2},s=>{s.roster[3]=piece(3,2);s.shop[2].roleId=-1;});
  push({type:'set_deployment'},s=>s.roster[3].deployed=true);
  push({type:'prepare_battle'},s=>s.fight=8);
  push({type:'start_battle'},s=>++s.fight);
  push({type:'prepare_battle'},s=>s.fight=16);
  push({type:'start_battle'},s=>++s.fight);
  push({type:'start_challenge',challenge_name:'測試遠征'});
  push({type:'start_battle'});
  push({type:'prepare_battle'});
  push({type:'start_battle'},s=>++s.fight);
  for (const s of snapshots) {
    const natural = !!s.roster[1]?.deployed, gear = !!s.roster[3]?.deployed;
    s.combos = [{active:natural||gear,activeThresholdIndex:gear?1:0,effectiveCount:gear?3:natural?1:0,
      contributions:natural?[{naturalMember:true}]:gear?[{naturalMember:false}]:[]}];
  }
  const checkpoint = {replay:{header:{content_fingerprint:'rules',root_seed:'family-a',talent:'alliance'},decisions},
    state:{roster:snapshots.at(-1).roster,campaignComplete:false,completedChallengeNames:[]}};
  const file = 'fixture.json', bytes = JSON.stringify({checkpoint});
  fs.writeFileSync(path.join(directory,file),bytes);
  const entry = {index:0,file,sha256:sha256(bytes),valid:true,unchanged:true,trace:'trace.jsonl.gz'};
  const records = [catalog,...snapshots,{validation:{valid:true,sequence:decisions.length,message:''}}];
  const write = () => fs.writeFileSync(path.join(directory,entry.trace),zlib.gzipSync(records.map(r=>JSON.stringify(r)).join('\n')+'\n'));
  write(); return {directory,entry,records,write};
}

test('early purchases survive sales; brief enlisting differs from actual battle use',async t=>{
  const f=fixture(t),{run}=await analyzeRun(f.directory,f.entry,f.directory);
  assert.equal(run.roles[0].buys,1); assert.equal(run.roles[0].sales,1);
  assert.equal(run.roles[0].early,1); assert.equal(run.roles[0].final,0);
  assert.equal(run.roles[1].enlisted,1); assert.equal(run.roles[1].fielded,0);
  assert.equal(run.roles[2].mid,1); assert.equal(run.roles[2].late,1); assert.equal(run.roles[2].expedition,1);
  assert.deepEqual(run.battles.map(b=>b.stage),['early','mid','late','expedition','late']);
  assert.equal(run.combos[8].equipment_battles,4);
  const report=buildUsageReport([run],catalog);
  assert.equal(report.roles[0].history_only_runs,1);
  assert.deepEqual(report.combos[0].threshold_runs,{1:1,3:1});
  assert.equal(report.combos[0].equipment_runs,1);
});

test('family weights and tournament denominators are recomputed on exact matching saves',async t=>{
  const f=fixture(t),{run}=await analyzeRun(f.directory,f.entry,f.directory);
  const second=structuredClone(run); second.index=1; second.file='variant.json'; second.roles[0].fielded=0;
  const third=structuredClone(second); third.index=2; third.file='other.json'; third.seed='family-b';
  const report=buildUsageReport([run,second,third],catalog);
  assert.equal(report.roles[0].fielded_pct,33.33);
  assert.equal(report.roles[0].fielded_seed_weighted_pct,25);
  const subset=selectUsageForTournament(report,[{source:run.file,payload_sha256:run.sha256},{source:second.file,payload_sha256:second.sha256}]);
  assert.equal(subset.n,2); assert.equal(subset.seeds,1);
  assert.equal(subset.roles[0].fielded_seed_weighted_pct,50);
  assert.throws(()=>selectUsageForTournament(report,[{source:run.file,payload_sha256:'wrong'}]),/沒有相符/);
});

test('missing or failed full validation cannot be counted despite all decision snapshots',async t=>{
  const f=fixture(t);
  f.records.at(-1).validation.valid=false; f.write();
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/缺少完整驗證/);
  f.records.pop(); f.write();
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/快照不完整/);
});

test('input hash, rules fingerprint, snapshot order and final roster must match',async t=>{
  const f=fixture(t);
  await assert.rejects(analyzeRun(f.directory,{...f.entry,sha256:'wrong'},f.directory),/SHA-256/);
  f.records[0]={...catalog,content_fingerprint:'wrong'}; f.write();
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/規則指紋/);
  f.records[0]=catalog; f.records[2].sequence=100; f.write();
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/序號不連續/);
  f.records[2].sequence=1; f.records.at(-2).roster={}; f.write();
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/最終棋子快照/);
});

test('missing and damaged compressed traces produce analysis errors',async t=>{
  const f=fixture(t);
  await assert.rejects(analyzeRun(f.directory,{...f.entry,trace:'missing.jsonl.gz'},f.directory),/ENOENT/);
  fs.writeFileSync(path.join(f.directory,f.entry.trace),'damaged gzip');
  await assert.rejects(analyzeRun(f.directory,f.entry,f.directory),/header/);
});

test('rejected malformed inputs are preserved in the manifest without entering counts',async t=>{
  const f=fixture(t),bad='broken.json'; fs.writeFileSync(path.join(f.directory,bad),'bad json');
  const rejected={file:bad,sha256:sha256('bad json'),valid:false,unchanged:true,error:'格式無效'};
  const manifest=path.join(f.directory,'manifest.json');
  fs.writeFileSync(manifest,JSON.stringify({format:'kys_chess_usage_traces',format_version:1,source_directory:f.directory,entries:[f.entry,rejected]}));
  const report=await analyzeManifest(manifest);
  assert.equal(report.n,1); assert.deepEqual(report.rejected,[rejected]);
});

test('standalone HTML and tournament generator safely embed user-controlled names',async t=>{
  const f=fixture(t),{run}=await analyzeRun(f.directory,f.entry,f.directory);
  run.name='</script><img onerror="bad()">$&';
  const report=buildUsageReport([run],catalog);
  const html=standaloneUsageHtml(report);
  assert.ok(!html.includes(run.name)); assert.ok(html.includes('\\u003c/script>'));
  assert.equal(JSON.parse(scriptJson(run.name)),run.name);
  const second={...run,index:1,file:'variant.json'};
  fs.copyFileSync(path.join(f.directory,run.file),path.join(f.directory,second.file));
  const usage=path.join(f.directory,'usage.json'),results=path.join(f.directory,'results.json'),output=path.join(f.directory,'report.html');
  fs.writeFileSync(usage,JSON.stringify(buildUsageReport([run,second],catalog)));
  fs.writeFileSync(results,JSON.stringify({format:'full_round_robin_mirrored',submissions:[run,second].map(r=>({index:r.index,name:r.name,source:r.file,payload_sha256:r.sha256})),battle_seeds:[],pairings:[],battles:[],standings:[],champion_entrant_indices:[],run_metadata:{data_root:f.directory}}));
  execFileSync(process.execPath,[fileURLToPath(new URL('./generate-tournament-report.mjs',import.meta.url)),results,output,f.directory,'--usage',usage]);
  const integrated=fs.readFileSync(output,'utf8');
  assert.ok(integrated.includes('id="reportTabs"')); assert.ok(!integrated.includes(run.name));
  assert.ok(!/__TOURNAMENT_DATA__|__REPORT_TABS__|__USAGE_PANEL__|__USAGE_SCRIPT__/.test(integrated));
  execFileSync(process.execPath,[fileURLToPath(new URL('./generate-tournament-report.mjs',import.meta.url)),results,output,f.directory]);
  const tournamentOnly=fs.readFileSync(output,'utf8');
  assert.ok(!tournamentOnly.includes('id="reportTabs"'));
  assert.ok(!tournamentOnly.includes('__USAGE_'));
  assert.equal(sha256(fs.readFileSync(path.join(f.directory,run.file))),run.sha256);
});
