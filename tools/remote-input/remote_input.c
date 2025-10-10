/*
 * Copyright 2025 NXP
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your pconfigion) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <sys/types.h>
#include "unistd.h"
#include "stdlib.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdarg.h>
#include <glib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libinput.h>
#include <libudev.h>
#include "remote_input.h"

#define INPUT_DEVICE_PATH	"/dev/input/"
#define HID_PKT_QUEUE		20

typedef struct
{
  int fd;
  RINPUT_DEV_TYPE dev_type;
  char *sys_name;
  unsigned int bus_type;
} RINPUT_DEVICE;

typedef struct
{
  RINPUT_TYPE type;
  GList *dev_list;
  int socket_fd;
  int conn;
  int timeout;
  struct udev *udev;
  struct libinput *libinput;
  int libinput_fd;
  int reserved[4];
} RINPUT;

typedef struct
{
  RINPUT_DEV_TYPE dev_type;
  unsigned int bus_type;
  struct input_event event;
} RINPUT_PKT;

static void (*remote_input_print) (const char *format, ...) = NULL;

void
remote_input_enable_log (int enable)
{
  if (enable) {
    remote_input_print = g_print;
  } else {
    remote_input_print = NULL;
  }
}

static gboolean
remote_input_log_is_enable (void)
{
  return remote_input_print != NULL;
}

void
remote_input_debug (const char *msg, ...)
{
  va_list args;
  gchar *str = NULL;

  if (!remote_input_print) {
    return;
  }

  va_start (args, msg);
  str = g_strdup_vprintf (msg, args);
  va_end (args);

  remote_input_print ("%s", str);
  g_free (str);
}

static RINPUT_RESULT
remote_input_configure_uinput_device (RINPUT_DEVICE * rinput_device,
    RINPUT_DEV_TYPE dev_type, unsigned int bus_type)
{
  struct uinput_setup usetup;
  int version, rc;
  RINPUT_RESULT ret = RINPUT_OK;

  if (!rinput_device) {
    ret = RINPUT_PARAM_ERROR;
    goto fail;
  }

  memset (&usetup, 0, sizeof (usetup));
  switch (dev_type) {
    case RINPUT_DEV_VIRTUAL_MOUSE:{
      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_SYN);
      ioctl (rinput_device->fd, UI_SET_MSCBIT, SYN_REPORT);

      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_KEY);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_LEFT);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_RIGHT);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_MIDDLE);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_SIDE);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_EXTRA);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_FORWARD);
      ioctl (rinput_device->fd, UI_SET_KEYBIT, BTN_BACK);

      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_REL);
      ioctl (rinput_device->fd, UI_SET_RELBIT, REL_X);
      ioctl (rinput_device->fd, UI_SET_RELBIT, REL_Y);
      ioctl (rinput_device->fd, UI_SET_RELBIT, REL_WHEEL);

      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_MSC);
      ioctl (rinput_device->fd, UI_SET_MSCBIT, MSC_SCAN);

      usetup.id.bustype = bus_type;
      strcpy (usetup.name, "virtual mouse device");
      break;
    }
    case RINPUT_DEV_VIRTUAL_KEYBOARD:{
      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_SYN);
      ioctl (rinput_device->fd, UI_SET_MSCBIT, SYN_REPORT);

      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_KEY);
      for (uint value = KEY_RESERVED; value <= KEY_MAX; value++) {
        ioctl (rinput_device->fd, UI_SET_KEYBIT, value);
      }

      ioctl (rinput_device->fd, UI_SET_EVBIT, EV_MSC);
      ioctl (rinput_device->fd, UI_SET_MSCBIT, MSC_SCAN);

      usetup.id.bustype = bus_type;
      strcpy (usetup.name, "virtual keyboard device");
      break;
    }
    default:{
      remote_input_debug ("Unsupported device type\n");
      ret = RINPUT_PARAM_ERROR;
      goto fail;
    }
  }

  rc = ioctl (rinput_device->fd, UI_GET_VERSION, &version);
  if (rc == 0 && version >= 5) {
    /* support UI_DEV_SETUP */
    ret = RINPUT_OK;
  } else {
    remote_input_debug ("Can not support UI_DEV_SETUP\n");
    ret = RINPUT_FAIL;
    goto fail;
  }

  ioctl (rinput_device->fd, UI_DEV_SETUP, &usetup);
  ioctl (rinput_device->fd, UI_DEV_CREATE);

  /*
   * On UI_DEV_CREATE the kernel will create the device node for this
   * device. We are inserting a pause here so that userspace has time
   * to detect, initialize the new device, and can start listening to
   * the event, otherwise it will not notice the event we are about
   * to send.
   */
  sleep (1);
  return RINPUT_OK;

fail:
  return ret;
}

static RINPUT_RESULT
remote_input_open_device (RINPUT * rinput, RINPUT_TYPE rinput_type,
    RINPUT_DEV_TYPE dev_type, const char *sys_name, unsigned int bus_type)
{
  RINPUT_DEVICE *rinput_device = NULL;
  RINPUT_RESULT ret = RINPUT_OK;

  /* If the type is fixed, should not change it when opending new device */
  if (rinput->type != RINPUT_NULL && rinput->type != rinput_type) {
    remote_input_debug ("new rinput type: %d, current rinput type: %d\n",
        rinput_type, rinput->type);
    ret = RINPUT_PARAM_ERROR;
    goto fail;
  }

  rinput_device = (RINPUT_DEVICE *) g_new0 (RINPUT_DEVICE, 1);
  if (!rinput_device) {
    ret = RINPUT_NO_MEMORY;
    goto fail;
  }

  switch (dev_type) {
    case RINPUT_DEV_MOUSE:
    case RINPUT_DEV_KEYBOARD:{
      char *path = g_strdup_printf ("%s%s", INPUT_DEVICE_PATH, sys_name);
      rinput_device->fd = open (path, O_RDONLY);
      g_free (path);

      if (rinput_device->fd < 0) {
        remote_input_debug ("Failed to open device: %s\n", sys_name);
        ret = RINPUT_FAIL;
        goto fail;
      }
      rinput_device->dev_type = dev_type;
      rinput_device->sys_name = g_strdup_printf ("%s", sys_name);
      break;
    }
    case RINPUT_DEV_VIRTUAL_MOUSE:
    case RINPUT_DEV_VIRTUAL_KEYBOARD:{
      rinput_device->fd = open ("/dev/uinput", O_WRONLY | O_NONBLOCK);
      if (rinput_device->fd < 0) {
        remote_input_debug ("Failed to open uinput\n");
        ret = RINPUT_FAIL;
        goto fail;
      }
      rinput_device->dev_type = dev_type;
      rinput_device->sys_name = g_strdup_printf ("%s", sys_name);
      ret =
          remote_input_configure_uinput_device (rinput_device, dev_type,
          bus_type);
      if (ret != RINPUT_OK) {
        goto fail;
      }
      break;
    }
    default:{
      remote_input_debug ("Invalid device type: %d\n", dev_type);
      ret = RINPUT_PARAM_ERROR;
      goto fail;
    }
  }

  rinput_device->bus_type = bus_type;
  rinput->dev_list = g_list_append (rinput->dev_list, rinput_device);
  rinput->type = rinput_type;
  remote_input_debug ("Add to device list, fd: %d\n", rinput_device->fd);
  return ret;

fail:
  g_free (rinput_device);
  return ret;
}

static RINPUT_RESULT
remote_input_close_device (RINPUT * rinput, const char *sys_name)
{
  GList *iter;
  RINPUT_DEVICE *rinput_device;
  RINPUT_RESULT ret = RINPUT_OK;
  gboolean found_device = FALSE;

  for (iter = rinput->dev_list; iter; iter = g_list_next (iter)) {
    rinput_device = iter->data;
    if (strcmp (rinput_device->sys_name, sys_name) == 0) {
      found_device = TRUE;
      break;
    }
  }

  if (!found_device) {
    remote_input_debug ("Device: %s is not in the list!\n", sys_name);
    ret = RINPUT_PARAM_ERROR;
    goto done;
  }

  /* Destory the device if it was a virtual device */
  if (rinput_device->dev_type == RINPUT_DEV_VIRTUAL_MOUSE ||
      rinput_device->dev_type == RINPUT_DEV_VIRTUAL_KEYBOARD) {
    ioctl (rinput_device->fd, UI_DEV_DESTROY);
  }

  if (close (rinput_device->fd) < 0) {
    remote_input_debug ("Fail to close device, fd: %d\n", rinput_device->fd);
    ret = RINPUT_FAIL;
  }

  remote_input_debug ("Remove from device list, fd: %d\n", rinput_device->fd);
  rinput->dev_list = g_list_remove (rinput->dev_list, rinput_device);
  g_free (rinput_device->sys_name);
  g_free (rinput_device);

done:
  return ret;
}

static RINPUT_RESULT
remote_input_device_added (RINPUT * rinput,
    struct libinput_device *device)
{
  unsigned int bus_type;
  RINPUT_RESULT ret = RINPUT_OK;
  const char *sys_name = libinput_device_get_sysname (device);

  bus_type = libinput_device_get_id_bustype (device);
  if (libinput_device_has_capability (device, LIBINPUT_DEVICE_CAP_POINTER)) {
    remote_input_debug ("Mouse added, sys_name: %s, bus_type: %d\n", sys_name,
        bus_type);
    ret =
        remote_input_open_device (rinput, RINPUT_CLIENT, RINPUT_DEV_MOUSE,
        sys_name, bus_type);
    goto done;
  }

  if (libinput_device_has_capability (device, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
    remote_input_debug ("Keyboard added, sys_name: %s, bus_type: %d\n",
        sys_name, bus_type);
    ret =
        remote_input_open_device (rinput, RINPUT_CLIENT, RINPUT_DEV_KEYBOARD,
        sys_name, bus_type);
  }

done:
  return ret;
}

static RINPUT_RESULT
remote_input_device_removed (RINPUT * rinput,
    struct libinput_device *device)
{
  unsigned int bus_type;
  RINPUT_RESULT ret = RINPUT_OK;
  const char *sys_name = libinput_device_get_sysname (device);

  bus_type = libinput_device_get_id_bustype (device);
  if (libinput_device_has_capability (device, LIBINPUT_DEVICE_CAP_POINTER)) {
    remote_input_debug ("Mouse removed, sys_name: %s, bus_type: %d\n", sys_name,
        bus_type);
    ret = remote_input_close_device (rinput, sys_name);
    goto done;
  }

  if (libinput_device_has_capability (device, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
    remote_input_debug ("Keyboard removed, sys_name: %s, bus_type: %d\n",
        sys_name, bus_type);
    ret = remote_input_close_device (rinput, sys_name);
  }

done:
  return ret;
}

static int
remote_input_open_restricted (const char *path, int flags, void *user_data)
{
  int fd = open (path, flags);
  remote_input_debug ("libinput: open fd: %d, path: %s\n", fd, path);
  return fd < 0 ? -errno : fd;
}

static void
remote_input_close_restricted (int fd, void *user_data)
{
  remote_input_debug ("libinput: close fd: %d\n", fd);
  close (fd);
}

const struct libinput_interface remote_input_interface = {
  .open_restricted = remote_input_open_restricted,
  .close_restricted = remote_input_close_restricted,
};

static RINPUT_RESULT
remote_input_process_udev_input_event (RINPUT * rinput)
{
  struct libinput *libinput = rinput->libinput;
  struct libinput_event *event;
  enum libinput_event_type event_type;
  struct libinput_device *device;
  RINPUT_RESULT ret = RINPUT_OK;

  if (libinput_dispatch (rinput->libinput) != 0) {
    remote_input_debug ("libinput: Failed to dispatch libinput\n");
  }

  while ((event = libinput_get_event (libinput)) != NULL) {
    device = libinput_event_get_device (event);
    event_type = libinput_event_get_type (event);

    switch (event_type) {
      case LIBINPUT_EVENT_DEVICE_ADDED:{
        ret = remote_input_device_added (rinput, device);
        break;
      }
      case LIBINPUT_EVENT_DEVICE_REMOVED:{
        ret = remote_input_device_removed (rinput, device);
        break;
      }
      default:{
        //remote_input_debug ("unhandle event type: %d\n", event_type);
        break;
      }
    }

    libinput_event_destroy (event);
  }

  return ret;
}

static RINPUT_RESULT
remote_udev_input_init (RINPUT * rinput)
{
  int fd = -1;
  struct udev *udev = NULL;
  static struct libinput *libinput = NULL;
  RINPUT_RESULT ret = RINPUT_OK;

  udev = udev_new ();
  if (!udev) {
    remote_input_debug ("Failed to initialize udev\n");
    return RINPUT_FAIL;
  }

  libinput =
      libinput_udev_create_context (&remote_input_interface, rinput, udev);
  if (!libinput) {
    remote_input_debug ("Failed to create libinput context\n");
    ret = RINPUT_FAIL;
    goto fail;
  }

  fd = libinput_get_fd(libinput);
  if (fd < 0) {
      remote_input_debug("Failed to get libinput file descriptor\n");
      ret = RINPUT_FAIL;
      goto fail;
  }

  if (remote_input_log_is_enable ()) {
    libinput_log_set_priority (libinput, LIBINPUT_LOG_PRIORITY_INFO);
  }

  if (libinput_udev_assign_seat (libinput, "seat0") != 0) {
    remote_input_debug ("Failed to assign seat\n");
    ret = RINPUT_FAIL;
    goto fail;
  }

  rinput->udev = udev;
  rinput->libinput = libinput;
  rinput->libinput_fd = fd;
  ret = remote_input_process_udev_input_event (rinput);
  return ret;

fail:
  if (libinput)
    libinput_unref (libinput);
  if (udev)
    udev_unref (udev);
  return ret;
}

static RINPUT_RESULT
remote_input_send (RINPUT * rinput)
{
  int max_fd = -1;
  fd_set read_fds;
  struct timeval tv;
  RINPUT_DEVICE *rinput_device;
  GList *iter;
  unsigned bus_type = 0;
  RINPUT_PKT pkt;

  if (rinput->type != RINPUT_CLIENT
      || rinput->dev_list == NULL || rinput->socket_fd < 0) {
    remote_input_debug ("Invalid param\n");
    return RINPUT_PARAM_ERROR;
  }

  FD_ZERO (&read_fds);
  tv.tv_sec = 0;
  tv.tv_usec = rinput->timeout;
  for (iter = rinput->dev_list; iter; iter = g_list_next (iter)) {
    rinput_device = iter->data;
    bus_type = rinput_device->bus_type;

    if (bus_type != BUS_USB && bus_type != BUS_BLUETOOTH) {
      continue;
    }

    FD_SET (rinput_device->fd, &read_fds);
    if (rinput_device->fd > max_fd) {
      max_fd = rinput_device->fd;
    }
  }

  if (rinput->libinput_fd >= 0) {
    FD_SET(rinput->libinput_fd, &read_fds);

    if (rinput->libinput_fd > max_fd) {
      max_fd = rinput->libinput_fd;
    }
  }

  if (select (max_fd + 1, &read_fds, NULL, NULL, &tv) <= 0) {
    goto done;
  }

  if (rinput->libinput_fd >= 0 && FD_ISSET(rinput->libinput_fd, &read_fds)) {
    remote_input_process_udev_input_event (rinput);
  }

  for (iter = rinput->dev_list; iter; iter = g_list_next (iter)) {
    rinput_device = iter->data;
    bus_type = rinput_device->bus_type;

    if (bus_type != BUS_USB && bus_type != BUS_BLUETOOTH) {
      continue;
    }

    if (!FD_ISSET (rinput_device->fd, &read_fds)) {
      continue;
    }

    memset (&pkt.event, 0, sizeof (struct input_event));
    if (read (rinput_device->fd, &pkt.event, sizeof (struct input_event))
        != sizeof (struct input_event)) {
      if (errno == ENODEV || errno == EBADF) {
        remote_input_debug ("Disconnected or invalid, input device fd: %d\n",
            rinput_device->fd);
      }
      continue;
    }

    pkt.dev_type = rinput_device->dev_type;
    pkt.bus_type = rinput_device->bus_type;
    send (rinput->socket_fd, &pkt, sizeof (pkt), 0);
    switch (pkt.event.type) {
      case EV_KEY:{
        remote_input_debug ("key %d %s\n", pkt.event.code,
            pkt.event.value ? "Press" : "Released");
        break;
      }
      case EV_REL:{
        remote_input_debug ("motion axis: %d, value: %d\n", pkt.event.code,
            pkt.event.value);
        break;
      }
      default:{
        remote_input_debug
            ("fd: %d, type: %d, code: %d, value: %d\n",
            rinput_device->fd, pkt.event.type, pkt.event.code, pkt.event.value);
        break;
      }
    }
  }

done:
  return RINPUT_OK;
}

static RINPUT_RESULT
remote_input_recv (RINPUT * rinput)
{
  int len = 0;
  RINPUT_DEVICE *rinput_device;
  RINPUT_PKT pkt;
  GList *iter;
  gboolean found_device = FALSE;

  if (!rinput || rinput->dev_list == NULL || rinput->conn < 0) {
    return RINPUT_PARAM_ERROR;
  }

  if (rinput->type != RINPUT_SERVER) {
    remote_input_debug ("Invalid rinput type\n");
    return RINPUT_FAIL;
  }

  len = recv (rinput->conn, &pkt, sizeof (RINPUT_PKT), 0);
  if (len != sizeof (RINPUT_PKT)) {
    remote_input_debug ("Failed to receive data on the socket, ret: %d\n", len);
    return RINPUT_READ_ERROR;
  }

  switch (pkt.event.type) {
    case EV_KEY:{
      remote_input_debug ("dev type: %d, key %d %s\n", pkt.dev_type,
          pkt.event.code, pkt.event.value ? "Press" : "Released");
      break;
    }
    case EV_REL:{
      remote_input_debug ("dev type: %d, motion axis: %d, value: %d\n",
          pkt.dev_type, pkt.event.code, pkt.event.value);
      break;
    }
    default:{
      remote_input_debug ("dev type: %d, event type: %d, code: %d, value: %d\n",
          pkt.dev_type, pkt.event.type, pkt.event.code, pkt.event.value);
      break;
    }
  }

  //gettimeofday (&pkt.event.time, 0);
  for (iter = rinput->dev_list; iter; iter = g_list_next (iter)) {
    rinput_device = iter->data;
    found_device = FALSE;

    switch (rinput_device->dev_type) {
      case RINPUT_DEV_MOUSE:
      case RINPUT_DEV_VIRTUAL_MOUSE:{
        if (pkt.dev_type == RINPUT_DEV_MOUSE) {
          found_device = TRUE;
        }
        break;
      }
      case RINPUT_DEV_KEYBOARD:
      case RINPUT_DEV_VIRTUAL_KEYBOARD:{
        if (pkt.dev_type == RINPUT_DEV_KEYBOARD) {
          found_device = TRUE;
        }
        break;
      }
      default:{
        remote_input_debug ("Invalid device type: %d\n",
            rinput_device->dev_type);
        break;
      }
    }

    if (!found_device) {
      continue;
    }

    len = write (rinput_device->fd, &pkt.event, sizeof (pkt.event));
    if (len != sizeof (pkt.event)) {
      remote_input_debug ("Write failed, device fd: %d\n", rinput_device->fd);
      return RINPUT_WRITE_ERROR;
    }
    break;
  }

  return RINPUT_OK;
}

RINPUT_RESULT
remote_input_init (void **handle, RINPUT_TYPE rinput_type)
{
  RINPUT *rinput = NULL;
  RINPUT_RESULT ret = RINPUT_OK;

  if (rinput_type == RINPUT_NULL) {
    return RINPUT_PARAM_ERROR;
  }

  rinput = (RINPUT *) g_new0 (RINPUT, 1);
  if (!rinput) {
    return RINPUT_NO_MEMORY;
  }
  rinput->type = RINPUT_NULL;
  rinput->dev_list = NULL;
  rinput->socket_fd = -1;
  rinput->conn = -1;
  /* Timeout of every read event in millisecond */
  rinput->timeout = 5;
  rinput->libinput_fd = -1;
  *handle = (void *) rinput;

  if (rinput_type == RINPUT_CLIENT) {
    ret = remote_udev_input_init (rinput);
  } else {
    ret = remote_input_open_device (rinput, rinput_type,
        RINPUT_DEV_VIRTUAL_MOUSE, "virtual_mouse", BUS_USB);
    if (ret != RINPUT_OK) {
      remote_input_debug ("Failed to open virtual mouse\n");
      goto done;
    }

    ret = remote_input_open_device (rinput, rinput_type,
        RINPUT_DEV_VIRTUAL_KEYBOARD, "virtual_keyboard", BUS_USB);
    if (ret != RINPUT_OK) {
      remote_input_debug ("Failed to open virtual keyboard\n");
      goto done;
    }
  }

done:
  return ret;
}

RINPUT_RESULT
remote_input_connect (void *handle, char *ip)
{
  RINPUT *rinput = (RINPUT *) handle;
  int socket_fd = -1;
  int conn = -1;
  RINPUT_RESULT ret = RINPUT_OK;

  if (!rinput || rinput->type == RINPUT_NULL) {
    return RINPUT_PARAM_ERROR;
  }

  if (rinput->type == RINPUT_SERVER) {
    if (rinput->socket_fd < 0) {
      if (!ip) {
        return RINPUT_PARAM_ERROR;
      }

      struct sockaddr_in server_sockaddr;
      server_sockaddr.sin_family = AF_INET;
      server_sockaddr.sin_port = htons (8887);
      server_sockaddr.sin_addr.s_addr = inet_addr (ip);

      socket_fd = socket (AF_INET, SOCK_STREAM, 0);
      if (socket_fd < 0) {
        remote_input_debug ("Failed to create socket\n");
        ret = RINPUT_FAIL;
        goto done;
      }

      if (bind (socket_fd, (struct sockaddr *) &server_sockaddr,
              sizeof (server_sockaddr)) == -1) {
        remote_input_debug ("Failed to bind\n");
        ret = RINPUT_FAIL;
        goto done;
      }
    } else {
      socket_fd = rinput->socket_fd;
    }

    if (listen (socket_fd, HID_PKT_QUEUE) == -1) {
      remote_input_debug ("Failed to listen\n");
      ret = RINPUT_FAIL;
      goto done;
    }

    struct sockaddr_in client_addr;
    socklen_t length = sizeof (client_addr);

    conn = accept (socket_fd, (struct sockaddr *) &client_addr, &length);
    if (conn < 0) {
      remote_input_debug ("Failed to connect\n");
      ret = RINPUT_FAIL;
      goto done;
    }
  } else if (rinput->type == RINPUT_CLIENT) {
    if (rinput->socket_fd >= 0) {
      close (rinput->socket_fd);
      rinput->socket_fd = -1;
    }

    socket_fd = socket (AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
      remote_input_debug ("Failed to create socket\n");
      ret = RINPUT_FAIL;
      goto done;
    }

    struct sockaddr_in servaddr;
    memset (&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (8887);
    servaddr.sin_addr.s_addr = inet_addr (ip);
    if (connect (socket_fd, (struct sockaddr *) &servaddr,
            sizeof (servaddr)) < 0) {
      remote_input_debug ("Failed to connect server\n");
      ret = RINPUT_FAIL;
      goto done;
    }
  }

done:
  rinput->socket_fd = socket_fd;
  rinput->conn = conn;
  return ret;
}

RINPUT_RESULT
remote_input_dispatch (void *handle)
{
  RINPUT *rinput = (RINPUT *) handle;
  RINPUT_RESULT ret = RINPUT_OK;

  if (rinput->type == RINPUT_CLIENT) {
    ret = remote_input_send (rinput);
  } else if (rinput->type == RINPUT_SERVER) {
    ret = remote_input_recv (rinput);
  } else {
    ret = RINPUT_FAIL;
  }

  return ret;
}

RINPUT_RESULT
remote_input_free (void *handle)
{
  RINPUT *rinput = (RINPUT *) handle;
  RINPUT_RESULT ret = RINPUT_OK;
  RINPUT_DEVICE *rinput_device;
  GList *iter;

  if (!rinput) {
    return ret;
  }

  close (rinput->conn);
  close (rinput->socket_fd);

  if (rinput->type == RINPUT_CLIENT) {
    if (rinput->libinput) {
      libinput_unref (rinput->libinput);
      rinput->libinput = NULL;
    }
    if (rinput->udev) {
      udev_unref (rinput->udev);
      rinput->udev = NULL;
    }
  }

  remote_input_debug ("free device list\n");
  for (iter = rinput->dev_list; iter; iter = g_list_next (iter)) {
    rinput_device = iter->data;

    if (rinput_device->dev_type == RINPUT_DEV_VIRTUAL_MOUSE ||
        rinput_device->dev_type == RINPUT_DEV_VIRTUAL_KEYBOARD) {
      ioctl (rinput_device->fd, UI_DEV_DESTROY);
    }

    if (close (rinput_device->fd) < 0) {
      remote_input_debug ("Fail to close device, fd: %d\n", rinput_device->fd);
      ret = RINPUT_FAIL;
    }
    g_free (rinput_device->sys_name);
  }
  g_list_free_full (rinput->dev_list, g_free);
  rinput->dev_list = NULL;

  g_free (rinput);
  return ret;
}
