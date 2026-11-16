import type {AIArrangement} from './ai-arrangement.ts';
import {arrangeColorRanges} from './range-layout.ts';
import {layoutRules,capAdjacentCounts,arrangeForRequests} from './layout-requirements.ts';
import {interpretMood,type MoodPlan,type Reading} from './mood-interpreter.ts';
import {type SemanticAnalysis,type SemanticContext} from './mood-semantics.ts';
import type {ConceptEvidence} from './concept-input.ts';
import {oklab,deltaOK,hslHex,contrastRatio,warmth,harmonyOffset,PREVIEW_BACKGROUND,type Harmony} from './color-science.ts';
import {resolveTheories,theoryCatalog,type TheorySettings,type TheoryId} from './theory-settings.ts';
import {opticalMean,optimizeTheoryLayout,layoutMeasurements} from './theory-layout.ts';
import {assignColors} from './color-assignment.ts';
export const SKIN_LENGTH=250;
export const percentage=(count:number)=>Math.round(count*1000/SKIN_LENGTH)/10;
export type Color={key:string;hex:string;name:string;star?:boolean};
export const palette:Color[]=[
['1','#AD4448','연한 빨강'],['2','#9F252B','빨강'],['3','#747474','은회색'],['4','#29219A','남색'],['5','#882328','진한 빨강'],['6','#72762C','올리브'],['7','#805324','갈색'],['8','#782C78','자주'],['9','#237B29','초록'],['0','#656CB9','페리윙클'],['-','#542183','진보라'],
['q','#8342B8','보라'],['w','#B5B93E','노랑'],['e','#3935B9','파랑 별'],['r','#3D3CC0','청보라 별'],['t','#4B239E','바이올렛'],['y','#B94805','주황'],['u','#4F9AAE','청록'],['i','#79766E','회색 금테'],['o','#3C963C','풀색'],['p','#16B645','선명한 초록'],
['a','#A92CB3','마젠타'],['s','#BABABA','밝은 회색'],['d','#777DB7','연보라 별'],['f','#3B3B3B','차콜'],['g','#B69743','금색'],['h','#2B7D60','깊은 청록'],['j','#5056B2','인디고'],['k','#656AC0','라벤더 블루'],['l','#3936B5','로열 블루'],
['z','#9A66B9','연보라'],['x','#878BC3','연한 파랑'],['c','#77B8BC','밝은 청록'],['v','#70BE69','연두'],['b','#B6BD61','레몬'],['n','#B98153','살구'],['m','#B87276','로즈'],[',','#B73138','루비']
].map(([key,hex,name])=>({key,hex,name,star:['e','r','d'].includes(key)}));
export const presets=[{name:'신비로운 바다',prompt:'신비로운 밤바다, 은은하게 빛나는 느낌',hex:'#649cad'},{name:'몽환적인 우주',prompt:'몽환적인 보라색 우주와 별빛',hex:'#9377d0'},{name:'강렬한 불꽃',prompt:'강렬하고 뜨거운 붉은 불꽃',hex:'#d77a4d'},{name:'싱그러운 숲',prompt:'차분하고 싱그러운 초록 숲',hex:'#76ad63'},{name:'사랑스러운 꿈',prompt:'부드럽고 사랑스러운 분홍빛 꿈',hex:'#ce8eb0'},{name:'차가운 금속',prompt:'차갑고 세련된 무채색 금속',hex:'#a6acb4'},{name:'다채로운 축제',prompt:'알록달록한 축제, 삼각 배색',hex:'#c98d43'},{name:'화려한 보석',prompt:'화려한 보석의 사각 배색',hex:'#a94a92'}];
type Profile={name:string;words:string[];h:number;s:number;l:number;mode:Harmony};
const profiles:Profile[]=[
{name:'깊은 바다의 빛',words:['바다','물결','파도','청록','수중','해양'],h:192,s:.5,l:.44,mode:'analogous'},
{name:'별빛이 흐르는 우주',words:['우주','몽환','신비','보라','별','밤','오로라'],h:253,s:.55,l:.48,mode:'split'},
{name:'타오르는 불꽃',words:['불','뜨거','강렬','열정','붉','빨강'],h:3,s:.75,l:.43,mode:'complementary'},
{name:'숲을 닮은 리듬',words:['숲','초록','싱그','자연','나무','풀'],h:127,s:.5,l:.45,mode:'analogous'},
{name:'부드러운 꿈',words:['사랑','분홍','핑크','꿈','귀여','달콤','벚꽃'],h:319,s:.4,l:.65,mode:'analogous'},
{name:'차가운 금속',words:['금속','무채','회색','검정','흰색','모노','세련'],h:225,s:.06,l:.42,mode:'mono'},
{name:'따스한 노을',words:['노을','따뜻','가을','주황','황금','금색','햇살'],h:31,s:.55,l:.52,mode:'complementary'},
{name:'다채로운 축제',words:['축제','다채','알록달록','삼각','삼색','트라이어드'],h:15,s:.72,l:.5,mode:'triad'},
{name:'보석의 네 가지 빛',words:['보석','사각','테트라드','네 가지','화려','쌍보색'],h:285,s:.68,l:.47,mode:'tetrad'},
{name:'네온의 에너지',words:['네온','사이버','형광','독특','미래'],h:285,s:.85,l:.48,mode:'split'}];
export function hsl(hex:string){const [r,g,b]=[1,3,5].map(i=>parseInt(hex.slice(i,i+2),16)/255);const max=Math.max(r,g,b),min=Math.min(r,g,b),d=max-min,l=(max+min)/2;let h=0;if(d)h=(max===r?((g-b)/d)%6:max===g?(b-r)/d+2:(r-g)/d+4)*60;return {h:(h+360)%360,s:d?d/(1-Math.abs(2*l-1)):0,l};}
function distance(a:number,b:number){return Math.min(Math.abs(a-b),360-Math.abs(a-b));}
export function requestedHarmony(text:string):Harmony|undefined{
 const matches=[...text.matchAll(/분할\s*보색|쌍보색|삼각(?:\s*배색)?|트라이어드|사각(?:\s*배색)?|테트라드|보색|유사색|무채색/g)];
 let result:Harmony|undefined;
 matches.forEach((m,i)=>{const tail=text.slice(m.index!+m[0].length,matches[i+1]?.index??text.length).split(/[,.!?;\n]/)[0];if(/말고|아닌|아니라|제외|금지|쓰지|사용하지|하지\s*마/.test(tail))return;result=/분할/.test(m[0])?'split':/사각|테트라드|쌍보색/.test(m[0])?'tetrad':/삼각|트라이어드/.test(m[0])?'triad':/유사색/.test(m[0])?'analogous':/무채색/.test(m[0])?'mono':'complementary';});
 return result;
}
type Role='main'|'support'|'accent';
const roleNames:Record<Role,string>={main:'주조',support:'보조',accent:'강조'};
export type MoodReport={arrangement?:(AIArrangement&{overridden:boolean});instructionChecks?:{quote:string;actual:string}[];ambiguities?:string[];ai?:{model:string};concept?:ConceptEvidence;semantic:SemanticAnalysis;input:string;readings:Reading[];warnings:string[];unhandled:string[];actuals:{label:string;value:string}[];requestedColors:number;actualColors:number};
export type Skin={theories:TheorySettings;report:MoodReport;name:string;colors:Color[];sequence:Color[];counts:number[];roles:Role[];groupCounts:number[];tags:string[];interpretation:string;reasons:{theoryId?:TheoryId;enabled?:boolean;title:string;body:string;status?:string;source?:string;sourceTitle?:string}[];metrics:{meanStep:number;warmPercent:number;coolPercent:number;neutralPercent:number;minBackgroundContrast:number;harmony:Harmony}};
type Target={h:number;s:number;l:number};
const labs=new Map(palette.map(c=>[c.hex,oklab(c.hex)]));
const lab=(c:Color)=>labs.get(c.hex)!;
function similarity(c:Color,target:Target,role:number,theories:TheorySettings){
 const v=hsl(c.hex),targetLab=oklab(hslHex(target.h,target.s,target.l));
 const hueTerm=distance(target.h,v.h)/180*Math.min(1,target.s*2,v.s*2)*.2;
 const temperature=theories.temperature?Math.abs(warmth(v.h,v.s)-warmth(target.h,target.s))*.12:0;
 const background=theories.background&&role===2?.25/contrastRatio(c.hex,PREVIEW_BACKGROUND):0;
 return deltaOK(lab(c),targetLab)*2+hueTerm+temperature+background;
}
function selectColors(targets:Target[],sizes:number[],mode:Harmony,plan:MoodPlan,available:Color[],theories:TheorySettings){
 const groups:Color[][]=[[],[],[]],slots:{role:number;target:Target;preferred:Set<string>}[]=[];
 for(let round=0;round<Math.max(...sizes);round++)for(let role=0;role<3;role++){
  if(round>=sizes[role])continue;
  const preferences=role===0?plan.main:role===2?plan.accent:[];
  const preference=preferences[round%preferences.length];
  const target={...targets[role],h:preference?preference.h:plan.semantic.active?(targets[role].h+(theories.wheel&&mode==='tetrad'&&role===0&&!plan.semantic.transition?(round%2?60:0):0)+360)%360:(targets[0].h+(theories.wheel?harmonyOffset(mode,role,round):0))%360};
  const preferred=new Set(preference?.keys??[]);
  slots.push({role,target,preferred});
 }
 const obeysRole=(c:Color,role:number)=>(plan.strictInstructions??[]).filter(r=>r.kind===(role===0?'main_colors':role===2?'accent_colors':'')).every(r=>r.keys.includes(c.key));
 const assigned=assignColors(slots.map(({role,target,preferred})=>available.map(c=>(obeysRole(c,role)?0:1000000)+selectionCost(c,target,targets[0],role,plan,theories)+(preferred.size&&!preferred.has(c.key)?3:0))));
 slots.forEach((slot,i)=>groups[slot.role].push(available[assigned[i]]));
 return groups;
}
function selectionCost(c:Color,target:Target,base:Target,role:number,plan:MoodPlan,t:TheorySettings){
 const temperature=plan.temperature?Math.sign(plan.temperature):plan.semantic.active?plan.semantic.dimensions.temperature:0;
 return similarity(c,target,role,t)+extraSelection(c,base,role,t,plan.semantic.active?(plan.semantic.dimensions.contrast+1)/2:1)+(t.temperature&&temperature?Math.abs(temperature-warmth(hsl(c.hex).h,hsl(c.hex).s))*.32:0);
}
function extraSelection(c:Color,base:Target,role:number,t:TheorySettings,strength:number){
 const v=lab(c),b=oklab(hslHex(base.h,base.s,base.l));let score=0;
 if(t.hueUnity&&role!==2)score+=distance(v.h,b.h)/180*.55*Math.min(1,b.C/.1,v.C/.1);
 if(t.nuance&&role!==2)score+=(Math.abs(v.L-b.L)+Math.abs(v.C-b.C))*.8;
 if(t.chromaUnity)score+=Math.abs(v.C-b.C)*1.8;
 if(t.figureGround&&role===2)score-=Math.min(.5,deltaOK(v,b))*.8*strength;
 return score;
}
function distribute(total:number,size:number,priority?:number[]){
 if(!size)return [];
 const result=Array(size).fill(1) as number[],extra=total-size,weights=priority??result.map((_,i)=>1/(1+i*.18)),sum=weights.reduce((a,b)=>a+b,0),shares=weights.map(w=>extra*w/sum);
 shares.forEach((s,i)=>result[i]+=Math.floor(s));
 const order=shares.map((s,i)=>({i,remainder:s-Math.floor(s)})).sort((a,b)=>b.remainder-a.remainder);
 for(let i=0,left=total-result.reduce((a,b)=>a+b,0);i<left;i++)result[order[i].i]++;
 return result;
}
function compose(colors:Color[],counts:number[],roles:Role[],seedText:string,calm:boolean,semantic:SemanticAnalysis,theories:TheorySettings,base:Target){
 let seed=2166136261;for(const char of seedText)seed=Math.imul(seed^char.charCodeAt(0),16777619);
 const random=()=>{seed|=0;seed=seed+0x6D2B79F5|0;let t=Math.imul(seed^seed>>>15,1|seed);t=t+Math.imul(t^t>>>7,61|t)^t;return ((t^t>>>14)>>>0)/4294967296;};
 const remaining=[...counts],indices:number[]=[];
 const differences=colors.map(a=>colors.map(b=>deltaOK(lab(a),lab(b))));
 const targetLab=oklab(hslHex(base.h,base.s,base.l));
 const mixes=theories.opticalMix?colors.map(a=>colors.map(b=>deltaOK(oklab(opticalMean(a.hex,b.hex)),targetLab))):[];
 const lightGaps=colors.map(a=>colors.map(b=>Math.abs(lab(a).L-lab(b).L)));
 for(let i=0;i<SKIN_LENGTH;i++){
  const focus=Math.sin(Math.PI*i/(SKIN_LENGTH-1))**2;
  const weights=remaining.map((n,j)=>{
   let w=n*(theories.areaBalance?(roles[j]==='main'?1.4-.5*focus:roles[j]==='support'?1:.3+1.9*focus):1);
   const previous=indices.at(-1);
   if(previous!==undefined){
    const difference=differences[previous][j];
    if((roles[j]==='accent')!==(roles[previous]==='accent')){if(theories.interaction){const strength=semantic.active?(semantic.dimensions.contrast+1)/2:1;w*=1+strength*(Math.min(1.5,difference*4+(theories.lightContrast?lightGaps[previous][j]*2:0))-.5);}}
    else if(theories.continuity)w*=Math.exp(-difference*(calm?9:3));
    if(semantic.active){const tension=(semantic.dimensions.contrast+semantic.dimensions.movement+2)/4;w*=Math.exp(-Math.abs(difference-(.035+tension*.29))*22);}
    if(theories.opticalMix)w*=Math.exp(-mixes[previous][j]*5);
    const groupAnchor=indices[i-i%18];if(theories.grouping&&groupAnchor!==undefined)w*=Math.exp(-differences[groupAnchor][j]*7);
    if(previous===j)w*=.12;
   }
   if(i>=2){let occurrences=0;for(let k=0;k<i-2;k++)if(indices[k]===indices[i-2]&&indices[k+1]===indices[i-1]&&indices[k+2]===j)occurrences++;w/=1+occurrences*.4;}
   return w;
  });
  let choice=weights.reduce((sum,w)=>sum+w,0)*random(),pick=remaining.findLastIndex(n=>n>0);
  for(let j=0;j<colors.length;j++){choice-=weights[j];if(choice<0){pick=j;break;}}
  indices.push(pick);remaining[pick]--;
 }
 return indices.map(index=>colors[index]);
}
function applySemanticTransition(sequence:Color[],semantic:SemanticAnalysis){
 if(!semantic.transition)return sequence;
 const {from,to}=semantic.transition,a=oklab(hslHex(from.h,from.s,from.l)),b=oklab(hslHex(to.h,to.s,to.l));
 const direction={L:b.L-a.L,a:b.a-a.a,b:b.b-a.b};
 const project=(c:Color)=>{const v=lab(c);return v.L*direction.L+v.a*direction.a+v.b*direction.b;};
 const ordered=sequence.map((c,i)=>({c,i})).sort((x,y)=>project(x.c)-project(y.c)||x.i-y.i);
 // Ten successive ranges follow the semantic direction; preserve local variation and all quotas.
 const result:Color[]=[];for(let i=0;i<SKIN_LENGTH;i+=25)result.push(...ordered.slice(i,i+25).sort((a,b)=>a.i-b.i).map(x=>x.c));
 return result;
}
function applyZoneTargets(sequence:Color[],plan:MoodPlan){
 const bounds={head:[0,50],middle:[100,150],tail:[200,250]} as const;
 const result:Array<Color|undefined>=Array(SKIN_LENGTH).fill(undefined);let pool=sequence.map((c,i)=>({c,i}));
 for(const zone of ['head','middle','tail'] as const){
  const direction=plan.zones[zone];if(!direction)continue;
  const [start,end]=bounds[zone],selected=[...pool].sort((a,b)=>(lab(a.c).L-lab(b.c).L)*(direction==='dark'?1:-1)||a.i-b.i).slice(0,end-start).sort((a,b)=>a.i-b.i);
  selected.forEach((item,i)=>result[start+i]=item.c);const used=new Set(selected.map(x=>x.i));pool=pool.filter(x=>!used.has(x.i));
 }
 let i=0;return result.map(c=>c??pool[i++].c);
}
export function generate(mood:string,accent=15,variation=0,colorCount=16,context:SemanticContext={},options:Partial<TheorySettings>={}):Skin{
 const theories=resolveTheories(options);
 const plan=interpretMood(mood,context);
 const rangeInstructions=[...(context.strictInstructions??[]),...(context.arrangementRanges??[])];
 const hasRanges=rangeInstructions.some(r=>r.kind==='range_colors');
 const layout=layoutRules(mood,context.analysis?.requirements);
 if(context.strictInstructions)layout.noAdjacent=context.strictInstructions.some(r=>r.kind==='no_adjacent_repeat');
 const sceneWords=new Set(['바다','우주','숲','금속','노을','축제','보석','네온','불']);
 const matched=(context.analysis?[]:profiles).map(p=>({p,hits:p.words.filter(w=>plan.positiveText.includes(w))})).filter(x=>x.hits.length).map(x=>({...x,score:x.hits.reduce((n,w)=>n+(sceneWords.has(w)?3:1),0)})).sort((a,b)=>b.score-a.score);
 const p={...(matched[0]?.p??profiles[0])};let hue=p.h,sat=p.s,light=p.l;
 const second=matched[1];if(second&&p.s>.1){const a=second.hits.length/(matched[0].hits.length+second.hits.length)*.5;hue=(hue+(((second.p.h-hue+540)%360)-180)*a+360)%360;}
 if(/밤|깊은/.test(plan.positiveText))light-=.06;
 const semantic=plan.semantic;
 if(semantic.active){
  const hasScene=matched.some(m=>m.hits.some(h=>sceneWords.has(h)));
  if(!hasScene){hue=semantic.base.h;sat=semantic.base.s;light=semantic.base.l;}
  else {sat=sat*.3+semantic.base.s*.7;light=light*.3+semantic.base.l*.7;}
  if(!matched.length)p.mode=semantic.dimensions.contrast>.35?'split':'analogous';
 }
 const explicitHarmony=requestedHarmony(mood);if(explicitHarmony)p.mode=explicitHarmony;
 if(plan.main.length){hue=plan.main[0].h;if(!context.analysis){sat=plan.main[0].s;light=plan.main[0].l;}}
 light+=plan.brightness;sat+=plan.chroma;
 const shift=plan.main.length?[0,3,-3,6,-6]:[0,12,-12,24,-24];
 hue=(hue+shift[Math.abs(variation)%5]+360)%360;sat=Math.max(.01,Math.min(.9,sat));light=Math.max(.15,Math.min(.8,light));
 const requestedColors=Math.round(Math.max(6,Math.min(palette.length,Number.isFinite(colorCount)?colorCount:16)));
 const excludedKeys=new Set(plan.excluded.flatMap(f=>f.keys)),onlyKeys=new Set(plan.only.flatMap(f=>f.keys));
 const available=palette.filter(c=>!excludedKeys.has(c.key)&&(!onlyKeys.size||onlyKeys.has(c.key))&&(context.strictInstructions??[]).every(r=>r.kind==='allowed_colors'?r.keys.includes(c.key):r.kind==='excluded_colors'?!r.keys.includes(c.key):true));
 if(!available.length)throw new Error('사용할 수 있는 색이 없습니다. “이 색만 사용”과 “제외할 색” 조건을 확인해 주세요.');
 let totalColors=Math.min(requestedColors,available.length);
 if(totalColors<requestedColors)plan.warnings.push(`색상 조건을 우선해 요청 ${requestedColors}색 대신 사용 가능한 ${totalColors}색을 적용했습니다.`);
 const nameKnown=matched.length>0||plan.readings.length>0;
 const unresolved=plan.unknown.filter(clause=>!profiles.some(p=>p.words.some(w=>clause.includes(w))));
 for(const item of matched.slice(0,3))plan.readings.push({quote:item.hits.join(' · '),meaning:item.p.name+'의 분위기',application:'일반 색조 후보에 반영하되 직접 지정한 색·제외 조건을 우선'});
 const targetPercent=Math.round(Math.max(5,Math.min(30,accent))/5)*5,n=Math.round(SKIN_LENGTH*targetPercent/100),groupCounts=[Math.round((SKIN_LENGTH-n)*.7),0,n];groupCounts[1]=SKIN_LENGTH-groupCounts[0]-n;
 const accentSize=totalColors===1?0:Math.min(n,Math.max(1,Math.round(totalColors*.2))),mainSize=Math.max(1,Math.round((totalColors-accentSize)*.62)),sizes=[mainSize,totalColors-accentSize-mainSize,accentSize];
 if(!theories.areaBalance&&totalColors>2){groupCounts[0]=Math.round((SKIN_LENGTH-n)*sizes[0]/(sizes[0]+sizes[1]));groupCounts[1]=SKIN_LENGTH-n-groupCounts[0];}
 if(totalColors===1){groupCounts[0]=SKIN_LENGTH;groupCounts[1]=0;groupCounts[2]=0;plan.warnings.push('허용색이 1개라 단색으로 구성하며 강조 비율·비반복 배치를 적용할 수 없습니다.');}
 if(totalColors===2){groupCounts[0]+=groupCounts[1];groupCounts[1]=0;}
 // A narrow explicit role (e.g. black main) must not require several distinct
 // colors of that role merely because the default slot ratio chose them.
 if(context.strictInstructions&&totalColors>2)for(const [role,kind] of [[0,'main_colors'],[2,'accent_colors']] as const){
  const rules=context.strictInstructions.filter(r=>r.kind===kind);if(!rules.length)continue;
  const capacity=available.filter(c=>rules.every(r=>r.keys.includes(c.key))).length;
  if(!capacity)throw new Error('지정한 주조색·강조색이 허용색 또는 제외 조건과 충돌합니다.');
  if(sizes[role]>capacity){sizes[1]+=sizes[role]-capacity;sizes[role]=capacity;}
 }
 const preferredMainKeys=new Set(plan.main.flatMap(f=>f.keys)),preferredAccentKeys=new Set(plan.accent.flatMap(f=>f.keys));
 const targets=[{h:hue,s:sat,l:light},{h:hue,s:sat*.82,l:light+.13},{h:hue,s:p.mode==='mono'?.04:Math.min(.85,sat+.15),l:p.mode==='mono'?.73:Math.max(.5,light+.16)}];
 if(semantic.active){
  targets[1].h=(hue+(theories.wheel?harmonyOffset(p.mode,1,0):0)+360)%360;
  targets[2].h=(hue+(theories.wheel?harmonyOffset(p.mode,2,0):0)+360)%360;
  const contrast=(semantic.dimensions.contrast+1)/2;
  targets[1].l=Math.min(.8,light+.035+contrast*.1);
  targets[2].l=Math.min(.8,light+.055+contrast*.22);
  targets[2].s=Math.min(.9,sat+.02+contrast*.17);
  if(semantic.accent)Object.assign(targets[2],semantic.accent);
  if(semantic.transition){Object.assign(targets[0],semantic.transition.from);Object.assign(targets[1],semantic.base);Object.assign(targets[2],semantic.accent??semantic.transition.to);for(const target of targets){target.l=Math.max(.15,Math.min(.8,target.l+plan.brightness));target.s=Math.max(.01,Math.min(.9,target.s+plan.chroma));}if(plan.main.length)Object.assign(targets[0],{h:hue,s:sat,l:light});}
 }
 if(plan.accent.length){targets[2].h=plan.accent[0].h;targets[2].s=Math.min(.9,plan.accent[0].s+Math.max(0,plan.chroma));targets[2].l=plan.accent[0].l;}
 if(!theories.lightContrast){if(!semantic.transition)targets[1].l=light;if(!semantic.accent&&!semantic.transition&&!plan.accent.length)targets[2].l=light;}
 if(!theories.chromaContrast){if(!semantic.transition)targets[1].s=sat;if(!semantic.accent&&!semantic.transition&&!plan.accent.length)targets[2].s=sat;}
 if(context.analysis){
  const contrast=(semantic.dimensions.contrast+1)/2;
  if(!semantic.transition){targets[1].h=(hue+(theories.wheel?harmonyOffset(p.mode,1,0):0)+360)%360;}
  if(!semantic.accent&&!semantic.transition&&!plan.accent.length){targets[2]={h:(hue+(theories.wheel?harmonyOffset(p.mode,2,0):0)+360)%360,s:Math.min(.9,sat+(theories.chromaContrast?.12*contrast:0)),l:Math.min(.8,light+(theories.lightContrast?.16*contrast:0))};}
 }
 if(theories.wheel&&p.mode==='mono')targets.forEach((target,i)=>{if(!(i===2&&(semantic.accent||plan.accent.length))&&!semantic.transition)target.s=.02;});
 const groups=selectColors(targets,sizes,p.mode,plan,available,theories);let colors=groups.flat(),roles=groups.flatMap((g,i)=>g.map(()=>['main','support','accent'][i] as Role)),counts=groups.flatMap((g,i)=>distribute(groupCounts[i],g.length,g.map((c,j)=>{
  const preferred=i===0?preferredMainKeys:i===2?preferredAccentKeys:new Set<string>();
  return (preferred.size?(preferred.has(c.key)?8:.15):1)*Math.exp(-selectionCost(c,semantic.active?targets[i]:{...targets[i],h:targets[0].h+(theories.wheel?harmonyOffset(p.mode,i,0):0)},targets[0],i,plan,theories)*(semantic.active?5:1.5))/(1+j*.1);
 })));
 if(layout.noAdjacent&&!hasRanges){const capped=capAdjacentCounts(counts,roles,Math.ceil(SKIN_LENGTH/2));if(capped.some((n,i)=>n!==counts[i]))plan.warnings.push('동일색 연속 금지를 지키도록 많이 쓰인 색의 칸 수를 같은 역할의 다른 색에 나눴습니다.');counts.splice(0,counts.length,...capped);}
 const calm=!plan.energy&&(!semantic.active||semantic.dimensions.movement<.2);
 const reference=applyZoneTargets(applySemanticTransition(compose(colors,counts,roles,`${mood}|${n}|${variation}|${totalColors}`,calm,semantic,theories,targets[0]),semantic),plan);
 let sequence=hasRanges?reference:optimizeTheoryLayout(arrangeForRequests(reference,layout,plan.zones),colors,roles,{theories,semantic,rules:layout,zones:plan.zones,base:targets[0],reference});
 if(hasRanges){
  const ranged=arrangeColorRanges(reference,available,colors,roles,requestedColors,[...rangeInstructions,...(layout.noAdjacent&&!rangeInstructions.some(r=>r.kind==='no_adjacent_repeat')?[{kind:'no_adjacent_repeat' as const,quote:mood,keys:[],value:null}]:[])],layout.smooth||theories.continuity);
  sequence=ranged.sequence;colors=ranged.colors;roles=ranged.roles;counts=ranged.counts;totalColors=colors.length;
  groupCounts.splice(0,3,...(['main','support','accent'] as const).map(role=>counts.reduce((sum,n,i)=>sum+(roles[i]===role?n:0),0)));
  plan.warnings.push('정확한 구간 색상을 자동 색 수·강조 비율·이야기 흐름·배색 이론보다 우선했습니다. 지정하지 않은 칸과 구간 안의 세부 색에 분위기를 반영했습니다.');
  if(totalColors!==requestedColors)plan.warnings.push(`구간 조건에 맞춰 설정 ${requestedColors}색 대신 실제 ${totalColors}색을 사용했습니다.`);
 }
 const names:Record<Harmony,string>={analogous:'유사색 중심',complementary:'보색 중심',split:'분할 보색 중심',mono:'무채색 중심',triad:'삼각 배색',tetrad:'사각 배색'};
 const harmonyDescription:Record<Harmony,string>={analogous:'0°·28°·55°의 인접한 색상각을 기본 축으로 사용합니다.',complementary:'주조색과 180° 떨어진 강조색을 기본 축으로 사용합니다.',split:'주조색에서 150°·210° 떨어진 강조·보조색을 기본 축으로 사용합니다.',mono:'색상각보다 낮은 크로마와 밝기 차이를 우선합니다.',triad:'0°·120°·240°로 떨어진 세 축 주변에서 색을 고릅니다.',tetrad:'0°·60°·180°·240°의 두 보색 쌍을 사용합니다. 주조색을 두 축에 나누어 네 방향을 반영합니다.'};
 const stepDiffs=sequence.slice(1).map((c,i)=>deltaOK(lab(c),lab(sequence[i])));
 const meanStep=stepDiffs.reduce((a,b)=>a+b,0)/(SKIN_LENGTH-1);
 const proportions={warm:0,cool:0,neutral:0};colors.forEach((c,i)=>{const v=hsl(c.hex),w=warmth(v.h,v.s);proportions[Math.abs(w)<.12?'neutral':w>0?'warm':'cool']+=counts[i];});
 const bgContrasts=colors.map(c=>contrastRatio(c.hex,PREVIEW_BACKGROUND));
 const L=colors.map(c=>lab(c).L),C=colors.map(c=>lab(c).C);
 const source={wheel:'https://www.adobe.com/uk/creativecloud/design/discover/color-wheel.html',oklab:'https://bottosson.github.io/posts/oklab/',delta:'https://www.w3.org/TR/css-color-4/#color-difference-OK',albers:'https://www.albersfoundation.org/learning/workshops/one-color-becomes-two',itten:'https://www.aic-publishing.org/ojs/index.php/JAIC/article/download/287/274/548',contrast:'https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html',munsell:'https://munsell.com/color-blog/the-munsell-book-of-color-1929-the-color-sphere/1000/'};
 const repeats=sequence.slice(1).filter((c,i)=>c.key===sequence[i].key).length;
 const layoutMeasured=layoutMeasurements(sequence,colors,roles,targets[0]);
 const roleMean=(role:Role,value:(c:Color)=>number)=>{let sum=0,n=0;colors.forEach((c,i)=>{if(roles[i]===role){sum+=value(c)*counts[i];n+=counts[i];}});return n?sum/n:0;};
 const baseLab=oklab(hslHex(targets[0].h,targets[0].s,targets[0].l));
 const theoryNotes:Partial<Record<TheoryId,string>>={};
 const note=(ids:TheoryId[],message:string)=>{ids.filter(id=>theories[id]).forEach(id=>theoryNotes[id]=(theoryNotes[id]?theoryNotes[id]+' ':'')+message);};
 if(theories.chromaUnity&&theories.chromaContrast)note(['chromaUnity','chromaContrast'],'크로마 통일과 대비가 함께 켜져 두 목표를 절충합니다.');
 if(theories.nuance&&theories.lightContrast)note(['nuance','lightContrast'],'밝기를 비슷하게 하는 목표와 역할별 밝기 차이를 절충합니다.');
 if(theories.hueUnity&&theories.wheel&&p.mode!=='analogous'&&p.mode!=='mono')note(['hueUnity','wheel'],'중심색 통일과 여러 색상각의 배색이 충돌해 절충합니다.');
 if(semantic.accent||semantic.transition||plan.main.length||plan.accent.length)note(['wheel'],'입력의 목표색·직접 지정한 색이 우선하므로 기본 색상환 축과 달라질 수 있습니다.');
 if(layout.smooth)note(['interaction','continuity','grouping','opticalMix'],'비슷한 이웃색 요청을 우선해 마지막 조정에서 전체 이웃 색차가 커지는 교환을 막습니다.');
 if(semantic.transition||Object.keys(plan.zones).length)note(['areaBalance','grouping','interaction'],'구간 밝기와 이야기의 변화가 우선하므로 강조색의 위치와 묶음이 제한됩니다.');
 if(semantic.active&&semantic.dimensions.contrast<0)note(['interaction','figureGround'],'낮은 대비의 분위기에 맞춰 추가 대비 강도를 줄입니다.');
 if(totalColors===38)plan.warnings.push('38색을 모두 사용하므로 특정 이론에 맞지 않는 색을 제외할 수 없습니다. 역할·사용량·배치에서 목표를 절충합니다.');
 plan.warnings.push(...new Set(Object.values(theoryNotes)));
 if(hasRanges)note(theoryCatalog.map(t=>t.id),'구간 위치와 허용색을 고정하고, 그 안에서 참고 배열과 색 차이에 반영합니다. 자동 비율·중앙 강조·전체 색 흐름은 구간을 바꿀 수 없습니다.');
 const measured:Partial<Record<TheoryId,string>>={
  wheel:`역할별 목표 HSL 색상각: ${targets.map(v=>Math.round(v.h)%360+'°').join(' · ')}${p.mode==='tetrad'&&theories.wheel&&!semantic.transition?' · 주조의 두 번째 축 '+Math.round((targets[0].h+60)%360)+'°':''}.`,
  lightContrast:`실제 사용량 가중 평균 OKLab L: 주조 ${(roleMean('main',c=>lab(c).L)*100).toFixed(1)}, 강조 ${(roleMean('accent',c=>lab(c).L)*100).toFixed(1)}.`,
  chromaContrast:`실제 평균 OKLCH 크로마: 주조 ${roleMean('main',c=>lab(c).C).toFixed(3)}, 강조 ${roleMean('accent',c=>lab(c).C).toFixed(3)}.`,
  continuity:`최종 배열의 일반 경계 평균 ΔEOK ${layoutMeasured.normalStep.toFixed(3)}.`,
  interaction:`최종 배열에서 강조 역할이 바뀌는 경계 ${layoutMeasured.accentBoundaries}곳, 평균 ΔEOK ${layoutMeasured.accentStep.toFixed(3)}.`,
  grouping:`최종 18칸 묶음에서 첫 색과 나머지 색의 평균 ΔEOK ${layoutMeasured.groupSpread.toFixed(3)}.`,
  opticalMix:`최종 이웃색의 선형 RGB 평균과 주조 목표 간 평균 ΔEOK ${layoutMeasured.mixError.toFixed(3)}. 실제 눈의 혼색 측정값은 아닙니다.`,
  areaBalance:`중앙 84~167칸의 강조색 ${sequence.slice(83,167).filter(c=>roles[colors.findIndex(x=>x.key===c.key)]==='accent').length}칸 / 전체 강조 ${groupCounts[2]}칸.`,
  chromaUnity:`중심 목표와 주조색의 사용량 가중 평균 크로마 차이 ${roleMean('main',c=>Math.abs(lab(c).C-baseLab.C)).toFixed(3)}.`,
  nuance:`중심 목표와 주조색의 평균 명도 차이 ${roleMean('main',c=>Math.abs(lab(c).L-baseLab.L)).toFixed(3)}, 크로마 차이 ${roleMean('main',c=>Math.abs(lab(c).C-baseLab.C)).toFixed(3)}.`,
  figureGround:`주조 목표와 강조색의 사용량 가중 평균 ΔEOK ${roleMean('accent',c=>deltaOK(lab(c),baseLab)).toFixed(3)}.`,
  background:`실제 강조색의 사용량 가중 평균 배경 대비율 ${roleMean('accent',c=>contrastRatio(c.hex,PREVIEW_BACKGROUND)).toFixed(2)}:1.`,
 };
 const actuals: MoodReport['actuals']=[
  {label:'이웃 색 연결 확인',value:`동일색이 이어진 경계 ${repeats}곳 / 249곳${layout.noAdjacent?' · 연속 금지 적용':''}${layout.smooth?' · 비슷한 색 우선 연결':''}`},
  {label:'선택한 배색 이론',value:theoryCatalog.filter(t=>theories[t.id]).map(t=>t.name).join(' · ')||'추가 배색 이론 모두 끔 · 분위기 목표와 직접 지정한 색 조건으로 구성'},
  {label:'설정 반영 순서',value:hasRanges?'정확한 구간·명시한 색상 조건 → 명시한 수치·연속 조건 → 분위기·배색 이론·자동 설정':'제외·한정 색 → 직접 지정한 중심·포인트 → 구간 밝기 → 감정·비유와 분위기 흐름 → 색 다양성'},
  {label:'밝기·선명도',value:`실제 OKLab L ${(Math.min(...L)*100).toFixed(1)}~${(Math.max(...L)*100).toFixed(1)} · 크로마 ${Math.min(...C).toFixed(3)}~${Math.max(...C).toFixed(3)}`},
  {label:'따뜻함·차가움',value:`실제 따뜻한 계열 ${percentage(proportions.warm)}% · 차가운 계열 ${percentage(proportions.cool)}% · 중간/저채도 ${percentage(proportions.neutral)}%`},
  {label:'포인트 비중',value:`실제 ${percentage(groupCounts[2])}% (${groupCounts[2]}칸) · 요청 ${targetPercent}%`},
 ];
 if(semantic.active){
  actuals.push({label:'실제 색조와 흐름',value:`250칸 평균 밝기 ${(sequence.reduce((n,c)=>n+lab(c).L,0)/SKIN_LENGTH*100).toFixed(1)} · 평균 크로마 ${(sequence.reduce((n,c)=>n+lab(c).C,0)/SKIN_LENGTH).toFixed(3)} · 이웃 색차 ${meanStep.toFixed(3)}`});
  if(semantic.transition)actuals.push({label:'머리 → 꼬리의 실제 변화',value:`첫 25칸 평균 밝기 ${(sequence.slice(0,25).reduce((n,c)=>n+lab(c).L,0)/25*100).toFixed(1)} → 끝 25칸 ${(sequence.slice(-25).reduce((n,c)=>n+lab(c).L,0)/25*100).toFixed(1)} · ${semantic.transition.label}`});
  if(semantic.transition&&Object.keys(plan.zones).length)plan.warnings.push('이야기의 색 변화보다 직접 지정한 구간 밝기를 우선했습니다.');
 }
 for(const f of [...plan.main,...plan.accent]){const count=sequence.filter(c=>f.keys.includes(c.key)).length;actuals.push({label:`${f.name} 계열 실제 사용`,value:`${count}칸 · ${percentage(count)}% · 키 ${colors.filter(c=>f.keys.includes(c.key)).map(c=>c.key).join(' ')||'없음'}`});if(!count)plan.warnings.push(`${f.name} 요청은 다른 색상 조건·색 수와 충돌해 이번 결과에 포함되지 않았습니다.`);}
 if(plan.excluded.length)actuals.push({label:'제외 조건 확인',value:plan.excluded.map(f=>f.name).join(' · ')+' 계열 0칸'});
 for(const zone of ['head','middle','tail'] as const)if(plan.zones[zone]){const start=zone==='head'?0:zone==='middle'?100:200;actuals.push({label:`${start+1}~${start+50}칸 실제 밝기`,value:`평균 OKLab L ${(sequence.slice(start,start+50).reduce((sum,c)=>sum+lab(c).L,0)/50*100).toFixed(1)} · ${plan.zones[zone]==='dark'?'어두운':'밝은'} 색 우선`});}
 if(requestedColors>24&&plan.main.length)plan.warnings.push('많은 색을 모두 사용하면서 중심색의 칸 수를 늘렸습니다. 중심 색조를 더 좁히려면 색 수를 줄이거나 “파랑만”처럼 한정할 수 있습니다.');
 const report:MoodReport={semantic,input:mood,readings:plan.readings,warnings:[...new Set(plan.warnings)],unhandled:unresolved,actuals,requestedColors,actualColors:totalColors};
 const moodName=semantic.active&&!matched.some(m=>m.hits.some(h=>sceneWords.has(h)))?[...new Set(semantic.hits.filter(h=>!h.negated).map(h=>h.label))].slice(0,3).join(' · '):p.name;
 const reasonIds:Array<TheoryId|undefined>=['wheel',undefined,'lightContrast','chromaContrast','temperature','areaBalance','interaction','continuity','background',undefined];
 return {theories,report,name:plan.main.length?plan.main.map(f=>f.name).join(' · ')+' 중심 · '+moodName:moodName,colors,sequence,counts,roles,groupCounts,metrics:{meanStep,warmPercent:percentage(proportions.warm),coolPercent:percentage(proportions.cool),neutralPercent:percentage(proportions.neutral),minBackgroundContrast:Math.min(...bgContrasts),harmony:p.mode},tags:[theories.wheel?names[p.mode]:'색상환 조화 끔',`${totalColors}색 사용`,'OKLab 색차'],interpretation:(semantic.active?semantic.summary:nameKnown?'입력에서 읽은 색상 조건과 분위기를 우선해 설계했습니다.':'등록되지 않은 분위기여서 기본 바다 색조로 시작했습니다.')+` 요청 ${requestedColors}색 → 실제 ${totalColors}색 · 아래 해석 기록에서 적용 내용을 확인하세요.`,reasons:[
  {title:'색상환 조화 · '+names[p.mode],status:'선택에 적용',source:source.wheel,sourceTitle:'Adobe · 색상환 조화',body:`${semantic.active?'감정·비유의 목표 색조와 복합 분위기를 우선하며, 색상환 각도는 남은 후보의 참고 축입니다. ':''}${plan.main.length||plan.accent.length?'직접 지정한 중심·포인트 색을 먼저 반영하고 남은 색에 일반 조화 규칙을 적용합니다. ':''}${harmonyDescription[p.mode]} 유사·보색·분할 보색·삼각·사각·무채색 중 분위기에 맞는 방식을 고릅니다. 각도는 RGB/HSL 기준이며 회화용 RYB 색상환과 다릅니다. 첨부 색으로 근사하므로 모든 색이 정확한 각도에 놓이지는 않습니다.`},
  {title:'OKLab · 지각 색차',status:'수식 구현',source:source.oklab,sourceTitle:'Björn Ottosson · OKLab',body:`첨부 대표색을 sRGB에서 OKLab으로 변환하고 ΔEOK 거리로 목표색과의 차이, 선택색끼리의 차이를 비교합니다. 250칸의 이웃한 색 사이 평균 ΔEOK는 ${meanStep.toFixed(3)}입니다. 이는 조화 점수나 모든 관찰 조건의 지각을 보장하는 수치는 아닙니다.`},
  {title:'명도 대비',status:'선택·배치에 적용',source:source.delta,sourceTitle:'W3C · OKLab 색차',body:`선택색의 OKLab L 범위는 ${(Math.min(...L)*100).toFixed(1)}~${(Math.max(...L)*100).toFixed(1)}입니다(0~100 환산). 목표색 유사도와 강조색 주변의 밝기 차이에 적용합니다. 기존 HSL 명도보다 지각을 고려하는 좌표를 사용합니다.`},
  {title:'크로마 대비 · 색의 선명도',status:'선택에 적용',source:source.oklab,sourceTitle:'Björn Ottosson · OKLab',body:`OKLCH 크로마 C=√(a²+b²)의 범위는 ${Math.min(...C).toFixed(3)}~${Math.max(...C).toFixed(3)}입니다. 채도를 조절한 목표색을 OKLab으로 변환해 비교합니다. 크로마는 HSL 채도 백분율과 다른 양이며, 각 색의 선명도를 같은 좌표계에서 다룹니다.`},
  {title:'한난 대비 · 따뜻함과 차가움',status:'응용 규칙',source:source.itten,sourceTitle:'JAIC · 이텐의 색채 대비 연구',body:`색 선택에 목표색과 한난 성향의 차이를 추가했습니다. 250칸 중 따뜻한 계열 ${percentage(proportions.warm)}%, 차가운 계열 ${percentage(proportions.cool)}%, 중간·저채도 ${percentage(proportions.neutral)}%입니다. 색상각·채도로 정한 연속 점수와 분류 경계는 자체 기준이며 켈빈 색온도나 보편적인 감정 공식이 아닙니다.`},
  {title:'면적 대비 · 이텐의 대비 체계 참고',status:'응용 규칙',source:source.itten,sourceTitle:'JAIC · 이텐의 색채 대비 연구',body:`250칸 중 주조 ${groupCounts[0]}칸(${percentage(groupCounts[0])}%) · 보조 ${groupCounts[1]}칸(${percentage(groupCounts[1])}%) · 강조 ${groupCounts[2]}칸(${percentage(groupCounts[2])}%)입니다. ${hasRanges?'구간 조건을 우선한 실제 역할별 사용량입니다.':`강조 목표 ${targetPercent}%를 가장 가까운 정수 칸 수로 반올림합니다.`} 색이 차지하는 양으로 역할을 나눕니다. 이텐의 대비 체계를 참고하되 약 7:3 분배와 개별 가중치는 자체 설계입니다. 선택된 ${totalColors}색이 모두 등장합니다.`},
  {title:'이웃색 대비 · 알버스 참고',status:'원리를 배치에 응용',source:source.albers,sourceTitle:'Albers Foundation · One Color Becomes Two',body:`같은 색도 주변에 따라 다르게 보인다는 원리를 참고했습니다. 강조 역할이 바뀌는 양쪽 경계의 OKLab 색차를 높이는 자체 규칙으로 응용합니다. 모든 재정렬이 끝난 뒤에도 이 경계를 평가합니다. 실제 동시 대비 착시나 색 외관을 시뮬레이션하는 모델은 아닙니다.`},
  {title:'색의 연속성 · 단계 변화',status:'배치 규칙',source:source.oklab,sourceTitle:'Björn Ottosson · 색 보간',body:`같은 역할 쪽의 이웃 색차를 줄이며, 강조 역할이 바뀌는 경계의 대비는 별도 스위치로 제어합니다. 최종 배열에서 양쪽 이웃을 다시 평가합니다. 이번 분위기에는 ${calm?'강한':'완만한'} 연결 가중치를 적용했습니다. 새 중간색을 만들지 않고 첨부 색만으로 이산적인 흐름을 만듭니다.`},
  {title:'배경과의 상대 휘도 대비',status:'계산·선택에 적용',source:source.contrast,sourceTitle:'W3C · 상대 휘도 대비',body:`미리보기 기준 배경 ${PREVIEW_BACKGROUND}와 대표색의 상대 휘도 대비율 범위는 ${Math.min(...bgContrasts).toFixed(2)}:1~${Math.max(...bgContrasts).toFixed(2)}:1입니다. 강조색 선택에도 같은 상대 휘도 대비율을 사용합니다. 휘도 대비율은 참고 측정값이며 게임의 광택·무늬까지 평가하거나 WCAG 합격을 뜻하지 않습니다.`},
  {title:'먼셀 · 색상·명도·채도의 분리',status:'개념 참고',source:source.munsell,sourceTitle:'Munsell · Hue, Value, Chroma',body:'색을 색상·명도·채도로 나누어 생각하는 구조를 참고했습니다. 실제 수치 계산에는 OKLab/OKLCH와 RGB/HSL을 사용합니다. 먼셀 표색계의 H V/C 좌표 변환이나 먼셀 색표 데이터는 구현하지 않았습니다.'},
 ].map((reason,i)=>{const id=reasonIds[i];return id?{...reason,theoryId:id,enabled:theories[id],status:theories[id]?(theoryNotes[id]?'다른 조건과 절충':reason.status):'적용 안 함',body:theories[id]?[reason.body,measured[id],theoryNotes[id]].filter(Boolean).join(' '):'이 이론의 추가 가중치를 제외했습니다. 입력의 색상 조건과 AI 목표, 실제 수치 측정은 유지합니다.'}:reason;}).concat(theoryCatalog.filter(t=>t.group==='추가').map(t=>({theoryId:t.id,enabled:theories[t.id],title:t.name,status:theories[t.id]?(theoryNotes[t.id]?'다른 조건과 절충':'응용 규칙 반영'):'적용 안 함',body:t.detail+(theories[t.id]?' '+[measured[t.id],theoryNotes[t.id]].filter(Boolean).join(' '):' 현재는 이 가중치를 계산에서 제외했습니다.'),source:t.source,sourceTitle:t.sourceTitle}))) as Skin['reasons']};
}
export {roleNames};
