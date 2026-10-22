// Execute the production Device script with synthetic HTTP/file/flash responses.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/web_server.cpp','utf8');
const start=source.indexOf('case WebSection::Device:',source.indexOf('void appendSectionContent'));
const section=source.slice(start,source.indexOf('\n        break;',start));
// Extract the production script from the C++ raw string.
const production=section.slice(section.indexOf('R"JS((()=>{')+5,section.indexOf('</script>)JS"'));
new vm.Script(production);
const flush=()=>new Promise(r=>setImmediate(r));
function firmware({name='firmware.bin',size=128,chip=9,magic=0xe9,app=0xabcd5432,hold=false}={}) {
 let release;
 return {name,size,slice(){return {async arrayBuffer(){if(hold)await new Promise(r=>release=r);const b=new ArrayBuffer(36),v=new DataView(b);v.setUint8(0,magic);v.setUint16(12,chip,true);v.setUint32(32,app,true);return b;}};},release(){release();}};
}
async function fixture({down=false,busy=false}={}) {
 const nodes={},timers=[],events={},requests=[];let now=100000;
 const $=id=>nodes[id]||=({id,disabled:false,hidden:false,textContent:'',dataset:{},files:[],value:'',className:'',focus(){document.activeElement=this;}});
 const actions=['firmware-start','device-restart','device-reset','device-calibration-start','release-check','release-install'].map($);
 const document={activeElement:null,getElementById:$,querySelectorAll:()=>actions,addEventListener(name,fn){events[name]=fn;},dispatchEvent(){}};
 let data={freeSketchBytes:1000,updateInProgress:busy,uptimeSeconds:80,firmwareVersion:'0.2.3',firmwareBuild:'esp32s3',touchCalibrationState:'idle'};
 class XHR {
  constructor(){this.upload={};requests.push(this);}
  open(method,url){this.method=method;this.url=url;}
  send(form){this.form=form;}
  respond(status,body){this.status=status;this.responseText=JSON.stringify(body);this.onload();}
 }
 class Form {set(key,file,name){this.key=key;this.file=file;this.name=name;}}
 vm.runInNewContext(production,{document,window:{},URLSearchParams,DataView,FormData:Form,XMLHttpRequest:XHR,Event:class {},Date:{now:()=>now},AbortSignal:{timeout:()=>({})},
  setTimeout(fn,delay){timers.push({fn,delay});return timers.length;},clearTimeout(){},async fetch(url){if(down)throw Error('Disconnected');return {ok:true,json:async()=>url==='/api/update/status'?{busy:false,status:'idle',autoInstall:true}:{...data}};}});
 await flush();
 return {$,requests,events,change:next=>data={...data,...next},up:()=>down=false,tick:ms=>now+=ms,
  async choose(file){$('firmware-file').files=file?[file]:[];await $('firmware-file').onchange();},
  async confirm(){await $('device-dialog-confirm').onclick();},
  async poll(){const t=timers.findIndex(t=>t.delay===2000);assert(t>=0);const timer=timers.splice(t,1)[0];await timer.fn();await flush();},
  async upload(){await this.choose(firmware());$('firmware-start').onclick();assert.equal($('device-dialog').hidden,false);await this.confirm();}};
}
(async()=>{
 const f=await fixture();assert(f.$('firmware-start').disabled);await f.upload();
 assert.equal(f.requests.length,1);const request=f.requests[0];assert.equal(request.url,'/api/device/ota?size=128');assert.equal(request.form.key,'firmware');assert.equal(request.form.name,'firmware.bin');
 assert(f.$('firmware-file').disabled&&f.$('release-auto').disabled&&f.$('device-restart').disabled);
 request.upload.onprogress({lengthComputable:true,loaded:128,total:128});assert.match(f.$('firmware-status').textContent,/Waiting for firmware validation/);
 request.respond(202,{written:true,restarting:true});assert.match(f.$('firmware-status').textContent,/Waiting for the radio to restart/);
 f.tick(2000);f.change({uptimeSeconds:1,firmwareVersion:'0.2.4'});await f.poll();assert.match(f.$('firmware-status').textContent,/Running firmware 0.2.4/);assert(!f.$('firmware-file').disabled);
 for(const invalid of [{name:'firmware.elf'},{size:0},{size:1001},{chip:0},{magic:0xff},{app:0}]) {const bad=await fixture();await bad.choose(firmware(invalid));assert(bad.$('firmware-start').disabled);assert.equal(bad.requests.length,0);}
 const cancel=await fixture();await cancel.choose(firmware());cancel.$('firmware-start').onclick();cancel.$('device-dialog-cancel').onclick();assert.equal(cancel.requests.length,0);assert(cancel.$('device-dialog').hidden);
 const conflict=await fixture({busy:true});await conflict.choose(firmware());assert(conflict.$('firmware-start').disabled);
 const failed=await fixture();await failed.upload();failed.requests[0].respond(400,{error:'Invalid application image'});assert.match(failed.$('firmware-status').textContent,/Invalid application/);assert(!failed.$('firmware-start').disabled);assert(!failed.$('firmware-file').disabled);
 const timeout=await fixture();await timeout.upload();timeout.requests[0].ontimeout();assert.match(timeout.$('firmware-status').textContent,/response was lost/);assert(timeout.$('firmware-file').disabled);timeout.tick(2000);timeout.change({uptimeSeconds:1});await timeout.poll();assert.match(timeout.$('firmware-status').textContent,/check that this is the expected build/);
 const malformed=await fixture();await malformed.upload();malformed.requests[0].respond(200,{});assert.match(malformed.$('firmware-status').textContent,/response was lost/);
 const down=await fixture({down:true});await down.choose(firmware());assert(down.$('firmware-start').disabled);
 const stale=await fixture();const slow=firmware({hold:true});const pending=stale.choose(slow);await stale.choose(firmware({chip:0}));slow.release();await pending;assert(stale.$('firmware-start').disabled);
 const removed=await fixture();await removed.choose(firmware());await removed.choose(null);assert(removed.$('firmware-start').disabled);
 console.log('Production firmware browser flow: review/cancel, validation, transfer, exclusion, retry and reboot checks passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
