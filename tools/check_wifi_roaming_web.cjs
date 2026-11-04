/* Exercise the production roaming control with a DOM/fetch seam. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/web_server.cpp', 'utf8');
const section = source.slice(source.indexOf('    case WebSection::Network:', source.indexOf('void appendSectionContent')), source.indexOf('    case WebSection::Weather:', source.indexOf('void appendSectionContent')));
const scripts = [...section.matchAll(/R"JS\((?:<script>)?([\s\S]*?)<\/script>\)JS"/g)].map(m => m[1]);
scripts.forEach(script => new vm.Script(script));
const script = scripts.find(script => script.includes("getElementById('network-roaming-toggle')"));
assert.ok(script);
assert.match(section, /Changes apply immediately and stay saved after restarting/);
const elements = {};
const $ = id => elements[id] ||= {disabled: true, textContent: '', attributes: {}, setAttribute(key, value) {this.attributes[key] = value;}};
let enabled = true, posts = 0, reads = 0, saveFail = false, readFail = false, lostReply = false, malformed = false;
let poll;
const document = {hidden: false, getElementById: $};
const context = {document, URLSearchParams, AbortSignal, setInterval(fn, ms) {assert.equal(ms, 5000); poll = fn;}, async fetch(url, options) {
  assert.equal(url, '/api/radio/diagnostics/roaming');
  if (options.method === 'POST') {
    ++posts;
    const target = options.body.get('enabled') === '1';
    const expected = options.body.get('expected') === '1';
    if (expected !== enabled) return {ok: false, json: async () => ({error: 'Roaming setting changed; reload first.'})};
    if (saveFail) return {ok: false, json: async () => ({error: 'Could not save roaming setting; previous setting kept.'})};
    enabled = target;
    if (lostReply) throw Error('Response lost.');
  } else {
    ++reads;
    if (readFail) throw Error('Radio unreachable.');
  }
  return {ok: true, json: async () => ({enabled: malformed ? 'invalid' : enabled})};
}};
const flush = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  vm.createContext(context);
  vm.runInContext(script, context);
  await flush();
  const button = $('network-roaming-toggle'), status = $('network-roaming-status');
  assert.equal(button.attributes['aria-pressed'], 'true');
  assert.equal(button.disabled, false);
  assert.match(status.textContent, /Saved across restarts/);
  const first = button.onclick();
  assert.equal(button.disabled, true);
  await button.onclick();
  await first;
  assert.equal(posts, 1);
  assert.equal(button.attributes['aria-pressed'], 'false');
  assert.match(status.textContent, /turned off and saved/);
  saveFail = true;
  await button.onclick();
  assert.equal(enabled, false);
  assert.equal(button.attributes['aria-pressed'], 'false');
  assert.match(status.textContent, /previous setting kept/);
  saveFail = false;
  enabled = true; // Another client changed the setting.
  await button.onclick();
  assert.equal(button.attributes['aria-pressed'], 'true');
  assert.match(status.textContent, /changed; reload/);
  lostReply = true;
  await button.onclick();
  assert.equal(enabled, false);
  assert.equal(button.attributes['aria-pressed'], 'false'); // Read after an uncertain write.
  assert.match(status.textContent, /Response lost/);
  lostReply = false; readFail = true;
  await poll(); await flush();
  assert.equal(button.disabled, true);
  assert.match(status.textContent, /Retrying/);
  const before = posts;
  await button.onclick(); assert.equal(posts, before);
  readFail = false; enabled = true;
  await poll(); await flush();
  assert.equal(button.disabled, false);
  assert.equal(button.attributes['aria-pressed'], 'true');
  malformed = true;
  await poll(); await flush();
  assert.equal(button.disabled, true);
  malformed = false;
  await poll(); await flush();
  assert.equal(button.disabled, false);
  const readsBefore = reads;
  document.hidden = true;
  await poll(); await flush();
  assert.equal(reads, readsBefore);
  console.log('Roaming UI: loaded state, saved toggle, duplicate suppression, save failure, stale state, lost reply, invalid data, retry and hidden-page polling passed.');
})().catch(error => {console.error(error); process.exit(1);});
