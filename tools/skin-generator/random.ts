import {generate, presets} from './lib/colors.ts';
import {concreteConcepts} from './lib/concept-library.ts';
import {theoryCatalog, type TheorySettings} from './lib/theory-settings.ts';

const themes=[...presets.map(p=>p.prompt),...concreteConcepts.filter(c=>c.category==='object'||c.category==='scene').map(c=>c.label)];
const moods=['차분한','신비로운','따뜻한','차가운','몽환적인','부드러운','선명한','은은한'];
export function randomSkin(seed:number) {
  let state=(seed>>>0)||0x9e3779b9;
  const next=()=>{state^=state<<13;state^=state>>>17;state^=state<<5;return state>>>0;};
  const topic=moods[next()%moods.length]+' '+themes[next()%themes.length];
  const mask=next()&((1<<theoryCatalog.length)-1);
  const rules=Object.fromEntries(theoryCatalog.map((t,i)=>[t.id,!!(mask&(1<<i))])) as TheorySettings;
  const variation=next()%0x7fffffff;
  const skin=generate(topic,15,variation,16,{},rules);
  return {code:skin.sequence.map(c=>c.key).join(''),topic,mask,
    theories:theoryCatalog.filter((t,i)=>mask&(1<<i)).map(t=>t.name).join('\n'),variation};
}
(globalThis as any).nativeRandomSkin=randomSkin;
