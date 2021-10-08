/* evdev.h
 */
 
#ifndef EVDEV_H
#define EVDEV_H

struct evdev_device;
struct evdev;
struct inotify;
struct poller;

/* Collection.
 * This gives you a single object to interact with, probably convenient.
 * It requires our inotify and poller APIs.
 * Technically, all you need is the Device API below (which is self-contained).
 ****************************************************************/
 
struct evdev_delegate {
  void *userdata;
  int (*cb_connect)(struct evdev *evdev,struct evdev_device *device);
  int (*cb_disconnect)(struct evdev *evdev,struct evdev_device *device);
  int (*cb_event)(struct evdev *evdev,struct evdev_device *device,int btnid,int value);
};
 
void evdev_del(struct evdev *evdev);
int evdev_ref(struct evdev *evdev);

/* (path) is "/dev/input" if you don't specify.
 * This creates an inotify object and registers that with the poller.
 * Devices already connected will be reported during this call.
 */
struct evdev *evdev_new(
  const char *path,
  struct poller *poller,
  const struct evdev_delegate *delegate
);

void *evdev_get_userdata(const struct evdev *evdev);

/* Dropped devices stay dropped until something on their underlying file changes.
 * Then it will be reported again, we'll connect it, call you, and you'll probably decide again to drop it.
 * Connecting a device is pretty cheap, so I'm not really worried about this back-and-forth.
 */
int evdev_drop_device(struct evdev *evdev,struct evdev_device *device);

/* Notify evdev that some file in its directory changed.
 * Normally this comes from inotify.
 * If it looks like evdev and we don't have it yet, we add to the collection.
 * Optionally return a WEAK reference to the device, if newly connected.
 */
int evdev_examine_file(
  struct evdev_device **device,
  struct evdev *evdev,
  const char *path,const char *base
);

// "evid" is the number on the file name, eg 2 for /dev/input/event2
struct evdev_device *evdev_get_device_by_evid(const struct evdev *evdev,int evid);
struct evdev_device *evdev_get_device_by_fd(const struct evdev *evdev,int fd);

/* Device.
 ****************************************************************/

void evdev_device_del(struct evdev_device *device);
int evdev_device_ref(struct evdev_device *device);

/* Callback is required and can't be changed.
 * It receives (btnid), which is ((type<<16)|code).
 */
struct evdev_device *evdev_device_new(
  const char *path,
  int (*cb)(struct evdev_device *device,int btnid,int value),
  void *userdata
);

int evdev_device_read(struct evdev_device *device);
int evdev_device_receive_input(struct evdev_device *device,const void *src,int srcc);

/* Trigger your callback for every button this device declares itself to report.
 * The list is not necessarily correct or complete.
 * Return nonzero to stop iteration immediately and return the same.
 * This does perform some I/O.
 */
struct evdev_button {
  int btnid; // What we will report to the event callback. (type<<16)|code
  int lo,hi; // Expected minimum and maximum values.
  int value; // Current value if known, or zero.
};
int evdev_device_list_buttons(
  struct evdev_device *device,
  int (*cb)(struct evdev_device *device,struct evdev_button *button,void *userdata),
  void *userdata
);

/* "driver_version" should be the same for all devices.
 * The other "version" is what's reported via USB per device.
 */
void *evdev_device_get_userdata(const struct evdev_device *device);
int evdev_device_get_fd(const struct evdev_device *device);
int evdev_device_get_driver_version(const struct evdev_device *device);
int evdev_device_get_bustype(const struct evdev_device *device);
int evdev_device_get_vendor(const struct evdev_device *device);
int evdev_device_get_product(const struct evdev_device *device);
int evdev_device_get_version(const struct evdev_device *device);
const char *evdev_device_get_name(const struct evdev_device *device);

#endif
