#include <stdint.h>
#include "tinyc.h"

void audio_init();
void audio_play(const int16_t *v,int c); // Borrows (v)

#define PLAY_SOUND(name) audio_play(name,sizeof(name)/sizeof(int16_t));

/* My stuff...
 ******************************************************************************************/

static int16_t beep[1000];
static int16_t boop[1000];

// Framebuffer is *big-endian* BGR565 in 16-bit, or BGR332 in 8-bit
static uint8_t fb[96*64]={0};
static uint8_t bgbits[96*64];

static double next_time=0.0;

static uint8_t input_state=0;

#define SPRITE_IMAGE_SIZE_LIMIT (16*16)
#define SPRITEC 1
#define REDRAWC 1 /* to increase sprite count for performance testing; so we don't exhaust the static heap */
//    1:  5469 = 183 Hz
//   10:  5735 = 174 Hz
//  100:  8120 = 123 Hz
//  200:  9778 = 102 Hz
//  500: 16176 =  61 Hz
// 1000: 26783 =  37 Hz
// ...those are 8x8. 16x16 also looks good, up to 120 or so

static const uint8_t spritebits[SPRITE_IMAGE_SIZE_LIMIT]={
  0x1e,0x1e,0x1e,0x00,0x00,0x1e,0x1e,0x1e, 0xe0,0xe0,0xe0,0x1e,0x1e,0x1e,0x1e,0x1e,
  0x1e,0x1e,0x00,0xff,0xff,0x00,0x1e,0x1e, 0xe0,0x1e,0x1e,0xe0,0x1e,0x1e,0x1e,0x1e,
  0x1e,0x00,0xff,0xff,0xff,0xff,0x00,0x1e, 0xe0,0x1e,0x1e,0x1e,0xe0,0x1e,0x1e,0x1e,
  0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00, 0xe0,0x1e,0x1e,0x1e,0x1e,0xe0,0x1e,0x1e,
  0x00,0xff,0xff,0xe0,0x03,0xff,0xff,0x00, 0xe0,0x1e,0x1e,0x1e,0x1e,0x1e,0xe0,0x1e,
  0x00,0xff,0xe0,0xff,0xff,0x03,0xff,0x00, 0xe0,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0xe0,
  0x00,0xff,0xe0,0xff,0xff,0x03,0xff,0x00, 0xe0,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0xe0,
  0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x1e, 0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,

  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03, 0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,
  0x03,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x03, 0x18,0x1e,0x1e,0x1e,0x1e,0x1e,0x18,0x1e,
  0x1e,0x03,0x03,0x03,0x03,0x03,0x03,0x03, 0x18,0x18,0x18,0x18,0x18,0x18,0x1e,0x1e,
};

static struct sprite {
  int x,y;
  int w,h;
  int dx,dy;
  const uint8_t *bits;
  uint8_t hilite;
} spritev[SPRITEC];

static int framec=0;

static void draw_bgbits(uint8_t *dst) {
  /* entire color table 24 times *
  int i=sizeof(bgbits);
  for (;i-->0;dst++) *dst=i;
  /**/
  /* flat color */
  uint8_t v=0x92;
  int i=sizeof(bgbits);
  for (;i-->0;dst++) *dst=v;
  /**/
}

static void init_sprites() {
  struct sprite *sprite=spritev;
  int i=SPRITEC;
  for (;i-->0;sprite++) {
    sprite->w=16;
    sprite->h=16;
    sprite->x=rand()%(96-sprite->w);
    sprite->y=rand()%(64-sprite->h);
    sprite->bits=spritebits;
    do {
      sprite->dx=(rand()%3)-1;
      sprite->dy=(rand()%3)-1;
    } while (!sprite->dx&&!sprite->dy);
  }
}

static void apply_envelope(int16_t *v,int c,uint8_t a,uint8_t z) {
  int i=c,p=0;
  for (;i-->0;v++,p++) {
    uint8_t r=a+((p*(z-a))/c); // TODO would be cheaper to get this incrementally
    *v=((*v)*r)>>8;
  }
}

static void generate_noise_effect(int16_t *v,int c) {
  int16_t *p=v;
  int i=c;
  for (;i-->0;p++) *p=rand();
  int peakp=9*(c/10);
  apply_envelope(v,peakp,0,0x30);
  apply_envelope(v+peakp,c-peakp,0x30,0);
}

static void generate_tone_effect(int16_t *v,int c) {
  int16_t *p=v;
  int i=c;
  int16_t level=0x7fff;
  int halfperiod=40;
  int phase=0;
  for (;i-->0;p++) {
    *p=level;
    if (++phase>=halfperiod) {
      phase=0;
      level=-level;
    }
  }
  int peakp=c/5;
  apply_envelope(v,peakp,0,0x80);
  apply_envelope(v+peakp,c-peakp,0x80,0);
}

void my_main_setup(void) {
  tinyc_init();

  draw_bgbits(bgbits);
  init_sprites();
  generate_noise_effect(beep,sizeof(beep)/sizeof(int16_t));
  generate_tone_effect(boop,sizeof(boop)/sizeof(int16_t));
  
  tinyc_init_usb_log();

  audio_init();
}

static void update_sprites(struct sprite *sprite,int i) {
  for (;i-->0;sprite++) {

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
    if (input_state&TINYC_BUTTON_A) sprite->hilite|=0x01;
    else sprite->hilite&=~0x01;
    if (input_state&TINYC_BUTTON_B) sprite->hilite|=0x02;
    else sprite->hilite&=~0x02;
    
    sprite->x+=sprite->dx;
    if ((sprite->x<0)&&(sprite->dx<0)) sprite->dx=-sprite->dx;
    else if ((sprite->x+sprite->w>96)&&(sprite->dx>0)) sprite->dx=-sprite->dx;

    sprite->y+=sprite->dy;
    if ((sprite->y<0)&&(sprite->dy<0)) sprite->dy=-sprite->dy;
    else if ((sprite->y+sprite->h>64)&&(sprite->dy>0)) sprite->dy=-sprite->dy;
  }
}

static void draw_image(uint8_t *dst,int dststride,const uint8_t *src,int srcstride,int w,int h) {
  for (;h-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=w; for (;xi-->0;dstp++,srcp++) {
      if (*srcp==0x1e) continue; // colorkey
      *dstp=*srcp;
    }
  }
}

static void draw_hilite(uint8_t *dst,int dststride,int w,int h,int position,uint8_t color) {
  switch (position) {
    case 1: {
      for (;h-->0;dst+=dststride) *dst=color;
    } break;
    case 2: {
      dst+=w-1;
      for (;h-->0;dst+=dststride) *dst=color;
    } break;
  }
}

static void draw_sprite(uint8_t *dst,struct sprite *sprite) {
  
  int dstx=sprite->x,srcx=0,w=sprite->w;
  if (dstx<0) { w+=dstx; srcx-=dstx; dstx=0; }
  if (dstx+w>96) w=96-dstx;
  if (w<1) return;
  
  int dsty=sprite->y,srcy=0,h=sprite->h;
  if (dsty<0) { h+=dsty; srcy-=dsty; dsty=0; }
  if (dsty+h>64) h=64-dsty;
  if (h<1) return;

  dst+=dsty*96+dstx;
  draw_image(dst,96,sprite->bits+srcy*sprite->w+srcx,sprite->w,w,h);

  if (sprite->hilite&0x01) draw_hilite(dst,96,w,h,1,0x03);
  if (sprite->hilite&0x02) draw_hilite(dst,96,w,h,2,0xe0);
}

static void draw_sprites(uint8_t *dst,struct sprite *sprite,int i) {
  for (;i-->0;sprite++) draw_sprite(dst,sprite);
}

static void draw_hex_nybble(uint8_t *dst,uint8_t src,uint8_t color) {
  static const uint16_t bitsv[]={
    0x56d4, // 0: 010 101 101 101 010 = 0101 0110 1101 0100
    0xc92e, // 1: 110 010 010 010 111 = 1100 1001 0010 1110
    0xc54e, // 2: 110 001 010 100 111 = 1100 0101 0100 1110
    0xc51c, // 3: 110 001 010 001 110 = 1100 0101 0001 1100
    0xb792, // 4: 101 101 111 001 001 = 1011 0111 1001 0010
    0xf31c, // 5: 111 100 110 001 110 = 1111 0011 0001 1100
    0x7354, // 6: 011 100 110 101 010 = 0111 0011 0101 0100
    0xe492, // 7: 111 001 001 001 001 = 1110 0100 1001 0010
    0x5554, // 8: 010 101 010 101 010 = 0101 0101 0101 0100
    0x5592, // 9: 010 101 011 001 001 = 0101 0101 1001 0010
    0x57da, // a: 010 101 111 101 101 = 0101 0111 1101 1010
    0xd75c, // b: 110 101 110 101 110 = 1101 0111 0101 1100
    0x7246, // c: 011 100 100 100 011 = 0111 0010 0100 0110
    0xd6dc, // d: 110 101 101 101 110 = 1101 0110 1101 1100
    0xf34e, // e: 111 100 110 100 111 = 1111 0011 0100 1110
    0xf348, // f: 111 100 110 100 100 = 1111 0011 0100 1000
  };
  uint16_t bits=bitsv[src&15];
  int i=5;
  for (;i-->0;dst+=96,bits<<=3) {
    if (bits&0x8000) dst[0]=color;
    if (bits&0x4000) dst[1]=color;
    if (bits&0x2000) dst[2]=color;
  }
}

// WARNING: No clipping. Stride must be 96, and there must be 3x5 pixels left and down.
static void draw_hex_byte(uint8_t *dst,uint8_t src) {
  draw_hex_nybble(dst,src>>4,0x00);
  draw_hex_nybble(dst+4,src,0x00);
}

static void check_time() {
  framec++;
  if (framec>=1000) {
    char report[64];
    snprintf(report,sizeof(report),
      "%d frames (SPRITEC=%d) millis=%d\n",
      framec,SPRITEC*REDRAWC,millis()
    );
    tinyc_usb_log(report);
    framec=0;
  }
}

void my_main_loop() {

  unsigned long now=millis();//TODO seems there is also "micros()"
  if (now<next_time) {
    delay(1);
    return;
  }
  next_time+=16.66666;
  if (next_time<now) {
    tinyc_usb_log("dropped frame\n");
    next_time=now+16.66666;
  }

  uint8_t pvstate=input_state;
  input_state=tinyc_read_input();

  if ((input_state&TINYC_BUTTON_A)&&!(pvstate&TINYC_BUTTON_A)) PLAY_SOUND(beep)
  if ((input_state&TINYC_BUTTON_B)&&!(pvstate&TINYC_BUTTON_B)) PLAY_SOUND(boop)

  update_sprites(spritev,SPRITEC);

  memcpy(fb,bgbits,sizeof(fb));
  int i=REDRAWC; while (i-->0) {
    draw_sprites(fb,spritev,SPRITEC);
  }

  // My glyphs are 3x5, so advance by (3+1)*2+4=12 per byte.
  draw_hex_byte(fb+96+1+12*0,input_state);

  tinyc_send_framebuffer(fb);
  
  check_time();
}

/* Public entry points.
 */
 
void setup() { my_main_setup(); }
void loop() { my_main_loop(); }
