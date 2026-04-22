/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018,2026 NXP
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

#include <string.h>
#include <gst/gst.h>

#include "gstimxsocfeatures.h"
#include "gstimxplugins.h"
#include "gstvpuenc.h"
#include "gstvpudec.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (imx_soc_has_feature ("vpu")) {
    guint rank = IMX_GST_PLUGIN_RANK;
    if (imx_soc_is_chip ("MX6Q") || imx_soc_is_chip ("MX8MM") || imx_soc_is_chip ("MX8MP"))
      if (!gst_vpu_enc_register (plugin))
        return FALSE;

    if (imx_soc_in_group ("hantro"))
      rank = GST_RANK_SECONDARY;
    if (!gst_element_register (plugin, "vpudec", rank, GST_TYPE_VPU_DEC))
      return FALSE;

    return TRUE;
  } else {
    return FALSE;
  }
}

IMX_GST_PLUGIN_DEFINE (vpu, "VPU video codec", plugin_init);
