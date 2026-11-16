import type {LayoutRequirement} from './ai-contract.ts';
import type {Color} from './colors.ts';
import {oklab,deltaOK} from './color-science.ts';
export type LayoutRules={noAdjacent:boolean;smooth:boolean};
export function layoutRules(description:string,requirements?:LayoutRequirement[]):LayoutRules{
 if(requirements!==undefined)return {noAdjacent:requirements.some(r=>r.kind==='no_adjacent_repeat'),smooth:requirements.some(r=>r.kind==='smooth_neighbors')};
 // Compatibility for previously saved analyses. New analyses supply source-validated rules.
 const noAdjacent=/(?:같은|동일한?|똑같은)\s*색.{0,12}(?:연속|연달|붙|반복).{0,12}(?:안\s*돼|안\s*되|안\s*되게|않|금지|피해|말아)/.test(description)&&!/(?:금지하지|안\s*해도|않아도|상관없)/.test(description);
 const smooth=/(?:비슷한|유사한?|가까운)\s*색.{0,15}(?:연결|이어|이웃|끼리|배치)/.test(description)&&!/(?:연결하지|이어지지|배치하지)/.test(description);
 return {noAdjacent,smooth};
}
export function capAdjacentCounts(counts:number[],roles:string[],limit:number){
 const result=[...counts];
 for(let i=0;i<result.length;i++)if(result[i]>limit){let excess=result[i]-limit;result[i]=limit;
  const candidates=result.map((_,j)=>j).filter(j=>j!==i&&roles[j]===roles[i]);
  while(excess>0){const target=candidates.filter(j=>result[j]<limit).sort((a,b)=>result[a]-result[b]||a-b)[0];if(target===undefined)throw new Error('같은 색의 연속 금지와 현재 색 수·강조 비율을 함께 만족할 수 없습니다. 색 수를 늘리거나 강조 비율을 조정해 주세요.');result[target]++;excess--;}
 }
 return result;
}
export function arrangeForRequests(sequence:Color[],rules:LayoutRules,zones:Record<string,unknown>={}){
 if(!rules.noAdjacent&&!rules.smooth)return sequence;
 const colors=[...new Map(sequence.map(c=>[c.key,c])).values()],labs=colors.map(c=>oklab(c.hex)),index=new Map(colors.map((c,i)=>[c.key,i])),dist=labs.map(a=>labs.map(b=>deltaOK(a,b)));
 const cuts=new Set([0,sequence.length]);for(const [key,a,b] of [['head',0,50],['middle',100,150],['tail',200,250]] as const)if(zones[key]){cuts.add(a);cuts.add(b);}
 const bounds=[...cuts].sort((a,b)=>a-b),result:Color[]=[];
 for(let part=1;part<bounds.length;part++){
  const start=bounds[part-1],end=bounds[part],remaining=colors.map(c=>sequence.slice(start,end).filter(x=>x.key===c.key).length);
  for(let i=start;i<end;i++){
   const previous=result.length?index.get(result.at(-1)!.key)!:undefined,desired=index.get(sequence[i].key)!;
   const candidates=remaining.map((n,j)=>({n,j})).filter(({n,j})=>n>0&&(!rules.noAdjacent||j!==previous)).filter(({j})=>{
    if(!rules.noAdjacent)return true;const left=end-i-1;
    return remaining.every((n,k)=>n-(k===j?1:0)<=(k===j?Math.floor(left/2):Math.ceil(left/2)));
   });
   if(!candidates.length)throw new Error('구간별 색 사용량과 동일색 연속 금지를 동시에 지킬 수 없습니다. 구간 밝기 조건이나 색 수를 조정해 주세요.');
   candidates.sort((a,b)=>{
    const score=(j:number)=>dist[j][desired]*(rules.smooth?.8:3)+(previous===undefined?0:dist[j][previous]*(rules.smooth?5:.3));
    return score(a.j)-score(b.j)||b.n-a.n||a.j-b.j;
   });const chosen=candidates[0].j;remaining[chosen]--;result.push(colors[chosen]);
  }
 }
 return result;
}
