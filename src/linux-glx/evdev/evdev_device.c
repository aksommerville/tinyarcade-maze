#include "evdev_internal.h"

/* Delete.
 */
 
void evdev_device_del(struct evdev_device *device) {
  if (!device) return;
  if (device->refc-->1) return;
  if (device->ownfd&&(device->fd>=0)) {
    evdev_device_ungrab(device);
    close(device->fd);
  }
  if (device->path) free(device->path);
  free(device);
}

/* Retain.
 */
 
int evdev_device_ref(struct evdev_device *device) {
  if (!device) return -1;
  if (device->refc<1) return -1;
  if (device->refc==INT_MAX) return -1;
  device->refc++;
  return 0;
}

/* New.
 */

struct evdev_device *evdev_device_new(
  const char *path,
  int (*cb)(struct evdev_device *device,int btnid,int value),
  void *userdata
) {
  if (!path||!path[0]||!cb) return 0;
  
  struct evdev_device *device=calloc(1,sizeof(struct evdev_device));
  if (!device) return 0;
  device->refc=1;
  device->fd=-1;
  device->ownfd=1;
  device->evid=-1;
  device->cb=cb;
  device->userdata=userdata;
  
  if ((device->fd=open(path,O_RDONLY))<0) {
    evdev_device_del(device);
    // Errors here are perfectly normal, eg we'll never be allowed to read the keyboard.
    //evdev_error("%s:open: %m",path);
    return 0;
  }
  
  if (
    !(device->path=strdup(path))||
    (evdev_device_hello(device)<0)
  ) {
    evdev_device_del(device);
    return 0;
  }
  
  return device;
}

/* Read.
 */

int evdev_device_read(struct evdev_device *device) {
  if (!device||(device->fd<0)) return -1;
  char tmp[1024];
  int tmpc=read(device->fd,tmp,sizeof(tmp));
  if (tmpc<1) return evdev_error("%s:read: %m",device->path);
  return evdev_device_receive_input(device,tmp,tmpc);
}

/* Trivial accessors.
 */
 
void *evdev_device_get_userdata(const struct evdev_device *device) {
  return device->userdata;
}
 
int evdev_device_get_fd(const struct evdev_device *device) {
  return device->fd;
}
 
int evdev_device_get_driver_version(const struct evdev_device *device) {
  return device->version;
}

int evdev_device_get_bustype(const struct evdev_device *device) {
  return device->id.bustype;
}

int evdev_device_get_vendor(const struct evdev_device *device) {
  return device->id.vendor;
}

int evdev_device_get_product(const struct evdev_device *device) {
  return device->id.product;
}

int evdev_device_get_version(const struct evdev_device *device) {
  return device->id.version;
}

const char *evdev_device_get_name(const struct evdev_device *device) {
  return device->name;
}
