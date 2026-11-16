// Minimum-cost distinct assignment of palette colors to role slots (rectangular Hungarian).
// Solves the stated additive design cost; it does not score aesthetic harmony.
export function assignColors(costs:number[][]):number[]{
 const n=costs.length,m=costs[0]?.length??0;if(!n)return [];
 if(n>m||costs.some(row=>row.length!==m||row.some(v=>!Number.isFinite(v))))throw new Error('색 후보의 배정 조건을 확인해 주세요.');
 const u=Array(n+1).fill(0),v=Array(m+1).fill(0),p=Array(m+1).fill(0),way=Array(m+1).fill(0);
 for(let i=1;i<=n;i++){
  p[0]=i;let j0=0;const min=Array(m+1).fill(Infinity),used=Array(m+1).fill(false);
  do{
   used[j0]=true;const i0=p[j0];let delta=Infinity,j1=0;
   for(let j=1;j<=m;j++)if(!used[j]){const cur=costs[i0-1][j-1]-u[i0]-v[j];if(cur<min[j]){min[j]=cur;way[j]=j0;}if(min[j]<delta){delta=min[j];j1=j;}}
   for(let j=0;j<=m;j++)if(used[j]){u[p[j]]+=delta;v[j]-=delta;}else min[j]-=delta;
   j0=j1;
  }while(p[j0]!==0);
  do{const j1=way[j0];p[j0]=p[j1];j0=j1;}while(j0!==0);
 }
 const result=Array(n).fill(-1);for(let j=1;j<=m;j++)if(p[j])result[p[j]-1]=j-1;return result;
}
