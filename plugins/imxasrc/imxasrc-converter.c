/* GStreamer
 * Copyright 2024 NXP
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

#include "imxasrc-converter.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("imxasrc-converter", 0,
        "imxasrc-converter object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

/**
 * gst_imxasrc_converter_get_out_frames:
 * @convert: a #GstImxASRCConverter
 * @in_frames: number of input frames
 *
 * Calculate how many output frames can be produced when @in_frames input
 * frames are given to @convert.
 *
 * Returns: the number of output frames
 */
gsize
gst_imxasrc_converter_get_out_frames (GstImxASRCConverter * convert,
    gsize in_frames)
{
  if (convert->resampler)
    return gst_imxasrc_resampler_get_out_frames (convert->resampler, in_frames);
  else
    return in_frames;
}

/**
 * gst_imxasrc_converter_reset:
 * @convert: a #GstImxASRCConverter
 *
 * Reset @convert to the state it was when it was first created, clearing
 * any history it might currently have.
 */
void
gst_imxasrc_converter_reset (GstImxASRCConverter * convert)
{
  if (convert->resampler)
    gst_imxasrc_resampler_reset (convert->resampler);
}

static gboolean
copy_config (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstImxASRCConverter *convert = user_data;

  gst_structure_id_set_value (convert->config, field_id, value);

  return TRUE;
}

/**
 * gst_imxasrc_converter_update_config:
 * @convert: a #GstImxASRCConverter
 * @in_rate: input rate
 * @out_rate: output rate
 * @config: (transfer full) (allow-none): a #GstStructure or %NULL
 *
 * Set @in_rate, @out_rate and @config as extra configuration for @convert.
 *
 * @in_rate and @out_rate specify the new sample rates of input and output
 * formats. A value of 0 leaves the sample rate unchanged.
 *
 * @config can be %NULL, in which case, the current configuration is not
 * changed.
 *
 * If the parameters in @config can not be set exactly, this function returns
 * %FALSE and will try to update as much state as possible. The new state can
 * then be retrieved and refined with gst_audio_converter_get_config().
 *
 * Look at the `GST_AUDIO_CONVERTER_OPT_*` fields to check valid configuration
 * option and values.
 *
 * Returns: %TRUE when the new parameters could be set
 */
gboolean
gst_imxasrc_converter_update_config (GstImxASRCConverter * convert,
    gint in_rate, gint out_rate, GstStructure * config)
{
  g_return_val_if_fail (convert != NULL, FALSE);

  GST_LOG ("new rate %d -> %d", in_rate, out_rate);

  if (in_rate <= 0)
    in_rate = convert->in.rate;
  if (out_rate <= 0)
    out_rate = convert->out.rate;

  convert->in.rate = in_rate;
  convert->out.rate = out_rate;

  if (convert->resampler) {
    if (!gst_imxasrc_resampler_update (convert->resampler, in_rate, out_rate, config))
      return FALSE;
  }

  if (config) {
    gst_structure_foreach (config, copy_config, convert);
    gst_structure_free (config);
  }

  return TRUE;
}

/**
 * gst_audio_converter_new:
 * @flags: extra #GstAudioConverterFlags
 * @in_info: a source #GstAudioInfo
 * @out_info: a destination #GstAudioInfo
 * @config: (transfer full) (nullable): a #GstStructure with configuration options
 *
 * Create a new #GstImxASRCConverter that is able to convert between @in and @out
 * audio formats.
 *
 * @config contains extra configuration options, see `GST_AUDIO_CONVERTER_OPT_*`
 * parameters for details about the options and values.
 *
 * Returns: (nullable): a #GstImxASRCConverter or %NULL if conversion is not possible.
 */
GstImxASRCConverter *
gst_imxasrc_converter_new (GstImxASRCMethod method, GstAudioInfo * in_info,
    GstAudioInfo * out_info, GstStructure * config)
{
  GstImxASRCConverter *convert;

  g_return_val_if_fail (in_info != NULL, FALSE);
  g_return_val_if_fail (out_info != NULL, FALSE);

  convert = g_slice_new0 (GstImxASRCConverter);

  convert->in = *in_info;
  convert->out = *out_info;
  convert->channels = in_info->channels;
  convert->format = in_info->finfo->format;
  convert->method = method;

  convert->resampler =
    gst_imxasrc_resampler_new (method, 0, convert->format, convert->channels, in_info->rate,
                               out_info->rate, convert->config);
  if (!convert->resampler) {
    GST_ERROR ("converter create resampler failed");
    g_slice_free (GstImxASRCConverter, convert);
    return NULL;
  }

  return convert;
}

/**
 * gst_imxasrc_converter_free:
 * @convert: a #GstImxASRCConverter
 *
 * Free a previously allocated @convert instance.
 */
void
gst_imxasrc_converter_free (GstImxASRCConverter * convert)
{
  g_return_if_fail (convert != NULL);

  if (convert->resampler)
    gst_imxasrc_resampler_free (convert->resampler);
  gst_audio_info_init (&convert->in);
  gst_audio_info_init (&convert->out);

  g_slice_free (GstImxASRCConverter, convert);
}

gboolean
gst_imxasrc_converter_samples (GstImxASRCConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  if (in_frames == 0) {
    GST_LOG ("skipping empty buffer");
    return TRUE;
  }

  if (convert->resampler)
    if (gst_imxasrc_resampler_resample (convert->resampler, in, in_frames, out, out_frames))
      return FALSE;
  return TRUE;
}

gboolean
gst_imxasrc_converter_set_quality (GstImxASRCConverter * convert, IMXASRCResamplerQuality quality)
{
  if (convert->resampler)
    convert->resampler->quality = convert->quality;
  else
    return FALSE;

  return TRUE;
}
