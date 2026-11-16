// sRGB D65 → OKLab. Matrix coefficients: Björn Ottosson, 2021 update.
// https://bottosson.github.io/posts/oklab/
export type Lab={L:number;a:number;b:number;C:number;h:number};
export const PREVIEW_BACKGROUND='#11171e';
export function linearRgb(hex:string){return [1,3,5].map(i=>{const v=parseInt(hex.slice(i,i+2),16)/255;return v<=.04045?v/12.92:((v+.055)/1.055)**2.4;});}
export function oklab(hex:string):Lab{
 const [r,g,b]=linearRgb(hex);
 const l=Math.cbrt(.4122214708*r+.5363325363*g+.0514459929*b),m=Math.cbrt(.2119034982*r+.6806995451*g+.1073969566*b),s=Math.cbrt(.0883024619*r+.2817188376*g+.6299787005*b);
 const L=.2104542553*l+.793617785*m-.0040720468*s,a=1.9779984951*l-2.428592205*m+.4505937099*s,B=.0259040371*l+.7827717662*m-.808675766*s;
 const C=Math.hypot(a,B);return {L,a,b:B,C,h:C<1e-7?0:(Math.atan2(B,a)*180/Math.PI+360)%360};
}
export function deltaOK(a:Lab,b:Lab){return Math.hypot(a.L-b.L,a.a-b.a,a.b-b.b);}
export function relativeLuminance(hex:string){const [r,g,b]=linearRgb(hex);return .2126*r+.7152*g+.0722*b;}
export function contrastRatio(a:string,b:string){const x=relativeLuminance(a),y=relativeLuminance(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);}
export function hslHex(h:number,s:number,l:number){
 h=(h%360+360)%360;s=Math.max(0,Math.min(1,s));l=Math.max(0,Math.min(1,l));
 const a=s*Math.min(l,1-l),channel=(n:number)=>{const k=(n+h/30)%12;return l-a*Math.max(-1,Math.min(k-3,9-k,1));};
 return '#'+[0,8,4].map(n=>Math.round(channel(n)*255).toString(16).padStart(2,'0')).join('');
}
// A conventional warm/cool hue score, not physical color temperature in Kelvin.
export function warmth(h:number,s:number){return Math.cos((h-30)*Math.PI/180)*s;}
export type Harmony='analogous'|'complementary'|'split'|'mono'|'triad'|'tetrad';
export function harmonyOffset(mode:Harmony,role:number,round:number){
 if(mode==='triad')return [0,120,240][role];
 if(mode==='tetrad')return role===0?(round%2?60:0):role===1?180:240;
 if(mode==='mono')return 0;
 if(mode==='split')return [0,210,150][role];
 return [0,28,mode==='analogous'?55:180][role];
}
