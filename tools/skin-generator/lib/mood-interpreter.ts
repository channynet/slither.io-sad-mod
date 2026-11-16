import type {StrictInstruction} from './strict-instructions.ts';
import {interpretConceptSemantics,type SemanticAnalysis,type SemanticContext} from './mood-semantics.ts';
export type Family={id:string;name:string;pattern:string;keys:string[];h:number;s:number;l:number};
export const families:Family[]=[
 {id:'red',name:'빨강',pattern:'빨강|빨간|빨갛|붉은|붉게|적색|레드|red',keys:['1','2','5',','],h:2,s:.7,l:.43},
 {id:'blue',name:'파랑',pattern:'파랑|파란|파랗|푸른|푸르게|청색|블루|blue',keys:['4','0','e','r','d','j','k','l','x'],h:237,s:.55,l:.48},
 {id:'purple',name:'보라',pattern:'보라|퍼플|바이올렛|purple|violet',keys:['-','q','t','8','z'],h:275,s:.55,l:.48},
 {id:'green',name:'초록',pattern:'초록|녹색|그린|green|연두',keys:['9','o','p','v','6'],h:125,s:.55,l:.46},
 {id:'cyan',name:'청록',pattern:'청록|민트|하늘색|시안|cyan|teal',keys:['u','c','h'],h:182,s:.45,l:.52},
 {id:'yellow',name:'노랑',pattern:'노랑|노란|노랗|노란색|옐로|yellow|레몬',keys:['w','b','g'],h:57,s:.6,l:.54},
 {id:'orange',name:'주황',pattern:'주황|오렌지|orange|살구',keys:['y','n'],h:28,s:.67,l:.48},
 {id:'pink',name:'분홍',pattern:'분홍|핑크|pink|로즈|마젠타',keys:['a','m'],h:325,s:.5,l:.6},
 {id:'brown',name:'갈색',pattern:'갈색|브라운|brown',keys:['7','n'],h:30,s:.5,l:.38},
 {id:'gold',name:'금색',pattern:'금색|골드|gold|황금',keys:['g','i'],h:43,s:.6,l:.5},
 {id:'black',name:'검정',pattern:'검정|검은색|검은|블랙|black|차콜',keys:['f'],h:0,s:0,l:.23},
 {id:'white',name:'흰색',pattern:'흰색|하얀색|하양|하얀|화이트|white',keys:['s'],h:0,s:0,l:.73},
 {id:'gray',name:'회색',pattern:'회색|그레이|그레이|gray|grey|은색|실버',keys:['3','i','s','f'],h:0,s:0,l:.5},
];
export type Reading={quote:string;meaning:string;application:string};
export type Zone='head'|'middle'|'tail';
export type MoodPlan={strictInstructions?:StrictInstruction[];semantic:SemanticAnalysis;input:string;positiveText:string;main:Family[];accent:Family[];excluded:Family[];only:Family[];readings:Reading[];warnings:string[];zones:Partial<Record<Zone,'dark'|'light'>>;brightness:number;chroma:number;temperature:number;energy:boolean;unknown:string[]};
const traits=[
 {pattern:'어둡|어두|깊은|깊게|다크|dark',kind:'light',value:-.13,label:'깊고 어두운 톤'},
 {pattern:'밝은|밝게|환한|환하게|화사|빛나는|bright',kind:'light',value:.13,label:'밝은 톤'},
 {pattern:'은은|부드|차분|고요|잔잔|파스텔|몽환|soft|calm',kind:'chroma',value:-.16,label:'절제된 선명도와 부드러운 연결'},
 {pattern:'선명|강렬|쨍|네온|형광|화려|vivid|neon',kind:'chroma',value:.19,label:'선명한 색과 강한 포인트'},
];
const negative=/(?:제외|빼|없|금지|쓰지|사용하지|넣지|싫|말고|아닌|아니라|아니고|빼줘|빼고|without|exclude|avoid)/i;
const negateTrait=/(?:지\s*않|하지\s*않|하지\s*마|하지\s*말|않은|않게|말고|아닌|아니라|없이)/;
export function interpretMood(input:string,context:SemanticContext={}):MoodPlan{
 const semantic=context.analysis??interpretConceptSemantics(input,context);
 const plan:MoodPlan={semantic,input,positiveText:input.toLowerCase(),main:[],accent:[],excluded:[],only:[],readings:[...semantic.readings],warnings:[],zones:{},brightness:0,chroma:0,temperature:0,energy:false,unknown:[]};
 for(const hit of semantic.hits.filter(h=>h.negated&&(!h.origin||h.origin==='description')))plan.positiveText=plan.positiveText.replace(hit.quote.toLowerCase(),'');
 const clauses=input.split(/[,，。.!?;；\n]|그리고|하지만|대신/).map(s=>s.trim()).filter(Boolean);
 const add=(list:Family[],f:Family)=>{if(!list.some(x=>x.id===f.id))list.push(f);};
 for(const clause of clauses){
  let recognized=false;
  const hits=families.flatMap(f=>Array.from(clause.matchAll(new RegExp(f.pattern,'gi')),m=>({f,index:m.index!,text:m[0]}))).sort((a,b)=>a.index-b.index||b.text.length-a.text.length).filter((x,i,a)=>i===0||x.index>=a[i-1].index+a[i-1].text.length);
  for(let i=0;i<hits.length;i++){
   const hit=hits[i],end=hits[i+1]?.index??clause.length,after=clause.slice(hit.index+hit.text.length,end),before=clause.slice(Math.max(0,hit.index-12),hit.index);
   const rest=clause.slice(hit.index+hit.text.length);
   const chained=/^(?:\s|과|와|색|은|는|이랑|랑|하고|및)+$/.test(after)&&negative.test(rest);
   const isExcluded=(negative.test(after)||chained||/제외(?:색)?\s*[:：]?\s*$/.test(before))&&!/제외하지|빼지/.test(after);
   recognized=true;
   if(isExcluded){add(plan.excluded,hit.f);plan.readings.push({quote:clause,meaning:`${hit.f.name} 계열 제외`,application:`첨부 색상표에서 ${hit.f.keys.join(' ')} 키를 후보에서 제거`});plan.positiveText=plan.positiveText.replace(new RegExp(hit.f.pattern,'gi'),'');continue;}
   const only=/^(?:색|계열)?\s*(?:만|으로만)|only/i.test(after)||(/^(?:\s|과|와|색|은|는|이랑|랑|하고|및)+$/.test(after)&&/만(?:\s|$|사용|써|넣)/.test(rest));
   const accent=/(포인트|강조|한\s*방울|조금|살짝)/.test(after)||/(포인트|강조)(?:색)?(?:은|는|으로)?\s*$/.test(before);
   if(only)add(plan.only,hit.f);
   add(accent?plan.accent:plan.main,hit.f);
   plan.readings.push({quote:clause,meaning:`${hit.f.name} ${only?'계열만 사용':accent?'포인트':'중심 색조'}`,application:only?'허용 색을 이 계열로 제한':accent?'강조색 후보를 우선 선택':'주조색 선택과 사용 칸 수에 높은 우선순위 부여'});
  }
  const zoneMatches=Array.from(clause.matchAll(/머리|앞쪽|중앙|중간|몸통|꼬리|끝쪽/g));
  for(let i=0;i<zoneMatches.length;i++){
   const match=zoneMatches[i],fragment=clause.slice(match.index!,zoneMatches[i+1]?.index??clause.length);
   const value=/어둡|어두|다크/.test(fragment)?'dark':/밝|환하|환한/.test(fragment)?'light':undefined;
   if(!value)continue;
   if(negateTrait.test(fragment)){plan.readings.push({quote:fragment,meaning:'부정한 밝기 지시',application:'해당 방향으로 밝기를 바꾸지 않음'});continue;}
   const zone:Zone=/머리|앞쪽/.test(match[0])?'head':/꼬리|끝쪽/.test(match[0])?'tail':'middle';
   plan.zones[zone]=value;recognized=true;plan.readings.push({quote:fragment,meaning:`${zone==='head'?'1~50':zone==='tail'?'201~250':'101~150'}칸을 ${value==='dark'?'어둡게':'밝게'}`,application:'해당 구간에 선택색 중 낮은/높은 명도 색을 우선 배치'});
  }
  if(context.analysis)continue; // AI owns meaning/tone; deterministic parsing only enforces explicit colors and zones.
  const temperature=clause.match(/따뜻|따스|포근|온화|차갑|차가|서늘|시원|warm|cool/i);
  if(temperature){
   recognized=true;const value=/차갑|차가|서늘|시원|cool/i.test(temperature[0])?-1:1;
   const tail=clause.slice(temperature.index!+temperature[0].length,temperature.index!+temperature[0].length+12);
   if(negateTrait.test(tail)||semantic.hits.some(h=>h.negated&&(!h.origin||h.origin==='description')&&h.quote.includes(temperature[0])&&clause.includes(h.quote)))plan.readings.push({quote:clause,meaning:'한난 지시를 부정함',application:'이 표현으로 따뜻함·차가움을 조정하지 않음'});
   else{plan.temperature+=value;plan.readings.push({quote:temperature[0],meaning:value<0?'차가운 계열 우선':'따뜻한 계열 우선',application:'직접 지정한 색을 유지하며 한난 성향에 선택 가중치 추가'});}
  }
  for(const trait of traits){
   const match=clause.match(new RegExp(trait.pattern,'i'));if(!match)continue;
   const tail=clause.slice(match.index!+match[0].length);
   if(negateTrait.test(tail.slice(0,14))||semantic.hits.some(h=>h.negated&&(!h.origin||h.origin==='description')&&h.quote.includes(match[0])&&clause.includes(h.quote))){plan.positiveText=plan.positiveText.replace(new RegExp(trait.pattern,'gi'),'');recognized=true;plan.readings.push({quote:clause,meaning:`${trait.label} 요청을 부정함`,application:'이 단어로 목표색을 조정하지 않음'});continue;}
   if(trait.kind==='light'&&zoneMatches.length)continue;
   const factor=/매우|아주|최대한|엄청|훨씬/.test(clause)?1.4:/약간|조금|살짝/.test(clause)?.55:1;
   if(trait.kind==='light')plan.brightness+=trait.value*factor;else plan.chroma+=trait.value*factor;
   if(trait.value>0&&trait.kind==='chroma')plan.energy=true;
   recognized=true;plan.readings.push({quote:match[0],meaning:trait.label,application:`목표 ${trait.kind==='light'?'명도':'채도'} ${trait.value>0?'상향':'하향'}${factor>1?' · 강하게':factor<1?' · 약하게':''}`});
  }
  // Unmatched phrases remain visible; no fabricated semantic confidence score.
  if(!recognized&&!semantic.hits.some(h=>clause.includes(h.quote)))plan.unknown.push(clause);
 }
 const excludedIds=new Set(plan.excluded.map(f=>f.id));
 for(const f of [...plan.main,...plan.accent,...plan.only])if(excludedIds.has(f.id))plan.warnings.push(`${f.name}의 사용·제외 요청이 겹쳐 제외를 우선했습니다.`);
 plan.main=plan.main.filter(f=>!excludedIds.has(f.id));plan.accent=plan.accent.filter(f=>!excludedIds.has(f.id));
 if(plan.brightness===0&&/어둡|어두/.test(input)&&/밝/.test(input)&&!Object.keys(plan.zones).length)plan.warnings.push('밝게·어둡게 요청이 함께 있어 중간 밝기를 목표로 했습니다. 구간을 지정하면 나누어 적용할 수 있습니다.');
 if(context.strictInstructions){plan.strictInstructions=context.strictInstructions;plan.main=[];plan.accent=[];plan.only=[];plan.excluded=[];plan.warnings=[];plan.readings=[...semantic.readings];}
 return plan;
}
