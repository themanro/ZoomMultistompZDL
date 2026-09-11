const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const src=fs.readFileSync(require('node:path').join(__dirname,'../patch_editor.html'),'utf8');
function fn(name){const start=src.search(new RegExp('(?:async )?function '+name+'\\('));assert(start>=0);return src.slice(start,src.indexOf('\n}',start)+2);}
function context(extra={}){return vm.createContext({setTimeout,clearTimeout,performance,Promise,Map,Set,Math,Array,Number,Error,busy:false,syncing:false,midiActionActive:false,actionCancelled:false,patchGeneration:0,nextSend:0,dumpCooldownUntil:0,pendingDump:null,sxAccum:null,autoApplyPending:false,sleep:async()=>{},banner:()=>{},log:()=>{},updateActionState:()=>{},...extra});}
test('transaction rejects overlap and releases ownership after failure',async()=>{
 const c=context();vm.runInContext(fn('cancelDump')+'\n'+fn('midiAction'),c);
 let finish;let calls=0;
 const first=c.midiAction(()=>new Promise(r=>finish=r))();
 assert.equal(await c.midiAction(()=>calls++)(),false);assert.equal(calls,0);
 finish();await first;assert.equal(c.midiActionActive,false);
 await c.midiAction(()=>{throw Error('failed')})();assert.equal(c.midiActionActive,false);assert.equal(c.busy,false);
});
test('busy timeout never grants access',async()=>{
 let now=0;const c=context({busy:true,performance:{now:()=>now},sleep:async()=>{now+=50;}});
 vm.runInContext(fn('waitIdle'),c);await assert.rejects(c.waitIdle(100),/still busy/);assert.equal(c.busy,true);
});
test('dump registers before sending and refuses competing requests',async()=>{
 const c=context({zx:x=>x,send:()=>{}});vm.runInContext(fn('awaitDump')+'\n'+fn('cancelDump'),c);
 const first=c.awaitDump(1000);await new Promise(setImmediate);const owner=c.pendingDump;
 await assert.rejects(c.awaitDump(1000),/already pending/);assert.equal(c.pendingDump,owner);
 c.cancelDump();assert.equal(await first,null);
 c.send=()=>{const r=c.pendingDump;assert(r);clearTimeout(r.t);c.pendingDump=null;r.res([42]);};
 assert.deepEqual(await c.awaitDump(),[42]);
});
function applyContext(){
 const live={name:'Pedal rename',fx:[[1,42,25,70]]};let sent;
 const c=context({patch:{name:'Old name',fx:[[1,42,60,10]]},midiOut:{},READ_ONLY:false,DEVID:97,
 dirty:new Set(['0,2']),dirtyVersions:new Map([['0,2',1]]),lastEditedSlot:null,
 waitIdle:async()=>{},decodePatch:x=>JSON.parse(JSON.stringify(x)),encodePatch:x=>{sent=structuredClone(x);return sent;},
 zx:x=>x,send:()=>{},render:()=>{},reconcilePatch:p=>{c.patch=p;},readCurrent:async()=>structuredClone(live)});
 vm.runInContext(fn('applyToPedal'),c);return {c,live,getSent:()=>sent};
}
test('Apply preserves live name and untouched fields; clears only verified edits',async()=>{
 const {c,live}=applyContext();let reads=0;
 c.readCurrent=async()=>{if(reads++)live.fx[0][2]=60;const r=structuredClone(live);r.slice=()=>structuredClone(live);return r;};
 await c.applyToPedal();assert.equal(c.patch.name,'Pedal rename');assert.equal(c.patch.fx[0][3],70);assert.equal(c.dirty.size,0);
});
test('failed verification and newer revisions retain edits',async()=>{
 for(const newer of [false,true]){
 const {c,live}=applyContext();let reads=0;
 c.readCurrent=async()=>{if(reads++){if(newer){live.fx[0][2]=60;c.dirtyVersions.set('0,2',2);c.patch.fx[0][2]=61;}}
 const r=structuredClone(live);r.slice=()=>structuredClone(live);return r;};
 await c.applyToPedal();assert(c.dirty.has('0,2'));assert.equal(c.patch.fx[0][2],newer?61:60);
 }
});
test('read failure retains all edits and releases busy state',async()=>{
 const {c}=applyContext();c.readCurrent=async()=>null;
 await assert.rejects(c.applyToPedal(),/read failed/);assert(c.dirty.has('0,2'));assert.equal(c.busy,false);
});
test('obsolete timeout cannot erase a newer dump',async()=>{
 const timers=[];const c=context({setTimeout:f=>{timers.push(f);return timers.length;},clearTimeout:()=>{},zx:x=>x,send:()=>{}});
 vm.runInContext(fn('awaitDump')+'\n'+fn('cancelDump'),c);
 const first=c.awaitDump();await new Promise(setImmediate);c.cancelDump();await first;
 const second=c.awaitDump();await new Promise(setImmediate);const owner=c.pendingDump;
 timers[0]();assert.equal(c.pendingDump,owner);c.cancelDump();await second;
});
test('Apply aborts when pedal changes patch during the initial read',async()=>{
 const {c,live}=applyContext();let writes=0;c.send=()=>writes++;
 c.readCurrent=async()=>{c.patchGeneration++;return live;};
 await assert.rejects(c.applyToPedal(),/patch changed/);assert.equal(writes,0);assert(c.dirty.has('0,2'));
});
test('connect bank dialog reloads only for Yes; No and dismissal do nothing',async()=>{
 for(const answer of ['yes','no',null]){
  let reads=0;const dialog={returnValue:'yes',showModal(){if(answer!==null)this.returnValue=answer;this.onclose();}};
  const c=context({$:()=>dialog,readAll:async()=>{reads++;}});
  vm.runInContext(fn('offerBankReload'),c);await c.offerBankReload();
  assert.equal(reads,answer==='yes'?1:0);
 }
});

test('late-slot knob edits never emit unsupported live messages',()=>{
 const sent=[];let enables=0;const c=context({liveEditable:s=>s<3,editEnable:()=>enables++,zx:x=>x,send:x=>sent.push(x)});
 vm.runInContext(fn('sendParam'),c);
 for(let s=3;s<6;s++)c.sendParam(s,2,42);
 assert.equal(sent.length,0);assert.equal(enables,0);
 c.sendParam(2,2,300);assert.deepEqual(Array.from(sent[0]),[0x31,2,2,44,2]);
});
test('parameter echo updates only its knob, with no render or name mutation',()=>{
 let received;let renders=0;
 const c=context({DEVID:97,patch:{name:'DreamShot',fx:[[1,42,0]]},BYID:{42:{name:'Effect'}},
 KNOBREG:[[{_receive:v=>received=v}]],render:()=>renders++});
 vm.runInContext(fn('handleSysex'),c);
 c.handleSysex([240,82,0,97,49,0,2,63,0,247]);
 assert.equal(c.patch.fx[0][2],63);assert.equal(received,63);assert.equal(renders,0);assert.equal(c.patch.name,'DreamShot');
});
test('all slot parameter edits preserve raw name bytes and collectors',()=>{
 const codec=src.slice(src.indexOf('const BITS='),src.indexOf('function emptyPatch'));
 const c=context();vm.runInContext(codec+";globalThis.base=EMPTY146.slice();globalThis.nameidx=NAMIDX;",c);
 c.base[125]|=64;c.base[133]=85;c.base[141]|=3;
 const original=Array.from(c.nameidx,i=>c.base[i]);
 for(let s=0;s<6;s++)for(let k=2;k<11;k++){
  const p=c.decodePatch(c.base);p.fx[s][k]=73;const raw=c.encodePatch(p,c.base);
  assert.deepEqual(Array.from(c.nameidx,i=>raw[i]),original);
  assert.equal(raw[125]&64,c.base[125]&64);assert.equal(raw[133],85);assert.equal(raw[141]&3,3);
 }
 const p=c.decodePatch(c.base);p.name='New name';const raw=c.encodePatch(p,c.base);
 assert.equal(c.decodePatch(raw).name,'New name');assert.equal(raw[133],0);
});
test('same-topology refresh retains slot arrays and DOM while accepting real renames',()=>{
 let renders=0;let updates=0;const values=[];const old=[1,42,7];
 const c=context({patch:{name:'Old',fx:[old]},KNOBREG:[[{_receive:v=>values.push(v)}]],
 $:()=>({}),updateActionState:()=>updates++,render:()=>renders++});
 vm.runInContext(fn('reconcilePatch'),c);
 c.reconcilePatch({name:'New',fx:[[1,42,9]],dsp:0,curfx:0,maxfx:1});
 assert.equal(c.patch.fx[0],old);assert.equal(old[2],9);assert.equal(c.patch.name,'New');assert.equal(renders,0);assert.equal(updates,1);
 c.reconcilePatch({name:'New',fx:[[1,43,9]]});assert.equal(renders,1);
});
