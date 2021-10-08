#include "softarcade.h"

/* Blit image, unchecked.
 */
 
void softarcade_blit_unchecked(
  struct softarcade_image *dst,int8_t dstx,int8_t dsty,
  const struct softarcade_image *src
) {
  uint8_t *dstrow=dst->v+dsty*dst->stride+dstx;
  const uint8_t *srcrow=src->v;
  int8_t yi=src->h;
  if (src->colorkey) {
    for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
      uint8_t *dstp=dstrow;
      const uint8_t *srcp=srcrow;
      int8_t xi=src->w;
      for (;xi-->0;dstp++,srcp++) {
        if ((*srcp)!=src->colorkey) *dstp=*srcp;
      }
    }
  } else {
    for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
      memcpy(dstrow,srcrow,src->w);
    }
  }
}

/* Check bounds, then blit.
 */

void softarcade_blit(
  struct softarcade_image *dst,int8_t dstx,int8_t dsty,
  const struct softarcade_image *src
) {
  if (!dst||!src) return;
  struct softarcade_image sub=*src;
  if (dsty<0) {
    if ((sub.h+=dsty)<1) return;
    sub.v+=sub.stride*-dsty;
    dsty=0;
  }
  if (dsty>dst->h-sub.h) {
    if ((sub.h=dst->h-dsty)<1) return;
  }
  if (dstx<0) {
    if ((sub.w+=dstx)<1) return;
    sub.v-=dstx;
    dstx=0;
  }
  if (dstx>dst->w-sub.w) {
    if ((sub.w=dst->w-dstx)<1) return;
  }
  softarcade_blit_unchecked(dst,dstx,dsty,&sub);
}

/* Slice image.
 */

void softarcade_image_crop(
  struct softarcade_image *dst,
  const struct softarcade_image *src,
  int8_t x,int8_t y,int8_t w,int8_t h
) {
  if (!dst||!src) return;
  dst->stride=src->stride;
  dst->colorkey=src->colorkey;
  if (x<0) {
    if ((w+=x)<0) w=0;
    x=0;
  }
  if (x>src->w-w) {
    if ((w=src->w-x)<0) w=0;
  }
  if (y<0) {
    if ((h+=y)<0) h=0;
    y=0;
  }
  if (y>src->h-h) {
    if ((h=src->h-y)<0) h=0;
  }
  dst->w=w;
  dst->h=h;
  dst->v=src->v+src->stride*y+x;
}

/* Render glyph.
 */
 
static void softarcade_glyph_render(
  struct softarcade_image *dst,
  int8_t dstx,int8_t dsty,
  int8_t w,int8_t h,
  uint32_t bits,
  uint8_t pixel
) {
  int8_t srcstride=w;
  if (dstx<0) {
    if ((w+=dstx)<1) return;
    bits<<=-dstx;
    dstx=0;
  }
  if (dstx>dst->w-w) {
    if ((w=dst->w-dstx)<1) return;
  }
  if (dsty<0) {
    if ((h+=dsty)<1) return;
    bits<<=-dsty*srcstride;
    dsty=0;
  }
  if (dsty>dst->h-h) {
    if ((h=dst->h-dsty)<1) return;
  }
  uint8_t *dstrow=dst->v+dsty*dst->stride+dstx;
  for (;h-->0;dstrow+=dst->stride,bits<<=srcstride) {
    uint8_t *dstp=dstrow;
    uint32_t bitsp=bits;
    int8_t xi=w;
    for (;xi-->0;dstp++,bitsp<<=1) {
      if (bitsp&0x80000000) *dstp=pixel;
    }
  }
}

/* Render text line.
 */
 
int softarcade_font_render(
  struct softarcade_image *dst,
  int8_t dstx,int8_t dsty,
  const struct softarcade_font *font,
  const char *src,int8_t srcc,
  uint8_t pixel
) {
  int rtn=0;
  for (;srcc-->0;src++) {
  
    // Skip anything outside G0*, and shuffle from codepoint to index.
    // [*] We do allow 0x7f DEL, and 0x20 SPC which I don't know if it's strictly "G0" or "C0", whatever.
    uint8_t p=(*src)-0x20;
    if (p>=0x60) continue;
    
    uint8_t metrics=font->metrics[p];
    uint32_t bits=font->bits[p];
    uint8_t w=metrics>>5;
    if (!w) continue; // Important: Zero-width glyphs do not cause implicit advancement either.
    uint8_t h=(metrics>>2)&7;
    uint8_t y=metrics&3;
    
    softarcade_glyph_render(dst,dstx,dsty+y,w,h,bits,pixel);
    
    w+=1;
    dstx+=w;
    rtn+=w;
  }
  return rtn;
}

/* Measure text line.
 */

int softarcade_font_measure(
  const struct softarcade_font *font,
  const char *src,int8_t srcc
) {
  int rtn=0;
  for (;srcc-->0;src++) {
    uint8_t p=(*src)-0x20;
    if (p>=0x60) continue;
    uint8_t metrics=font->metrics[p];
    uint8_t w=metrics>>5;
    if (!w) continue; // Important: Zero-width glyphs do not cause implicit advancement either.
    rtn+=w+1;
  }
  return rtn;
}

#if SOFTARCADE_AUDIO_ENABLE

/* Update voice.
 */

int16_t softarcade_voice_update(struct softarcade_voice *voice) {
  if (!voice->wave) return 0;
  
  int16_t sample=voice->wave[voice->wavep>>SOFTARCADE_WAVE_FRACT_SIZE_BITS];
  sample=((int32_t)sample*voice->level)>>8;
  
  voice->wavep+=voice->rate;
  
  if (voice->ratec) {
    voice->ratep++;
    if (voice->ratep>=voice->ratec) {
      voice->rate=voice->ratez;
      voice->ratep=voice->ratec=0;
    } else {
      //TODO This is a lot of division... Is it feasible to pre-print ramps instead? Force them to power-of-two lengths?
      voice->rate=voice->ratea+((voice->ratez-voice->ratea)*voice->ratep)/voice->ratec;
    }
  }
  
  if (voice->levelc) {
    voice->levelp++;
    if (voice->levelp>=voice->levelc) {
      voice->level=voice->levelz;
      voice->levelp=voice->levelc=0;
    } else {
      voice->level=voice->levela+((voice->levelz-voice->levela)*voice->levelp)/voice->levelc;
    }
  }
  
  return sample;
}

/* Set voice ramps.
 */

void softarcade_voice_adjust_rate(struct softarcade_voice *voice,uint16_t rate,uint16_t ramptime) {
  voice->ratea=voice->rate;
  voice->ratez=rate;
  if (ramptime&&(voice->ratea!=voice->ratez)) {
    voice->ratep=0;
    voice->ratec=ramptime;
  } else {
    voice->rate=rate;
    voice->ratep=0;
    voice->ratec=0;
  }
}

void softarcade_voice_adjust_level(struct softarcade_voice *voice,uint8_t level,uint16_t ramptime) {
  voice->levela=voice->level;
  voice->levelz=level;
  if (ramptime&&(voice->levela!=voice->levelz)) {
    voice->levelp=0;
    voice->levelc=ramptime;
  } else {
    voice->level=level;
    voice->levelp=0;
    voice->levelc=0;
  }
}

void softarcade_voice_terminate(struct softarcade_voice *voice,uint16_t ramptime) {
  softarcade_voice_adjust_level(voice,0,ramptime);
  voice->term=1;
}

/* Update PCM runner.
 */
 
int16_t softarcade_pcmrun_update(struct softarcade_pcmrun *pcmrun) {
  if (!pcmrun->v) return 0;
  if (pcmrun->p>=pcmrun->c) {
    pcmrun->v=0;
    pcmrun->p=0;
    pcmrun->c=0;
    return 0;
  }
  return pcmrun->v[pcmrun->p++];
}

/* Initialize synthesizer.
 */
 
static const int16_t *softarcade_get_wave_default(uint8_t waveid,void *userdata) {
  return 0;
}

static uint16_t softarcade_get_pcm_default(void *pcmpp,uint8_t waveid,void *userdata) {
  return 0;
}
 
int8_t softarcade_synth_init(
  struct softarcade_synth *synth,
  uint32_t rate,
  const int16_t *(*get_wave)(uint8_t waveid,void *userdata),
  uint16_t (*get_pcm)(void *pcmpp,uint8_t waveid,void *userdata),
  void *userdata
) {
  if (!synth) return -1;
  if ((rate<100)||(rate>100000)) return -1;
  memset(synth,0,sizeof(struct softarcade_synth));
  synth->rate=rate;
  synth->framespertick=synth->rate/24; // 120 bpm and 12 ticks/qnote
  synth->get_wave=get_wave?get_wave:softarcade_get_wave_default;
  synth->get_pcm=get_pcm?get_pcm:softarcade_get_pcm_default;
  synth->userdata=userdata;
  return 0;
}

/* Update synthesizer.
 */
 
int16_t softarcade_synth_update(struct softarcade_synth *synth) {
  int16_t sample=0;
  
  int8_t i=synth->voicec;
  struct softarcade_voice *voice=synth->voicev+i-1;
  for (;i-->0;voice--) {
    sample+=softarcade_voice_update(voice);
  }
  
  i=synth->pcmrunc;
  struct softarcade_pcmrun *pcmrun=synth->pcmrunv+i-1;
  for (;i-->0;pcmrun--) {
    sample+=softarcade_pcmrun_update(pcmrun);
  }
  
  // Drop the last of each object type if it's terminated.
  // No need to check in a loop, we can catch the others next time around.
  if (synth->voicec&&softarcade_voice_is_terminated(synth->voicev+synth->voicec-1)) synth->voicec--;
  if (synth->pcmrunc&&!synth->pcmrunv[synth->pcmrunc-1].v) synth->pcmrunc--;
  
  return sample;
}

/* Process one synthesizer event.
 */

void softarcade_synth_event(struct softarcade_synth *synth,uint16_t event) {
  switch (event&0xf000) {
  
    // 0x0000..0x7000 reserved
  
    case 0x8000: { // play pcm
        uint8_t pcmid=event&0xff;
        int16_t *v=0;
        uint16_t c=synth->get_pcm(&v,pcmid,synth->userdata);
        softarcade_synth_play_pcm(synth,v,c);
      } break;
      
    case 0x9000: { // reset voice
        uint8_t voiceid=(event>>8)&7;
        struct softarcade_voice *voice=synth->voicev+voiceid;
        uint8_t waveid=event&0xff;
        if (voice->wave=synth->get_wave(waveid,synth->userdata)) {
          voice->rate=0;
          voice->ratep=voice->ratec=0;
          voice->level=0;
          voice->levelp=voice->levelc=0;
          voice->term=0;
          if (voiceid>=synth->voicec) synth->voicec=voiceid+1;
        }
      } break;
      
    case 0xa000: { // set voice rate
        uint8_t voiceid=(event>>8)&7;
        struct softarcade_voice *voice=synth->voicev+voiceid;
        uint16_t rate=softarcade_rate_from_noteid(synth,event&0x7f);
        softarcade_voice_adjust_rate(voice,rate,synth->ramptime);
      } break;
      
    case 0xb000: { // set voice level
        uint8_t voiceid=(event>>8)&7;
        struct softarcade_voice *voice=synth->voicev+voiceid;
        uint8_t level=event&0xff;
        softarcade_voice_adjust_level(voice,level,synth->ramptime);
      } break;
      
    case 0xc000: { // stash ramp time
        synth->ramptime=(event&0x0fff)*synth->framespertick;
      } return; // not 'break': every other command clears the stashed ramp time
      
    case 0xd000: { // terminate voice
        uint8_t voiceid=(event>>8)&7;
        struct softarcade_voice *voice=synth->voicev+voiceid;
        uint16_t framec=(event&0xff)*synth->framespertick;
        softarcade_voice_terminate(voice,framec);
      } break;
      
    // 0xe000,0xf000 reserved
  }
  synth->ramptime=0;
}

/* Begin PCM playback.
 */
 
void softarcade_synth_play_pcm(struct softarcade_synth *synth,const int16_t *v,uint16_t c) {
  if (!v||!c) return;
  struct softarcade_pcmrun *oldest=0;
  struct softarcade_pcmrun *pcmrun=synth->pcmrunv;
  int i=SOFTARCADE_PCMRUN_LIMIT;
  for (;i-->0;pcmrun++) {
    if (!pcmrun->v) {
      pcmrun->v=v;
      pcmrun->c=c;
      pcmrun->p=0;
      int id=pcmrun-synth->pcmrunv;
      if (id>=synth->pcmrunc) synth->pcmrunc=id+1;
      return;
    }
    if (!oldest||(pcmrun->p>oldest->p)) oldest=pcmrun;
  }
  // Terminate whatever's been playing the longest and take it over.
  // I think this is the right strategy? Certainly a matter of opinion.
  oldest->v=v;
  oldest->c=c;
  oldest->p=0;
}

/* Rate from MIDI note.
 */
 
#define TWELFTH_ROOT_TWO 1.0594630943592953
 
static uint16_t softarcade_rate_by_noteid[0x80];
static uint32_t softarcade_rate_by_noteid_mainrate=0;
 
uint16_t softarcade_rate_from_noteid(const struct softarcade_synth *synth,uint8_t noteid) {
  if (noteid>=0x80) return 0;
  
  if (synth->rate!=softarcade_rate_by_noteid_mainrate) {
    uint16_t *v=softarcade_rate_by_noteid;
    
    // Begin with the 440 Hz reference note. TODO Confirm with the spec...
    v[0x45]=(440l<<16)/synth->rate;
    
    // Populate a full octave the hard way: Multiply each note by the twelfth root of two.
    int16_t p=0x46;
    for (;p<0x51;p++) v[p]=(uint16_t)((double)v[p-1]*TWELFTH_ROOT_TWO);
    
    // Proceed to the bottom, 1/2 times the note one octave higher.
    for (p=0x45;p-->0;) v[p]=v[p+12]>>1;
    
    // '' to the top, 2x ''
    for (p=0x51;p<0x80;p++) v[p]=v[p-12]<<1;
    
    softarcade_rate_by_noteid_mainrate=synth->rate;
  }
  
  return softarcade_rate_by_noteid[noteid];
}

#endif
