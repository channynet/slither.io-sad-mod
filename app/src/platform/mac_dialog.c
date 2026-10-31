// The macOS file picker (NSOpenPanel) through the Objective-C runtime, so the C code needs no
// Objective-C compiler. Kept apart from compat.h, whose BOOL differs from Objective-C's.
#ifdef __APPLE__
#include "mac_dialog.h"
#include <objc/message.h>
#include <objc/runtime.h>
#include <string.h>

static id cls(const char* name) { return (id)objc_getClass(name); }
static id mac_send(id target, const char* selector) {
  return ((id (*)(id, SEL))objc_msgSend)(target, sel_registerName(selector));
}
static void mac_send_bool(id target, const char* selector, BOOL value) {
  ((void (*)(id, SEL, BOOL))objc_msgSend)(target, sel_registerName(selector), value);
}
static void mac_send_id(id target, const char* selector, id value) {
  ((void (*)(id, SEL, id))objc_msgSend)(target, sel_registerName(selector), value);
}
static id string(const char* utf8) {
  return ((id (*)(id, SEL, const char*))objc_msgSend)(cls("NSString"), sel_registerName("stringWithUTF8String:"), utf8);
}

bool mac_choose_image(const char* message, char* path, size_t capacity) {
  bool chosen = false;
  if (!capacity) return false;
  path[0] = 0;
  id pool = mac_send(mac_send(cls("NSAutoreleasePool"), "alloc"), "init");
  id panel = mac_send(cls("NSOpenPanel"), "openPanel");
  if (panel) {
    mac_send_bool(panel, "setCanChooseFiles:", YES);
    mac_send_bool(panel, "setCanChooseDirectories:", NO);
    mac_send_bool(panel, "setAllowsMultipleSelection:", NO);
    mac_send_id(panel, "setMessage:", string(message));
    id types[] = {string("png"), string("jpg"), string("jpeg"), string("bmp"), string("PNG"), string("JPG"), string("JPEG"), string("BMP")};
    id list = ((id (*)(id, SEL, const id*, unsigned long))objc_msgSend)(cls("NSArray"),
        sel_registerName("arrayWithObjects:count:"), types, (unsigned long)(sizeof(types) / sizeof(types[0])));
    mac_send_id(panel, "setAllowedFileTypes:", list);       // deprecated in macOS 12, still honoured
    long answer = ((long (*)(id, SEL))objc_msgSend)(panel, sel_registerName("runModal"));
    if (answer == 1) {                                    // NSModalResponseOK
      id url = mac_send(panel, "URL");
      id file = url ? mac_send(url, "path") : (id)0;
      const char* utf8 = file ? ((const char* (*)(id, SEL))objc_msgSend)(file, sel_registerName("UTF8String")) : NULL;
      if (utf8 && strlen(utf8) < capacity) { strcpy(path, utf8); chosen = true; }
    }
  }
  mac_send(pool, "drain");
  return chosen;
}
#endif
