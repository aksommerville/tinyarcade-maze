#ifndef EVDEV_INTERNAL_H
#define EVDEV_INTERNAL_H

#define USE_inotify 1

#include "evdev.h"
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct evdev {
  int refc;
  char *path;
  int pathc;
  struct evdev_device **devv;
  int devc,deva;
  struct poller *poller;
  struct inotify *ino;
  struct evdev_delegate delegate;
};

struct evdev_device {
  int refc;
  int fd;
  int ownfd;
  int (*cb)(struct evdev_device *device,int btnid,int value);
  void *userdata;
  int evid; // eg 9 for /dev/input/event9. <0 if unknown
  int version; // EVIOCGVERSION, ie the driver version
  char *path;
  struct input_id id;
  char name[64];
  int namec;
};

#define evdev_error(fmt,...) ({ \
  /**/ fprintf(stderr,"evdev:"fmt"\n",##__VA_ARGS__); /**/ \
  (-1); \
})

int evdev_device_hello(struct evdev_device *device);
void evdev_device_ungrab(struct evdev_device *device);

#endif
