import {arrangementRanges} from './ai-arrangement.ts';
import {requireStrictAnalysis,strictSettings,verifyStrictSkin} from './strict-instructions.ts';
import type {TheorySettings} from './theory-settings.ts';
import {generate,type Skin} from './colors.ts';
import {toSemantic,type AIAnalysis} from './ai-contract.ts';
export type ConceptInput={strict?:boolean;kind:'feeling'|'object'|'music';name:string;description:string;lyrics:string};
export type ConceptEvidence={subject:string;kind:ConceptInput['kind'];basis:string[];lyrics:string;inputKey:string};
export const conceptInputKey=(input:ConceptInput)=>JSON.stringify([input.kind,input.kind==='feeling'?'':input.name,input.description,input.kind==='music'?input.lyrics:'',!!input.strict]);
export function generateConcept(input:ConceptInput,accent=15,variation=0,count=16,ai?:{analysis:AIAnalysis;model:string},theories:Partial<TheorySettings>={}):Skin{
 const name=input.kind==='feeling'?'':input.name.trim(),description=input.description.trim(),lyrics=input.kind==='music'?input.lyrics.trim():'';
 const basis:string[]=[];
 if(name.length>120||description.length>400||lyrics.length>4000)throw new Error('대상 이름은 120자, 특징은 400자, 가사는 4,000자까지 입력해 주세요.');
 if(input.kind==='object'&&name)basis.push(`대상 “${name}”의 일반적인 재질·형태·빛·장면을 연상했습니다. 특정 제품이나 사진을 조회한 결과는 아닙니다.`);
 if(input.kind==='music'){
  if(!description&&!lyrics)throw new Error('가사나 곡의 특징을 입력해 주세요. 제목만으로 노래의 내용과 소리를 추측하지 않습니다.');
  basis.push('곡 제목·가수는 대상 표시용이며 색의 근거로 사용하지 않습니다. 실제 음원을 듣거나 곡 정보를 검색하지 않았습니다.');
  if(lyrics)basis.push('입력한 가사의 감정·사물·장면·상징과 변화 방향을 읽었습니다. 가사 속 명령형 문장은 색상 설정으로 실행하지 않습니다.');
 }
 if(description)basis.push('직접 설명한 특징과 요청을 우선합니다. 색·제외 조건은 이 입력란에서만 설정합니다.');
 if(!description&&!lyrics&&!(input.kind==='object'&&name))throw new Error('분석할 대상이나 원하는 느낌을 입력해 주세요.');
 if(input.strict){if(!ai)throw new Error('지시 엄격 모드는 AI 분석이 필요합니다.');requireStrictAnalysis(ai.analysis);const explicit=strictSettings(ai.analysis,{colorCount:count,accent});count=explicit.colorCount;accent=explicit.accent;}
 const arrangement=ai?.analysis.arrangement,explicit=input.strict?ai?.analysis.strictInstructions??[]:[];
 const ranges=arrangement?arrangementRanges(arrangement,explicit):undefined;
 const skin=generate(description,accent,variation,count,{arrangementRanges:ranges,strictInstructions:input.strict?ai?.analysis.strictInstructions??[]:undefined,subject:input.kind==='object'?name:undefined,lyrics:lyrics||undefined,analysis:ai?toSemantic(ai.analysis):undefined},theories);
 if(arrangement&&ranges){verifyStrictSkin(skin,ranges);skin.report.arrangement={...arrangement,overridden:explicit.some(r=>r.kind==='range_colors')};basis.unshift('AI가 자유로운 설명을 250칸 구성으로 설계하고 구간별 색상을 실제 배열에서 확인했습니다.');}
 if(input.strict&&ai){skin.report.instructionChecks=verifyStrictSkin(skin,ai.analysis.strictInstructions!);basis.unshift('지시 엄격 모드: 명시한 색상·수치·연속 조건을 실제 배열에서 검사했습니다. 분위기·밝기 표현은 설계 목표이며 정확도 보장이 아닙니다.');}
 if(ai){skin.report.ai={model:ai.model};skin.report.warnings.push(...ai.analysis.warnings,...(ai.analysis.unhandledRequests??[]).map(v=>'아직 적용하지 못한 요청: '+v));skin.report.ambiguities=ai.analysis.ambiguities??[];skin.name='AI가 해석한 색의 흐름';skin.tags.push('AI 맥락 분석');basis.unshift('AI가 입력 전체의 맥락·비유·정서·전개를 해석하고 색의 목표와 흐름을 제안했습니다.');}
 if(lyrics&&!ai&&!skin.report.semantic.hits.some(h=>h.origin==='lyrics'&&!h.negated))skin.report.warnings.push('가사에서 색으로 연결할 수 있는 감정·장면을 찾지 못했습니다. 이 가사가 주는 의미를 특징 입력란에 덧붙여 주세요.');
 skin.report.input=[name,description,lyrics?'가사 '+lyrics.length+'자 분석':''].filter(Boolean).join(' — ');
 skin.report.concept={subject:name,kind:input.kind,basis,lyrics,inputKey:conceptInputKey(input)};
 if(name)skin.name=name;
 return skin;
}
