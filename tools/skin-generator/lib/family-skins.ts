import type {ConceptInput} from './concept-input.ts';
import type {Tone} from './mood-semantics.ts';
export const MAX_FAMILY_SIZE=8;
export type FamilyContext={summary:string;base:Tone};
export function parseFamilyContext(value:unknown):FamilyContext|undefined{
 if(value===undefined)return undefined;
 if(!value||typeof value!=='object')throw new Error('공통 테마 정보를 확인해 주세요.');
 const r=value as Record<string,unknown>,b=r.base as Record<string,unknown>;
 if(typeof r.summary!=='string'||!r.summary.trim()||r.summary.length>600||!b||typeof b!=='object'||!['h','s','l'].every(k=>typeof b[k]==='number'&&Number.isFinite(b[k])))throw new Error('공통 테마 정보를 확인해 주세요.');
 const base=b as Tone;if(base.h<0||base.h>360||base.s<0||base.s>1||base.l<.15||base.l>.8)throw new Error('공통 테마의 색상 범위를 확인해 주세요.');
 return {summary:r.summary,base:{h:base.h,s:base.s,l:base.l}};
}
export function familyInput(theme:string,detail?:string):ConceptInput{
 if(!theme.trim()||theme.length>160||(detail!==undefined&&(!detail.trim()||detail.length>160)))throw new Error('공통 테마와 세부 분위기를 각각 1~160자로 적어 주세요.');
 return {kind:'feeling',name:'',description:detail===undefined?theme.trim():`공통 테마: ${theme.trim()}\n이 지렁이의 세부 분위기: ${detail.trim()}`,lyrics:''};
}
