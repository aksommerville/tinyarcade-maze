#include "rb_glx_internal.h"
#include "tinyc.h"
#include <X11/keysym.h>

/* Key press, release, or repeat.
 */
 
static int rb_glx_evt_key(struct rb_video *video,XKeyEvent *evt,int value) {
  KeySym keysym=XkbKeycodeToKeysym(VIDEO->dpy,evt->keycode,0,0);
  uint8_t btnid=0;
  switch (keysym) {
    case XK_Down: btnid=TINYC_BUTTON_DOWN; break;
    case XK_Up: btnid=TINYC_BUTTON_UP; break;
    case XK_Left: btnid=TINYC_BUTTON_LEFT; break;
    case XK_Right: btnid=TINYC_BUTTON_RIGHT; break;
    case XK_z: btnid=TINYC_BUTTON_A; break;
    case XK_x: btnid=TINYC_BUTTON_B; break;
  }
  if (btnid) {
    if (value) VIDEO->input|=btnid;
    else VIDEO->input&=~btnid;
  }
  return 0;
}

/* Client message.
 */
 
static int rb_glx_evt_client(struct rb_video *video,XClientMessageEvent *evt) {
  if (evt->message_type==VIDEO->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==VIDEO->atom_WM_DELETE_WINDOW) {
        VIDEO->quit_requested=1;
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int rb_glx_evt_configure(struct rb_video *video,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=video->winw)||(nh!=video->winh)) {
    video->winw=nw;
    video->winh=nh;
    VIDEO->dstdirty=1;
  }
  return 0;
}

/* Focus.
 */
 
static int rb_glx_evt_focus(struct rb_video *video,XFocusInEvent *evt,int value) {
  if (value==VIDEO->focus) return 0;
  VIDEO->focus=value;
  //if (video->delegate.cb_focus) {
  //  return video->delegate.cb_focus(video,value);
  //}
  return 0;
}

/* Dispatch single event.
 */
 
static int rb_glx_receive_event(struct rb_video *video,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return rb_glx_evt_key(video,&evt->xkey,1);
    case KeyRelease: return rb_glx_evt_key(video,&evt->xkey,0);
    case KeyRepeat: return rb_glx_evt_key(video,&evt->xkey,2);
    
    case ClientMessage: return rb_glx_evt_client(video,&evt->xclient);
    
    case ConfigureNotify: return rb_glx_evt_configure(video,&evt->xconfigure);
    
    case FocusIn: return rb_glx_evt_focus(video,&evt->xfocus,1);
    case FocusOut: return rb_glx_evt_focus(video,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int glx_update(struct rb_video_glx *glx) {
  struct rb_video *video=&glx->hdr;
  
  if (VIDEO->quit_requested) {
    VIDEO->quit_requested=0;
    return 0;
  }
  
  int evtc=XEventsQueued(VIDEO->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(VIDEO->dpy,&evt);
    
    /* If we detect an auto-repeated key, drop one of the events, and turn the other into KeyRepeat.
     * This is a hack to force single events for key repeat.
     */
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(VIDEO->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-RB_GLX_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (rb_glx_receive_event(video,&evt)<0) return -1;
      } else {
        if (rb_glx_receive_event(video,&evt)<0) return -1;
        if (rb_glx_receive_event(video,&next)<0) return -1;
      }
    } else {
      if (rb_glx_receive_event(video,&evt)<0) return -1;
    }
  }
  return 1;
}
