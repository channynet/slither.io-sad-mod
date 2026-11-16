import type {AIArrangement} from './ai-arrangement.ts';
import {instructionKinds,type StrictInstruction} from './strict-instructions.ts';
import {palette} from './colors.ts';
import type {ConceptInput} from './concept-input.ts';
import type {SemanticAnalysis,Tone} from './mood-semantics.ts';
import {theoryCatalog,type TheoryId} from './theory-settings.ts';
export type LayoutRequirement={kind:'no_adjacent_repeat'|'smooth_neighbors';quote:string};
export type RecommendedSettings={colorCount:number;accentPercent:number;reason:string};
export type AIAnalysis={arrangement?:AIArrangement|null;strictInstructions?:StrictInstruction[]|null;recommendedTheories?:{enabled:TheoryId[];reason:string};recommendedSettings?:RecommendedSettings;requirements?:LayoutRequirement[];ambiguities?:string[];unhandledRequests?:string[];summary:string;dimensions:SemanticAnalysis['dimensions'];base:Tone;accent:Tone|null;transition:{from:Tone;to:Tone;label:string}|null;readings:{source:'description'|'lyrics'|'subject';quote:string;meaning:string;application:string}[];warnings:string[]};
const object=(properties:Record<string,unknown>)=>({type:'object',properties,required:Object.keys(properties),additionalProperties:false});
const number=(minimum:number,maximum:number)=>({type:'number',minimum,maximum});
const string=(maxLength:number)=>({type:'string',minLength:1,maxLength});
const tone=object({h:number(0,360),s:number(0,1),l:number(.15,.8)});
export const analysisSchema=object({arrangement:{anyOf:[object({quote:string(120),summary:string(400),segments:{type:'array',minItems:1,maxItems:250,items:object({length:{type:'integer',minimum:1,maximum:250},keys:{type:'array',minItems:1,maxItems:38,items:{type:'string'}},note:string(80)})}}),{type:'null'}]},strictInstructions:{anyOf:[{type:'array',maxItems:12,items:object({kind:{type:'string',enum:instructionKinds},quote:string(120),keys:{type:'array',maxItems:38,items:{type:'string'}},value:{anyOf:[number(0,38),{type:'null'}]},start:{anyOf:[{type:'integer',minimum:1,maximum:250},{type:'null'}]},end:{anyOf:[{type:'integer',minimum:1,maximum:250},{type:'null'}]}})},{type:'null'}]},recommendedTheories:object({enabled:{type:'array',maxItems:14,items:{type:'string',enum:theoryCatalog.map(t=>t.id)}},reason:string(300)}),recommendedSettings:object({colorCount:{type:'integer',minimum:6,maximum:38},accentPercent:{type:'integer',enum:[5,10,15,20,25,30]},reason:string(300)}),requirements:{type:'array',maxItems:2,items:object({kind:{type:'string',enum:['no_adjacent_repeat','smooth_neighbors']},quote:string(120)})},ambiguities:{type:'array',maxItems:3,items:string(300)},unhandledRequests:{type:'array',maxItems:4,items:string(300)},summary:string(600),dimensions:object(Object.fromEntries(['brightness','chroma','temperature','contrast','movement'].map(k=>[k,number(-1,1)]))),base:tone,accent:{anyOf:[tone,{type:'null'}]},transition:{anyOf:[object({from:tone,to:tone,label:string(150)}),{type:'null'}]},readings:{type:'array',minItems:1,maxItems:12,items:object({source:{type:'string',enum:['description','lyrics','subject']},quote:string(120),meaning:string(300),application:string(300)})},warnings:{type:'array',maxItems:6,items:string(300)}});
function record(v:unknown):Record<string,unknown>{if(!v||typeof v!=='object'||Array.isArray(v))throw new Error('분석 결과 형식이 올바르지 않습니다.');return v as Record<string,unknown>;}
function text(v:unknown,max:number){if(typeof v!=='string'||!v.trim()||v.length>max)throw new Error('분석 결과의 설명을 확인하지 못했습니다.');return v;}
function numeric(v:unknown,min:number,max:number){if(typeof v!=='number'||!Number.isFinite(v)||v<min||v>max)throw new Error('분석 결과의 색상 수치가 범위를 벗어났습니다.');return v;}
function parseTone(v:unknown):Tone{const r=record(v);return {h:numeric(r.h,0,360),s:numeric(r.s,0,1),l:numeric(r.l,.15,.8)};}
export function parseConceptInput(value:unknown):ConceptInput{
 const r=record(value);if(!['feeling','object','music'].includes(String(r.kind)))throw new Error('분석할 대상 유형을 확인해 주세요.');
 const read=(v:unknown,max:number)=>{if(typeof v!=='string'||v.length>max)throw new Error('입력 내용과 글자 수를 확인해 주세요.');return v.trim();};
 const input:ConceptInput={kind:r.kind as ConceptInput['kind'],name:read(r.name,120),description:read(r.description,400),lyrics:read(r.lyrics,4000)};
 if(r.strict!==undefined){if(typeof r.strict!=='boolean')throw new Error('지시 모드 설정을 확인해 주세요.');if(r.strict)input.strict=true;}
 if(input.kind==='feeling')input.name='';if(input.kind!=='music')input.lyrics='';
 if(!input.description&&!input.lyrics&&!(input.kind==='object'&&input.name))throw new Error('느낌·특징 또는 가사를 입력해 주세요. 곡 제목만으로 내용을 추측하지 않습니다.');
 return input;
}
export function parseAnalysis(value:unknown,input:ConceptInput):AIAnalysis{
 const r=record(value),d=record(r.dimensions);
 const dimensions={brightness:numeric(d.brightness,-1,1),chroma:numeric(d.chroma,-1,1),temperature:numeric(d.temperature,-1,1),contrast:numeric(d.contrast,-1,1),movement:numeric(d.movement,-1,1)};
 if(!Array.isArray(r.readings)||!r.readings.length||r.readings.length>12||!Array.isArray(r.warnings)||r.warnings.length>6)throw new Error('분석 근거를 확인하지 못했습니다.');
 const readings=r.readings.map(value=>{const row=record(value),source=row.source as AIAnalysis['readings'][number]['source'];if(!['description','lyrics','subject'].includes(source))throw new Error('분석 근거의 출처가 올바르지 않습니다.');const quote=text(row.quote,120),original=source==='subject'?(input.kind==='object'?input.name:''):input[source];if(!original.includes(quote))throw new Error('AI가 제시한 인용이 입력 원문과 일치하지 않습니다. 다시 분석해 주세요.');return {source,quote,meaning:text(row.meaning,300),application:text(row.application,300)};});
 const transition=r.transition===null?null:record(r.transition);
 const additions:Partial<AIAnalysis>={};
 if(r.arrangement!==undefined&&r.arrangement!==null){
  const plan=record(r.arrangement),quote=text(plan.quote,120);
  if(!input.description.includes(quote))throw new Error('AI 배열 설계에는 특징·요청란의 원문 근거가 필요합니다.');
  if(!Array.isArray(plan.segments)||!plan.segments.length||plan.segments.length>250)throw new Error('AI 배열의 구간 목록을 확인해 주세요.');
  const segments=plan.segments.map(item=>{const v=record(item),length=numeric(v.length,1,250);
   if(!Number.isInteger(length)||!Array.isArray(v.keys)||!v.keys.length||v.keys.length>38||new Set(v.keys).size!==v.keys.length||v.keys.some(k=>!palette.some(c=>c.key===k)))throw new Error('AI 배열의 칸 수와 색상 키를 확인해 주세요.');
   return {length,keys:v.keys as string[],note:text(v.note,80)};
  });
  if(segments.reduce((sum,s)=>sum+s.length,0)!==250)throw new Error('AI가 설계한 배열의 총길이가 250칸이 아닙니다. 다시 분석해 주세요.');
  additions.arrangement={quote,summary:text(plan.summary,400),segments};
 }
 if(r.strictInstructions!==undefined&&r.strictInstructions!==null){
  if(!Array.isArray(r.strictInstructions)||r.strictInstructions.length>12)throw new Error('지시 점검 목록을 확인해 주세요.');
  additions.strictInstructions=r.strictInstructions.map(item=>{const v=record(item),kind=v.kind as StrictInstruction['kind'],quote=text(v.quote,120);
   if(!instructionKinds.includes(kind)||!input.description.includes(quote))throw new Error('지시에는 특징·요청란의 정확한 인용이 필요합니다.');
   if(!Array.isArray(v.keys)||v.keys.length>38||new Set(v.keys).size!==v.keys.length||v.keys.some(k=>!palette.some(c=>c.key===k)))throw new Error('지시에 지정된 색상 키를 확인해 주세요.');
   const colorRule=kind.endsWith('_colors');if(colorRule?!v.keys.length:!!v.keys.length)throw new Error('색상 지시 형식이 올바르지 않습니다.');
   const value=kind==='color_count'?numeric(v.value,6,38):kind==='accent_percent'?numeric(v.value,5,30):null;
   if(value!==null&&(!Number.isInteger(value)||(kind==='accent_percent'&&value%5!==0))||value===null&&v.value!==null)throw new Error('지원하는 색 수·비율을 확인해 주세요.');
   if(kind==='range_colors'){const start=numeric(v.start,1,250),end=numeric(v.end,1,250);if(!Number.isInteger(start)||!Number.isInteger(end)||start>end)throw new Error('구간은 머리 1칸부터 꼬리 250칸 사이의 시작·끝 정수로 지정해 주세요.');return {kind,quote,keys:v.keys as string[],value,start,end};}
   if(v.start!=null||v.end!=null)throw new Error('칸 번호는 구간 색상 지시에만 사용할 수 있습니다.');
   return {kind,quote,keys:v.keys as string[],value};
  });
 }
 if(r.recommendedTheories!==undefined){const selected=record(r.recommendedTheories);if(!Array.isArray(selected.enabled)||selected.enabled.length>theoryCatalog.length||new Set(selected.enabled).size!==selected.enabled.length||selected.enabled.some(id=>!theoryCatalog.some(t=>t.id===id)))throw new Error('추천 배색 이론 목록을 확인해 주세요.');additions.recommendedTheories={enabled:selected.enabled as TheoryId[],reason:text(selected.reason,300)};}
 if(r.recommendedSettings!==undefined){const settings=record(r.recommendedSettings),colorCount=numeric(settings.colorCount,6,38),accentPercent=numeric(settings.accentPercent,5,30);if(!Number.isInteger(colorCount)||accentPercent%5!==0)throw new Error('추천 색 수와 강조 비율의 단위를 확인해 주세요.');additions.recommendedSettings={colorCount,accentPercent,reason:text(settings.reason,300)};}
 if(r.requirements!==undefined){if(!Array.isArray(r.requirements)||r.requirements.length>2)throw new Error('배치 요청 형식이 올바르지 않습니다.');additions.requirements=r.requirements.map(item=>{const v=record(item);if(!['no_adjacent_repeat','smooth_neighbors'].includes(String(v.kind)))throw new Error('지원하지 않는 배치 조건입니다.');const quote=text(v.quote,120);if(!input.description.includes(quote))throw new Error('배치 조건은 특징·요청란의 원문 근거가 필요합니다.');return {kind:v.kind as LayoutRequirement['kind'],quote};});}
 for(const key of ['ambiguities','unhandledRequests'] as const)if(r[key]!==undefined){if(!Array.isArray(r[key])||r[key].length>(key==='ambiguities'?3:4))throw new Error('해석 확인 항목이 올바르지 않습니다.');additions[key]=r[key].map(v=>text(v,300));}
 return {...additions,summary:text(r.summary,600),dimensions,base:parseTone(r.base),accent:r.accent===null?null:parseTone(r.accent),transition:transition?{from:parseTone(transition.from),to:parseTone(transition.to),label:text(transition.label,150)}:null,readings,warnings:r.warnings.map(w=>text(w,300))};
}
export function toSemantic(analysis:AIAnalysis):SemanticAnalysis{
 const labels={description:'특징·요청',lyrics:'가사',subject:'대상'};
 return {requirements:analysis.requirements,active:true,hits:[],summary:analysis.summary,dimensions:analysis.dimensions,base:analysis.base,accent:analysis.accent??undefined,transition:analysis.transition??undefined,readings:analysis.readings.map(r=>({quote:`[${labels[r.source]}] ${r.quote}`,meaning:r.meaning,application:r.application}))};
}
