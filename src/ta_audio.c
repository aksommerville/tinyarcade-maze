/* ta_audio.cpp
 * Driver stuff copied from SuperOteme.
 */

#include "Arduino.h"

#define SOUND_FREQUENCY 11025

#define AUDIO_VOICE_LIMIT 8
static struct audio_voice {
  const int16_t *v;
  int c;
  int p;
} audio_voicev[AUDIO_VOICE_LIMIT]={0};
static int audio_voicec=0; // 0..AUDIO_VOICE_LIMIT, voices may be sparse

static int tcIsSyncing() {
  return TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
}

static void tcReset() {
  TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while(tcIsSyncing());
  while(TC5->COUNT16.CTRLA.bit.SWRST);
}

void audio_init() {
  analogWrite(A0, 0);
  // Enable GCLK for TCC2 and TC5 (timer counter input clock)
  GCLK->CLKCTRL.reg = (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while(GCLK->STATUS.bit.SYNCBUSY);
  tcReset();
  // Set Timer counter Mode to 16 bits
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
  // Set TC5 mode as match frequency
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1;
  TC5->COUNT16.CC[0].reg = (SystemCoreClock / SOUND_FREQUENCY - 1);
  while(tcIsSyncing());
  // Configure interrupt request
  NVIC_DisableIRQ(TC5_IRQn);
  NVIC_ClearPendingIRQ(TC5_IRQn);
  NVIC_SetPriority(TC5_IRQn, 0);
  NVIC_EnableIRQ(TC5_IRQn);
  // Enable the TC5 interrupt request
  TC5->COUNT16.INTENSET.bit.MC0 = 1;
  while(tcIsSyncing());
  // Enable TC
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; // Fails here if TC5_Handler unset or unpopulated.
  while(tcIsSyncing());
}

/* Seems this is called for every output frame.
 * We must set DAC->DATA.reg to unsigned PCM in 0..0x03ff
 * ...not sure about that. There's a click on startup, suggesting our zero is in the wrong place.
 * But whatever, this is fine for now.
 * TODO Actually I'm not sure about using PCM dumps across the board, given our memory constraints.
 * Consider adding some basic synthesizer voices.
 */
void TC5_Handler() {
  while(DAC->STATUS.bit.SYNCBUSY == 1);

  int sample=0;
  struct audio_voice *voice=audio_voicev;
  int i=audio_voicec;
  for (;i-->0;voice++) {
    if (!voice->v) continue;
    if (voice->p>=voice->c) {
      voice->v=0;
      voice->p=0;
      voice->c=0;
    } else {
      sample+=voice->v[voice->p++];
    }
  }
  DAC->DATA.reg=((sample>>6)+0x200)&0x3ff;
  
  while(DAC->STATUS.bit.SYNCBUSY == 1);
  TC5->COUNT16.INTFLAG.bit.MC0 = 1;
}

void audio_play(const int16_t *v,int c) {
  struct audio_voice *voice=0;
  int voicep=0,hip=-1;
  for (;voicep<AUDIO_VOICE_LIMIT;voicep++) {
    if (!audio_voicev[voicep].v) {
      if (!voice) {
        voice=audio_voicev+voicep;
        if (voicep>=audio_voicec) {
          audio_voicec=voicep+1;
        }
        hip=voicep;
      }
    } else hip=voicep;
  }
  if (!voice) return;
  audio_voicec=hip+1;
  voice->v=v;
  voice->p=0;
  voice->c=c;
}
