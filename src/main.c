#include <stdint.h>
#include <math.h>
#include "tinyc.h"
#include "softarcade.h"

/* Audio.
 *************************************************************/

#if SOFTARCADE_AUDIO_ENABLE

struct softarcade_synth synth={0};

int16_t tinyc_client_update_synthesizer() {
  return softarcade_synth_update(&synth);
}

// There can be up to 256 waves and 256 PCMs -- we'd run out of memory way before that, I think.
static int16_t mywave1[SOFTARCADE_WAVE_SIZE_SAMPLES];
static uint8_t mywave1_init=0;
static int16_t mywave2[SOFTARCADE_WAVE_SIZE_SAMPLES];
static uint8_t mywave2_init=0;
static int16_t mywave3[SOFTARCADE_WAVE_SIZE_SAMPLES];
static uint8_t mywave3_init=0;

static const int16_t *get_wave(uint8_t waveid,void *userdata) {
  switch (waveid%3+1) {
    case 1: { // square
        if (!mywave1_init) {
          memset(mywave1,0x18,SOFTARCADE_WAVE_SIZE_SAMPLES);
          memset(mywave1+(SOFTARCADE_WAVE_SIZE_SAMPLES>>1),0xe8,SOFTARCADE_WAVE_SIZE_SAMPLES);
          mywave1_init=1;
        }
        return mywave1;
      }
    case 2: { // sine
        if (!mywave2_init) {
          int16_t *v=mywave2;
          int i=SOFTARCADE_WAVE_SIZE_SAMPLES;
          for (;i-->0;v++) {
            *v=
              sinf((i*M_PI*2.0f)/SOFTARCADE_WAVE_SIZE_SAMPLES)*22000+
              sinf((i*M_PI*4.0f)/SOFTARCADE_WAVE_SIZE_SAMPLES)*10000
            ;
          }
          mywave2_init=1;
        }
        return mywave2;
      }
    case 3: { // saw
        if (!mywave3_init) {
          int16_t *v=mywave3;
          int i=SOFTARCADE_WAVE_SIZE_SAMPLES;
          for (;i-->0;v++) *v=(i*0x1fff)/SOFTARCADE_WAVE_SIZE_SAMPLES-0x2000;
          mywave3_init=1;
        }
        return mywave3;
      }
  }
  return 0;
}

static uint16_t get_pcm(void *pcmpp,uint8_t waveid,void *userdata) {
  return 0;
}

#else
int16_t tinyc_client_update_synthesizer() { return 0; }
#endif

/* Video.
 ***********************************************************************/
 
#define IDLE_BGCOLOR 0x92
static uint8_t bgcolor=IDLE_BGCOLOR;
 
SOFTARCADE_IMAGE_DECLARE(fb,96,64,0)

#define _ 0x1c,
#define K 0x00,
#define W 0xff,
#define Y 0x92,
#define B 0xe0,
#define G 0x18,
#define R 0x03,
SOFTARCADE_IMAGE_DECLARE(spritebits,16,16,
  _ _ _ _ _ K K K  K K K _ _ _ _ _
  _ _ _ K K W W W  W W W K K _ _ _
  _ _ K W W W W W  W W W W W K _ _
  _ K W W W W B W  W B W W W W K _
  _ K W W W B B W  W B B W W W K _
  K W W W W B B W  W B B W W W W K
  K W W W W B B W  W B B W W W W K
  K W W W W W W W  W W W W W W W K
  
  K W W W W W W W  W W W W W W W K
  K W W R W W W W  W W W W R W W K
  K W W W R W W W  W W W R W W W K
  _ K W W W R R R  R R R W W W K _
  _ K W W W W W W  W R W W W W K _
  _ _ K W W W W W  W W W W W K _ _
  _ K _ K K W W W  W W W K K _ K _
  K _ _ _ _ K K K  K K K _ _ _ _ K
)

SOFTARCADE_IMAGE_DECLARE(npcbits,8,8,
  B _ _ _ _ _ K _
  _ B _ _ _ K G K
  _ _ B _ K G G K
  _ _ _ K G G G K
  _ _ K G G G G K
  _ K G G G R R K
  K G G G G R R K
  _ K K K K K K _
)

SOFTARCADE_IMAGE_DECLARE(fourbits,4,4,
  _ K K _
  K R G K
  K B Y K
  _ K K _
)

SOFTARCADE_IMAGE_DECLARE(twobits,2,2,
  W Y
  Y K
)

SOFTARCADE_IMAGE_DECLARE(pixel,1,1,
  W
)

#undef _
#undef K
#undef W
#undef Y
#undef B
#undef G
#undef R

/* Sprites.
 **************************************************************/

/* Silent, 16x16 => 60
 * Silent, 8x8 => 210
 * Silent, 4x4 => 550
 * Silent, 2x2 => 900
 * Silent, 1x1 => 1100
 */
#define SPRITEC 20

static struct sprite {
  int8_t x,y;
  int8_t dx,dy;
  const struct softarcade_image *image;
} spritev[SPRITEC];

static int framec=0;

static void init_sprites() {
  struct sprite *sprite=spritev;
  int i=SPRITEC;
  for (;i-->0;sprite++) {
    if (i) sprite->image=&image_npcbits;
    else sprite->image=&image_spritebits;
    sprite->x=rand()%(96-sprite->image->w);
    sprite->y=rand()%(64-sprite->image->h);
    do {
      sprite->dx=(rand()%3)-1;
      sprite->dy=(rand()%3)-1;
    } while (!sprite->dx&&!sprite->dy);
  }
}

static void update_sprites(struct sprite *sprite,int i,uint8_t input_state) {
  for (;i-->0;sprite++) {

    if (!i) {
      switch (input_state&(TINYC_BUTTON_LEFT|TINYC_BUTTON_RIGHT)) {
        case TINYC_BUTTON_LEFT: sprite->dx=-1; break;
        case TINYC_BUTTON_RIGHT: sprite->dx=1; break;
        default: sprite->dx=0;
      }
      switch (input_state&(TINYC_BUTTON_UP|TINYC_BUTTON_DOWN)) {
        case TINYC_BUTTON_UP: sprite->dy=-1; break;
        case TINYC_BUTTON_DOWN: sprite->dy=1; break;
        default: sprite->dy=0;
      }
    }
    
    sprite->x+=sprite->dx;
    if ((sprite->x<0)&&(sprite->dx<0)) sprite->dx=-sprite->dx;
    else if ((sprite->x+sprite->image->w>96)&&(sprite->dx>0)) sprite->dx=-sprite->dx;

    sprite->y+=sprite->dy;
    if ((sprite->y<0)&&(sprite->dy<0)) sprite->dy=-sprite->dy;
    else if ((sprite->y+sprite->image->h>64)&&(sprite->dy>0)) sprite->dy=-sprite->dy;
  }
}

static void draw_sprites(struct sprite *sprite,int i) {
  for (;i-->0;sprite++) {
    softarcade_blit(&image_fb,sprite->x,sprite->y,sprite->image);
  }
}

/* Timing.
 **************************************************/

static double next_time=0.0;

static void check_time() {
  framec++;
  if (framec>=1000) {
    char report[64];
    snprintf(report,sizeof(report),
      "%d frames (SPRITEC=%d) millis=%d\n",
      framec,SPRITEC,millis()
    );
    tinyc_usb_log(report);
    framec=0;
  }
}

/* Main loop.
 ******************************************************************/

static uint8_t noteid=0x40;
static uint8_t voiceid=0;
static uint8_t waveid=0;
static uint16_t songp=0;

void loop() {

  unsigned long now=millis();//TODO seems there is also "micros()"
  if (now<next_time) {
    delay(1);
    return;
  }
  next_time+=16.66666;
  if (next_time<now) {
    tinyc_usb_log("dropped frame\n");
    next_time=now+16.66666;
    bgcolor=IDLE_BGCOLOR+15;
  }
  
  #if SOFTARCADE_AUDIO_ENABLE
  /**
  if (++songp>=20) {
    songp=0;
    softarcade_synth_event(&synth,0xd008|(voiceid<<8)); // terminate previous note
    if (++voiceid>=8) voiceid=0;
    if (++noteid>=0x60) noteid=0x40;
    waveid++;
    softarcade_synth_event(&synth,0x9000|(voiceid<<8)|waveid); // reset voice
    softarcade_synth_event(&synth,0xa000|(voiceid<<8)|noteid); // set rate
    softarcade_synth_event(&synth,0xc001); // ramp up
    softarcade_synth_event(&synth,0xb060|(voiceid<<8)); // set level (+ramp)
  }
  /**/
  #endif

  uint8_t input_state=tinyc_read_input();
  update_sprites(spritev,SPRITEC,input_state);

  if (bgcolor>IDLE_BGCOLOR) bgcolor--;
  softarcade_image_clear(&image_fb,bgcolor);
  draw_sprites(spritev,SPRITEC);

  tinyc_send_framebuffer(image_fb.v);
  
  check_time();
}

/* Initialize.
 */

void setup() {

  #if SOFTARCADE_AUDIO_ENABLE
  softarcade_synth_init(&synth,22050,get_wave,get_pcm,0);
  #endif

  tinyc_init();

  init_sprites();
  image_spritebits.colorkey=0x1c;
  image_npcbits.colorkey=0x1c;
  
  tinyc_init_usb_log();
}
