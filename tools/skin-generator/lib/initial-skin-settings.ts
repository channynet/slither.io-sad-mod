import {strictSettings} from './strict-instructions.ts';
import type {AIAnalysis,RecommendedSettings} from './ai-contract.ts';
import {theoryCatalog,type TheorySettings} from './theory-settings.ts';

export type AutomaticSettings={colorCount:boolean;accent:boolean;theories:boolean};
export const defaultAutomaticSettings:AutomaticSettings={colorCount:true,accent:true,theories:true};
export function initialTheorySettings(analysis:AIAnalysis){
 if(analysis.recommendedTheories)return {theories:Object.fromEntries(theoryCatalog.map(t=>[t.id,analysis.recommendedTheories!.enabled.includes(t.id)])) as TheorySettings,reason:analysis.recommendedTheories.reason,source:'ai' as const};
 const d=analysis.dimensions,smooth=analysis.requirements?.some(r=>r.kind==='smooth_neighbors'),accent=!!analysis.accent;
 const theories:TheorySettings={wheel:!analysis.transition,lightContrast:d.contrast>.15,chromaContrast:accent&&d.chroma>.1&&d.contrast>0,temperature:true,areaBalance:!analysis.transition,interaction:accent&&d.contrast>.3&&!smooth,continuity:d.movement<.45||!!smooth,background:accent&&d.brightness<0,hueUnity:d.contrast<-.2&&!analysis.transition,nuance:d.contrast<0,chromaUnity:d.chroma<-.25,grouping:d.movement<.3,figureGround:accent&&d.contrast>.5,opticalMix:false};
 return {theories,reason:'기존 분석의 대비·선명함·색 흐름과 변화 방향을 기준으로, 서로 반대되는 규칙을 줄여 초기 이론을 골랐습니다.',source:'derived' as const};
}
export function resolveInitialSettings(analysis:AIAnalysis,current:{colorCount:number;accent:number;theories:TheorySettings},automatic:AutomaticSettings,strict=false){
 const initial=initialSkinSettings(analysis),theoryInitial=initialTheorySettings(analysis);
 return {colorCount:automatic.colorCount?initial.colorCount:current.colorCount,accent:automatic.accent?initial.accentPercent:current.accent,theories:automatic.theories?theoryInitial.theories:current.theories,initial,theoryInitial,...(strict?strictSettings(analysis,{colorCount:automatic.colorCount?initial.colorCount:current.colorCount,accent:automatic.accent?initial.accentPercent:current.accent}):{})};
}

export function initialSkinSettings(analysis:AIAnalysis):RecommendedSettings&{source:'ai'|'derived'}{
 if(analysis.recommendedSettings)return {...analysis.recommendedSettings,source:'ai'};
 // Older transfers have no model-selected settings. Derive a starting proposal from
 // their semantic dimensions, never from a fixed word list or a fabricated fit score.
 const d=analysis.dimensions,contrast=(d.contrast+1)/2,movement=(d.movement+1)/2,chroma=(d.chroma+1)/2;
 const colorCount=Math.max(6,Math.min(38,Math.round(6+12*movement+8*chroma+4*contrast+(analysis.transition?4:0))));
 const accentPercent=analysis.accent?Math.max(5,Math.min(30,Math.round((5+15*contrast+5*movement)/5)*5)):5;
 return {colorCount,accentPercent,source:'derived',reason:`기존 분석의 색 흐름과 선명함${analysis.transition?'·머리에서 꼬리로의 변화':''}을 바탕으로 색 수를 정했습니다. ${analysis.accent?'강조색의 대비에 맞춰 포인트 비중을 조절했습니다.':'별도 강조색을 제안하지 않아 포인트 비중을 최소로 시작합니다.'}`};
}
