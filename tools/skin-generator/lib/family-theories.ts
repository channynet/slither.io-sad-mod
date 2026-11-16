import type {AIAnalysis} from './ai-contract.ts';
import {toSemantic} from './ai-contract.ts';
import type {Tone} from './mood-semantics.ts';
import {interpretMood} from './mood-interpreter.ts';
import {palette} from './colors.ts';
import {oklab,hslHex,deltaOK} from './color-science.ts';
export type FamilyTheorySettings={hue:boolean;nuance:boolean;accent:boolean};
export const defaultFamilyTheories:FamilyTheorySettings={hue:true,nuance:false,accent:false};
export const noFamilyTheories:FamilyTheorySettings={hue:false,nuance:false,accent:false};
const circular=(from:number,to:number)=>((to-from+540)%360)-180;
const hue=(h:number)=>(h%360+360)%360;
const clamp=(v:number,min:number,max:number)=>Math.max(min,Math.min(max,v));
export function familyTheoryCards(shared:AIAnalysis){
 const nearest=(tone:Tone)=>{const v=oklab(hslHex(tone.h,tone.s,tone.l));return [...palette].sort((a,b)=>deltaOK(oklab(a.hex),v)-deltaOK(oklab(b.hex),v))[0].name;};
 return [
  {id:'hue' as const,title:`${nearest(shared.base)} 계열로 연결`,body:'각 스킨의 중심 색조를 공통 테마 쪽으로 가까이 옮깁니다. 밝기와 선명함의 차이는 유지합니다.',basis:'동일 색상 조화를 참고한 테마 규칙',available:true},
  {id:'nuance' as const,title:'밝기·선명함으로 연결',body:`공통 테마의 ${nearest(shared.base)} 목표를 기준으로 밝기와 선명함의 차이를 줄입니다. 같은 계열의 작품처럼 톤을 맞출 때 사용합니다.`,basis:'NCS의 뉘앙스 공통성을 참고한 응용 규칙',available:true},
  {id:'accent' as const,title:shared.accent?`${nearest(shared.accent)} 포인트로 연결`:'포인트색으로 연결',body:shared.accent?'각 스킨에 이미 있는 포인트색을 공통 테마의 포인트색 쪽으로 가까이 옮깁니다. 포인트가 없는 스킨에는 새로 만들지 않습니다.':'공통 분석에서 별도 포인트색을 제안하지 않아 이번 묶음에는 적용할 수 없습니다.',basis:'포인트색의 공통성을 이용한 테마 규칙',available:!!shared.accent},
 ];
}
export function applyFamilyTheories(original:AIAnalysis,shared:AIAnalysis,settings:FamilyTheorySettings,description:string){
 const plan=interpretMood(description,{analysis:toSemantic(original)}),notes:string[]=[];
 const shift=settings.hue&&!plan.main.length?circular(original.base.h,shared.base.h)*.5:0;
 const lShift=settings.nuance?(shared.base.l-original.base.l)*.35:0,sShift=settings.nuance?(shared.base.s-original.base.s)*.35:0;
 const tone=(t:Tone):Tone=>({h:hue(t.h+shift),s:clamp(t.s+sShift,0,1),l:clamp(t.l+lShift,.15,.8)});
 if(settings.hue)notes.push(plan.main.length?'공통 색조 연결보다 직접 지정한 중심색을 우선했습니다.':'중심 색조와 이야기의 변화 색조를 공통 테마 쪽으로 가깝게 조정했습니다.');
 if(settings.nuance)notes.push('공통 테마를 기준으로 밝기·선명함 차이를 줄였습니다. 직접 지정한 구간 밝기는 우선합니다.');
 let accent=original.accent?{...original.accent}:null;
 if(settings.accent){
  if(!shared.accent||!accent)notes.push('공통 또는 개별 분석에 포인트색이 없어 포인트 연결은 생략했습니다.');
  else if(plan.accent.length)notes.push('공통 포인트 연결보다 직접 지정한 포인트색을 우선했습니다.');
  else{accent={h:hue(accent.h+circular(accent.h,shared.accent.h)*.5),s:(accent.s+shared.accent.s)/2,l:(accent.l+shared.accent.l)/2};notes.push('이미 있는 포인트색을 공통 포인트색에 가깝게 조정했습니다.');}
 }
 if(!settings.hue&&!settings.nuance&&!settings.accent)return {analysis:original,notes};
 return {analysis:{...original,base:tone(original.base),accent,transition:original.transition?{...original.transition,from:tone(original.transition.from),to:tone(original.transition.to)}:null},notes};
}
