#ifndef SNAKE_H
#define SNAKE_H

#include "body_part.h"
#include "gpt.h"
#include "name_layout.h"
#include <stdint.h>

typedef struct snake {
  int id;
  int ntl_id;
  char target_key[48];
  uint32_t serial;
  bool has_serial;
  uint64_t generation;
  int cv;
  int fpos;
  int ftg;
  int fapos;
  int fatg;
  int flpos;
  int fltg;
  int sct;
  int cusk_len;
  int dir;
  int rsc;
  int iiv;
  int edir;
  int ntl_tag;                    // the free NTL tag its skin code carries (-1: none)
  int official_skin;              // the official skin it shows (its custom skin code's own, or cv; -1: a custom skin)

  float xx;
  float yy;
  float chl;
  float tsp;
  float sfr;
  float sc;
  float ssp;
  float fsp;
  float msp;
  float fx;
  float fy;
  float fa;
  float ehang;
  float wehang;
  float ang;
  float eang;
  float wang;
  float spang;
  float rex;
  float rey;
  float sp;
  float fl;
  float tl;
  float cfl;
  float scang;
  float dead_amt;
  float alive_amt;
  float msl;
  float fam;
  float wsep;
  float sep;
  float fchl;

  float fxs[GD_EEZ];
  float fys[GD_EEZ];
  float fchls[GD_EEZ];
  float fas[GD_AFC];
  float fls[GD_EEZ];

  uint8_t accessory;
  uint8_t cusk_data[MAX_SKIN_CODE_LEN];
  char nk[MAX_NICKNAME_LEN + 1];
  name_layout name_layout;

  bool cusk;
  bool dead;

  body_part* pts;
  gpt* gptz;
} snake;

#endif
