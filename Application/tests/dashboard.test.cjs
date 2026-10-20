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
assert.equal(j.pf({power_factor_x1000:500}), .5);
assert.equal(j.apparent({apparent_mVA:5600}),5.6);
assert.equal(j.apparent({vrms_mV:14000,irms_mA:400}),null);
const wave = Array.from({length:32},(_,k)=>Math.sin(2*Math.PI*500*k*.000104)*19000);
assert.equal(j.frequency,undefined); // live frequency must come from AVR telemetry
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
assert.equal(element('download-calibration').disabled,true); // requires a reference and 8 unique captures
assert.equal(vm.runInContext('calibrationRows.length',context),1); // duplicate polls excluded
element('cal-voltage').value='13.7';
for(let k=1;k<8;k++){context.packet={...packet,sequence:50+k,calibration:{...calibration,sequence:k}};vm.runInContext('accept(packet)',context);}
assert.equal(element('download-calibration').disabled,false);
element('hold-calibration').events.click();
assert.match(element('cal-progress').textContent,/HELD/);
context.packet={...packet,calibration:{...calibration,age_ms:20000}};
vm.runInContext('accept(packet);updateCalibration()',context);
assert.equal(element('download-calibration').disabled,true); // held data cannot bypass disconnected/stale telemetry
vm.runInContext('networkError=true; refreshStatus(); updateCalibration()',context);
assert.equal(element('status').textContent,'Disconnected');
assert.equal(element('download-calibration').disabled,true);
context.packet={...packet,sequence:51,epoch:2,energy_mWh:null};
vm.runInContext('accept(packet)',context);
assert.equal(vm.runInContext('history.length',context),1);
assert.equal(element('energy').textContent,'—');
assert.equal(vm.runInContext('calibrationHeld',context),null);
context.packet={...packet,sequence:52,epoch:2,calibration:{...calibration,age_ms:20000}};
vm.runInContext('accept(packet); updateCalibration()',context);
assert.equal(element('download-calibration').disabled,true);
vm.runInContext("channel='e';draw();",context);
assert.equal(element('view-wave').disabled,true);
console.log('Dashboard math, protocol states, calibration and history tests passed.');

// The raw view uses actual unmodified ADC values, retains bias and disables overlay.
context.packet={...packet,sequence:53,epoch:2,dt_us:104,wave_age_ms:0,
  v_mV:[-19339,0,19339],i_mA:[-566,0,566],p_mW:[10946,0,10946],v_adc:[245,422,599],i_adc:[276,422,568],capture_stats:[[-19339,19339,0,15790],[-566,566,0,462],[0,10946,7297,8937],[245,599,422,446],[276,568,422,438]]};
vm.runInContext("accept(packet); channel='v';view='wave';",context);
element('signal-scale').value='raw';
element('overlay').checked=true;
element('signal-scale').events.change();
assert.equal(vm.runInContext('chartData().values.join()',context),'245,422,599');
assert.equal(element('stat-1').textContent,'245 counts');
assert.equal(element('stat-label-3').textContent,'RMS (includes DC)');
assert.equal(element('overlay').disabled,true);
assert.match(element('capture-note').textContent,/DC offset retained/);
vm.runInContext("channel='i';draw()",context);
assert.equal(element('stat-1').textContent,'276 counts');
element('signal-scale').value='scaled';element('overlay').checked=false;
element('signal-scale').events.change();
assert.equal(element('stat-1').textContent,'-566 mA');
assert.equal(element('stat-label-3').textContent,'Capture RMS');
context.packet={...context.packet,v_adc:[],i_adc:[]};
vm.runInContext('accept(packet)',context);
element('signal-scale').value='raw';element('signal-scale').events.change();
assert.equal(element('stat-1').textContent,'—');
assert.match(element('capture-note').textContent,/updated AVR and ESP32/);
assert.equal(j.rawWave({dt_us:104,v_mV:[-1,1],i_mA:[0,0],v_adc:[0,1024],i_adc:[360,360]}),false);
console.log('Signed waveform and raw ADC view tests passed.');

// RMS definitions, averaged variance, stable-reference gating and multipoint fitting.
assert.equal(j.stats([359,361]).acRms,1);
assert.ok(j.stats([359,361]).rms>360);
const average=j.averageCalibration([{...calibration,v_rms_x100:10000},{...calibration,v_rms_x100:10020}]);
assert.ok(Math.abs(j.rawRms(average.data,'v')-Math.sqrt((100**2+100.2**2)/2))<1e-10);
assert.ok(average.spread.v<1);
const fit=j.fitCalibration([{v:{raw:50,ref:5.5}},{v:{raw:100,ref:11}},{v:{raw:150,ref:16.5}}],{vref:5000,v:10926,i:3877});
assert.equal(fit.cfg.v,11000);assert.equal(fit.cfg.i,3877);assert.equal(fit.errors.length,0);
assert.ok(j.fitCalibration([{v:{raw:50,ref:5}},{v:{raw:100,ref:15}}],fit.cfg).errors.length>0);
for(const values of [[0,5,-5,2],[360,360,360],[0,1,2,10,11],[10,0]]){
 const curve=j.curve(values);
 for(let k=0;k<values.length;k++)assert.equal(curve[k*8][1],values[k]);
 for(const [x,y] of curve){const k=Math.min(values.length-2,Math.floor(x));assert.ok(y>=Math.min(values[k],values[k+1])-1e-10&&y<=Math.max(values[k],values[k+1])+1e-10);}
}
// Fresh captures after a reset; unstable voltage blocks its calibration only.
context.packet={...packet,epoch:3,calibration:{...calibration,sequence:100}};
element('cal-voltage').value='13.7';element('cal-current').value='';
for(let k=0;k<8;k++){context.packet={...context.packet,sequence:100+k,calibration:{...calibration,sequence:100+k,v_rms_x100:k===7?14000:12540}};vm.runInContext('accept(packet)',context);}
assert.equal(element('download-calibration').disabled,true);
assert.match(element('cal-status').textContent,/more than 1%/);
// Eight steady captures replace the moving-load window.
for(let k=0;k<8;k++){context.packet={...context.packet,sequence:108+k,calibration:{...calibration,sequence:108+k,v_rms_x100:12540}};vm.runInContext('accept(packet)',context);}
assert.equal(element('cal-add-point').disabled,false);
element('cal-add-point').events.click();
assert.equal(vm.runInContext('calibrationPoints.length',context),1);
assert.equal(vm.runInContext('calibrationRows.length',context),0);
assert.equal(element('download-calibration').disabled,false);
// Changing gain metadata invalidates reference points from the old firmware.
context.packet={...context.packet,calibration:{...context.packet.calibration,voltage_scale_x100:11000,sequence:200}};
vm.runInContext('accept(packet)',context);
assert.equal(vm.runInContext('calibrationPoints.length',context),0);
console.log('Precision RMS, calibration stability, multipoint and interpolation tests passed.');

assert.match(html,/id="current-peak"/);
context.packet={...packet,epoch:4,sequence:300,display_mode:1,ipeak_mA:566};
vm.runInContext('accept(packet)',context);
assert.equal(element('current-peak').textContent,'Peak 566 mA');
assert.equal(element('display-mode').textContent,'Current peak (mA)');
context.packet={...context.packet,ipeak_mA:null};vm.runInContext('accept(packet)',context);
assert.equal(element('current-peak').textContent,'Peak — mA');
assert.equal(j.valid({...packet,ipeak_mA:-1}),false);

// Display only the transmitted values, even when input summaries would imply others.
context.packet={...packet,epoch:5,sequence:301,apparent_mVA:6400,power_factor_x1000:567,frequency_mHz:499321};
vm.runInContext('accept(packet)',context);
assert.equal(element('apparent').textContent,'6.40 VA');
assert.equal(element('pf').textContent,'0.57');
assert.equal(element('frequency').textContent,'499.3 Hz');
console.log('AVR-only live measurement display tests passed.');
