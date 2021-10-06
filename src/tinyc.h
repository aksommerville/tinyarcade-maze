/* tinyc.h
 * Alternative to TinyArcade libraries, with C linkage.
 * Not supporting audio, because I've already got ta_audio.h doing that.
 */

#ifndef TINYC_H
#define TINYC_H

#ifdef __cplusplus
extern "C" {
#endif

void tinyc_init();

// 96*64=6144 bytes
void tinyc_send_framebuffer(const void *fb);

// Logging via serial monitor. TODO
static inline void tinyc_init_usb_log() {}
static inline void tinyc_usb_log(const char *src) {}

uint8_t tinyc_read_input();
#define TINYC_BUTTON_LEFT     0x01
#define TINYC_BUTTON_RIGHT    0x02
#define TINYC_BUTTON_UP       0x04
#define TINYC_BUTTON_DOWN     0x08
#define TINYC_BUTTON_A        0x10
#define TINYC_BUTTON_B        0x20

#ifdef __cplusplus
}
#endif

#endif
