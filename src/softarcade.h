/* softarcade.h
 * Video and audio primitives designed with TinyArcade in mind, but pure software.
 * This can be built for any platform.
 */
 
#ifndef SOFTARCADE_H
#define SOFTARCADE_H

#include <stdint.h>

/* Image: bgr332 image with optional colorkey.
 * Limit 127 pixels in each direction.
 *************************************************************/
 
struct softarcade_image {
  int8_t w,h;
  uint8_t stride;
  uint8_t colorkey; // If nonzero, this color is transparent.
  uint8_t *v;
};

#define SOFTARCADE_IMAGE_DECLARE(name,_w,_h,...) \
  static uint8_t _img_storage_##name[_w*_h]={__VA_ARGS__}; \
  static struct softarcade_image image_##name={ \
    .w=_w, \
    .h=_h, \
    .stride=_w, \
    .colorkey=0, \
    .v=_img_storage_##name, \
  };
  
static inline void softarcade_image_clear(struct softarcade_image *image,uint8_t pixel) {
  memset(image->v,pixel,image->w*image->h);
}

/* Copy from (src) to (dst) respecting (src)'s colorkey.
 * !!! You must ensure that the output rectangle is in bounds !!!
 */
void softarcade_blit_unchecked(
  struct softarcade_image *dst,int8_t dstx,int8_t dsty,
  struct softarcade_image *src
);

/* Copy from (src) to (dst) respecting (src)'s colorkey.
 * It's OK if the output rect exceeds (dst)'s bounds, we check and clip.
 * There's no (srcx,srcy,w,h) -- if you want a subimage from (src), make a fresh handle.
 * TODO xform
 */
void softarcade_blit(
  struct softarcade_image *dst,int8_t dstx,int8_t dsty,
  struct softarcade_image *src
);

/* Overwrite (dst) with an image handle pointing into (src) at the given rectangle.
 * We check bounds and clip.
 */
void softarcade_image_crop(
  struct softarcade_image *dst,
  const struct softarcade_image *src,
  int8_t x,int8_t y,int8_t w,int8_t h
);

static inline uint8_t softarcade_pixel_from_xrgb(uint32_t xrgb) {
  return ((xrgb&0x00c00000)>>22)|((xrgb&0x0000e000)>>11)|(xrgb&0x000000e0);
}
static inline uint8_t softarcade_pixel_from_components(uint8_t r,uint8_t g,uint8_t b) {
  return (r>>6)|((g&0xe0)>>3)|(b&0xe0);
}
static inline uint32_t softarcade_xrgb_from_pixel(uint8_t pixel) {
  uint8_t b=pixel&0xe0; b|=b>>3; b|=b>>6;
  uint8_t g=pixel&0x1c; g|=g<<3; g|=g>>6;
  uint8_t r=pixel&0x03; r|=r<<2; r|=r<<4;
  return (r<<16)|(g<<8)|b;
}

/* Font: 1-bit images drawn with a fresh color each time.
 * Only codepoints 0x20..0x7f are printable.
 * Maximum glyph size 7x7 (but only the first 32 bits are addressable).
 * TODO compile-time tool to generate font from image.
 ***************************************************************/
 
struct softarcade_font {
  uint8_t metrics[0x60]; // (w<<5)|(h<<2)|y
  uint32_t bits[0x60]; // LRTB big-endianly
};

/* Render text in one color.
 * No line breaking; do that yourself before calling.
 * (dstx,dsty) is the top-left corner of the first glyph.
 * Returns the total advancement, including 1 blank column after the last glyph.
 */
int softarcade_font_render(
  struct softarcade_image *dst,
  int8_t dstx,int8_t dsty,
  const struct softarcade_font *font,
  const char *src,int8_t srcc,
  uint8_t pixel
);

int softarcade_font_measure(
  const struct softarcade_font *font,
  const char *src,int8_t srcc
);

/* Audio -- both the data and the code -- consumes a huge chunk of precious memory.
 * Making it optional.
 */
#ifndef SOFTARCADE_AUDIO_ENABLE
  #define SOFTARCADE_AUDIO_ENABLE 0
#endif

#if SOFTARCADE_AUDIO_ENABLE

/* Tuned wave playback.
 *************************************************************/
 
#define SOFTARCADE_WAVE_SIZE_BITS 8
#define SOFTARCADE_WAVE_SIZE_SAMPLES (1<<SOFTARCADE_WAVE_SIZE_BITS)
#define SOFTARCADE_WAVE_FRACT_SIZE_BITS (16-SOFTARCADE_WAVE_SIZE_BITS)
 
struct softarcade_voice {

  const int16_t *wave; // length=SOFTARCADE_WAVE_SIZE_SAMPLES
  uint16_t wavep;
  uint16_t rate;
  
  uint8_t level;
  
  uint16_t ratea,ratez;
  uint16_t ratep,ratec;
  
  uint8_t levela,levelz;
  uint16_t levelp,levelc;
  
  uint8_t term;
};

static inline uint8_t softarcade_voice_is_terminated(const struct softarcade_voice *voice) {
  if (!voice->wave) return 1;
  if (voice->level) return 0;
  if (voice->term) return 1;
  return 0;
}

int16_t softarcade_voice_update(struct softarcade_voice *voice);

void softarcade_voice_adjust_rate(struct softarcade_voice *voice,uint16_t rate,uint16_t ramptime);
void softarcade_voice_adjust_level(struct softarcade_voice *voice,uint8_t level,uint16_t ramptime);
void softarcade_voice_terminate(struct softarcade_voice *voice,uint16_t ramptime);

/* Verbatim PCM playback.
 **************************************************************/
 
struct softarcade_pcmrun {
  const int16_t *v;
  uint16_t c;
  uint16_t p;
};

// Zeroes all three fields when finished, and then returns zero forever.
int16_t softarcade_pcmrun_update(struct softarcade_pcmrun *pcmrun);

/* Synthesizer.
 ****************************************************************/
 
/* Events are structured around a voice limit of 8, it can't easily be changed.
 * PCM runner limit, whatever.
 */
#define SOFTARCADE_VOICE_LIMIT 8
#define SOFTARCADE_PCMRUN_LIMIT 8

struct softarcade_synth {
  struct softarcade_voice voicev[SOFTARCADE_VOICE_LIMIT];
  struct softarcade_pcmrun pcmrunv[SOFTARCADE_PCMRUN_LIMIT];
  uint8_t voicec,pcmrunc;
  //TODO song
  //TODO assets, how do we acquire them?
  uint32_t rate;
  uint16_t framespertick;
  uint16_t ramptime; // for next event only
  const int16_t *(*get_wave)(uint8_t waveid,void *userdata);
  uint16_t (*get_pcm)(void *pcmpp,uint8_t waveid,void *userdata);
  void *userdata;
};

/* Zero on success, or <0 on error.
 * Your (get_wave) must return a constant PCM dump exactly (SOFTARCADE_WAVE_SIZE_SAMPLES) long.
 * (get_wave,get_pcm) can also return zero to quietly reject the request.
 */
int8_t softarcade_synth_init(
  struct softarcade_synth *synth,
  uint32_t rate,
  const int16_t *(*get_wave)(uint8_t waveid,void *userdata),
  uint16_t (*get_pcm)(void *pcmpp,uint8_t waveid,void *userdata),
  void *userdata
);

int16_t softarcade_synth_update(struct softarcade_synth *synth);

/* 0xxx xxxx xxxx xxxx : Reserved for song-only events.
 * 1000 xxxx AAAA AAAA : Play PCM id (A).
 * 1001 xVVV AAAA AAAA : Reset voice (V), waveid (A). Sets level and rate to zero immediately.
 * 1010 xVVV xNNN NNNN : Set rate of voice (V) to MIDI note (N).
 * 1011 xVVV LLLL LLLL : Set level of voice (V) to (L).
 * 1100 TTTT TTTT TTTT : Ramp the next rate or level event over (T) ticks instead of immediate.
 * 1101 xVVV TTTT TTTT : Drop level of voice (V) to zero over (T) ticks, then terminate it.
 * 1110 xxxx xxxx xxxx : Reserved.
 * 1111 xxxx xxxx xxxx : Reserved.
 */
void softarcade_synth_event(struct softarcade_synth *synth,uint16_t event);

/* Caller must guarantee that (v) remain valid and unchanged while playing.
 */
void softarcade_synth_play_pcm(struct softarcade_synth *synth,const int16_t *v,uint16_t c);

uint16_t softarcade_rate_from_noteid(const struct softarcade_synth *synth,uint8_t noteid);

#endif /* SOFTARCADE_AUDIO_ENABLE */

#endif
