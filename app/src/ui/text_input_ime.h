#ifndef TEXT_INPUT_IME_H
#define TEXT_INPUT_IME_H
#ifdef __cplusplus
extern "C" {
#endif
void text_input_ime_install(void);
void text_input_ime_restore(void);
// The syllable still being composed is drawn by us at the caret, once per frame after the UI.
void text_input_ime_draw(void);
const char* text_input_ime_composing(void);
#ifdef __cplusplus
}
#endif
#endif
