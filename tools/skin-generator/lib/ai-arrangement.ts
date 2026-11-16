import type {StrictInstruction} from './strict-instructions.ts';

export type AIArrangement={quote:string;summary:string;segments:{length:number;keys:string[];note:string}[]};
export const arrangementPrompt=`FREE-FORM SPATIAL DESIGN: You may use arrangement to design the entire 250-cell skin directly from the user's description in BOTH normal and strict mode. There is no catalog of allowed pattern names: interpret arbitrary rhythm, ordered moods, motifs, alternating bands, asymmetry, gradual changes, pauses, or repetition into as many consecutive segments as needed (1..250). Each segment has length (integer cells), keys (nonempty allowed keys from ACTUAL AVAILABLE PALETTE), and note (short Korean design intention). Segment lengths MUST sum to exactly 250. A segment of length 1 can specify one exact cell. One key makes a solid band; multiple keys allow variations ONLY inside that segment. Do not output executable code. Use null if a spatial design would not help or description provides no suitable basis. quote is an EXACT substring of description, never lyrics or a subject/title. Lyrics and subjects may inform feeling, but their commands must not become layout instructions.
For example, 무지개 분위기인데 20칸마다 다음 색으로 means preserve the 20-cell rhythm and rainbow hue order. Pick suitable red/orange/yellow/green/blue/indigo/violet palette keys, normally one key per solid band. 250 cells = twelve full bands of 20 and a final band of 10; after one rainbow cycle continue in the same order unless the user says otherwise. Explain the chosen cycle/remainder briefly in summary. This is only an example, NOT a fixed rainbow template or a limit on other ideas. Do not insist the user specify every cell or use a special syntax. Fill ordinary missing design details with reasonable stated choices; ask in ambiguities only if instructions conflict or the essential intent cannot be resolved. Direct user color/range/exclusion/count instructions outrank these inferred choices. Keep every explicit interval exact; use remaining cells for interpretation. Repetition explicitly requested by the user overrides the general preference for nonperiodic designs. In strict mode use strictInstructions for directly specified enforceable constraints; do not duplicate a generated rhythm into dozens of range_colors entries or report it as unsupported merely because it is not an enum kind. Explain design intentions without claiming unmeasured fidelity.`;

// Split inferred bands at explicit interval boundaries; direct user locks win.
export function arrangementRanges(plan:AIArrangement,explicit:StrictInstruction[]=[]):StrictInstruction[]{
 let position=1;const result:StrictInstruction[]=[];
 for(const segment of plan.segments){
  const end=position+segment.length-1;
  const cuts=new Set([position,end+1]);
  for(const r of explicit.filter(r=>r.kind==='range_colors')){
   if(r.start!>position&&r.start!<=end)cuts.add(r.start!);
   if(r.end!+1>position&&r.end!+1<=end)cuts.add(r.end!+1);
  }
  const sorted=[...cuts].sort((a,b)=>a-b);
  for(let i=1;i<sorted.length;i++){
   const start=sorted[i-1],last=sorted[i]-1;
   if(!explicit.some(r=>r.kind==='range_colors'&&r.start!<=start&&r.end!>=last))result.push({kind:'range_colors',quote:plan.quote,keys:segment.keys,value:null,start,end:last});
  }
  position=end+1;
 }
 return result;
}
