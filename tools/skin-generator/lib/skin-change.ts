import type {Skin} from './colors.ts';

export function compareSkins(before:Skin,after:Skin){
 const old=new Map(before.colors.map((c,i)=>[c.key,{count:before.counts[i],role:before.roles[i]}]));
 const next=new Map(after.colors.map((c,i)=>[c.key,{count:after.counts[i],role:after.roles[i]}]));
 const keys=new Set([...old.keys(),...next.keys()]);
 return {
  changedCells:after.sequence.filter((c,i)=>c.key!==before.sequence[i]?.key).length,
  addedColors:after.colors.filter(c=>!old.has(c.key)).length,
  usageChangedColors:[...keys].filter(k=>(old.get(k)?.count??0)!==(next.get(k)?.count??0)).length,
  roleChangedColors:[...keys].filter(k=>old.has(k)&&next.has(k)&&old.get(k)!.role!==next.get(k)!.role).length,
 };
}
export type SkinChange=ReturnType<typeof compareSkins>;
export function skinChangeMessage(change:SkinChange){
 if(!change.changedCells)return `설정은 반영됐지만 현재 목표색·허용색·사용량 조건에서는 250칸의 색이 이전과 같습니다.${change.roleChangedColors?` 색의 역할 분류만 ${change.roleChangedColors}개 바뀌었습니다.`:''}`;
 return `250칸 중 ${change.changedCells}칸 색 변경 · 새로 선택한 색 ${change.addedColors}개 · 사용량이 바뀐 색 ${change.usageChangedColors}개`;
}
