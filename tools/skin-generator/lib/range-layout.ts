import type {Color} from './colors.ts';
import type {StrictInstruction} from './strict-instructions.ts';
import {deltaOK,oklab} from './color-science.ts';

type Role='main'|'support'|'accent';
// Explicit position masks are kept throughout selection and final arrangement.
// Soft counts/role proportions may yield; explicit constraints never do.
export function arrangeColorRanges(reference:Color[],available:Color[],originalColors:Color[],originalRoles:Role[],requestedCount:number,instructions:StrictInstruction[],smooth:boolean){
 const ranges=instructions.filter(r=>r.kind==='range_colors'),length=reference.length;
 for(const r of ranges)if(!Number.isInteger(r.start)||!Number.isInteger(r.end)||r.start!<1||r.end!>length||r.start!>r.end!)throw new Error('구간의 시작·끝은 1~250칸 사이의 정수여야 합니다.');
 const masks=reference.map((_,i)=>available.filter(c=>ranges.filter(r=>r.start!<=i+1&&r.end!>=i+1).every(r=>r.keys.includes(c.key))));
 const empty=masks.findIndex(m=>!m.length);
 if(empty>=0)throw new Error(`${empty+1}칸에 사용할 수 있는 색이 없습니다. 겹치는 구간 지시와 허용·제외색을 확인해 주세요.`);
 const pool=available.filter(c=>masks.some(m=>m.includes(c))),labs=new Map([...pool,...reference].map(c=>[c.key,oklab(c.hex)]));
 const distance=(a:Color,b:Color)=>deltaOK(labs.get(a.key)!,labs.get(b.key)!);
 const score=(c:Color)=>masks.reduce((sum,m,i)=>sum+(m.includes(c)?distance(c,reference[i]):0),0)/masks.filter(m=>m.includes(c)).length;
 const ranked=[...pool].sort((a,b)=>score(a)-score(b)||a.key.localeCompare(b.key));
 const noAdjacent=instructions.some(r=>r.kind==='no_adjacent_repeat');
 const explicitCount=instructions.find(r=>r.kind==='color_count')?.value;
 const selected:Color[]=[];
 // Cover every allowed set before filling optional diversity slots. If repeated
 // cells forbid equal neighbors, include a second candidate where available.
 const need=masks.map((m,i)=>noAdjacent&&m.length>1&&((i>0&&masks[i-1].every(c=>m.includes(c)))||(i+1<length&&masks[i+1].every(c=>m.includes(c))))?2:1);
 while(masks.some((m,i)=>selected.filter(c=>m.includes(c)).length<need[i])){
  const candidates=ranked.filter(c=>!selected.includes(c));
  candidates.sort((a,b)=>{
   const gain=(c:Color)=>masks.filter((m,i)=>m.includes(c)&&selected.filter(x=>m.includes(x)).length<need[i]).length;
   return gain(b)-gain(a)||score(a)-score(b);
  });
  if(!candidates.length)throw new Error('구간 색상 조건을 구성하지 못했습니다.');
  selected.push(candidates[0]);
 }
 const target=Math.min(pool.length,explicitCount??Math.max(requestedCount,selected.length));
 if(explicitCount&&selected.length>explicitCount)throw new Error('지정한 색 수로 모든 구간과 연속 조건을 구성하지 못했습니다. 색 수 또는 구간을 조정해 주세요.');
 // Find distinct cells in which each selected color can appear at least once.
 const match=(colors:Color[])=>{
  const owners=Array<number>(length).fill(-1);
  function place(color:number,seen:Set<number>):boolean{
   const positions=masks.map((m,i)=>({i,valid:m.includes(colors[color])})).filter(p=>p.valid).map(p=>p.i).sort((a,b)=>distance(colors[color],reference[a])-distance(colors[color],reference[b])||a-b);
   for(const i of positions){if(seen.has(i))continue;seen.add(i);if(owners[i]===-1||place(owners[i],seen)){owners[i]=color;return true;}}
   return false;
  }
  for(let c=0;c<colors.length;c++)if(!place(c,new Set()))return null;
  return owners;
 };
 for(const c of ranked){if(selected.length>=target)break;if(!selected.includes(c)&&match([...selected,c]))selected.push(c);}
 if(explicitCount&&selected.length!==explicitCount)throw new Error(`지정한 ${explicitCount}색을 구간 안에 모두 배치할 수 없습니다.`);
 const pins=match(selected);if(!pins)throw new Error('선택한 색을 지정 구간 안에 배치하지 못했습니다.');
 const roles:Role[]=selected.map(c=>{
  const old=originalRoles[originalColors.findIndex(x=>x.key===c.key)]??'support';
  const permitted=(role:Role)=>instructions.filter(r=>r.kind===(role==='main'?'main_colors':role==='accent'?'accent_colors':'')).every(r=>r.keys.includes(c.key));
  return permitted(old)?old:'support';
 });
 for(const [kind,role] of [['main_colors','main'],['accent_colors','accent']] as const){
  const rules=instructions.filter(r=>r.kind===kind);if(!rules.length||roles.includes(role))continue;
  const index=selected.findIndex((c,i)=>roles[i]==='support'&&rules.every(r=>r.keys.includes(c.key)));
  if(index>=0)roles[index]=role;
 }
 const accentPercent=instructions.find(r=>r.kind==='accent_percent')?.value;
 const quota=accentPercent==null?null:Math.round(length*accentPercent/100);
 if(quota!==null&&!roles.includes('accent')){
  const index=selected.findIndex((c,i)=>roles[i]==='support'&&instructions.filter(r=>r.kind==='accent_colors').every(r=>r.keys.includes(c.key)));
  if(index>=0)roles[index]='accent';
 }
 // Dynamic programming keeps every positional mask, optional adjacency rule,
 // pinned diversity cell and explicit accent quota intact through the last step.
 const n=selected.length,width=(quota??0)+1,size=n*width,back=Array.from({length},()=>new Int32Array(size).fill(-1));
 let previous=new Float64Array(size).fill(Infinity);
 const options=masks.map((m,i)=>selected.map((c,j)=>m.includes(c)&&(pins[i]<0||pins[i]===j)?j:-1).filter(j=>j>=0));
 for(let i=0;i<length;i++){
  const next=new Float64Array(size).fill(Infinity);
  for(const c of options[i]){
   const inc=quota!==null&&roles[c]==='accent'?1:0,cost=distance(selected[c],reference[i]);
   if(i===0){if(inc<width)next[c*width+inc]=cost;continue;}
   for(let p=0;p<n;p++){
    if(noAdjacent&&p===c)continue;
    const step=distance(selected[p],selected[c])*(smooth?.6:.05);
    for(let q=inc;q<width;q++){
     const from=p*width+q-inc,to=c*width+q,value=previous[from]+cost+step;
     if(value<next[to]){next[to]=value;back[i][to]=from;}
    }
   }
  }
  previous=next;
 }
 let last=-1;for(let c=0;c<n;c++){const index=c*width+(quota??0);if(Number.isFinite(previous[index])&&(last<0||previous[index]<previous[last]))last=index;}
 if(last<0)throw new Error('구간 색상·색 수·강조 비율·연속 조건을 함께 지키는 배열을 만들지 못했습니다. 조건을 조정해 주세요.');
 const sequence=Array<Color>(length);
 for(let i=length-1;i>=0;i--){sequence[i]=selected[Math.floor(last/width)];last=back[i][last];}
 return {sequence,colors:selected,roles,counts:selected.map(c=>sequence.filter(x=>x.key===c.key).length)};
}
