/*
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

#ifndef __IMX_PLUGINS_H__
#define __IMX_PLUGINS_H__

#include <gst/gst.h>

#define IMX_GST_PLUGIN_AUTHOR "i.MX Multimedia Team <mpusw_mm-mpuswmm@nxp.com>"
#define IMX_GST_PLUGIN_PACKAGE_NAME "i.MX Gstreamer Multimedia Plugins"
#define IMX_GST_PLUGIN_PACKAGE_ORIG "http://www.nxp.com"
#define IMX_GST_PLUGIN_LICENSE "LGPL"

#define IMX_GST_PLUGIN_RANK (GST_RANK_PRIMARY+1)

#define IMX_GST_PLUGIN_DEFINE(name, description, initfunc)\
  GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,\
      GST_VERSION_MINOR,\
      name, \
      description,\
      initfunc,\
      VERSION,\
      IMX_GST_PLUGIN_LICENSE,\
      IMX_GST_PLUGIN_PACKAGE_NAME, IMX_GST_PLUGIN_PACKAGE_ORIG)

/* used by legacy platform plugin overlaysink and imxv4l2sink */
typedef enum
{
  GST_IMX_ROTATION_0 = 0,
  GST_IMX_ROTATION_90,
  GST_IMX_ROTATION_180,
  GST_IMX_ROTATION_270,
  GST_IMX_ROTATION_HFLIP,
  GST_IMX_ROTATION_VFLIP
}GstImxRotateMethod;

#endif /* __IMX_COMMON_H__ */ 