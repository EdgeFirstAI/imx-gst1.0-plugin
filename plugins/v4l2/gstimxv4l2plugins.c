/*
 * Copyright (c) 2013-2014, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2026 NXP
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstimxplugins.h"

#include "gstimxv4l2sink.h"
#include "gstimxv4l2src.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "imxv4l2sink", IMX_GST_PLUGIN_RANK,
        GST_TYPE_IMX_V4L2SINK))
    return FALSE;

  if (!gst_element_register (plugin, "imxv4l2src", IMX_GST_PLUGIN_RANK,
        GST_TYPE_IMX_V4L2SRC))
    return FALSE;

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (imxv4l2, "IMX SoC v4l2-based video source/sink", plugin_init);
