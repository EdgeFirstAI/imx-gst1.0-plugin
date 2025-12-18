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

#ifndef _REMOTE_INPUT_H_
#define _REMOTE_INPUT_H_

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define REMOTE_INPUT_VERSION_STR "REMOTE_INPUT_00.00.01"

  typedef enum
  {
    RINPUT_CLIENT = 0,
    RINPUT_SERVER = 1,
    RINPUT_NULL = -1,
  } RINPUT_TYPE;

  typedef enum
  {
    RINPUT_OK = 0,
    RINPUT_FAIL = -1,
    RINPUT_NO_MEMORY = -2,
    RINPUT_PARAM_ERROR = -3,
    RINPUT_READ_ERROR = -4,
    RINPUT_WRITE_ERROR = -5,
  } RINPUT_RESULT;

  typedef enum
  {
    RINPUT_DEV_NUL = 0,
    RINPUT_DEV_MOUSE = 1,
    RINPUT_DEV_KEYBOARD = 2,
    RINPUT_DEV_TOUCH = 3,
    RINPUT_DEV_TABLET_TOOL = 4,
    RINPUT_DEV_VIRTUAL_MOUSE = 5,
    RINPUT_DEV_VIRTUAL_KEYBOARD = 6,
  } RINPUT_DEV_TYPE;

  void remote_input_enable_log (int enable);
  RINPUT_RESULT remote_input_init (void **handle, RINPUT_TYPE record_type);
  RINPUT_RESULT remote_input_connect (void *handle, char *ip);
  RINPUT_RESULT remote_input_dispatch (void *handle);
  RINPUT_RESULT remote_input_free (void *handle);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
