#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
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
 
SOFTARCADE_IMAGE_DECLARE(fb,96,64,0)
SOFTARCADE_IMAGE_DECLARE(bgbits,96,64,0)

SOFTARCADE_IMAGE_DECLARE(mouse,7,7,
  28,28,109,109,109,109,109,
  109,28,28,0,109,0,28,
  28,109,146,109,109,109,28,
  146,146,146,109,109,109,146,
  146,146,146,146,146,146,146,
  146,146,146,146,146,146,146,
  146,28,146,28,146,28,146,
)

SOFTARCADE_IMAGE_DECLARE(cheese,7,7,
  28,28,28,28,28,28,28,
  28,55,191,191,28,28,28,
  28,55,55,191,191,28,28,
  28,191,55,55,191,191,28,
  28,55,55,55,55,55,28,
  28,28,28,55,191,55,28,
  28,28,28,28,28,28,28,
)

static struct softarcade_font font={
  .metrics={
    96,53,105,181,124,181,184,41,85,85,181,181,83,167,47,181,149,117,149,117,149,149,149,149,149,
    149,46,83,117,142,117,153,149,149,149,117,149,149,149,149,149,117,149,149,117,181,181,181,149,
    149,149,149,181,149,181,181,181,181,117,85,181,85,173,175,108,149,149,114,149,149,117,150,149,
    50,89,117,85,175,143,146,150,150,146,149,117,143,175,175,111,150,114,117,53,117,174,0
  },
  .bits={
    0,3892314112,3019898880,1473639680,1584648192,2290649216,1158764852,3221225472,1782579200,
    2508193792,2881415808,557728256,218103808,4160749568,536870912,143165440,1805475840,3375235072,
    3781750784,3306946560,2582712320,4175552512,1760124928,4045684736,1768513536,1769017344,
    2684354560,1291845632,706871296,4042260480,2292711424,3781428224,1773694976,1777963008,
    3924418560,1917190144,3919175680,4176015360,4175986688,1756983296,2583269376,3912105984,
    286875648,2596966400,2454585344,2397771904,2389391488,1952651008,3924328448,1771794432,
    3924398080,2019680256,4178067968,2576965632,2354356736,2355844352,2324211840,2354856448,
    3847094272,3938451456,2181570688,3586129920,581042176,4063232,2290089984,1635348480,
    2297028608,1915748352,293171200,1776836608,732168192,1769037824,2297008128,2952790016,
    1163919360,2464808960,2856321024,3580493824,3918528512,1771438080,1776844800,1769017344,
    3918004224,2019680256,1268908032,2574254080,2324168704,2371092480,2860515328,2574344192,
    4017094656,1780875264,4160749568,3366715392,1162084352,0
  },
};


/* Game logic.
 **************************************************************/
 
#define COLC 12
#define ROWC 8

// Each cell of the maze has two flags.
#define WALL_RIGHT 0x01
#define WALL_DOWN  0x02
 
static uint8_t maze[COLC*ROWC]={0};
static uint8_t mousecol=0;
static uint8_t mouserow=0;
static int8_t mousex=0;
static int8_t mousey=0;
static int8_t mousedx=0;
static int8_t mousedy=0;
static uint8_t cheesecol=0;
static uint8_t cheeserow=0;

/* From unvisited cell (col,row), knock down walls into a random unvisited neighbor until we can't.
 */
static void drunk_walk(uint8_t col,uint8_t row,uint8_t *visited) {
  while (1) {
    uint8_t p=row*COLC+col;
    visited[p]=1;

    struct neighbor { int8_t dx,dy; } neighborv[4];
    uint8_t neighborc=0;
    if ((col>0)&&!visited[p-1]) neighborv[neighborc++]=(struct neighbor){-1,0};
    if ((row>0)&&!visited[p-COLC]) neighborv[neighborc++]=(struct neighbor){0,-1};
    if ((col<COLC-1)&&!visited[p+1]) neighborv[neighborc++]=(struct neighbor){1,0};
    if ((row<ROWC-1)&&!visited[p+COLC]) neighborv[neighborc++]=(struct neighbor){0,1};
    if (!neighborc) return; // Painted into a corner.
  
    struct neighbor neighbor=neighborv[rand()%neighborc];
         if (neighbor.dx<0) maze[p-1]&=~WALL_RIGHT;
    else if (neighbor.dx>0) maze[p]&=~WALL_RIGHT;
    else if (neighbor.dy<0) maze[p-COLC]&=~WALL_DOWN;
    else if (neighbor.dy>0) maze[p]&=~WALL_DOWN;
  
    col+=neighbor.dx;
    row+=neighbor.dy;
  }
}

/* Select an unvisited cell with a visited cardinal neighbor.
 * ("bar hop" = where the next "drunk walk" begins).
 * Returns nonzero if we found one, zero if there's none.
 * If we find one, we break the wall to the visited neighbor.
 */
static uint8_t bar_hop(uint8_t *col,uint8_t *row,const uint8_t *visited) {

  /* Gather all the candidates and select randomly among them.
   * This might be overkill -- we could search in order and use the first candidate.
   * But I suspect if we operated in order like that, we'd see longer paths up top and stubs near the bottom.
   * So random, in the interest of uniformity.
   */
  uint8_t candidatev[COLC*ROWC];
  uint8_t candidatec=0;
  const uint8_t *ckv=visited;
  uint8_t cky=0;
  for (;cky<ROWC;cky++) {
    uint8_t ckx=0;
    for (;ckx<COLC;ckx++,ckv++) {
      if (*ckv) continue; // visited
      if (
        ((ckx>0)&&ckv[-1])||
        ((cky>0)&&ckv[-COLC])||
        ((ckx<COLC-1)&&ckv[1])||
        ((cky<ROWC-1)&&ckv[COLC])
      ) {
        candidatev[candidatec++]=cky*COLC+ckx;
      }
    }
  }
  if (!candidatec) return 0;
  
  uint8_t p=candidatev[rand()%candidatec];
  *col=p%COLC;
  *row=p/COLC;
  
  /* If there's more than one visited neighbor, again pick one randomly.
   */
  struct neighbor { int8_t dx,dy; } neighborv[4];
  uint8_t neighborc=0;
  if ((*col>0)&&visited[p-1]) neighborv[neighborc++]=(struct neighbor){-1,0};
  if ((*row>0)&&visited[p-COLC]) neighborv[neighborc++]=(struct neighbor){0,-1};
  if ((*col<COLC-1)&&visited[p+1]) neighborv[neighborc++]=(struct neighbor){1,0};
  if ((*row<ROWC-1)&&visited[p+COLC]) neighborv[neighborc++]=(struct neighbor){0,1};
  if (!neighborc) return 0; // oops!
  
  struct neighbor neighbor=neighborv[rand()%neighborc];
       if (neighbor.dx<0) maze[p-1]&=~WALL_RIGHT;
  else if (neighbor.dx>0) maze[p]&=~WALL_RIGHT;
  else if (neighbor.dy<0) maze[p-COLC]&=~WALL_DOWN;
  else if (neighbor.dy>0) maze[p]&=~WALL_DOWN;

  return 1;
}

static void generate_maze() {
  srand(millis());

  // Select random positions for the mouse and the cheese.
  // To prevent collisions, put the mouse always in the left half and cheese always in the right.
  // (I think that is also good psychologically? Player always moves rightward.)
  mousecol=rand()%(COLC>>1);
  mouserow=rand()%ROWC;
  cheesecol=(COLC>>1)+(rand()%(COLC>>1));
  cheeserow=rand()%ROWC;
  
  // Start with every wall, and a blank "visited" map.
  memset(maze,WALL_RIGHT|WALL_DOWN,sizeof(maze));
  uint8_t visited[COLC*ROWC]={0};
  
  /* 1. Pick a random cell.
   * 2. Drunk-walk until we're painted into a corner.
   * 3. Select an unvisited cell with a visited cardinal neighbor.
   * 4. If there isn't one, we're done.
   * 5. Break the wall between the new cell and its visited neighbor.
   * 6. Goto 2.
   */
  uint8_t col=rand()%COLC;
  uint8_t row=rand()%ROWC;
  while (1) {
    drunk_walk(col,row,visited);
    if (!bar_hop(&col,&row,visited)) break;
  }
  
  // It might make better mazes if we now move mouse and cheese until they reach a dead-end.
  // That's more complicated than it sounds.
  // Ignoring it for now.
}

static void draw_right_wall(uint8_t *dst) {
  dst+=7;
  int8_t i=8; 
  for (;i-->0;dst+=96) *dst=0x24;
}

static void draw_down_wall(uint8_t *dst) {
  dst+=96*7;
  int8_t i=8;
  for (;i-->0;dst++) *dst=0x24;
}

static void draw_bottom_right_corner(uint8_t *dst) {
  dst[96*7+7]=0x24;
}

static void draw_maze() {
  memset(image_bgbits.v,0x49,96*64);
  const uint8_t *src=maze;
  uint8_t row=0; for (;row<ROWC;row++) {
    uint8_t col=0; for (;col<COLC;col++,src++) {
      if (!*src) {
        // A quirk that the meeting of a rightward and downward wall would drop its corner pixel.
        if (
          (col<COLC-1)&&(row<ROWC-1)&&
          (src[1]&WALL_DOWN)&&
          (src[COLC]&WALL_RIGHT)
        ) {
          draw_bottom_right_corner(image_bgbits.v+(row<<3)*96+(col<<3));
        }
        continue;
      }
      if ((*src)&WALL_RIGHT) {
        draw_right_wall(image_bgbits.v+(row<<3)*96+(col<<3));
      }
      if ((*src)&WALL_DOWN) {
        draw_down_wall(image_bgbits.v+(row<<3)*96+(col<<3));
      }
    }
  }
}

static void new_maze() {
  generate_maze();
  draw_maze();
  mousex=mousecol<<3;
  mousey=mouserow<<3;
  mousedx=0;
  mousedy=0;
}

static uint8_t check_cheese() {
  if ((mousecol==cheesecol)&&(mouserow==cheeserow)) {
    new_maze();
    return 1;
  }
  return 0;
}

static void update_mouse(uint8_t input) {

  // Animate motion.
  if (mousedx) {
    mousex+=mousedx;
    if (!(mousex&7)) {
      mousedx=0;
      if (check_cheese()) return;
    }
  }
  if (mousedy) {
    mousey+=mousedy;
    if (!(mousey&7)) {
      mousedy=0;
      if (check_cheese()) return;
    }
  }
  
  // Consider new motion.
  if (!mousedx&&!mousedy) {
    switch (input&(TINYC_BUTTON_LEFT|TINYC_BUTTON_RIGHT)) {
      case TINYC_BUTTON_LEFT: if ((mousecol>0)&&!(maze[mouserow*COLC+mousecol-1]&WALL_RIGHT)) {
          mousecol--;
          mousedx=-1;
        } break;
      case TINYC_BUTTON_RIGHT: if ((mousecol<COLC-1)&&!(maze[mouserow*COLC+mousecol]&WALL_RIGHT)) {
          mousecol++;
          mousedx=1;
        } break;
    }
  }
  if (!mousedx&&!mousedy) {
    switch (input&(TINYC_BUTTON_UP|TINYC_BUTTON_DOWN)) {
      case TINYC_BUTTON_UP: if ((mouserow>0)&&!(maze[(mouserow-1)*COLC+mousecol]&WALL_DOWN)) {
          mouserow--;
          mousedy=-1;
        } break;
      case TINYC_BUTTON_DOWN: if ((mouserow<ROWC-1)&&!(maze[mouserow*COLC+mousecol]&WALL_DOWN)) {
          mouserow++;
          mousedy=1;
        } break;
    }
  }
}

/* Main loop.
 ******************************************************************/

static double next_time=0.0;
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
  }

  uint8_t input_state=tinyc_read_input();
  update_mouse(input_state);

  memcpy(image_fb.v,image_bgbits.v,96*64);
  softarcade_blit_unchecked(&image_fb,mousex,mousey,&image_mouse);
  softarcade_blit_unchecked(&image_fb,cheesecol<<3,cheeserow<<3,&image_cheese);

  tinyc_send_framebuffer(image_fb.v);
}

/* Initialize.
 */

void setup() {

  #if SOFTARCADE_AUDIO_ENABLE
  softarcade_synth_init(&synth,22050,get_wave,get_pcm,0);
  #endif

  tinyc_init();

  image_mouse.colorkey=0x1c;
  image_cheese.colorkey=0x1c;
  
  new_maze();
  
  tinyc_init_usb_log();
}
