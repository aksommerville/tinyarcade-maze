#include "evdev_internal.h"

/* Initial handshake.
 */
 
int evdev_device_hello(struct evdev_device *device) {

  /* EVIOCGVERSION to confirm it's an evdev device, and possibly for version assertions.
   */
  if (ioctl(device->fd,EVIOCGVERSION,&device->version)<0) {
    return evdev_error("%s:EVIOCGVERSION: %m",device->path);
  }
  
  // EVIOCGID optional.
  if (ioctl(device->fd,EVIOCGID,&device->id)<0) {
    evdev_error("%s:EVIOCGID: %m",device->path);
  }
  
  // EVIOCGNAME optional, and we sanitize aggressively.
  device->namec=ioctl(device->fd,EVIOCGNAME(sizeof(device->name)),device->name);
  if (device->namec<0) {
    evdev_error("%s:EVIOCGNAME: %m",device->path);
    device->namec=0;
  } else if (device->namec>=sizeof(device->name)) {
    evdev_error("%s: Name too long",device->path);
    device->namec=0;
  }
  while (device->namec&&((unsigned char)device->name[device->namec-1]<=0x20)) device->namec--;
  int spcc=0;
  while ((spcc<device->namec)&&((unsigned char)device->name[spcc]<=0x20)) spcc++;
  if (spcc) {
    device->namec-=spcc;
    memmove(device->name,device->name+spcc,device->namec);
  }
  device->name[device->namec]=0;
  int i=device->namec;
  while (i-->0) {
    if ((device->name[i]<0x20)||(device->name[i]>0x7e)) device->name[i]='?';
  }
  
  // EVIOCGRAB optional.
  if (ioctl(device->fd,EVIOCGRAB,1)<0) {
    evdev_error("%s:EVIOCGRAB: %m",device->path);
  }

  return 0;
}

/* Release grab.
 */
 
void evdev_device_ungrab(struct evdev_device *device) {
  ioctl(device->fd,EVIOCGRAB,0);
}

/* Receive input.
 */
 
int evdev_device_receive_input(struct evdev_device *device,const void *src,int srcc) {
  if (!device||(srcc<0)||(srcc&&!src)) return -1;
  int eventc=srcc/sizeof(struct input_event);
  const struct input_event *event=src;
  for (;eventc-->0;event++) {
  
    switch (event->type) {
      case EV_SYN: continue;
      case EV_MSC: continue;
    }
  
    int err=device->cb(device,(event->type<<16)|event->code,event->value);
    if (err<0) return -1;
  }
  return 0;
}

/* List buttons, for EV_ABS only.
 */
 
static int evdev_list_ABS(
  struct evdev_device *device,
  int (*cb)(struct evdev_device *device,struct evdev_button *button,void *userdata),
  void *userdata
) {
  uint8_t v[(ABS_CNT+7)>>3];
  int bytec=ioctl(device->fd,EVIOCGBIT(EV_ABS,sizeof(v)),v);
  if (bytec<0) return 0; // ignore errors
  if (bytec>sizeof(v)) bytec=sizeof(v);
  int major=0; for (;major<bytec;major++) {
    uint8_t bits=v[major];
    if (!bits) continue;
    uint8_t mask=1; //TODO Is this the same on big-endian systems?
    int minor=0; for (;minor<8;minor++,mask<<=1) {
      if (!(bits&mask)) continue;
      int code=(major<<3)|minor;
      
      struct input_absinfo absinfo={0};
      ioctl(device->fd,EVIOCGABS(code),&absinfo);
      
      struct evdev_button button={
        .btnid=(EV_ABS<<16)|code,
        .lo=absinfo.minimum,
        .hi=absinfo.maximum,
        .value=absinfo.value,
      };
      int err=cb(device,&button,userdata);
      if (err) return err;
    }
  }
  return 0;
}

/* List buttons, for most event types.
 */
 
static int evdev_list_bit(
  struct evdev_device *device,
  int (*cb)(struct evdev_device *device,struct evdev_button *button,void *userdata),
  void *userdata,
  int type,int c,int lo,int hi
) {
  // 256 bytes is plenty for EV_KEY, which is by far the largest namespace.
  uint8_t v[256];
  c=(c+7)>>3;
  if (c>sizeof(v)) c=sizeof(v);
  int err=ioctl(device->fd,EVIOCGBIT(type,c),v);
  if (err<0) return 0; // ignore errors
  if (c>err) c=err;
  int major=0; for (;major<c;major++) {
    uint8_t bits=v[major];
    if (!bits) continue;
    uint8_t mask=1; //TODO Is this the same on big-endian systems?
    int minor=0; for (;minor<8;minor++,mask<<=1) {
      if (!(bits&mask)) continue;
      int code=(major<<3)|minor;
      struct evdev_button button={
        .btnid=(type<<16)|code,
        .lo=lo,
        .hi=hi,
        .value=0,
      };
      if (err=cb(device,&button,userdata)) return err;
    }
  }
  return 0;
}

/* List buttons.
 */
 
int evdev_device_list_buttons(
  struct evdev_device *device,
  int (*cb)(struct evdev_device *device,struct evdev_button *button,void *userdata),
  void *userdata
) {
  if (!device||(device->fd<0)||!cb) return -1;
  int err;
  if (err=evdev_list_ABS(device,cb,userdata)) return err;
  if (err=evdev_list_bit(device,cb,userdata,EV_KEY,KEY_CNT,0,2)) return err;
  if (err=evdev_list_bit(device,cb,userdata,EV_REL,REL_CNT,-32768,32767)) return err;
  if (err=evdev_list_bit(device,cb,userdata,EV_SW,SW_CNT,0,1)) return err;
  return 0;
}
