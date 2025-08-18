/* GStreamer
 * Copyright 2024-2025 NXP
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

#ifndef __GST_IMXASRC_CONVERTER_H__
#define __GST_IMXASRC_CONVERTER_H__

#include <gst/audio/audio.h>
#include "imxasrc-resampler.h"

G_BEGIN_DECLS

typedef struct _GstImxASRCConverter GstImxASRCConverter;

struct _GstImxASRCConverter
{
  GstAudioInfo in;
  GstAudioInfo out;

  GstStructure *config;

  gint channels;
  GstAudioFormat format;
  GstImxASRCMethod method;
  IMXASRCResamplerQuality quality;
  GstImxASRCResampler *resampler;
};

GST_AUDIO_API
gsize gst_imxasrc_converter_get_out_frames (GstImxASRCConverter *convert,
                                            gsize in_frames);

GST_AUDIO_API
gsize gst_imxasrc_converter_get_in_frames (GstImxASRCConverter *convert,
                                           gsize out_frames);

GST_AUDIO_API
void gst_imxasrc_converter_reset (GstImxASRCConverter * convert);

GST_AUDIO_API
gboolean gst_imxasrc_converter_update_config (GstImxASRCConverter * convert,
                                              gint in_rate,
                                              gint out_rate,
                                              GstStructure * config);

GST_AUDIO_API
GstImxASRCConverter * gst_imxasrc_converter_new (GstImxASRCMethod method,
                                                 GstAudioInfo * in_info,
                                                 GstAudioInfo * out_info, GstStructure * config);

GST_AUDIO_API
void gst_imxasrc_converter_free (GstImxASRCConverter * convert);

GST_AUDIO_API
gint gst_imxasrc_converter_samples (GstImxASRCConverter * convert,
                                        GstAudioConverterFlags flags,
                                        gpointer in[], gsize in_frames,
                                        gpointer out[], gsize out_frames);

GST_AUDIO_API
gboolean gst_imxasrc_converter_set_quality (GstImxASRCConverter * convert, IMXASRCResamplerQuality quality);

G_END_DECLS

#endif /* __GST_IMXASRC_CONVERTER_H__ */
