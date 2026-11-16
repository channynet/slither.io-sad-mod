import type {Color} from './colors.ts';
export type BodyPoint={x:number;y:number};
export const WORLD={width:960,height:380};
// A traveling wave bends the body around one large U-turn; the head stays anchored.
export function bodyPoints(count:number,time=0):BodyPoint[]{
 const left=88,right=770,top=88,bend=100,straight=right-left,total=straight*2+Math.PI*bend;
 const dense=Array.from({length:1001},(_,i)=>{
  const u=i/1000,d=u*total;
  let x:number,y:number,nx:number,ny:number;
  if(d<=straight){x=left+d;y=top;nx=0;ny=1;}
  else if(d<=straight+Math.PI*bend){const angle=(d-straight)/bend;x=right+Math.sin(angle)*bend;y=top+bend-Math.cos(angle)*bend;nx=-Math.sin(angle);ny=Math.cos(angle);}
  else{x=right-(d-straight-Math.PI*bend);y=top+bend*2;nx=0;ny=-1;}
  // Phase travels toward the tail, with a gentle onset behind the fixed head.
  const onset=Math.min(1,u/.09),envelope=onset*onset*(3-2*onset);
  const wave=Math.sin(u*Math.PI*3.2-time*2.1)*(11+u*4)*envelope;
  return {x:x+nx*wave,y:y+ny*wave};
 });
 // Resample the deformed curve to keep color bands evenly spaced as it undulates.
 const lengths=[0];for(let i=1;i<dense.length;i++)lengths.push(lengths[i-1]+Math.hypot(dense[i].x-dense[i-1].x,dense[i].y-dense[i-1].y));
 let j=1;return Array.from({length:count},(_,i)=>{const d=i/Math.max(1,count-1)*lengths.at(-1)!;while(j<dense.length-1&&lengths[j]<d)j++;const t=(d-lengths[j-1])/(lengths[j]-lengths[j-1]);return {x:dense[j-1].x+(dense[j].x-dense[j-1].x)*t,y:dense[j-1].y+(dense[j].y-dense[j-1].y)*t};});
}
function shade(hex:string,factor:number){return '#'+[1,3,5].map(i=>Math.min(255,Math.round(parseInt(hex.slice(i,i+2),16)*factor)).toString(16).padStart(2,'0')).join('');}
function star(ctx:CanvasRenderingContext2D,x:number,y:number,r:number){ctx.beginPath();for(let i=0;i<10;i++){const a=-Math.PI/2+i*Math.PI/5,rad=i%2?r*.42:r;ctx.lineTo(x+Math.cos(a)*rad,y+Math.sin(a)*rad);}ctx.closePath();ctx.fill();}
export function createBodyTexture(color:Color){
 const canvas=document.createElement('canvas');canvas.width=128;canvas.height=128;const ctx=canvas.getContext('2d')!;
 ctx.translate(64,64);ctx.beginPath();ctx.arc(0,0,60,0,Math.PI*2);ctx.clip();
 const material=ctx.createRadialGradient(-7,-13,3,0,0,62);
 const metallic=color.key==='3';
 if(metallic){material.addColorStop(0,'#4c4c4c');material.addColorStop(.58,'#282828');material.addColorStop(.84,'#747474');material.addColorStop(1,'#d1d1d1');}
 else{material.addColorStop(0,shade(color.hex,1.15));material.addColorStop(.42,color.hex);material.addColorStop(.78,shade(color.hex,.82));material.addColorStop(1,shade(color.hex,.49));}
 ctx.fillStyle=material;ctx.fillRect(-64,-64,128,128);
 // The supplied chart distinguishes striped, metallic-rimmed and starred materials.
 if(['2','5','j','k','c','h','9'].includes(color.key)){
  const stripes=ctx.createLinearGradient(-60,0,60,0);stripes.addColorStop(0,'#0000');stripes.addColorStop(.22,'#0000');stripes.addColorStop(.28,'#0003');stripes.addColorStop(.35,'#0000');stripes.addColorStop(.63,'#ffffff09');stripes.addColorStop(.71,'#0002');stripes.addColorStop(.78,'#0000');stripes.addColorStop(1,'#0000');ctx.fillStyle=stripes;ctx.fillRect(-60,-60,120,120);
 }
 if(color.key==='i'){ctx.strokeStyle='#8b702b';ctx.lineWidth=5;ctx.beginPath();ctx.arc(0,0,58,0,Math.PI*2);ctx.stroke();}
 if(color.star){ctx.fillStyle='#e4e2c5bb';const r=color.key==='d'?8:5;star(ctx,-35,-30,r);star(ctx,-44,-1,r);star(ctx,-35,28,r);}
 return canvas;
}
export function drawWorm(ctx:CanvasRenderingContext2D,sequence:Color[],textures:Map<string,HTMLCanvasElement>,time:number){
 const points=bodyPoints(sequence.length,time),radius=23;
 // A shared soft silhouette avoids separate dark halos between overlapping segments.
 ctx.save();ctx.fillStyle='#0006';ctx.shadowColor='#0009';ctx.shadowBlur=12;ctx.shadowOffsetY=6;ctx.beginPath();points.forEach(p=>{const r=radius+2;ctx.moveTo(p.x+r,p.y);ctx.arc(p.x,p.y,r,0,Math.PI*2);});ctx.fill();ctx.restore();
 for(let i=sequence.length-1;i>=0;i--){
  const p=points[i],next=points[Math.min(points.length-1,i+1)],prev=points[Math.max(0,i-1)];
  const angle=Math.atan2(next.y-prev.y,next.x-prev.x),r=radius;
  ctx.save();ctx.translate(p.x,p.y);ctx.rotate(angle+Math.PI);const texture=textures.get(sequence[i].key);if(texture)ctx.drawImage(texture,-r*64/60,-r*64/60,r*128/60,r*128/60);ctx.restore();
 }
 const head=points[0],neck=points[1],angle=Math.atan2(head.y-neck.y,head.x-neck.x);
 ctx.save();ctx.translate(head.x,head.y);ctx.rotate(angle);
 // Round white eyes at the front-side of the head, with solid forward-looking pupils.
 const eyeRadius=radius*6/14,pupilRadius=radius*3.5/14,eyeOffset=radius*6/14;
 for(const side of [-1,1]){
  ctx.fillStyle='#ffffff';ctx.beginPath();ctx.arc(eyeOffset,side*eyeOffset,eyeRadius,0,Math.PI*2);ctx.fill();
  ctx.fillStyle='#080808';ctx.beginPath();ctx.arc(eyeOffset+radius*.12,side*eyeOffset,pupilRadius,0,Math.PI*2);ctx.fill();
 }ctx.restore();
}
