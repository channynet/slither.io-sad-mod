import type {StrictInstruction} from './strict-instructions.ts';
import {concreteConcepts} from './concept-library.ts';
// These are editorial design associations, not universal emotion/color laws.
export type Tone={h:number;s:number;l:number};
type Dimensions={brightness:number;chroma:number;temperature:number;contrast:number;movement:number};
export type Concept={id:string;label:string;pattern:string;meaning:string;h:number;values:[number,number,number,number,number];category?:'object'|'music'|'scene'|'property'};
const concepts:Concept[]=[...concreteConcepts,
 {id:'solitude',label:'쓸쓸함',pattern:'쓸쓸|외로|외로운|고독|홀로|혼자 남|텅 빈|적막',meaning:'온기가 적고 여백이 느껴지는 정서',h:224,values:[-.5,-.55,-.7,-.4,-.7]},
 {id:'nostalgia',label:'그리움과 기억',pattern:'그리움|그리운|그립|추억|향수|오래된 기억|빛바랜|아련|옛날|그때 그',meaning:'시간이 지나 선명함은 줄고 온기가 남은 기억',h:32,values:[-.1,-.7,.5,-.6,-.65]},
 {id:'comfort',label:'위로와 안도',pattern:'위로|안도|안식|안락|품에|감싸|보듬|안아|쉬어 가|휴식',meaning:'경계가 풀리고 포근하게 보호받는 느낌',h:35,values:[.3,-.5,.75,-.8,-.8]},
 {id:'hope',label:'희망과 시작',pattern:'희망|희망찬|기대|설렘|설레|새로운 시작|새 출발|다시 피|새싹|여명|새벽빛',meaning:'닫힌 분위기가 열리고 작은 빛이 살아나는 느낌',h:62,values:[.8,.25,.25,.4,.3]},
 {id:'despair',label:'절망과 상실',pattern:'절망|상실|잃어버린|무너진|끝나 버린|버려진|허무|공허',meaning:'빛과 활력이 줄어든 무게감',h:242,values:[-.85,-.65,-.6,-.3,-.8]},
 {id:'freedom',label:'자유와 해방',pattern:'자유|해방|날아오|날아가는|비상|탁 트인|끝없이 펼쳐|해방감',meaning:'시야가 열리고 가볍게 뻗어나가는 느낌',h:185,values:[.65,.35,-.25,.2,.6]},
 {id:'elegance',label:'품격과 절제',pattern:'고급|우아|품격|기품|고귀|귀족|럭셔리',meaning:'차분한 바탕에 작은 대비로 존재감을 드러내는 느낌',h:275,values:[-.4,-.25,.05,.6,-.65]},
 {id:'minimal',label:'담백한 여백',pattern:'미니멀|담백|절제|여백|간결|단정|정갈',meaning:'자극을 덜고 정돈된 인상을 남기는 느낌',h:215,values:[.2,-.85,-.05,-.6,-.7]},
 {id:'innocence',label:'순수와 맑음',pattern:'순수|순진|청초|깨끗|맑은|맑게|투명|청명',meaning:'탁함과 무게를 덜어낸 가벼운 느낌',h:190,values:[.85,-.45,-.15,-.6,-.45]},
 {id:'playful',label:'장난과 경쾌함',pattern:'장난|발랄|경쾌|통통|통튀|톡톡|유쾌|명랑|즐거|즐겁|기쁨|행복',meaning:'가볍게 튀고 생기가 도는 리듬',h:48,values:[.6,.65,.5,.6,.85]},
 {id:'affection',label:'애정과 다정함',pattern:'사랑스|사랑|다정|애틋|애정|로맨틱|낭만|달콤',meaning:'가까이 다가가고 싶은 부드러운 온기',h:337,values:[.45,-.2,.6,-.45,-.3]},
 {id:'anxiety',label:'불안과 긴장',pattern:'불안|긴장|초조|위태|불길|불편|초현실적 공포',meaning:'안정되지 않은 대비와 멈추지 않는 긴장감',h:274,values:[-.4,.4,-.3,.9,.8]},
 {id:'chaos',label:'혼돈과 충돌',pattern:'혼돈|혼란|무질서|뒤엉|뒤섞|카오스|폭풍|소용돌이',meaning:'여러 힘이 부딪치며 방향이 바뀌는 느낌',h:290,values:[-.2,.65,-.1,.95,1]},
 {id:'order',label:'질서와 균형',pattern:'질서|균형|정돈|규칙적인|안정|견고|흔들리지',meaning:'일정한 무게와 흐름으로 중심을 잡는 느낌',h:205,values:[-.1,-.3,-.15,-.5,-.7]},
 {id:'sadness',label:'슬픔과 침잠',pattern:'슬픔|슬픈|슬프|우울|눈물|서글|비애|멜랑콜리',meaning:'활력이 가라앉고 조용히 안쪽으로 향하는 느낌',h:235,values:[-.55,-.45,-.65,-.5,-.75]},
 {id:'mystery',label:'신비와 미지',pattern:'신비|미지|비밀|수수께끼|알 수 없는|알수없는|오묘',meaning:'전부 드러나지 않은 깊이 속에 낯선 빛이 있는 느낌',h:265,values:[-.45,.15,-.45,.5,-.3]},
 {id:'dream',label:'꿈결과 몽환',pattern:'몽환|꿈결|꿈속|꿈 같은|꿈같|꿈꾸|현실과 꿈',meaning:'경계가 풀리며 부유하는 부드러운 비현실감',h:273,values:[.25,-.4,-.2,-.7,-.65]},
 {id:'ethereal',label:'덧없음과 잔향',pattern:'덧없|사라지|사라진|흩어지|흩어진|잔향|잔상|잔여|안개|희미',meaning:'또렷한 경계보다 남아 있는 기척에 가까운 느낌',h:235,values:[.3,-.75,-.25,-.8,-.8]},
 {id:'vast',label:'장엄함과 깊이',pattern:'장엄|웅장|광활|거대|끝없는|심연|압도|경외',meaning:'작은 빛과 넓은 어둠의 차이로 느끼는 규모감',h:238,values:[-.65,.1,-.4,.85,-.25]},
 {id:'resilience',label:'회복과 생명력',pattern:'회복|치유|되살아|살아나는|다시 일어|재생|생명|성장|피어나는',meaning:'가라앉은 상태에서 활력과 온기가 돌아오는 느낌',h:134,values:[.5,.35,.2,.25,.25]},
 {id:'quiet',label:'고요와 평온',pattern:'고요|평온|평화|잔잔|차분|평정|정적|침묵|숨을 고',meaning:'큰 자극 없이 천천히 이어지는 안정감',h:183,values:[.15,-.55,-.1,-.9,-.9]},
 {id:'passion',label:'열정과 분출',pattern:'열정|격정|분노|분출|폭발|타오르|타오른|불타|용솟음|뜨거운 마음',meaning:'안쪽의 힘이 밖으로 터져 나오는 느낌',h:8,values:[-.05,.9,.85,.85,.95]},
 {id:'fragile',label:'섬세함과 연약함',pattern:'섬세|연약|여린|여리|깨질 듯|깨질듯|유리 같은|유리같',meaning:'강한 자극보다 작고 미세한 차이가 느껴지는 인상',h:307,values:[.65,-.6,.1,-.75,-.6]},
 {id:'warmth',label:'온기와 포근함',pattern:'온기|포근|따스|따뜻|온화|햇살',meaning:'차가운 거리감이 줄어들고 가까워지는 느낌',h:32,values:[.35,-.2,.85,-.45,-.45]},
 {id:'urban',label:'도시의 거리감',pattern:'도시|도회|냉정|냉철|차갑|차가운|서늘|메마른|무심',meaning:'감정을 절제한 서늘하고 단단한 인상',h:220,values:[-.2,-.45,-.85,.35,-.25]},
 {id:'dread',label:'공포와 위압',pattern:'공포|섬뜩|으스스|무서|무섭|소름|음산|불길한|위압',meaning:'깊은 어둠 속 이질적인 대비로 생기는 긴장',h:276,values:[-.85,.1,-.5,1,.25]},
 {id:'antique',label:'낡음과 시간',pattern:'낡은|낡고|빈티지|앤티크|고풍|오래된|고대|세월|폐허',meaning:'빛과 선명함이 닳아 쌓인 시간의 질감',h:35,values:[-.35,-.65,.45,-.3,-.65]},
 {id:'resolve',label:'의지와 당당함',pattern:'의지|결의|결연|당당|자신감|용기|담대|강인|굳센',meaning:'중심을 분명히 세우고 힘을 집중하는 느낌',h:6,values:[-.25,.5,.35,.7,.2]},
 {id:'velvet',label:'벨벳 같은 깊이',pattern:'벨벳|실크|비단|매끈|보드라|부드러',meaning:'거친 차이를 줄이고 매끄럽게 이어지는 촉감의 비유',h:280,values:[-.15,-.3,.1,-.7,-.65]},
 {id:'electric',label:'낯선 미래감',pattern:'미래|사이버|디지털|전기|네온|기계적|인공적',meaning:'익숙한 자연색보다 선명한 이질감을 드러내는 느낌',h:285,values:[-.15,.9,-.3,.9,.65]},
];
const clamp=(v:number)=>Math.max(-1,Math.min(1,v));
export type SemanticHit={id:string;label:string;quote:string;meaning:string;weight:number;start:number;end:number;negated:boolean;role:'base'|'accent'|'start'|'end';tone:Tone;dimensions:Dimensions;origin?:'description'|'subject'|'lyrics';category?:Concept['category']};
export type SemanticAnalysis={requirements?:import('./ai-contract.ts').LayoutRequirement[];hits:SemanticHit[];active:boolean;summary:string;dimensions:Dimensions;base:Tone;accent?:Tone;transition?:{from:Tone;to:Tone;label:string};readings:{quote:string;meaning:string;application:string}[]};
const dimensions=(v:number[]):Dimensions=>({brightness:v[0],chroma:v[1],temperature:v[2],contrast:v[3],movement:v[4]});
function blend(hits:SemanticHit[]){
 hits=hits.filter((h,i)=>hits.findIndex(x=>x.id===h.id&&x.role===h.role)===i);
 const total=hits.reduce((n,h)=>n+h.weight,0)||1,values=[0,0,0,0,0];let x=0,y=0;
 for(const hit of hits){Object.values(hit.dimensions).forEach((v,i)=>values[i]+=v*hit.weight/total);x+=Math.cos(hit.tone.h*Math.PI/180)*hit.weight;y+=Math.sin(hit.tone.h*Math.PI/180)*hit.weight;}
 const d=dimensions(values.map(clamp));return {d,tone:{h:(Math.atan2(y,x)*180/Math.PI+360)%360,s:Math.max(.04,.45+d.chroma*.4),l:.46+d.brightness*.25}};
}
const feeling=(d:Dimensions)=>`${d.brightness>.2?'밝은':d.brightness<-.2?'깊고 어두운':'중간 밝기의'} 톤 · ${d.chroma>.2?'높은':d.chroma<-.2?'절제된':'중간'} 선명도 · ${d.contrast>.2?'큰':d.contrast<-.2?'작은':'중간'} 대비 · ${d.movement>.2?'활발한':d.movement<-.2?'잔잔한':'완만한'} 흐름`;
export function analyzeSemantics(input:string):SemanticAnalysis{
 const candidates=concepts.flatMap(c=>Array.from(input.matchAll(new RegExp(c.pattern,'gi')),m=>({c,start:m.index!,end:m.index!+m[0].length,quote:m[0]})));
 for(const match of input.matchAll(/\b(\d{2,3})\s*(?:bpm|비피엠)/gi)){
  const bpm=Number(match[1]);if(bpm<30||bpm>300)continue;const movement=clamp((bpm-110)/70);
  const c:Concept={id:'bpm',category:'music',label:`입력 템포 ${bpm} BPM`,pattern:'',meaning:`직접 입력한 ${bpm} BPM을 ${movement<-.2?'느린':movement>.2?'빠른':'중간'} 흐름의 설계 단서로 사용합니다. 음원에서 측정한 값은 아닙니다.`,h:210,values:[0,movement*.2,0,movement*.55,movement]};
  candidates.push({c,start:match.index!,end:match.index!+match[0].length,quote:match[0]});
 }
 candidates.sort((a,b)=>a.start-b.start||(b.end-b.start)-(a.end-a.start));
 const spans:typeof candidates=[];
 for(const candidate of candidates)if(!spans.some(s=>candidate.start<s.end&&candidate.end>s.start))spans.push(candidate);
 const hits:SemanticHit[]=spans.map((m,i)=>{
  const before=input.slice(Math.max(i?spans[i-1].end:0,m.start-12),m.start),after=input.slice(m.end,spans[i+1]?.start??input.length).split(/[,，;\n]|하지만|그리고/)[0];
  const negated=/^(?:적|적인|하게|한|로운|로움|스럽|스럽게|하지|롭지|지|은|은 느낌|한 느낌|함|함은|함이|감|감은|을|를|은|는|이|가|게|하게는|고|운|움|움은|\s){0,8}(?:않|아닌|아니라|말고|없이|없는|없게|싫|제외|빼|원하지)/.test(after)||/안\s+$/.test(before);
  const weight=/(아주|매우|극도로|최대한|압도적으로|엄청)\s*$/.test(before)?1.6:/(약간|조금|살짝|작은|한 줄기|한줄기)\s*$/.test(before)?.45:1;
  const d=dimensions(m.c.values.map(v=>clamp(v*weight)));return {id:m.c.id,category:m.c.category,label:m.c.label,quote:input.slice(m.start,negated?Math.min(input.length,m.end+after.length):m.end),meaning:m.c.meaning,weight,start:m.start,end:m.end,negated,role:'base' as const,tone:{h:m.c.h,s:Math.max(.04,.45+d.chroma*.4),l:.46+d.brightness*.25},dimensions:d};
 });
 const active=hits.filter(h=>!h.negated);
 const relation=input.match(/지나간\s*뒤|지나고|(?:에서|부터)[\s\S]*?(?:으로|로|까지)|끝에|뒤에|뒤의/);
 let transition:SemanticAnalysis['transition'];
 if(relation&&active.length>1){
  const pivot=relation.index!,left=active.filter(h=>h.start<pivot),right=active.filter(h=>h.start>=pivot);
  if(left.length&&right.length){left.forEach(h=>h.role='start');right.forEach(h=>h.role='end');transition={from:blend(left).tone,to:blend(right).tone,label:`${left.map(h=>h.label).join('·')} → ${right.map(h=>h.label).join('·')}`};}
 }
 if(!transition&&active.length>1){
  const pivot=input.match(/하지만|(?:하)?면서|지만|속(?:의|에)?|사이|한 줄기|한줄기/);
  if(pivot){const after=active.filter(h=>h.start>pivot.index!);if(active.some(h=>h.start<pivot.index!))after.forEach(h=>h.role='accent');}
  active.filter(h=>h.weight<.7&&h!==active[0]).forEach(h=>h.role='accent');
 }
 const base=blend(active.filter(h=>h.role!=='accent')),accentHits=active.filter(h=>h.role==='accent'),all=blend(active);
 const labels=(hs:SemanticHit[])=>[...new Set(hs.map(h=>h.label))].join(' · ');
 const summary=!active.length?'':transition?`${transition.label}: 머리에서 꼬리로 분위기가 변화합니다.`:accentHits.length?`${labels(active.filter(h=>h.role==='base'))}을 바탕으로, ${labels(accentHits)}을 포인트에 담습니다.`:`${labels(active)}을 담아 ${feeling(all.d)}으로 해석했습니다.`;
 return {hits,active:active.length>0,summary,dimensions:all.d,base:base.tone,accent:accentHits.length?blend(accentHits).tone:undefined,transition,readings:hits.map(h=>({quote:h.quote,meaning:h.negated?`${h.label}을 원하지 않는 표현`:h.meaning,application:h.negated?'이 연상을 색조 목표에서 제외':`${h.role==='accent'?'포인트':h.role==='start'?'머리에서 시작하는 분위기':h.role==='end'?'꼬리에 도착하는 분위기':'전체 분위기'}: ${feeling(h.dimensions)}${h.weight>1?' · 강하게':h.weight<1?' · 약하게':''}`}))};
}

export type SemanticContext={arrangementRanges?:StrictInstruction[];strictInstructions?:StrictInstruction[];subject?:string;lyrics?:string;analysis?:SemanticAnalysis};
export function interpretConceptSemantics(description:string,context:SemanticContext={}):SemanticAnalysis{
 const primary=analyzeSemantics(description);
 if(!context.subject&&!context.lyrics)return primary;
 const parts=[{analysis:primary,origin:'description' as const,weight:2,label:'특징·요청'},...(context.subject?[{analysis:analyzeSemantics(context.subject),origin:'subject' as const,weight:1,label:'대상'}]:[]),...(context.lyrics?[{analysis:analyzeSemantics(context.lyrics),origin:'lyrics' as const,weight:1,label:'가사'}]:[])];
 const hits=parts.flatMap(p=>p.analysis.hits.map(h=>({...h,origin:p.origin,weight:h.weight*p.weight})));
 const active=hits.filter(h=>!h.negated),base=blend(active.filter(h=>h.role!=='accent')),accentHits=active.filter(h=>h.role==='accent'),all=blend(active);
 const transition=parts.find(p=>p.analysis.transition)?.analysis.transition;
 const labels=[...new Set(active.map(h=>h.label))];
 const summary=transition?`${transition.label}: 읽어낸 변화 방향을 머리에서 꼬리로 표현합니다.`:active.length?`${labels.slice(0,4).join(' · ')}${labels.length>4?' 등의 특징':''}을 종합해 ${feeling(all.d)}으로 해석했습니다.${accentHits.length?' 대비되는 느낌은 포인트로 분리했습니다.':''}`:'';
 return {active:active.length>0,hits,dimensions:all.d,base:base.tone,accent:accentHits.length?blend(accentHits).tone:undefined,transition,summary,readings:parts.flatMap(p=>p.analysis.readings.map(r=>({...r,quote:`[${p.label}] ${r.quote}`})))};
}
