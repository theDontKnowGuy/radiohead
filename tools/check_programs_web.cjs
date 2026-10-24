// Run the production Programs script with synthetic radio/directory responses.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/web_server.cpp','utf8');
const section=source.slice(source.indexOf('void appendProgramContent'),source.indexOf('void appendProgramEpisodes'));
const script=section.match(/R"JS\(<script>([\s\S]*?)<\/script>\)JS"/)[1];new vm.Script(script);
const flush=()=>new Promise(resolve=>setImmediate(resolve));
async function fixture({full=false,down=false}={}) {
 const nodes={},events={},posts=[],searches=[],timers=[];let now=10000,confirm=false,fail=0,hold=null,searchHold=null,previewState='ready';
 const initial={slot:0,id:'ilanadayan/podcast',name:'אילנה דיין',kind:'omny',program:'ilanadayan',playlist:'podcast',feedUrl:'',favorite:true};
 let data={revision:7,capacity:10,programs:full?Array.from({length:10},(_,slot)=>({...initial,slot,name:'Show '+slot,id:'id'+slot,program:'show'+slot})):[initial]};
 const $=id=>nodes[id]||=({id,hidden:id==='program-editor'||id==='program-dialog',value:id==='program-kind'?'rss':'',checked:false,disabled:id==='program-add',textContent:'',innerHTML:'',dataset:{},listeners:{},addEventListener(name,fn){this.listeners[name]=fn;},querySelectorAll(selector){const attr=selector.slice(1,-1),pattern=new RegExp(attr+'="(\\d+)"','g');return [...this.innerHTML.matchAll(pattern)].map(match=>{const name=attr.replace(/^data-/,'').replace(/-([a-z])/g,(_,c)=>c.toUpperCase());return {dataset:{[name]:match[1]},set onclick(fn){$('button-'+attr+'-'+match[1]).onclick=fn;}};});},focus(){document.activeElement=this;},scrollIntoView(){}});
 const document={activeElement:null,getElementById:$,addEventListener(name,fn){events[name]=fn;}};
 vm.runInNewContext(script,{document,window:{addEventListener(name,fn){events[name]=fn;}},URLSearchParams,TextEncoder,AbortController,Date:{now:()=>now},confirm:()=>confirm,setTimeout(fn,ms){timers.push({fn,ms});return timers.length;},clearTimeout(){},async fetch(url,options={}){
  if(url.startsWith('https://itunes.apple.com/')){searches.push(url);if(searchHold)await new Promise(resolve=>searchHold.resolve=resolve);return {ok:true,json:async()=>({results:[{collectionName:'Found <show>',artistName:'An author',feedUrl:'https://feeds.example/show.rss'},{collectionName:'Bad feed',feedUrl:'javascript:alert(1)'}]})};}
  if(down)throw Error('Disconnected');
  if(options.method==='POST') {
   const body=Object.fromEntries(options.body);posts.push({url,body});if(hold)await new Promise(resolve=>hold.resolve=resolve);
   if(fail)return {ok:false,status:fail,json:async()=>({error:'Save rejected.'})};
   if(url==='/api/programs/preview')return {ok:true,json:async()=>({token:9})};
   if(Number(body.revision)!==data.revision)return {ok:false,status:409,json:async()=>({error:'Changed elsewhere.'})};
   if(url.endsWith('/save')){const slot=body.slot===''?data.programs.length:Number(body.slot);data={...data,revision:data.revision+1,programs:[...data.programs.filter(p=>p.slot!==slot),{...body,slot,id:'new-id',favorite:body.favorite==='1'}]};}
   if(url.endsWith('/favorite'))data={...data,revision:data.revision+1,programs:data.programs.map(p=>p.slot===Number(body.slot)?{...p,favorite:!p.favorite}:p)};
   if(url.endsWith('/remove'))data={...data,revision:data.revision+1,programs:data.programs.filter(p=>p.slot!==Number(body.slot))};
  }
  if(url.startsWith('/api/programs/preview?'))return {ok:true,json:async()=>({state:previewState,episodes:[{title:'Episode <one>',published:'2026-10-22',duration:100}]})};
  return {ok:true,json:async()=>structuredClone(data)};
 }});await flush();
 const button=(attr,slot)=>$('button-data-program-'+attr+'-'+slot);
 return {$,events,posts,searches,button,timers,confirm:()=>confirm=true,reject:code=>fail=code,hold:()=>hold={},release:()=>{hold.resolve();hold=null;},holdSearch:()=>searchHold={},releaseSearch:()=>{searchHold.resolve();searchHold=null;},advance:ms=>now+=ms,previewState:value=>previewState=value,change:()=>data={...data,revision:data.revision+1},
  submit:()=>$('program-form').onsubmit({preventDefault(){}}),search:()=>$('program-directory').onsubmit({preventDefault(){}}),edit:()=>button('edit',0).onclick(),add:()=>$('program-add').onclick()};
}
(async()=>{
 const f=await fixture();assert.match(f.$('program-list').innerHTML,/אילנה דיין/);assert.equal(f.$('program-count').textContent,'1 / 10 programs');
 f.$('program-filter').value='אילנה';f.$('program-filter').oninput();assert.match(f.$('program-list').innerHTML,/Browse episodes/);
 f.$('program-filter').value='absent';f.$('program-filter').oninput();assert.match(f.$('program-list').innerHTML,/No saved programs match/);f.$('program-filter').value='';f.$('program-filter').oninput();
 f.edit();assert.equal(f.$('program-kind').value,'omny');assert.equal(f.$('program-omny-fields').hidden,false);
 f.$('program-name').value='Edited';await f.submit();assert.equal(f.posts[0].body.revision,'7');assert(f.$('program-editor').hidden);
 f.add();f.$('program-name').value='New';f.$('program-feed').value='https://feeds.example/new.rss';await f.$('program-preview').onclick();assert.match(f.$('program-status').textContent,/1 episodes read/);assert.match(f.$('program-preview-results').innerHTML,/Episode &lt;one&gt;/);
 f.$('program-feed').listeners.input();assert.equal(f.$('program-preview-results').innerHTML,'');await f.submit();assert.equal(f.posts.at(-1).body.slot,'');assert.equal(f.posts.at(-1).body.kind,'rss');
 const invalid=await fixture();invalid.add();invalid.$('program-name').value='New';for(const url of ['','javascript:alert(1)','https://','https://user:pass@host/feed','https://host/a b','https://host/'+ 'a'.repeat(520)]){invalid.$('program-feed').value=url;await invalid.submit();}assert.equal(invalid.posts.length,0);
 invalid.$('program-feed').value='https://example.org/feed';invalid.$('program-name').value='א'.repeat(81);await invalid.submit();assert.equal(invalid.posts.length,0);
 const stale=await fixture();stale.edit();stale.$('program-name').value='Keep draft';stale.change();await stale.submit();assert(!stale.$('program-editor').hidden);assert.equal(stale.$('program-name').value,'Keep draft');assert.match(stale.$('program-status').textContent,/draft is kept/);
 const failed=await fixture();failed.edit();failed.reject(500);await failed.submit();assert.equal(failed.$('program-name').value,'אילנה דיין');assert(!failed.$('program-save').disabled);assert.match(failed.$('program-status').textContent,/rejected/);
 const pending=await fixture();pending.edit();pending.hold();const saving=pending.submit();await flush();assert(pending.$('program-save').disabled&&pending.$('program-name').disabled);await pending.submit();assert.equal(pending.posts.length,1);pending.release();await saving;assert(!pending.$('program-save').disabled);
 const removal=await fixture();removal.edit();removal.$('program-remove').onclick();assert(!removal.$('program-dialog').hidden);assert.equal(removal.posts.length,0);removal.$('program-dialog-cancel').onclick();assert(removal.$('program-dialog').hidden);removal.$('program-remove').onclick();await removal.$('program-dialog-confirm').onclick();assert.equal(removal.posts[0].url,'/api/programs/remove');assert(removal.$('program-editor').hidden);
 const favorite=await fixture();await favorite.button('favorite',0).onclick();assert.equal(favorite.posts[0].url,'/api/programs/favorite');
 const search=await fixture();search.add();search.$('program-query').value='אילנה דיין';await search.search();assert.match(search.searches[0],/country=IL/);assert.match(search.$('program-results').innerHTML,/Found &lt;show&gt;/);assert.doesNotMatch(search.$('program-results').innerHTML,/javascript/);search.$('button-data-program-result-0').onclick();assert.equal(search.$('program-feed').value,'https://feeds.example/show.rss');assert.equal(search.$('program-kind').value,'rss');assert.equal(search.posts.length,0);await search.search();assert.equal(search.searches.length,1);
 const late=await fixture();late.add();late.$('program-query').value='Old request';late.holdSearch();const fetching=late.search();await flush();late.$('program-cancel').onclick();late.releaseSearch();await fetching;assert.equal(late.$('program-results').innerHTML,'');
 const protectedDraft=await fixture();protectedDraft.edit();protectedDraft.$('program-query').value='Search';protectedDraft.holdSearch();const searching=protectedDraft.search();await flush();protectedDraft.$('program-name').value='Typed while searching';protectedDraft.releaseSearch();await searching;protectedDraft.$('button-data-program-result-0').onclick();assert.equal(protectedDraft.$('program-name').value,'Typed while searching');
 const navigation=await fixture();navigation.edit();navigation.$('program-name').value='Unsaved';let prevented=false;navigation.events.click({target:{closest:()=>({})},preventDefault(){prevented=true;},stopImmediatePropagation(){}});assert(prevented);
 const full=await fixture({full:true});assert(full.$('program-add').disabled);full.edit();assert(!full.$('program-save').disabled);
 const down=await fixture({down:true});assert(down.$('program-add').disabled);assert.match(down.$('program-list').textContent,/Could not load/);
 console.log('Production Programs browser script: filtering/add/edit/remove/favorites/preview/search/escaping/stale edits/draft preservation/request races/failure/capacity checks passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
