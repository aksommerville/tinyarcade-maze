#ifndef GLX_H
#define GLX_H

struct rb_video_glx;
struct softarcade_image;

struct rb_video_glx *glx_new();
void glx_del(struct rb_video_glx *glx);

int glx_swap(struct rb_video_glx *glx,const void *fb_96_64_8);

// 0 if user requested to terminate, 1 to carry on
int glx_update(struct rb_video_glx *glx);

uint8_t glx_get_input(const struct rb_video_glx *glx);

#endif
