#ifndef RB_GLX_INTERNAL_H
#define RB_GLX_INTERNAL_H

#include "softarcade.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#define GL_GLEXT_PROTOTYPES 1
#define GLX_GLXEXT_PROTOTYPES 1
#include <GL/glx.h>

#define KeyRepeat (LASTEvent+2)
#define RB_GLX_KEY_REPEAT_INTERVAL 10

#define RB_FB_W 96
#define RB_FB_H 64

struct rb_video {
  struct {
    int fullscreen;
  } delegate;
  int winw,winh;
  int fullscreen;
};

struct rb_video_glx {
  struct rb_video hdr;
  
  Display *dpy;
  int screen;
  Window win;
  GLXContext ctx;
  int glx_version_minor,glx_version_major;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  
  int cursor_visible;
  int screensaver_inhibited;
  int focus;
  int quit_requested;
  uint8_t input;
  
  GLuint texid;
  
  int dstx,dsty,dstw,dsth;
  int dstdirty;
  
  uint8_t fb[96*64*3];
};

#define VIDEO ((struct rb_video_glx*)video)

int rb_glx_show_cursor(struct rb_video *video,int show);
int _rb_glx_update(struct rb_video *video);

int rb_glx_codepoint_from_keysym(int keysym);
int rb_glx_usb_usage_from_keysym(int keysym);

#endif
