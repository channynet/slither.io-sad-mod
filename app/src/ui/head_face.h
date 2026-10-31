#ifndef HEAD_FACE_H
#define HEAD_FACE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t head_face_texture(int index);
bool head_face_available(bool smile,bool claws);
// scale: pixels per head-size unit (snake scale * zoom). unit: pixels per world unit (zoom only);
// the original game adds a few fixed half-unit offsets that do not grow with the snake.
// head_color tints the eye stalks so they match the head.
void head_face_draw(float x,float y,float scale,float unit,float angle,float alpha,bool smile,bool claws,
                    float gaze_x,float gaze_y,const float head_color[3]);
#ifdef __cplusplus
}
#endif
#endif
