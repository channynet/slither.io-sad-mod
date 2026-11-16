import type {Color} from './colors.ts';
import type {TheorySettings} from './theory-settings.ts';
import type {SemanticAnalysis} from './mood-semantics.ts';
import type {LayoutRules} from './layout-requirements.ts';
import {oklab,deltaOK,hslHex,linearRgb} from './color-science.ts';

export function opticalMean(a:string,b:string){
 const x=linearRgb(a),y=linearRgb(b);
 return '#'+x.map((v,i)=>{const n=(v+y[i])/2,s=n<=.0031308?n*12.92:1.055*n**(1/2.4)-.055;return Math.round(s*255).toString(16).padStart(2,'0');}).join('');
}
type Options={theories:TheorySettings;semantic:SemanticAnalysis;rules:LayoutRules;zones:Record<string,unknown>;base:{h:number;s:number;l:number};reference?:Color[]};

// Improve the final layout, after every quota/zone/request pass. Swaps conserve all counts.
// This is a bounded local search over explicit design costs, not a visual-perception model.
export function optimizeTheoryLayout(sequence:Color[],colors:Color[],roles:string[],options:Options){
 const {theories:t,semantic,rules,zones,base,reference=sequence}=options;
 if(![t.continuity,t.interaction,t.grouping,t.opticalMix,t.areaBalance].some(Boolean))return sequence;
 const lookup=new Map(colors.map((c,i)=>[c.key,i])),values=colors.map(c=>oklab(c.hex)),n=sequence.length;
 const indices=sequence.map(c=>lookup.get(c.key)!),dist=values.map(a=>values.map(b=>deltaOK(a,b)));
 const b=oklab(hslHex(base.h,base.s,base.l));
 const mix=t.opticalMix?colors.map(a=>colors.map(c=>deltaOK(oklab(opticalMean(a.hex,c.hex)),b))):[];
 const strength=semantic.active?(semantic.dimensions.contrast+1)/2:1;
 const tension=semantic.active?(semantic.dimensions.contrast+semantic.dimensions.movement+2)/4:0;
 const pair=values.map((a,i)=>values.map((v,j)=>{
  const boundary=(roles[i]==='accent')!==(roles[j]==='accent'),d=dist[i][j];let cost=0;
  if(t.continuity&&!boundary)cost+=d*3;
  if(t.interaction&&boundary)cost-=d*strength*1.5;
  if(t.opticalMix)cost+=mix[i][j]*3;
  if(semantic.active)cost+=Math.abs(d-(.035+tension*.29))*2;
  if(i===j)cost+=.08;
  return cost;
 }));
 const edges:{a:number;b:number;group:boolean}[]=Array.from({length:n-1},(_,i)=>({a:i,b:i+1,group:false}));
 if(t.grouping)for(let i=0;i<n;i++)if(i%18)edges.push({a:i-i%18,b:i,group:true});
 const touching=Array.from({length:n},()=>[] as number[]);
 edges.forEach((e,i)=>{touching[e.a].push(i);touching[e.b].push(i);});
 const edgeCost=(k:number)=>{const e=edges[k];return e.group?dist[indices[e.a]][indices[e.b]]*.7:pair[indices[e.a]][indices[e.b]];};
 const focus=Array.from({length:n},(_,i)=>Math.sin(Math.PI*i/(n-1))**2);
 const area=(i:number)=>!t.areaBalance?0:(roles[indices[i]]==='accent'?-focus[i]:roles[indices[i]]==='main'?focus[i]*.25:0)*.3;
 const bucket=Array.from({length:n},(_,i)=>zones.head&&i<50?'head':zones.middle&&i>=100&&i<150?'middle':zones.tail&&i>=200?'tail':'free');
 const direction=semantic.transition?(()=>{const a=oklab(hslHex(semantic.transition.from.h,semantic.transition.from.s,semantic.transition.from.l)),z=oklab(hslHex(semantic.transition.to.h,semantic.transition.to.s,semantic.transition.to.l));return [z.L-a.L,z.a-a.a,z.b-a.b];})():null;
 const projection=direction?values.map(v=>v.L*direction[0]+v.a*direction[1]+v.b*direction[2]):[];
 const target=direction?reference.map(c=>projection[lookup.get(c.key)!]):[];
 const story=(i:number)=>direction?Math.abs(projection[indices[i]]-target[i]):0;
 const adjacent=(positions:number[])=>positions.reduce((s,i)=>s+dist[indices[i]][indices[i+1]],0);
 // Fixed offsets and traversal keep the same input/variation reproducible.
 for(let pass=0;pass<2;pass++)for(let i=0;i<n;i++)for(const offset of [1,3,7,17,29,61,113]){
  const j=(i+offset)%n;if(indices[i]===indices[j]||bucket[i]!==bucket[j])continue;
  const affected=[...new Set([...touching[i],...touching[j]])];
  const boundaries=[...new Set([i-1,i,j-1,j].filter(k=>k>=0&&k<n-1))];
  const before=affected.reduce((s,k)=>s+edgeCost(k),0)+area(i)+area(j),smoothBefore=rules.smooth?adjacent(boundaries):0,storyBefore=story(i)+story(j);
  [indices[i],indices[j]]=[indices[j],indices[i]];
  const invalid=rules.noAdjacent&&boundaries.some(k=>indices[k]===indices[k+1]);
  const losesRequest=rules.smooth&&adjacent(boundaries)>smoothBefore+1e-10;
  const losesStory=story(i)+story(j)>storyBefore+1e-10;
  const after=affected.reduce((s,k)=>s+edgeCost(k),0)+area(i)+area(j);
  if(invalid||losesRequest||losesStory||after>=before-1e-10)[indices[i],indices[j]]=[indices[j],indices[i]];
 }
 return indices.map(i=>colors[i]);
}

export function layoutMeasurements(sequence:Color[],colors:Color[],roles:string[],base:{h:number;s:number;l:number}){
 const role=new Map(colors.map((c,i)=>[c.key,roles[i]])),b=oklab(hslHex(base.h,base.s,base.l));
 let normal=0,normalN=0,boundary=0,boundaryN=0,group=0,groupN=0,mix=0,center=0,accents=0;
 sequence.forEach((c,i)=>{
  if(role.get(c.key)==='accent'){center+=Math.sin(Math.PI*i/(sequence.length-1))**2;accents++;}
  if(i%18){group+=deltaOK(oklab(c.hex),oklab(sequence[i-i%18].hex));groupN++;}
  if(!i)return;const prev=sequence[i-1],d=deltaOK(oklab(c.hex),oklab(prev.hex));
  if((role.get(c.key)==='accent')!==(role.get(prev.key)==='accent')){boundary+=d;boundaryN++;}else{normal+=d;normalN++;}
  mix+=deltaOK(oklab(opticalMean(c.hex,prev.hex)),b);
 });
 return {normalStep:normalN?normal/normalN:0,accentStep:boundaryN?boundary/boundaryN:0,accentBoundaries:boundaryN,groupSpread:groupN?group/groupN:0,mixError:mix/Math.max(1,sequence.length-1),accentCentrality:accents?center/accents:0};
}
