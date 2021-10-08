#include "tinyc.h"
#include "linux-glx/evdev/evdev.h"
#include "linux-glx/os/poller.h"
#include "linux-glx/glx/glx.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* Globals.
 */
 
static unsigned long starttime=0;
static unsigned long framec=0;
static struct poller *poller=0;
static struct evdev *evdev=0;
static uint8_t input=0;
static struct rb_video_glx *glx=0;
static int quit_requested=0;

/* Cleanup and report status -- not part of tinyc, specific to linux.
 */
 
void tinyc_quit() {
  if (framec>0) {
    unsigned long endtime=millis();
    double elapsed=(endtime-starttime)/1000.0f;
    fprintf(stderr,
      "%ld frames in %.03f s, average video rate %.03f Hz\n",
      framec,elapsed,framec/elapsed
    );
  }
  
  evdev_del(evdev);
  evdev=0;
  
  poller_del(poller);
  poller=0;
  
  glx_del(glx);
  glx=0;
}

/* evdev callbacks.
 */
 
static int tinyc_cb_evdev_connect(struct evdev *evdev,struct evdev_device *device) {
  fprintf(stderr,"%s %p\n",__func__,device);
  return 0;
}
 
static int tinyc_cb_evdev_disconnect(struct evdev *evdev,struct evdev_device *device) {
  fprintf(stderr,"%s %p\n",__func__,device);
  // Don't overthink it... Drop all input when any device disconnects, whatever.
  input=0;
  return 0;
}
 
static int tinyc_cb_evdev_event(struct evdev *evdev,struct evdev_device *device,int btnid,int value) {
  //fprintf(stderr,"%s %p.%d=%d\n",__func__,device,btnid,value);
  // Hard-coding config for my Xbox 360 joystick. TODO generalize.
  switch (btnid) {
  
    // dpad
    case 0x30010: if (value<0) {
        input=(input&~(TINYC_BUTTON_LEFT|TINYC_BUTTON_RIGHT))|TINYC_BUTTON_LEFT;
      } else if (value>0) {
        input=(input&~(TINYC_BUTTON_LEFT|TINYC_BUTTON_RIGHT))|TINYC_BUTTON_RIGHT;
      } else {
        input=(input&~(TINYC_BUTTON_LEFT|TINYC_BUTTON_RIGHT));
      } break;
    case 0x30011: if (value<0) {
        input=(input&~(TINYC_BUTTON_UP|TINYC_BUTTON_DOWN))|TINYC_BUTTON_UP;
      } else if (value>0) {
        input=(input&~(TINYC_BUTTON_UP|TINYC_BUTTON_DOWN))|TINYC_BUTTON_DOWN;
      } else {
        input=(input&~(TINYC_BUTTON_UP|TINYC_BUTTON_DOWN));
      } break;
      
    // buttons
    case 0x10130: if (value) input|=TINYC_BUTTON_A; else input&=~TINYC_BUTTON_A; break;
    case 0x10133: if (value) input|=TINYC_BUTTON_B; else input&=~TINYC_BUTTON_B; break;
      
  }
  return 0;
}

/* Init.
 */
 
void tinyc_init() {
  
  if (!(poller=poller_new())) {
    fprintf(stderr,"Failed to create poller\n");
    return;
  }
  
  struct evdev_delegate delegate={
    .cb_connect=tinyc_cb_evdev_connect,
    .cb_disconnect=tinyc_cb_evdev_disconnect,
    .cb_event=tinyc_cb_evdev_event,
  };
  if (!(evdev=evdev_new(0,poller,&delegate))) {
    fprintf(stderr,"Failed to initialize evdev\n");
    return;
  }
  
  if (!(glx=glx_new())) {
    fprintf(stderr,"Failed to initialize GLX\n");
    return;
  }
  
  starttime=millis();
}

/* Send 96x64 framebuffer.
 */

void tinyc_send_framebuffer(const void *fb) {
  framec++;
  if (glx) {
    glx_swap(glx,fb);
  }
  
  #if 0
  // Just for shits and giggles -- can we sensibly dump video to the terminal?
  // ...basically, yes.
  // Each "pixel" will be 11..13 bytes: ESC [48;5;NNNm SPC SPC
  // Plus a 5-byte terminator at each row: ESC [0m LF
  // Plus a clear at the start, 4 bytes: ESC [3J
  char text[4+(13*96+5)*64];
  int textc=0;
  const uint8_t *src=fb;
  char *dst=text;
  memcpy(dst,"\x1b[3J",4); dst+=4; textc+=4;
  int yi=64;
  for (;yi-->0;) {
    int xi=96;
    for (;xi-->0;src++) {
    
      // First normalize to 8-bit channels.
      uint8_t b=(*src)&0xe0; b|=b>>3; b|=b>>6;
      uint8_t g=(*src)&0x1c; g|=g<<3; g|=g>>6;
      uint8_t r=(*src)&0x03; r|=r<<2; r|=r<<4;
      
      // Grays are 232..255 = black..white
      int color;
      if ((b==g)&&(g==r)) {
        color=232+(b*24)/255;
        if (color<232) color=232;
        else if (color>255) color=255;
      // Ignore the first 16. Everything else is a 6x6x6 cube.
      } else {
        r=(r*6)>>8; if (r>5) r=5;
        g=(g*6)>>8; if (g>5) g=5;
        b=(b*6)>>8; if (b>5) b=5;
        color=16+r*36+g*6+b;
      }
    
      int err=snprintf(dst,sizeof(text)-textc,"\x1b[48;5;%dm  ",color);
      dst+=err;
      textc+=err;
    }
    memcpy(dst,"\x1b[0m\n",5); dst+=5; textc+=5;
  }
  write(STDOUT_FILENO,text,textc);
  #endif
}

/* Collect input.
 */

uint8_t tinyc_read_input() {
  if (poller) {
    poller_update(poller,0);
  }
  if (glx) {
    if (glx_update(glx)<=0) quit_requested=1;
  }
  return input;
}

// linux only
int tinyc_quit_requested() {
  if (quit_requested) {
    quit_requested=0;
    return 1;
  }
  return 0;
}
