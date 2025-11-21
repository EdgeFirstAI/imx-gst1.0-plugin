/* GStreamer IMX Video 2D device
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2023-2025 NXP
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

#include "imx_2d_device.h"

GST_DEBUG_CATEGORY (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

#ifdef USE_IPU
extern Imx2DDevice * imx_ipu_create(Imx2DDeviceType  device_type);
extern gint imx_ipu_destroy(Imx2DDevice *device);
extern gboolean imx_ipu_is_exist (void);
#endif

#ifdef USE_G2D
extern Imx2DDevice * imx_g2d_create(Imx2DDeviceType  device_type);
extern gint imx_g2d_destroy(Imx2DDevice *device);
extern gboolean imx_g2d_is_exist (void);
#endif

#ifdef USE_PXP
extern Imx2DDevice * imx_pxp_create(Imx2DDeviceType  device_type);
extern gint imx_pxp_destroy(Imx2DDevice *device);
extern gboolean imx_pxp_is_exist (void);
#endif

#ifdef USE_OCL
extern Imx2DDevice * imx_ocl_create(Imx2DDeviceType  device_type);
extern gint imx_ocl_destroy(Imx2DDevice *device);
extern gboolean imx_ocl_is_exist (void);
#endif

//#define USE_GST_VIDEO_SAMPLE_CONVERT  //bad performance

static const Imx2DDeviceInfo Imx2DDevices[] = {
#ifdef USE_IPU
    { .name                     ="ipu",
      .device_type              =IMX_2D_DEVICE_IPU,
      .create                   =imx_ipu_create,
      .destroy                  =imx_ipu_destroy,
      .is_exist                 =imx_ipu_is_exist
    },
#endif

#ifdef USE_G2D
    { .name                     ="g2d",
      .device_type              =IMX_2D_DEVICE_G2D,
      .create                   =imx_g2d_create,
      .destroy                  =imx_g2d_destroy,
      .is_exist                 =imx_g2d_is_exist
    },
#endif

#ifdef USE_PXP
    { .name                     ="pxp",
      .device_type              =IMX_2D_DEVICE_PXP,
      .create                   =imx_pxp_create,
      .destroy                  =imx_pxp_destroy,
      .is_exist                 =imx_pxp_is_exist
    },
#endif

#ifdef USE_OCL
    { .name                     ="ocl",
      .device_type              =IMX_2D_DEVICE_OCL,
      .create                   =imx_ocl_create,
      .destroy                  =imx_ocl_destroy,
      .is_exist                 =imx_ocl_is_exist
    },
#endif
    {
      NULL
    }
};

const Imx2DDeviceInfo * imx_get_2d_devices(void)
{
  static gint debug_init = 0;
  if (debug_init == 0) {
    GST_DEBUG_CATEGORY_INIT (imx2ddevice_debug, "imx2ddevice", 0,
                           "Freescale IMX 2D Devices");
    debug_init = 1;
  }

  return &Imx2DDevices[0];
}

Imx2DDevice * imx_2d_device_create(Imx2DDeviceType  device_type)
{
  const Imx2DDeviceInfo *dev_info = imx_get_2d_devices();
  while (dev_info->name) {
    if (dev_info->device_type == device_type) {
      if (dev_info->is_exist()) {
        return dev_info->create(device_type);
      } else {
        GST_ERROR("device %s not exist", dev_info->name);
        return NULL;
      }
    }
    dev_info++;
  }

  GST_ERROR("Unknown 2D device type %d\n", device_type);
  return NULL;
}

gint imx_2d_device_destroy(Imx2DDevice *device)
{
  if (!device)
    return -1;

  const Imx2DDeviceInfo *dev_info = imx_get_2d_devices();
  while (dev_info->name) {
    if (dev_info->device_type == device->device_type)
      return dev_info->destroy(device);
    dev_info++;
  }

  GST_ERROR("Unknown 2D device type %d\n", device->device_type);
  return -1;
}

/**
 * imx_2d_device_video_info_from_caps
 * @caps: a #GstCaps
 * @info: (out caller-allocates): #Imx2DVideoInfo
 *
 * Parse @caps and update @info no matter if the caps is fixed or not.
 * The function can be called during caps negotiation.
 *
 * Returns: TRUE if @caps has fixed format.
 */
gboolean imx_2d_device_video_info_from_caps (GstCaps * caps, Imx2DVideoInfo *info)
{
  gint i, caps_size;
  GstStructure *st;
  const GValue *format;
  const gchar *fmt_name;
  GstVideoFormat out_fmt = GST_VIDEO_FORMAT_UNKNOWN;
  const gchar *s;
  GstVideoInfo video_info;
  gboolean is_drm_format = FALSE;

  if (!caps || !info) {
    return FALSE;
  }

  memset (info, 0, sizeof (Imx2DVideoInfo));
  caps_size = gst_caps_get_size (caps);
  for (i = 0; i < caps_size; i++) {
    st = gst_caps_get_structure(caps, i);

    if (!g_strcmp0 (gst_structure_get_string (st, "format"), "DMA_DRM")) {
      format = gst_structure_get_value (st, "drm-format");
      is_drm_format = TRUE;
    } else {
      format = gst_structure_get_value (st, "format");
      is_drm_format = FALSE;
    }

    /* Check the selected caps if it has the fixed format */
    if (GST_VALUE_HOLDS_LIST (format)) {
      if (gst_value_list_get_size (format) == 1) {
        const GValue *val;
        val = gst_value_list_get_value (format, 0);
        if (!G_VALUE_HOLDS_STRING (val)) {
          out_fmt = GST_VIDEO_FORMAT_UNKNOWN;
          GST_TRACE ("No valid format in the list");
          break;
        }
        /* Has the fixed format and get it below */
        format = val;

      } else {
        out_fmt = GST_VIDEO_FORMAT_UNKNOWN;
        GST_TRACE ("No fixed format in the list");
        break;
      }
    }

    /* Get the fixed format in the selected caps */
    if (G_VALUE_HOLDS_STRING (format)) {
      if (is_drm_format) {
        guint32 fourcc;
        guint64 modifier;
        GstVideoFormat gst_format;

        fourcc = gst_video_dma_drm_fourcc_from_string (g_value_get_string (format), &modifier);
        gst_format = gst_video_dma_drm_fourcc_to_format (fourcc);
        if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
          return FALSE;
        fmt_name = gst_video_format_to_string(gst_format);
      } else {
        fmt_name = g_value_get_string (format);
      }

      if (out_fmt == GST_VIDEO_FORMAT_UNKNOWN) {
        /* Record the first fixed format */
        out_fmt = gst_video_format_from_string(fmt_name);

        gst_video_info_init (&video_info);
        video_info.finfo = gst_video_format_get_info (out_fmt);
        if (!gst_structure_get (st, "width", G_TYPE_INT,
              &(video_info.width), "height",
              G_TYPE_INT, &(video_info.height), NULL)) {
          video_info.width = 0;
          video_info.height = 0;
        }

        if ((s = gst_structure_get_string (st, "interlace-mode"))) {
          video_info.interlace_mode = gst_video_interlace_mode_from_string (s);
        } else {
          video_info.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
        }

        if ((s = gst_structure_get_string (st, "colorimetry"))) {
          gst_video_colorimetry_from_string (&(video_info.colorimetry), s);
        }

        info->fmt = GST_VIDEO_INFO_FORMAT(&video_info);
        info->w = GST_VIDEO_INFO_WIDTH(&video_info);
        info->h = GST_VIDEO_INFO_HEIGHT(&video_info);
        switch (video_info.colorimetry.range) {
          case GST_VIDEO_COLOR_RANGE_0_255:
            info->colorimetry.range = IMX_2D_COLOR_RANGE_FULL;
            break;
          case GST_VIDEO_COLOR_RANGE_16_235:
            info->colorimetry.range = IMX_2D_COLOR_RANGE_LIMITED;
            break;
          default:
            info->colorimetry.range = IMX_2D_COLOR_RANGE_DEFAULT;
            break;
        }

        switch (video_info.colorimetry.matrix) {
          case GST_VIDEO_COLOR_MATRIX_BT709:
            info->colorimetry.matrix = IMX_2D_COLOR_MATRIX_BT709;
            break;
          case GST_VIDEO_COLOR_MATRIX_BT601:
            info->colorimetry.matrix = IMX_2D_COLOR_MATRIX_BT601_625;
            break;
          default:
            info->colorimetry.matrix = IMX_2D_COLOR_MATRIX_DEFAULT;
            break;
        }

        switch (video_info.interlace_mode) {
          case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
            info->interlace_type = IMX_2D_INTERLACE_INTERLEAVED;
            break;
          case GST_VIDEO_INTERLACE_MODE_MIXED:
            info->interlace_type = IMX_2D_INTERLACE_MIXED;
            break;
          case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
            info->interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;
            break;
          case GST_VIDEO_INTERLACE_MODE_FIELDS:
            info->interlace_type = IMX_2D_INTERLACE_FIELDS;
            break;
          default:
            info->interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;
            break;
        }
      } else if (out_fmt != gst_video_format_from_string(fmt_name)) {
        out_fmt = GST_VIDEO_FORMAT_UNKNOWN;
        GST_TRACE ("No fixed format in the caps");
        break;
      }
    }
  }

  if (out_fmt == GST_VIDEO_FORMAT_UNKNOWN) {
    memset (info, 0, sizeof (Imx2DVideoInfo));
    return FALSE;
  } else {
    GST_TRACE ("Update info, fmt:%d, %dx%d, range:%d, matrix:%d interlace:%d",
        info->fmt, info->w, info->h, info->colorimetry.range,
        info->colorimetry.matrix, info->interlace_type);
    return TRUE;
  }
}

static gboolean imx_2d_device_probe_warp_header (Imx2DDevice *device,
     FILE *fp, gsize file_size, Imx2DVideoWarp *video_warp)
{
  guint32 header_size = 0;
  guint8 file_version = 0;
  guint32 width,height;
  GstBuffer *gstbuf;
  GstMapInfo map;
  guint32 map_data_size = 0;
  Imx2DVidoWarpArbitrary *p_arb_info = NULL;
  gboolean ret = FALSE;

  #define WARP_HEADER_WIDTH           4
  #define WARP_FILE_VERSION_WIDTH     1
  #define WARP_ALOGITHMS_OFFSET       5
  #define WARP_WIDTH_OFFSET           8
  #define WARP_HEIGHT_OFFSET          12
  #define WARP_ARB_START_X_OFFSET     16
  #define WARP_ARB_START_Y_OFFSET     20
  #define WARP_ARB_DELTA_XX_OFFSET    24
  #define WARP_ARB_DELTA_XY_OFFSET    28
  #define WARP_ARB_DELTA_YX_OFFSET    32
  #define WARP_ARB_DELTA_YY_OFFSET    36
  #define WARP_VERSION_1_HEADER_SZIE  40

  if (!device || !fp || !file_size || !video_warp ||
      file_size < WARP_HEADER_WIDTH) {
    goto exit;
  }

  /* Check header size */
  if (fread(&header_size, 1, WARP_HEADER_WIDTH,
      fp) != WARP_HEADER_WIDTH) {
    GST_DEBUG ("Can't read header size");
    goto exit;
  }

  if (!header_size || file_size < header_size) {
    goto exit;
  }

  if (fread(&file_version, 1, WARP_FILE_VERSION_WIDTH,
      fp) != WARP_FILE_VERSION_WIDTH) {
    GST_DEBUG ("Can't read file format version");
    goto exit;
  } else {
    if (file_version == 1) {
      /* The header size is fixed for file version 1 */
      if (header_size != WARP_VERSION_1_HEADER_SZIE) {
        goto exit;
      }
    } else {
      if (header_size < WARP_VERSION_1_HEADER_SZIE) {
        goto exit;
      }
    }
  }

  gstbuf = gst_buffer_new_and_alloc (header_size);
  gst_buffer_map (gstbuf, &map, GST_MAP_WRITE);
  if (fread(map.data + WARP_ALOGITHMS_OFFSET, 1,
      header_size - WARP_ALOGITHMS_OFFSET, fp) !=
      header_size - WARP_ALOGITHMS_OFFSET) {
    GST_DEBUG ("Can't read header data");
    goto done;
  }

  switch (map.data[WARP_ALOGITHMS_OFFSET]) {
    case IMX_2D_WARP_PNT_32BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_PNT;
      video_warp->bpp = 32;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DPNT_32BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DPNT;
      video_warp->bpp = 32;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DPNT_16BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DPNT;
      video_warp->bpp = 16;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DPNT_8BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DPNT;
      video_warp->bpp = 8;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DDPNT_32BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DDPNT;
      video_warp->bpp = 32;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DDPNT_16BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DDPNT;
      video_warp->bpp = 16;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DDPNT_8BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DDPNT;
      video_warp->bpp = 8;
      ret = TRUE;
      break;
    case IMX_2D_WARP_DDPNT_4BPP:
      video_warp->map_format = IMX_2D_WARP_MAP_DDPNT;
      video_warp->bpp = 4;
      ret = TRUE;
      break;
    default:
      GST_ERROR ("Invalid algorithms type");
      break;
  }

  /* Check file integrity */
  width = GST_READ_UINT32_LE(map.data + WARP_WIDTH_OFFSET);
  height = GST_READ_UINT32_LE(map.data + WARP_HEIGHT_OFFSET);
  map_data_size = width * height * video_warp->bpp / 8;
  if (!ret ||
      ((map_data_size + header_size) != (guint32)file_size)) {
    GST_DEBUG ("Invalid header data");
    goto done;
  }

  video_warp->width = width;
  video_warp->height = height;
  p_arb_info = &video_warp->arb_info;
  switch (video_warp->map_format) {
    case IMX_2D_WARP_MAP_PNT:
      p_arb_info->arb_start_x = 0;
      p_arb_info->arb_start_y = 0;
      ret = TRUE;
      break;
    case IMX_2D_WARP_MAP_DPNT:
      p_arb_info->arb_start_x = GST_READ_UINT32_LE(map.data + WARP_ARB_START_X_OFFSET);
      p_arb_info->arb_start_y = GST_READ_UINT32_LE(map.data + WARP_ARB_START_Y_OFFSET);
      video_warp->arb_num = 2;
      ret = TRUE;
      break;
    case IMX_2D_WARP_MAP_DDPNT:
      p_arb_info->arb_start_x = GST_READ_UINT32_LE(map.data + WARP_ARB_START_X_OFFSET);
      p_arb_info->arb_start_y = GST_READ_UINT32_LE(map.data + WARP_ARB_START_Y_OFFSET);
      p_arb_info->arb_delta_xx = GST_READ_UINT32_LE(map.data + WARP_ARB_DELTA_XX_OFFSET);
      p_arb_info->arb_delta_xy = GST_READ_UINT32_LE(map.data + WARP_ARB_DELTA_XY_OFFSET);
      p_arb_info->arb_delta_yx = GST_READ_UINT32_LE(map.data + WARP_ARB_DELTA_YX_OFFSET);
      p_arb_info->arb_delta_yy = GST_READ_UINT32_LE(map.data + WARP_ARB_DELTA_YY_OFFSET);
      video_warp->arb_num = 6;
      ret = TRUE;
      break;
    default:
      GST_ERROR ("Invalid warp map format");
      ret = FALSE;
      break;
  }

done:
  gst_buffer_unmap (gstbuf, &map);
  gst_buffer_unref (gstbuf);

exit:
  if (ret) {
    GST_DEBUG ("Get header data");
    video_warp->coordinates_size = file_size - header_size;
  } else {
    GST_DEBUG ("No header size info");
    video_warp->coordinates_size = file_size;
  }

  return ret;
}

gboolean imx_2d_device_read_warp_coordinates_file (Imx2DDevice *device,
    const char* file_name, Imx2DVideoWarp *video_warp)
{
  FILE *fp;
  gint ret = 0;
  gsize size = 0;
  PhyMemBlock *mem_blk;
  gsize w_aligned, h_aligned, size_aligned;

  if (!device || !file_name || !video_warp)
    return FALSE;

  mem_blk = &video_warp->coordinates_mem;

  do {
    fp = fopen (file_name, "rb");
    if (fp == NULL) {
      ret = -1;
      GST_DEBUG("Can't open file, file name: %s", file_name);
      break;
    }

    ret = fseek(fp, 0, SEEK_END);
    if (ret) {
      break;
    }

    size = ftell(fp);
    if (size == 0) {
      ret = -1;
      break;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
      break;
    }

    /* Probe header data*/
    if (!imx_2d_device_probe_warp_header (device, fp, size, video_warp)) {
      ret = fseek(fp, 0, SEEK_SET);
      if (ret) {
        break;
      }
    }
    size = video_warp->coordinates_size;

    w_aligned = ALIGNTO (video_warp->width, ALIGNMENT);
    h_aligned = ALIGNTO (video_warp->height, ALIGNMENT);
    size_aligned = w_aligned * h_aligned * video_warp->bpp / 8;
    size_aligned = PAGE_ALIGN (size_aligned);

    if (mem_blk->size) {
      device->free_mem (device, mem_blk);
    }
    mem_blk->size = (size > size_aligned) ? size: size_aligned;
    if (device->alloc_mem (device, mem_blk)) {
      ret = -1;
      break;
    }

    if(fread(mem_blk->vaddr, 1, size, fp) != size) {
      ret = -1;
      break;
    } else {
      ret = 0;
    }
  } while (0);

  if (fp)
    fclose(fp);

  if (ret) {
    if (mem_blk->vaddr)
      device->free_mem (device, mem_blk);
    GST_DEBUG("read file failed: %s", file_name);
    video_warp->coordinates_size = 0;
    return FALSE;
  } else {
    return TRUE;
  }
}

void imx_2d_device_set_warp_controls (const GstStructure * config,
    Imx2DVideoWarp *video_warp)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (video_warp != NULL);

  if (gst_structure_has_field(config, "map-format")) {
    gst_structure_get(config, "map-format",
        G_TYPE_INT, &video_warp->map_format, NULL);
  }

  if (gst_structure_has_field(config, "width")) {
    gst_structure_get(config, "width",
        G_TYPE_INT, &video_warp->width, NULL);
  }

  if (gst_structure_has_field(config, "height")) {
    gst_structure_get(config, "height",
        G_TYPE_INT, &video_warp->height, NULL);
  }

  if (gst_structure_has_field(config, "bpp")) {
    gst_structure_get(config, "bpp",
        G_TYPE_INT, &video_warp->bpp, NULL);
  }

  if (gst_structure_has_field(config, "arb_start_x")) {
    gst_structure_get(config, "arb_start_x",
        G_TYPE_INT, &video_warp->arb_info.arb_start_x, NULL);
    video_warp->arb_num++;
  }

  if (gst_structure_has_field(config, "arb_start_y")) {
    gst_structure_get(config, "arb_start_y",
        G_TYPE_INT, &video_warp->arb_info.arb_start_y, NULL);
    video_warp->arb_num++;
  }

  if (gst_structure_has_field(config, "arb_delta_xx")) {
    gst_structure_get(config, "arb_delta_xx",
        G_TYPE_INT, &video_warp->arb_info.arb_delta_xx, NULL);
    video_warp->arb_num++;
  }

  if (gst_structure_has_field(config, "arb_delta_xy")) {
    gst_structure_get(config, "arb_delta_xy",
        G_TYPE_INT, &video_warp->arb_info.arb_delta_xy, NULL);
    video_warp->arb_num++;
  }

  if (gst_structure_has_field(config, "arb_delta_yx")) {
    gst_structure_get(config, "arb_delta_yx",
        G_TYPE_INT, &video_warp->arb_info.arb_delta_yx, NULL);
    video_warp->arb_num++;
  }

  if (gst_structure_has_field(config, "arb_delta_yy")) {
    gst_structure_get(config, "arb_delta_yy",
        G_TYPE_INT, &video_warp->arb_info.arb_delta_yy, NULL);
    video_warp->arb_num++;
  }
}

#ifdef USE_GST_VIDEO_SAMPLE_CONVERT
void imx_2d_device_fill_background(Imx2DFrame *dst, guint RGBA8888)
{
  GstVideoInfo vinfo;
  GstCaps *from_caps, *to_caps;
  GstBuffer *from_buffer, *to_buffer;
  GstSample *from_sample, *to_sample;
  gint i;
  GstMapInfo map;

  from_buffer = gst_buffer_new_and_alloc (dst->info.stride * dst->info.h * 4);

  gst_buffer_map (from_buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < dst->info.stride * dst->info.h; i++) {
    map.data[4 * i + 0] = RGBA8888 & 0x000000FF;
    map.data[4 * i + 1] = (RGBA8888 & 0x0000FF00) >> 8;
    map.data[4 * i + 2] = (RGBA8888 & 0x00FF0000) >> 16;
    map.data[4 * i + 3] = (RGBA8888 & 0xFF000000) >> 24;
  }
  gst_buffer_unmap (from_buffer, &map);

  gst_video_info_init (&vinfo);
  gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGBA,
      dst->info.w, dst->info.h);
  from_caps = gst_video_info_to_caps (&vinfo);
  from_sample = gst_sample_new (from_buffer, from_caps, NULL, NULL);

  gst_video_info_set_format (&vinfo, dst->info.fmt, dst->info.w, dst->info.h);
  to_caps = gst_video_info_to_caps (&vinfo);
  to_sample = gst_video_convert_sample (from_sample, to_caps, GST_SECOND, NULL);
  if (to_sample) {
    to_buffer = gst_sample_get_buffer(to_sample);
    gst_buffer_map (to_buffer, &map, GST_MAP_READ);
    memcpy(dst->mem->vaddr, map.data, map.size);
    gst_buffer_unmap(to_buffer, &map);
    gst_sample_unref (to_sample);
  }
  gst_buffer_unref (from_buffer);
  gst_caps_unref (from_caps);
  gst_sample_unref (from_sample);
  gst_caps_unref (to_caps);
}
#else
void imx_2d_device_fill_background(Imx2DFrame *dst, guint RGBA8888)
{
  gchar *p = (gchar *)dst->mem->vaddr;
  gint i;
  gchar R,G,B,A,Y,U,V;
  gdouble y,u,v;

  R = RGBA8888 & 0x000000FF;
  G = (RGBA8888 & 0x0000FF00) >> 8;
  B = (RGBA8888 & 0x00FF0000) >> 16;
  A = (RGBA8888 & 0xFF000000) >> 24;

  //BT.709
  y = (0.213*R + 0.715*G + 0.072*B);
  u = -0.117*R - 0.394*G + 0.511*B + 128;
  v = 0.511*R - 0.464*G - 0.047*B + 128;

  if (y > 255.0)
    Y = 255;
  else
    Y = (gchar)y;
  if (u < 0.0)
    U = 0;
  else
    U = (gchar)u;
  if (u > 255.0)
    U = 255;
  else
    U = (gchar)u;
  if (v < 0.0)
    V = 0;
  else
    V = (gchar)v;
  if (v > 255.0)
    V = 255;
  else
    V = (gchar)v;

  GST_INFO("RGBA8888 to %s\n", gst_video_format_to_string(dst->info.fmt));

  switch (dst->info.fmt) {
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGBA:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = R;
        p[4 * i + 1] = G;
        p[4 * i + 2] = B;
        p[4 * i + 3] = A;
      }
      break;
    case GST_VIDEO_FORMAT_BGR:
      for (i = 0; i < dst->mem->size/3; i++) {
        p[3 * i + 0] = B;
        p[3 * i + 1] = G;
        p[3 * i + 2] = R;
      }
      break;
    case GST_VIDEO_FORMAT_RGB:
      for (i = 0; i < dst->mem->size/3; i++) {
        p[3 * i + 0] = R;
        p[3 * i + 1] = G;
        p[3 * i + 2] = B;
      }
      break;
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = B;
        p[4 * i + 1] = G;
        p[4 * i + 2] = R;
        p[4 * i + 3] = A;
      }
      break;
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = A;
        p[4 * i + 1] = B;
        p[4 * i + 2] = G;
        p[4 * i + 3] = R;
      }
      break;
    case GST_VIDEO_FORMAT_RGB16:
      for (i = 0; i < dst->mem->size/2; i++) {
        p[2 * i + 0] = ((G<<3) & 0xE0) | (B>>3);
        p[2 * i + 1] = (R & 0xF8) | (G>>5);
      }
      break;
    case GST_VIDEO_FORMAT_BGR16:
      for (i = 0; i < dst->mem->size/2; i++) {
        p[2 * i + 0] = ((G<<3) & 0xE0) | (R>>3);
        p[2 * i + 1] = (B & 0xF8) | (G>>5);
      }
      break;
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = A;
        p[4 * i + 1] = R;
        p[4 * i + 2] = G;
        p[4 * i + 3] = B;
      }
      break;
    case GST_VIDEO_FORMAT_Y444:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h*2, V, dst->info.w*dst->info.h);
      break;
    case GST_VIDEO_FORMAT_I420:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h/4);
      memset(p+dst->info.w*dst->info.h*5/4, V, dst->info.w*dst->info.h/4);
      break;
    case GST_VIDEO_FORMAT_YV12:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, V, dst->info.w*dst->info.h/4);
      memset(p+dst->info.w*dst->info.h*5/4, U, dst->info.w*dst->info.h/4);
      break;
    case GST_VIDEO_FORMAT_NV12:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/4; i++) {
        *p++ = U;
        *p++ = V;
      }
      break;
    case GST_VIDEO_FORMAT_NV21:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/4; i++) {
        *p++ = V;
        *p++ = U;
      }
      break;
    case GST_VIDEO_FORMAT_UYVY:
      for (i = 0; i < dst->info.w*dst->info.h/2; i++) {
        *p++ = U;
        *p++ = Y;
        *p++ = V;
        *p++ = Y;
      }
      break;
    case GST_VIDEO_FORMAT_YUY2:
      for (i = 0; i < dst->info.w*dst->info.h/2; i++) {
        *p++ = Y;
        *p++ = U;
        *p++ = Y;
        *p++ = V;
      }
      break;
    case GST_VIDEO_FORMAT_Y42B:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h/2);
      memset(p+dst->info.w*dst->info.h*3/2, V, dst->info.w*dst->info.h/2);
      break;
    case GST_VIDEO_FORMAT_v308:
      for (i = 0; i < dst->info.w*dst->info.h; i++) {
        *p++ = Y;
        *p++ = U;
        *p++ = V;
      }
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      memset(p, Y, dst->info.w*dst->info.h);
      break;
    case GST_VIDEO_FORMAT_NV16:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/2; i++) {
        *p++ = U;
        *p++ = V;
      }
      break;
    default:
      GST_FIXME("Add support for %d", dst->info.fmt);
      memset(dst->mem->vaddr, 0, dst->mem->size);
      break;
  }
}
#endif