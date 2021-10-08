/* tinyc.h
 * Alternative to TinyArcade libraries, with C linkage.
 * I'll implement this for other platforms, too, so we can build native workalikes of our TA apps.
 */

#ifndef TINYC_H
#define TINYC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* You must implement this and return mixed audio one sample at a time.
 * Use the full range -32768..32767.
 */
int16_t tinyc_client_update_synthesizer();

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
