/* GStreamer
 * Copyright 2025-2026 NXP
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __IMXASRC_DEVICE_H__
#define __IMXASRC_DEVICE_H__

#include <gst/audio/audio.h>
#include <gst/gst.h>
#include "gstimxsocfeatures.h"

#define SSRCLIBRARYDIR "/usr/lib/libssrcArmNeon.so"
#define DSPCLIBRARYDIR "/usr/lib/lib_dspc_asrc.so"

typedef enum {
  LIB_SSRC,
  LIB_DSPC
} ASRCSWLibType;

typedef enum {
  GST_IMXASRC_METHOD_HW,
  GST_IMXASRC_METHOD_SSRC,
  GST_IMXASRC_METHOD_DSPC,
} GstImxASRCMethod;

typedef enum {
  GST_IMXASRC_RESAMPLER_QUALITY_0,
  GST_IMXASRC_RESAMPLER_QUALITY_1,
  GST_IMXASRC_RESAMPLER_QUALITY_2,
} IMXASRCResamplerQuality;

typedef struct _ImxASRCDeviceInfo {
  gchar *name;
  GstImxASRCMethod method;
  gboolean           (*is_exist) (void);
  GList*             (*get_supported_fmts) (void);
  GList*             (*get_supported_rates) (void);
  gint               (*get_min_channels) (void);
  gint               (*get_max_channels) (void);
} ImxASRCDeviceInfo;

const ImxASRCDeviceInfo *imx_get_asrc_devices (void);

#endif
