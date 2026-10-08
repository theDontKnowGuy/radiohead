// Exercise the shipped credential-form script with synthetic values only.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../src/web_server.cpp'), 'utf8');
const script = source.match(/R"JS\(<script>\(\(\)=>\{const \$=id=>document\.getElementById\(id\);let state=null,saving=false;([\s\S]*?)<\/script>\)JS"/);
assert.ok(script, 'Production credential script is present');
const production = '(()=>{const $=id=>document.getElementById(id);let state=null,saving=false;' + script[1];
const flush = () => new Promise(resolve => setImmediate(resolve));

async function fixture({configured = false, failLoad = false, maintenance = false} = {}) {
  const elements = {}, handlers = {}, posts = [];
  let failSave = 0, holdSave = null, allowConfirm = false;
  const $ = id => elements[id] ||= {
    value:'', checked:false, type:id === 'spotify-client-secret' ? 'password':'text',
    disabled:true, attrs:{}, focus(){this.focused=true;},
    setAttribute(k,v){this.attrs[k]=v;}, removeAttribute(k){delete this.attrs[k];},
  };
  const state = {spotifyCredentialsConfigured:configured, spotifyCredentialsRevision:7, updateInProgress:maintenance};
  const context = {URLSearchParams, confirm:()=>allowConfirm,
    window:{addEventListener:(name,fn)=>handlers[name]=fn},
    document:{getElementById:$,addEventListener:(name,fn)=>handlers[name]=fn},
    fetch:async (url,options={})=>{
      if (options.method === 'POST') {
        assert.equal(url, '/api/device/spotify-credentials');
        assert.equal(options.headers['X-Radiohead-Config'], '1');
        posts.push(Object.fromEntries(options.body));
        if (holdSave) await new Promise(resolve=>holdSave.resolve=resolve);
        if (failSave) return {ok:false,status:failSave,json:async()=>({error:failSave===409?'Settings changed; reload.':'Save rejected.'})};
        state.spotifyCredentialsConfigured = true;
        state.spotifyCredentialsRevision++;
        return {ok:true,json:async()=>({...state})};
      }
      assert.equal(url, '/api/device');
      assert.equal(options.cache, 'no-store');
      return {ok:!failLoad,json:async()=>({...state})};
    },
  };
  vm.runInNewContext(production, context);
  await flush();
  const fill = (id='fixture-id',secret='fixture-secret')=>{
    $('spotify-client-id').value=id;$('spotify-client-secret').value=secret;
    $('spotify-client-id').oninput();$('spotify-client-secret').oninput();
  };
  const submit = ()=>$('spotify-credentials-form').onsubmit({preventDefault(){}});
  return {$,fill,submit,posts,handlers,
    reject:status=>failSave=status, confirm:value=>allowConfirm=value,
    hold:()=>holdSave={}, release:()=>holdSave.resolve(),
  };
}

(async()=>{
  const f=await fixture();
  assert.match(f.$('spotify-credentials-configured').textContent,/No Spotify/);
  assert.equal(f.$('spotify-credentials-save').disabled,false);
  await f.submit();assert.equal(f.posts.length,0);
  assert.equal(f.$('spotify-client-id').attrs['aria-invalid'],'true');
  f.fill('fixture-id','with space');await f.submit();assert.equal(f.posts.length,0);
  f.fill('fixture-id','a'.repeat(129));await f.submit();assert.equal(f.posts.length,0);
  f.fill('fixture-id','é');await f.submit();assert.equal(f.posts.length,0);
  f.fill('  fixture-id  ',' fixture-secret ');
  f.$('spotify-show-secret').checked=true;f.$('spotify-show-secret').onchange();
  assert.equal(f.$('spotify-client-secret').type,'text');
  await f.submit();
  assert.equal(f.posts[0].clientId,'fixture-id');assert.equal(f.posts[0].clientSecret,'fixture-secret');
  assert.equal(f.posts[0].revision,'7');
  assert.equal(f.$('spotify-client-secret').value,'');assert.equal(f.$('spotify-client-id').value,'');
  assert.equal(f.$('spotify-client-secret').type,'password');
  assert.equal(f.$('spotify-show-secret').checked,false);
  assert.match(f.$('spotify-credentials-status').textContent,/saved.*Restart/);
  f.fill();f.$('spotify-credentials-cancel').onclick();assert.equal(f.$('spotify-client-secret').value,'');

  const failed=await fixture({configured:true});
  assert.match(failed.$('spotify-credentials-configured').textContent,/configured/);
  assert.equal(failed.$('spotify-client-secret').value,'');
  failed.reject(500);failed.fill();await failed.submit();
  assert.equal(failed.$('spotify-client-secret').value,'fixture-secret');
  assert.equal(failed.$('spotify-credentials-save').disabled,false);
  failed.reject(409);await failed.submit();assert.match(failed.$('spotify-credentials-status').textContent,/reload/);
  await failed.$('spotify-credentials-reload').onclick();assert.equal(failed.$('spotify-client-secret').value,'fixture-secret');
  failed.confirm(true);await failed.$('spotify-credentials-reload').onclick();assert.equal(failed.$('spotify-client-secret').value,'');

  const duplicate=await fixture();duplicate.fill();duplicate.hold();const first=duplicate.submit();
  assert.equal(duplicate.$('spotify-credentials-save').disabled,true);
  await duplicate.submit();assert.equal(duplicate.posts.length,1);duplicate.release();await first;
  const down=await fixture({failLoad:true});assert.equal(down.$('spotify-credentials-save').disabled,true);
  const busy=await fixture({maintenance:true});busy.fill();await busy.submit();assert.equal(busy.posts.length,0);
  const nav=await fixture();nav.fill();let prevented=false;
  nav.handlers.click({target:{closest:()=>({})},preventDefault(){prevented=true;},stopImmediatePropagation(){}});
  assert.equal(prevented,true);assert.equal(nav.$('spotify-client-secret').value,'fixture-secret');
  nav.handlers.beforeunload({preventDefault(){prevented=true;}});
  nav.confirm(true);nav.handlers.click({target:{closest:()=>({})},preventDefault(){},stopImmediatePropagation(){}});
  assert.equal(nav.$('spotify-client-secret').value,'');
  console.log('Spotify web credential flow checks passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
