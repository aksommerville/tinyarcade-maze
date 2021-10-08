#include "evdev_internal.h"

#if USE_inotify

#include "linux-glx/inotify/inotify.h"
#include "linux-glx/os/poller.h"

/* Delete.
 */
 
void evdev_del(struct evdev *evdev) {
  if (!evdev) return;
  if (evdev->refc-->1) return;
  if (evdev->devv) {
    while (evdev->devc-->0) {
      poller_remove_file(evdev->poller,evdev->devv[evdev->devc]->fd);
      evdev_device_del(evdev->devv[evdev->devc]);
    }
    free(evdev->devv);
  }
  if (evdev->ino) {
    poller_remove_file(evdev->poller,inotify_get_fd(evdev->ino));
    inotify_del(evdev->ino);
  }
  poller_del(evdev->poller);
  if (evdev->path) free(evdev->path);
  free(evdev);
}

/* Retain.
 */
 
int evdev_ref(struct evdev *evdev) {
  if (!evdev) return -1;
  if (evdev->refc<1) return -1;
  if (evdev->refc==INT_MAX) return -1;
  evdev->refc++;
  return 0;
}

/* Event callback from device.
 */
 
static int evdev_cb_event(struct evdev_device *device,int btnid,int value) {
  struct evdev *evdev=evdev_device_get_userdata(device);
  if (!evdev) return -1;
  if (evdev->delegate.cb_event) {
    int err=evdev->delegate.cb_event(evdev,device,btnid,value);
    if (err<0) return err;
  }
  return 0;
}

/* Callbacks from poller.
 */
 
static int evdev_cb_device_error(int fd,void *userdata) {
  struct evdev *evdev=userdata;
  if (!evdev) return -1;
  struct evdev_device *device=evdev_get_device_by_fd(evdev,fd);
  if (device) {
    evdev_drop_device(evdev,device);
  }
  return 0;
}

static int evdev_cb_device_read(int fd,void *userdata,const void *src,int srcc) {
  struct evdev *evdev=userdata;
  if (!evdev) return -1;
  struct evdev_device *device=evdev_get_device_by_fd(evdev,fd);
  if (!device) return 0;
  if (evdev_device_receive_input(device,src,srcc)<0) {
    evdev_drop_device(evdev,device);
  }
  return 0;
}

/* Callback from inotify.
 */
 
static int evdev_cb_inotify(const char *path,const char *base,int wd,void *userdata) {
  struct evdev *evdev=userdata;
  if (!evdev) return -1;
  struct evdev_device *device=0;
  if (evdev_examine_file(&device,evdev,path,base)<0) return -1;
  if (!device) return 0;
  return 0;
}

/* Inotify readable.
 */
 
static int evdev_cb_inotify_readable(int fd,void *userdata) {
  return inotify_read(userdata);
}

/* Create and attach inotify.
 */
 
static int evdev_init_inotify(struct evdev *evdev) {
  if (!(evdev->ino=inotify_new(evdev_cb_inotify,evdev))) return -1;
  if (inotify_watch(evdev->ino,evdev->path)<0) return -1;
  if (inotify_scan(evdev->ino)<0) return -1;
  
  struct poller_file file={
    .fd=inotify_get_fd(evdev->ino),
    .userdata=evdev->ino,
    .cb_readable=evdev_cb_inotify_readable,
  };
  if (poller_add_file(evdev->poller,&file)<0) return -1;
  
  return 0;
}

/* New.
 */

struct evdev *evdev_new(
  const char *path,
  struct poller *poller,
  const struct evdev_delegate *delegate
) {
  if (!path||!path[0]) path="/dev/input";
  if (!poller) return 0;
  
  struct evdev *evdev=calloc(1,sizeof(struct evdev));
  if (!evdev) return 0;
  
  evdev->refc=1;
  if (delegate) evdev->delegate=*delegate;
  
  int pathc=0; while (path[pathc]) pathc++;
  if (!(evdev->path=malloc(pathc+1))) {
    evdev_del(evdev);
    return 0;
  }
  memcpy(evdev->path,path,pathc+1);
  evdev->pathc=pathc;
  
  if (poller_ref(poller)<0) {
    evdev_del(evdev);
    return 0;
  }
  evdev->poller=poller;
  
  if (evdev_init_inotify(evdev)<0) {
    evdev_del(evdev);
    return 0;
  }
  
  return evdev;
}

/* Trivial accessors.
 */

void *evdev_get_userdata(const struct evdev *evdev) {
  return evdev->delegate.userdata;
}

/* Drop device.
 */

int evdev_drop_device(struct evdev *evdev,struct evdev_device *device) {
  if (!device) return 0;
  int i=evdev->devc;
  while (i-->0) {
    if (evdev->devv[i]==device) {
      poller_remove_file(evdev->poller,device->fd);
      evdev->devc--;
      memmove(evdev->devv+i,evdev->devv+i+1,sizeof(void*)*(evdev->devc-i));
      int err=0;
      if (evdev->delegate.cb_disconnect) {
        err=evdev->delegate.cb_disconnect(evdev,device);
      }
      evdev_device_del(device);
      return err;
    }
  }
  return -1;
}

/* Examine file and maybe open a device.
 */

int evdev_examine_file(
  struct evdev_device **devicertn,
  struct evdev *evdev,
  const char *path,const char *base
) {
  if (!evdev||!path) return 0;
  if (!base) {
    const char *v=path;
    for (;*v;v++) if (*v=='/') base=v+1;
  }

  // We're only interested if it's under our path.
  if (memcmp(path,evdev->path,evdev->pathc)) return 0;
  if (path[evdev->pathc]!='/') return 0;
  
  // Basename must be "eventN" where N is a decimal integer.
  // evid must be unused.
  if (memcmp(base,"event",5)) return 0;
  int evid=0,p=5;
  for (;base[p];p++) {
    int digit=base[p]-'0';
    if ((digit<0)||(digit>9)) return 0;
    evid*=10;
    evid+=digit;
  }
  if (evdev_get_device_by_evid(evdev,evid)) return 0;
  
  // Make room for it.
  if (evdev->devc>=evdev->deva) {
    int na=evdev->deva+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(evdev->devv,sizeof(void*)*na);
    if (!nv) return -1;
    evdev->devv=nv;
    evdev->deva=na;
  }
  
  // Attempt to connect.
  struct evdev_device *device=evdev_device_new(path,evdev_cb_event,evdev);
  if (!device) return 0;
  device->evid=evid;
  evdev->devv[evdev->devc++]=device;
  
  // Notify delegate.
  if (evdev->delegate.cb_connect) {
    int err=evdev->delegate.cb_connect(evdev,device);
    if (err<0) return err;
    // Delegate is allowed to drop the device. If it did, get out fast.
    if (!evdev_get_device_by_evid(evdev,evid)) return 0;
  }
  
  // Register device with the poller.
  struct poller_file file={
    .fd=device->fd,
    .userdata=evdev,
    .cb_error=evdev_cb_device_error,
    .cb_read=evdev_cb_device_read,
  };
  if (poller_add_file(evdev->poller,&file)<0) {
    evdev_drop_device(evdev,device);
    return -1;
  }
  
  if (devicertn) *devicertn=device;
  return 1;
}

/* Get file by evid.
 */
 
struct evdev_device *evdev_get_device_by_evid(const struct evdev *evdev,int evid) {
  if (!evdev) return 0;
  int i=evdev->devc;
  while (i-->0) {
    struct evdev_device *device=evdev->devv[i];
    if (device->evid==evid) return device;
  }
  return 0;
}

struct evdev_device *evdev_get_device_by_fd(const struct evdev *evdev,int fd) {
  if (!evdev) return 0;
  int i=evdev->devc;
  while (i-->0) {
    struct evdev_device *device=evdev->devv[i];
    if (device->fd==fd) return device;
  }
  return 0;
}

/* Stubs if missing required units.
 */

#else

void evdev_del(struct evdev *evdev) {}
int evdev_ref(struct evdev *evdev) { return -1; }
void *evdev_get_userdata(const struct evdev *evdev) { return 0; }
int evdev_drop_device(struct evdev *evdev,struct evdev_device *device) { return -1; }
struct evdev_device *evdev_get_device_by_evid(const struct evdev *evdev,int evid) { return 0; }
struct evdev_device *evdev_get_device_by_fd(const struct evdev *evdev,int fd) { return 0; }

struct evdev *evdev_new(
  const char *path,
  struct poller *poller,
  const struct evdev_delegate *delegate
) { return 0; }

int evdev_examine_file(
  struct evdev_device **device,
  struct evdev *evdev,
  const char *path,const char *base
) { return -1; }

#endif
