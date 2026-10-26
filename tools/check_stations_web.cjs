// Exercise the shipped station script with synthetic DOM/fetch responses.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../src/web_server.cpp'),'utf8');
const start=source.indexOf('let state=null,draftSlot=null');
const script=source.slice(source.lastIndexOf('R"JS(',start)+5,source.indexOf('</script>)JS"',start));
new vm.Script(script);
const flush=()=>new Promise(resolve=>setImmediate(resolve));
async function fixture(){
 const nodes={},events={},posts=[];let hold=null,reject=false;
 let data={revision:7,capacity:10,saved:2,stations:[{slot:0,name:'Kan <88>',url:'https://example.org/88',playing:true},{slot:4,name:'רדיו',url:'https://example.org/radio',playing:false}]};
 const $=id=>nodes[id]||=( {value:'',hidden:false,disabled:false,dataset:{},textContent:'',innerHTML:'',addEventListener(){},scrollIntoView(){},querySelectorAll(selector){
  const attr=selector.slice(1,-1),key=attr.slice(5);
  return [...this.innerHTML.matchAll(new RegExp(attr+'="(\\d+)"','g'))].map(m=>{const b=$(`${key}-${m[1]}`);b.dataset[key]=m[1];return b;});
 }} );
 const document={getElementById:$,querySelectorAll:()=>[],addEventListener(name,fn){events[name]=fn;}};
 vm.runInNewContext(script,{document,window:{scrollTo(){}},URLSearchParams,clearInterval(){},setInterval(){},async fetch(url,options={}){
  if(options.method==='POST'){
   const body=Object.fromEntries(options.body);posts.push({url,body});if(hold)await new Promise(resolve=>hold.resolve=resolve);
   if(reject||Number(body.revision)!==data.revision)return {ok:false,json:async()=>({error:'Station list changed.'})};
   if(url.endsWith('/play'))data={...data,stations:data.stations.map(s=>({...s,playing:false}))};
  }
  return {ok:true,json:async()=>structuredClone(data)};
 }});await flush();
 return {$,posts,events,hold(){hold={};},release(){hold.resolve();hold=null;},change(){data.revision++;},reject(){reject=true;}};
}
(async()=>{
 const f=await fixture();assert.match(f.$('station-list').innerHTML,/Play Kan &lt;88&gt;/);assert.match(f.$('station-list').innerHTML,/type="button" data-play="4"/);
 await f.$('play-4').onclick();assert.deepEqual(f.posts,[{url:'/api/stations/play',body:{slot:'4',revision:'7'}}]);assert.match(f.$('station-feedback').textContent,/selected/);
 f.events['radiohead-player']({detail:{stationRevision:7,source:'radio',playbackState:'playing',playingStation:4}});assert(f.$('playing-0').hidden);assert(!f.$('playing-4').hidden);
 for(const detail of [{source:'spotify'},{playbackState:'connecting'},{playbackState:'failed'},{stationRevision:8}]){f.events['radiohead-player']({detail:{stationRevision:7,source:'radio',playbackState:'playing',playingStation:4,...detail}});assert(f.$('playing-4').hidden);}
 const pending=await fixture();pending.hold();const request=pending.$('play-4').onclick();await flush();assert(pending.$('play-0').disabled&&pending.$('play-4').disabled);await pending.$('play-0').onclick();assert.equal(pending.posts.length,1);pending.release();await request;assert(!pending.$('play-4').disabled);
 const stale=await fixture();stale.change();await stale.$('play-4').onclick();assert.match(stale.$('station-feedback').textContent,/changed/);assert(!stale.$('play-4').disabled);await stale.$('play-4').onclick();assert.equal(stale.posts[1].body.revision,'8');
 const actions=await fixture();actions.$('edit-4').onclick();assert.equal(actions.posts.length,0);assert.equal(actions.$('station-name').value,'רדיו');await actions.$('favorite-4').onclick();assert.equal(actions.posts[0].url,'/api/stations/favorite');
 const failed=await fixture();failed.reject();await failed.$('play-0').onclick();assert.match(failed.$('station-feedback').textContent,/changed/);assert(!failed.$('play-0').disabled);
 console.log('Station browser checks passed: selection, escaping, slot gaps, duplicate clicks, conflicts, failures, independent actions and live playback markers.');
})().catch(error=>{console.error(error);process.exitCode=1;});
