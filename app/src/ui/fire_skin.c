#include "fire_skin.h"
#include <math.h>

float skin_effect_time;

const char* special_skin_name(int kind) {
  static const char* names[] = {"끄기", "불꽃", "얼음", "번개", "오로라", "별빛", "물결"};
  return names[kind >= 0 && kind < SPECIAL_COUNT ? kind : 0];
}

void special_body_color(float out[3], int kind, int index, int count, float t) {
  if(kind == SPECIAL_FIRE) { fire_body_color(out,index,count,t); return; }
  if(!isfinite(t)) t=0;
  float u=count>1?(float)index/(count-1):0;
  float wave=.5f+.5f*sinf(u*18-t*2.5f);
  const float* a; const float* b;
  static const float ice[][3]={{.12f,.40f,.64f},{.77f,.97f,1}};
  static const float lightning[][3]={{.12f,.10f,.45f},{.80f,.87f,1}};
  static const float aurora[][3]={{.35f,.12f,.72f},{.10f,.95f,.60f}};
  static const float stars[][3]={{.09f,.06f,.25f},{.65f,.33f,.85f}};
  static const float water[][3]={{.02f,.23f,.52f},{.13f,.85f,.94f}};
  switch(kind) {
    case SPECIAL_ICE: a=ice[0];b=ice[1];wave=powf(wave,3);break;
    case SPECIAL_LIGHTNING: a=lightning[0];b=lightning[1];wave=powf(.5f+.5f*sinf(index*.18f-t*7),12);break;
    case SPECIAL_AURORA: a=aurora[0];b=aurora[1];break;
    case SPECIAL_STARS: a=stars[0];b=stars[1];wave=.5f+.5f*sinf(u*9+t*.8f);break;
    case SPECIAL_WATER: a=water[0];b=water[1];break;
    default: out[0]=out[1]=out[2]=1;return;
  }
  for(int c=0;c<3;c++) out[c]=a[c]+(b[c]-a[c])*wave;
}

static void sparkle(bst_renderer* r,float x,float y,float size,const float c[3],float alpha,int* n) {
  if(!r || alpha<=0 || size<=0) return;
  bst_renderer_push(r,&(bst_instance){{x-size,y-size,size*2},{c[0],c[1],c[2],alpha}}); (*n)++;
}

int special_skin_effects(bst_renderer* under, bst_renderer* over, const fire_body* b, int kind, float t) {
  if(!b || !b->x || !b->y || !b->angle || !b->visible || b->count<=0 ||
     !isfinite(t) || !isfinite(b->radius) || !isfinite(b->scale) || b->radius<=0 || b->scale<=0 ||
     !isfinite(b->alpha) || b->alpha<=0) return 0;
  if(kind==SPECIAL_FIRE) return fire_skin_effects(under,over,b,t);
  if(kind<SPECIAL_ICE || kind>=SPECIAL_COUNT) return 0;
  float s=isfinite(b->strength)?fminf(1.5f,fmaxf(.2f,b->strength)):1;
  float r=b->radius*b->scale, alpha=fminf(1,b->alpha);
  int n=0, step=(b->count+31)/32; // <=32 anchors and <=768 glow instances per snake.
  for(int j=0;j<b->count;j+=step) {
    if(!b->visible[j]) continue;
    float x=b->half_w+(b->x[j]-b->view_x)*b->scale;
    float y=b->half_h+(b->y[j]-b->view_y)*b->scale;
    float angle=b->angle[j], tx=cosf(angle),ty=sinf(angle),sx=-ty,sy=tx;
    float phase=t*.65f+j*.137f; phase-=floorf(phase);
    float c[3]; special_body_color(c,kind,j,b->count,t);
    if(j%(step*2)==0) sparkle(under,x,y,r*2.3f,c,alpha*.3f*s,&n);
    if(kind==SPECIAL_ICE || kind==SPECIAL_STARS) {
      float spin=kind==SPECIAL_ICE?angle+.7854f:t*.8f+j*2.4f;
      float distance=r*(1.2f+s*(kind==SPECIAL_ICE?phase*1.4f:.7f));
      float px=x+cosf(spin)*distance,py=y+sinf(spin)*distance;
      float fade=sinf(phase*3.14159265f),size=r*(kind==SPECIAL_ICE?.42f:.65f)*s;
      float light[3]={kind==SPECIAL_ICE?.60f:1,kind==SPECIAL_ICE?.90f:.86f,kind==SPECIAL_ICE?1:.52f};
      // Four tapered rays form an icy shard or an orbiting star.
      sparkle(over,px,py,size*.65f,light,alpha*fade,&n);
      for(int ray=0;ray<4;ray++) for(int p=1;p<=3;p++) {
        float a=spin+ray*1.57079633f, d=size*p*.45f;
        sparkle(over,px+cosf(a)*d,py+sinf(a)*d,size*(.48f-.10f*p),light,alpha*fade,&n);
      }
    } else if(kind==SPECIAL_LIGHTNING) {
      float pulse=.5f+.5f*sinf(t*9+j*.7f);
      if(pulse<.65f) continue;
      float light[3]={.65f,.78f,1};
      for(int p=0;p<15;p++) {
        float along=(p/14.0f-.5f)*r*3;
        float kink=sinf((p/3)*2.8f+floorf(t*9)+j)*r*.48f*s;
        sparkle(over,x+tx*along+sx*(r+kink),y+ty*along+sy*(r+kink),r*.22f,light,alpha*pulse,&n);
      }
    } else if(kind==SPECIAL_AURORA) {
      for(int p=0;p<6;p++) {
        float along=(p/5.0f-.5f)*r*3;
        float offset=r*(1.4f+s*sinf(t*1.5f+j*.15f+p*.3f));
        sparkle(over,x+tx*along+sx*offset,y+ty*along+sy*offset,r*.90f,c,alpha*.45f,&n);
      }
    } else {
      // A travelling, expanding ring with a rising bubble.
      float radius=r*(.6f+phase*2.5f*s),fade=alpha*(1-phase)*.7f;
      float light[3]={.35f,.90f,1};
      for(int p=0;p<20;p++) {
        float a=p*.314159265f;
        sparkle(over,x+cosf(a)*radius,y+sinf(a)*radius,r*.16f,light,fade,&n);
      }
      sparkle(over,x+sx*r*1.6f,y+sy*r*1.6f-phase*r*2,r*.30f,light,fade,&n);
    }
  }
  return n;
}

static float hash01(int a, int b) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + 0x9E3779B9u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (h & 0xFFFFFF) / 16777215.0f;
}

float fire_heat(int index, int count, float t) {
  float along = count > 1 ? (float)index / (count - 1) : 0;          // 0 head .. 1 tail
  float heat = .74f - .42f * along;
  heat += .17f * sinf(index * .55f - t * 9.0f);                        // waves running tailwards
  heat += .07f * sinf(index * 1.7f + t * 23.0f);                       // quick flicker
  heat += .08f * (hash01(index, (int)floorf(t * 14)) - .5f);           // a little crackle
  return heat < 0 ? 0 : heat > 1 ? 1 : heat;
}

void fire_color(float out[3], float heat) {
  static const float stops[5][3] = {{.30f, .02f, .00f}, {.85f, .10f, .00f}, {1.0f, .42f, .02f},
                                    {1.0f, .78f, .12f}, {1.0f, .96f, .62f}};
  static const float at[5] = {0, .35f, .6f, .82f, 1};
  if (!(heat > 0)) heat = 0;
  if (heat > 1) heat = 1;
  int i = 0;
  while (i < 3 && heat > at[i + 1]) i++;
  float f = (heat - at[i]) / (at[i + 1] - at[i]);
  for (int c = 0; c < 3; c++) out[c] = stops[i][c] + (stops[i + 1][c] - stops[i][c]) * f;
}

void fire_body_color(float out[3], int index, int count, float t) {
  fire_color(out, fire_heat(index, count, t));
}

int fire_skin_effects(bst_renderer* under, bst_renderer* over, const fire_body* b, float t) {
  int pushed = 0;
  float s = b->strength < .2f ? .2f : b->strength > 1.5f ? 1.5f : b->strength;
  float r = b->radius * b->scale;                                       // on screen
  for (int j = b->count - 1; j >= 0; j--) {
    if (b->visible[j] < 1) continue;
    float x = b->half_w + (b->x[j] - b->view_x) * b->scale;
    float y = b->half_h + (b->y[j] - b->view_y) * b->scale;
    float heat = fire_heat(j, b->count, t), col[3];
    // the glow under the body
    if (under && (j % 8 == 0 || b->count < 16)) {      // (big soft quads: few of them, the fill is what costs)
      fire_color(col, heat * .8f);
      float flick = .75f + .25f * sinf(t * 17 + j * 2.3f);
      float g = r * (2.2f + .4f * s) * flick;
      bst_renderer_push(under, &(bst_instance){{x - g, y - g, 2 * g}, {col[0], col[1], col[2], .6f * s * b->alpha}});
      pushed++;
    }
    if (!over || (j % 3 && b->count >= 16)) continue;     // flames off every third part
    // two flames per part, one off each side, trailing behind as they burn out
    float tail_x = cosf(b->angle[j]), tail_y = sinf(b->angle[j]);
    float side_x = -tail_y, side_y = tail_x;
    for (int e = 0; e < 2; e++) {
      float speed = 1.1f + .9f * hash01(j, e + 7);
      float phase = t * speed + hash01(j, e);
      phase -= floorf(phase);                                            // 0 born .. 1 gone
      float side = (e == 0 ? -1.0f : e == 1 ? 1.0f : (hash01(j, 11) < .5f ? -1.0f : 1.0f)) * (.35f + .5f * hash01(j, e + 3));
      float out = r * (1.0f + 2.2f * phase * s);
      float back = r * 3.6f * phase * s;
      float fx = x + side_x * side * out + tail_x * back, fy = y + side_y * side * out + tail_y * back;
      float size = r * (.42f + .75f * (1 - phase)) * (.7f + .3f * s);
      fire_color(col, 1 - phase * .9f);
      float born = phase < .12f ? phase / .12f : 1;                        // fade in off the body
      float a = born * powf(1 - phase, 1.5f) * .95f * (.6f + .4f * s) * b->alpha;
      bst_renderer_push(over, &(bst_instance){{fx - size, fy - size, 2 * size}, {col[0], col[1], col[2], a}});
      pushed++;
    }
  }
  return pushed;
}
