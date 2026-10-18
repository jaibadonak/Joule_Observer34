const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const html = fs.readFileSync(path.join(__dirname, '../web/index.html'), 'utf8');
const scripts = [...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(x => x[1]);
for (const script of scripts) new vm.Script(script);
const math = {module:{exports:{}}};
vm.runInNewContext(scripts[0], math);
const j = math.module.exports;
assert.equal(j.pf({vrms_mV:14000,irms_mA:0,power_mW:0}), null);
assert.equal(j.pf({vrms_mV:14000,irms_mA:100,power_mW:2000}), null);
assert.equal(j.pf({vrms_mV:14000,irms_mA:100,power_mW:700}), .5);
const wave = Array.from({length:32},(_,k)=>Math.sin(2*Math.PI*500*k*.000104)*19000);
assert.ok(Math.abs(j.frequency(wave,104)-500)<2);
assert.equal(j.frequency(Array(32).fill(360),104),null);
assert.equal(j.wave({dt_us:104,v_mV:[1,2],i_mA:[1]}),false);
assert.equal(j.valid({source:'uart',age_ms:null,vrms_mV:0,irms_mA:0,power_mW:0}),true);
assert.equal(j.valid({source:'uart',age_ms:-1,vrms_mV:0,irms_mA:0,power_mW:0}),false);
assert.match(j.csv([{t:0,v:14,i:0,p:0,e:null}]), /1970-01-01T00:00:00.000Z,14,0,0,\s*$/);
const calibration = {age_ms:0,adc_reference_mV:5000,voltage_scale_x100:10926,current_scale_x1000:3877,
  v_min:245,v_max:600,v_mean_x100:42200,v_rms_x10:1254,i_min:360,i_max:360,i_mean_x100:36000,i_rms_x10:0};
let result=j.calibrate(calibration,{vref:'5',voltage:'13.7',current:''});
assert.equal(result.errors.length,0);
assert.equal(result.cfg.v,10925);
assert.equal(result.cfg.i,3877);
assert.ok(j.calibrate(calibration,{vref:'5',voltage:'',current:'400'}).errors.length);
assert.ok(j.calibrate(calibration,{vref:'0',voltage:'',current:''}).errors.length);
assert.ok(j.calibrate({...calibration,v_min:0},{vref:'5',voltage:'13.7',current:''}).errors.length);
assert.match(j.header(result.cfg),/#define V_MV_PER_COUNT_X100 10925UL/);
fs.mkdirSync(path.join(__dirname,'Debug'),{recursive:true});
fs.writeFileSync(path.join(__dirname,'Debug/generated-calibration.h'),j.header(result.cfg));

// Execute the actual dashboard script with a small DOM/canvas adapter to cover
// deduplication, reset, stale, missing-waveform and calibration state handling.
const elements = new Map();
function element(id) {
  if (!elements.has(id)) elements.set(id, {id,textContent:'',value:id==='cal-vref'?'5':id==='window'?'0':'',
    disabled:false,hidden:false,checked:false,clientWidth:600,clientHeight:300,dataset:{},events:{},
    classList:{toggle(){}},setAttribute(k,v){this[k]=v;},addEventListener(k,f){this.events[k]=f;},
    scrollIntoView(){},click(){},getContext(){return new Proxy({measureText(){return{width:30};}},{get:(o,k)=>o[k]||(()=>{})});}});
  return elements.get(id);
}
const tabs=['v','i','p','e'].map(ch=>{const e=element('tab-'+ch);e.dataset.channel=ch;return e;});
const context={document:{getElementById:element,querySelectorAll:()=>tabs,createElement:()=>element('download')},
  window:{addEventListener(){}},location:{protocol:'http:',search:'?demo'},performance:{now:()=>10000},
  URLSearchParams,URL,Blob,Date,devicePixelRatio:1,setInterval(){},setTimeout(){},clearTimeout(){},console};
vm.createContext(context);
for(const script of scripts)vm.runInContext(script,context);
assert.equal(vm.runInContext('history.length',context),1);
const packet={source:'uart',age_ms:0,sequence:50,epoch:1,uptime_ms:20000,vrms_mV:13700,irms_mA:0,power_mW:0,
  energy_mWh:0,display_mode:0,wave_age_ms:null,dt_us:0,v_mV:[],i_mA:[],calibration};
context.packet=packet;
vm.runInContext('accept(packet); accept(packet)',context);
assert.equal(vm.runInContext('history.length',context),2);
assert.equal(element('irms').textContent,'0');
assert.equal(element('frequency').textContent,'— Hz');
assert.equal(element('download-calibration').disabled,false);
element('hold-calibration').events.click();
assert.match(element('cal-status').textContent,/Held at/);
context.packet={...packet,calibration:{...calibration,age_ms:4000}};
vm.runInContext('accept(packet);updateCalibration()',context);
assert.equal(element('download-calibration').disabled,false); // explicitly held capture
vm.runInContext('networkError=true; refreshStatus(); updateCalibration()',context);
assert.equal(element('status').textContent,'Disconnected');
assert.equal(element('download-calibration').disabled,true);
context.packet={...packet,sequence:51,epoch:2,energy_mWh:null};
vm.runInContext('accept(packet)',context);
assert.equal(vm.runInContext('history.length',context),1);
assert.equal(element('energy').textContent,'—');
assert.equal(vm.runInContext('calibrationHeld',context),null);
context.packet={...packet,sequence:52,epoch:2,calibration:{...calibration,age_ms:4000}};
vm.runInContext('accept(packet); updateCalibration()',context);
assert.equal(element('download-calibration').disabled,true);
vm.runInContext("channel='e';draw();",context);
assert.equal(element('view-wave').disabled,true);
console.log('Dashboard math, protocol states, calibration and history tests passed.');
