// Exercise the production Audio form against synthetic device responses.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/web_server.cpp','utf8');
assert.equal((source.match(/server\.on\("\/audio"/g)||[]).length,1,'Audio has one page handler, without its retired redirect');
const section=source.slice(source.indexOf('case WebSection::Audio:',source.indexOf('void appendSectionContent')),source.indexOf('case WebSection::Stations: {',source.indexOf('void appendSectionContent')));
const script=section.match(/R"JS\(<script>([\s\S]*?)<\/script>\)JS"/)[1];
new vm.Script(script);
const flush=()=>new Promise(resolve=>setImmediate(resolve));
async function fixture({down=false}={}) {
 const elements={},events={},posts=[];let fail=0,hold=null,holdRead=null,poll;
 let data={revision:9,audioRevision:7,bass:0,mid:1,treble:-2,notchEnabled:false,notchHz:3000,notchEndHz:0};
 const $=id=>elements[id]||=( {value:'',checked:false,disabled:id==='audio-save',textContent:'',addEventListener(name,fn){this[name]=fn;}} );
 const document={hidden:false,getElementById:$,addEventListener(name,fn){events[name]=fn;}};
 let confirm=false;
 vm.runInNewContext(script,{document,window:{addEventListener(name,fn){events[name]=fn;}},URLSearchParams,
  confirm:()=>confirm,setInterval(fn){poll=fn;},async fetch(url,options={}) {
   if(options.method==='POST') {
    assert.equal(url,'/api/audio');const body=Object.fromEntries(options.body);posts.push(body);
    if(hold)await new Promise(resolve=>hold.resolve=resolve);
    if(fail)return {ok:false,status:fail,json:async()=>({...data,error:'Save rejected.'})};
    data={...data,...Object.fromEntries(['bass','mid','treble','notchHz','notchEndHz'].map(key=>[key,Number(body[key])])),notchEnabled:body.notchEnabled==='1',audioRevision:data.audioRevision+1};
   } else {assert.equal(url,'/api/player');if(down)throw Error('Disconnected');const snapshot={...data};if(holdRead)await new Promise(resolve=>holdRead.resolve=resolve);return {ok:true,json:async()=>snapshot};}
   return {ok:true,json:async()=>({...data})};
  }});
 await flush();
 return {$,events,posts,submit:()=>$('audio-form').onsubmit({preventDefault(){}}),poll:async()=>{poll();await flush();},
   reject:code=>fail=code,confirm:()=>confirm=true,change:values=>data={...data,...values},
   hold:()=>hold={},release:()=>hold.resolve(),holdRead:()=>holdRead={},releaseRead:()=>{holdRead.resolve();holdRead=null;},up:()=>down=false};
}
(async()=>{
 const f=await fixture();assert.equal(f.$('audio-save').disabled,false);assert.equal(f.$('tone-mid').value,1);assert.equal(f.$('notch-frequency').value,3000);
 f.$('tone-bass').value='-15';f.$('tone-bass').input();f.$('tone-treble').value='15';f.$('notch-enabled').checked=true;f.$('notch-frequency').value='3100';
 f.change({revision:10});await f.poll();assert.equal(f.$('tone-bass').value,'-15');assert.doesNotMatch(f.$('audio-status').textContent,/elsewhere/); // Volume is independent.
 await f.submit();assert.deepEqual({...f.posts[0]},{bass:'-15',mid:'1',treble:'15',notchEnabled:'1',notchHz:'3100',notchEndHz:'0',revision:'7'});
 assert.match(f.$('audio-status').textContent,/applied/);
 const band=await fixture();band.$('notch-frequency').value='5000-5100';band.$('notch-enabled').checked=true;
 await band.submit();assert.equal(band.posts[0].notchHz,'5000');assert.equal(band.posts[0].notchEndHz,'5100');assert.equal(band.$('notch-frequency').value,'5000-5100');
 band.$('notch-frequency').value=' 5000 - 5200 ';await band.submit();assert.equal(band.posts[1].notchEndHz,'5200');
 band.$('notch-frequency').value='5000';await band.submit();assert.equal(band.posts[2].notchEndHz,'0');
 band.change({notchHz:5000,notchEndHz:5100,audioRevision:20});await band.poll();assert.equal(band.$('notch-frequency').value,'5000-5100');
 band.$('notch-frequency').value='5000-5300';band.$('notch-frequency').input();band.change({notchEndHz:5200,audioRevision:21});await band.poll();assert.equal(band.$('notch-frequency').value,'5000-5300');assert.match(band.$('audio-status').textContent,/elsewhere/);
 await band.$('audio-cancel').onclick();await flush();assert.equal(band.$('notch-frequency').value,'5000-5200');
 for (const invalid of ['', '99','10001','3.5','NaN','5100-5000','5000-5000','99-5100','5000-10001','5000-','-5100','5000-5100-5200','5e3-5100','5000Hz','5000/5100','5000-000','5000-00000']) {f.$('notch-frequency').value=invalid;await f.submit();assert.equal(f.posts.length,1);}
 f.$('notch-frequency').value='100';f.reject(500);await f.submit();assert.match(f.$('audio-status').textContent,/rejected/);assert.equal(f.$('notch-frequency').value,'100');
 f.reject(409);await f.submit();assert.match(f.$('audio-status').textContent,/draft is kept/);assert.equal(f.$('notch-frequency').value,'100');
 let prevented=false;f.events.click({target:{closest:()=>({})},preventDefault(){prevented=true;},stopImmediatePropagation(){}});assert(prevented);
 f.events.beforeunload({preventDefault(){prevented=true;}});
 f.change({notchHz:4000,audioRevision:12});await f.poll();assert.equal(f.$('notch-frequency').value,'100');assert.match(f.$('audio-status').textContent,/elsewhere/);
 await f.$('audio-cancel').onclick();await flush();assert.equal(f.$('notch-frequency').value,4000);assert.match(f.$('audio-status').textContent,/reloaded/);
 f.reject(0);f.$('notch-frequency').value='10000';f.hold();const pending=f.submit();await flush();assert.equal(f.$('audio-save').disabled,true);assert.equal(f.$('tone-bass').disabled,true);
 const count=f.posts.length;await f.submit();assert.equal(f.posts.length,count);f.release();await pending;assert.equal(f.$('audio-save').disabled,false);
 const down=await fixture({down:true});assert.equal(down.$('audio-save').disabled,true);assert.equal(down.$('tone-bass').disabled,true);down.up();await down.$('audio-cancel').onclick();await flush();assert.equal(down.$('tone-bass').disabled,false);
 const race=await fixture();race.holdRead();await race.poll();race.$('tone-mid').value='5';race.$('tone-mid').input();await race.submit();race.releaseRead();await flush();assert.equal(race.$('tone-mid').value,5,'A delayed poll cannot roll back a newer save');
 console.log('Production Audio form: save/bounds/conflict/draft/reload/polling/failure checks passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
