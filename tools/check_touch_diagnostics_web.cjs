/* Run the production Device scripts against a bounded DOM/fetch seam. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/web_server.cpp', 'utf8');
const device = source.slice(source.indexOf('    case WebSection::Device:', source.indexOf('void appendSectionContent')), source.indexOf('void handleConfigShell'));
const scripts = [...device.matchAll(/R"JS\((?:<script>)?([\s\S]*?)<\/script>\)JS"/g)].map(m => m[1]);
assert.equal(scripts.length, 3);
scripts.forEach(s => new vm.Script(s));
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  const elements = {}, events = {}, timers = new Map();
  let sequence = 0, reads = 0, posts = 0, recoveries = 0, fail = false, reject = false, recoverFail = false;
  let data = {touchDebugEnabled: false, touchDebugAvailable: true, touchContact: false, touchOnPanel: false, touchRaw: [0,0], touchScreen: [0,0]};
  const $ = id => elements[id] ||= {dataset: {}, disabled: false, files: [], value: '', open: false, textContent: '', innerHTML: '', attributes: {},
    focus() {}, setAttribute(k,v) {this.attributes[k] = v;}, addEventListener(type, fn) {this[type] = fn;}};
  const document = {hidden: false, getElementById: $, querySelectorAll: () => ['device-touch-debug', 'device-touch-recover', 'device-calibration-start', 'device-restart'].map($),
    addEventListener(type, fn) {(events[type] ||= []).push(fn);}, dispatchEvent(e) {for (const fn of events[e.type] || []) fn(e);}};
  const context = {document, URLSearchParams, Event, AbortSignal, confirm: () => true,
    setTimeout(fn, ms) {const id = ++sequence; timers.set(id, {fn, ms}); return id;}, clearTimeout(id) {timers.delete(id);},
    async fetch(url, options = {}) {
      if (url === '/api/device/touch-recover') {
        assert.equal(options.method,'POST');++recoveries;
        if(reject)return {ok:false,json:async()=>({error:'Maintenance busy.'})};
        data={...data,touchRecovery:{attempts:recoveries,successes:recoverFail?recoveries-1:recoveries,lastSucceeded:!recoverFail,lastAgeMs:0,lastDurationUs:500,beforeAdc:data.touchAdc}};
        return {ok:!recoverFail,json:async()=>({...data,...(recoverFail?{error:'Touch bus initialization failed.'}:{})})};
      }
      if (url === '/api/device/touch-debug') {
        if (options.method === 'POST') {
          ++posts; assert.equal(options.body.get('enabled'), data.touchDebugEnabled ? '0' : '1');
          if (reject) return {ok: false, json: async () => ({error: 'Maintenance busy.'})};
          data = {...data, touchDebugEnabled: options.body.get('enabled') === '1'};
        } else ++reads;
        if (fail) throw Error('Connection lost.');
        return {ok: true, json: async () => ({...data})};
      }
      if (url === '/api/device') return {ok: true, json: async () => ({touchCalibrationSaved: true, touchCalibrationState: 'idle', touchCalibrationParameters: [3677,459,3593,3739,475,464,535,3729], touchActiveCalibration: [3677,459,3593,3739,475,464,535,3729], touchCalibrationSource: 'saved at boot'})};
      assert.equal(url, '/api/update/status');
      return {ok: true, json: async () => ({busy: false, status: 'idle'})};
    }};
  vm.createContext(context); scripts.forEach(s => vm.runInContext(s, context)); await flush();
  assert.match($('device-diagnostics').innerHTML, /Saved touch points/);
  assert.match($('device-diagnostics').innerHTML, /P1: \(3677, 459\)/);
  assert.match($('device-diagnostics').innerHTML, /Active calibration source/);
  assert.equal(reads, 0); // Collapsed diagnostics do not poll.
  const panel = $('device-diagnostics-panel'), button = $('device-touch-debug');
  panel.open = true; panel.toggle(); await flush();
  assert.equal(button.disabled, false); assert.equal(button.attributes['aria-pressed'], 'false');
  assert.match($('device-touch-contact').textContent, /No contact/);
  const first = button.onclick(); await button.onclick(); await first;
  assert.equal(posts, 1); assert.equal(button.attributes['aria-pressed'], 'true');
  assert.equal(button.textContent, 'Disable touch debug');
  async function poll() {const [id,t] = [...timers].find(([,t]) => t.ms === 500);timers.delete(id); await t.fn();await flush();}
  data = {...data, touchContact: true, touchOnPanel: false, touchRaw: [3000,900], touchScreen: [-12,100], touchContactSamples: 3, touchPolls: 90, touchLastContactAgeMs: 0};
  await poll(); assert.match($('device-touch-contact').textContent, /raw \(3000, 900\).*screen \(-12, 100\).*outside screen/);
  data = {...data, touchContact: false, touchPolls: 150, touchLastContactAgeMs: 500};
  await poll(); assert.match($('device-touch-contact').textContent, /No contact now.*500 ms ago.*raw \(3000, 900\).*Detected samples: 3 \/ 150/);
  data={...data,touchAdc:{frames:300,acceptedFrames:20,pressureRejectedFrames:50,pressure:7,pressureValid:2,
    x:{minimum:100,maximum:3200,median:2000,valid:7,cluster:2,railFrames:100,sparseFrames:20,unstableFrames:30},
    y:{minimum:1400,maximum:1410,median:1405,valid:7,cluster:7,railFrames:100,sparseFrames:0,unstableFrames:0}},
    touchTiming:{lastGapMs:9,maxGapMs:230,lastReadUs:500,maxReadUs:700,presses:3,releases:3,offPanelSamples:2}};
  await poll();assert.match($('device-touch-x').textContent,/unstable frames 30/);
  assert.match($('device-touch-adc').textContent,/rejected by pressure with valid X\/Y: 50/);
  assert.match($('device-touch-timing').textContent,/maximum 230 ms/);
  const recovery=$('device-touch-recover');
  const attempt=recovery.onclick();await recovery.onclick();await button.onclick();await attempt;
  assert.equal(recoveries,1);assert.equal(posts,1); // Both actions share duplicate protection.
  assert.match($('device-touch-recover-status').textContent,/Lift your finger/);
  assert.match($('device-touch-recovery').textContent,/Before last recovery: 300 ADC frames/);
  recoverFail=true;await recovery.onclick();assert.equal(recoveries,2);
  assert.match($('device-touch-recover-status').textContent,/initialization failed/);
  assert.match($('device-touch-recovery').textContent,/last failed/);recoverFail=false;
  reject=true;await recovery.onclick();assert.match($('device-touch-recover-status').textContent,/Maintenance busy/);reject=false;
  data.touchDebugAvailable = false; await poll(); assert.equal(button.disabled, true);assert.equal(recovery.disabled,true);
  data.touchDebugAvailable = true; fail = true; await poll(); assert.equal(button.disabled, true);
  assert.match($('device-touch-debug-status').textContent, /Connection lost/);assert.equal(recovery.disabled,true);
  fail = false; await poll(); assert.equal(button.disabled, false);
  reject = true; await button.onclick(); assert.equal(data.touchDebugEnabled, true);
  assert.match($('device-touch-debug-status').textContent, /Maintenance busy/);
  reject = false; await poll(); await button.onclick(); assert.equal(button.attributes['aria-pressed'], 'false');
  panel.open = false; panel.toggle(); assert.equal([...timers.values()].some(t => t.ms === 500), false);
  panel.open = true; panel.toggle(); await flush(); document.hidden = true;
  document.dispatchEvent(new Event('visibilitychange'));assert.equal([...timers.values()].some(t => t.ms === 500), false);
  console.log('Device diagnostics: ADC/timing, calibration, debug, duplicate recovery, failure/maintenance handling and polling lifecycle passed.');
})().catch(error => {console.error(error); process.exit(1);});
