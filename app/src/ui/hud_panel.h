#ifndef HUD_PANEL_H
#define HUD_PANEL_H

// Measure the actual content before drawing its non-interactive background.
// No extra windows are created, so gameplay mouse input stays unchanged.
static inline void hud_panel_begin(void) {
  ImDrawList_ChannelsSplit(igGetWindowDrawList(),2);
  ImDrawList_ChannelsSetCurrent(igGetWindowDrawList(),1);
  igBeginGroup();
}
static inline void hud_panel_end(void) {
  igEndGroup();
  ImVec2 min,max; igGetItemRectMin(&min); igGetItemRectMax(&max);
  min.x-=10; min.y-=8; max.x+=10; max.y+=8;
  ImDrawList* draw=igGetWindowDrawList();
  ImDrawList_ChannelsSetCurrent(draw,0);
  ImDrawList_AddRectFilled(draw,min,max,
      igGetColorU32_Vec4((ImVec4){.12f,.12f,.12f,.62f}),12,0);
  ImDrawList_AddRect(draw,min,max,
      igGetColorU32_Vec4((ImVec4){1,1,1,.10f}),12,0,1);
  ImDrawList_ChannelsMerge(draw);
}
#endif
