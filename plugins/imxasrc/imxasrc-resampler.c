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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "imxasrc-resampler.h"

GST_DEBUG_CATEGORY_STATIC (imxasrc_resampler_debug);
#define GST_CAT_DEFAULT imxasrc_resampler_debug

static gint
gst_imxasrc_resampler_init (GstImxASRCResampler * resampler)
{
  static gsize init_gonce = 0;
  gint ret;

  if (g_once_init_enter (&init_gonce)) {

    GST_DEBUG_CATEGORY_INIT (imxasrc_resampler_debug, "imxasrc-resampler", 0,
        "imxasrc-resampler object");

    g_once_init_leave (&init_gonce, 1);
  }

  if (resampler->method == GST_IMXASRC_METHOD_HW) {
    resampler->asrc_hw = g_slice_new0 (ASRCHWConfig);
    ret = imx_asrc_hw_open (resampler->asrc_hw);
    if (ret) {
      GST_ERROR ("imx_asrc_hw_open failed");
      g_slice_free (ASRCHWConfig, resampler->asrc_hw);
      return ret;
    }
  } else if (resampler->method == GST_IMXASRC_METHOD_SSRC ||
             resampler->method == GST_IMXASRC_METHOD_DSPC) {
    resampler->asrc_sw = g_slice_new0 (ASRCSWConfig);
    resampler->asrc_sw->lib_type = LIB_SSRC;
    if (resampler->method == GST_IMXASRC_METHOD_DSPC)
      resampler->asrc_sw->lib_type = LIB_DSPC;
    ret = imx_asrc_sw_open (resampler->asrc_sw);
    if (ret) {
      GST_ERROR ("imx_asrc_sw_open failed");
      g_slice_free (ASRCHWConfig, resampler->asrc_hw);
      return ret;
    }
  }

  return ret;
}

static gint
gst_imxasrc_resampler_process (GstImxASRCResampler *resampler, gpointer in[],
    gsize in_frames, gpointer out[], gsize out_frames)
{
  gint ret;

  if (resampler->method == GST_IMXASRC_METHOD_HW) {
    ret = imx_asrc_hw_resample (resampler->asrc_hw, in, in_frames, out, out_frames);
    if (ret) {
      GST_ERROR ("imx_asrc_hw_resample failed");
      return ret;
    }
  } else if (resampler->method == GST_IMXASRC_METHOD_SSRC ||
             resampler->method == GST_IMXASRC_METHOD_DSPC) {
    ret = imx_asrc_sw_resample (resampler->asrc_sw, in, in_frames, out, out_frames);
    if (ret) {
      GST_ERROR ("imx_asrc_sw_resample failed");
      return ret;
    }
  }

  return ret;
}

static gint
gst_imxasrc_resampler_configure (GstImxASRCResampler *resampler, ASRCAudioInfo info)
{
  gint ret;

  if (resampler->method == GST_IMXASRC_METHOD_HW) {
    resampler->asrc_hw->audio_info = info;
    ret = imx_asrc_hw_config (resampler->asrc_hw);
    if (ret) {
      GST_ERROR ("imx_asrc_hw_config failed");
      return ret;
    }
  } else if (resampler->method == GST_IMXASRC_METHOD_SSRC ||
             resampler->method == GST_IMXASRC_METHOD_DSPC) {
    resampler->asrc_sw->audio_info = info;
    resampler->asrc_sw->quality = resampler->quality;
    ret = imx_asrc_sw_config (resampler->asrc_sw);
    if (ret) {
      if (info.input_sample_rate == info.output_sample_rate) {
        return 0;
      } else {
        GST_ERROR ("imx_asrc_sw_config failed");
        return ret;
      }
    }
  }

  resampler->resample = gst_imxasrc_resampler_process;
  return ret;
}

/**
 * gst_imxasrc_resampler_get_out_frames:
 * @resampler: a #GstImxASRCResampler
 * @in_frames: number of input frames
 *
 * Get the number of output frames that would be currently available when
 * @in_frames are given to @resampler.
 *
 * Returns: The number of frames that would be available after giving
 * @in_frames as input to @resampler.
 */

gsize
gst_imxasrc_resampler_get_out_frames (GstImxASRCResampler *resampler,
                                      gsize in_frames)
{
  gsize out = 0;

  GST_DEBUG("gst_imxasrc_resampler_get_out_frames");
  g_return_val_if_fail (resampler != NULL, 0);

  if (in_frames) {
    if (resampler->method == GST_IMXASRC_METHOD_HW)
      out = imx_asrc_hw_get_out_frames (resampler->asrc_hw, in_frames);
    else if (resampler->method == GST_IMXASRC_METHOD_SSRC ||
             resampler->method == GST_IMXASRC_METHOD_DSPC)
      out = imx_asrc_sw_get_out_frames (resampler->asrc_sw, in_frames);
  } else
    return 0;

  return out;
}

/**
 * gst_audio_resampler_reset:
 * @resampler: a #GstImxASRCResampler
 *
 * Reset @resampler to the state it was when it was first created, discarding
 * all sample history.
 */
void
gst_imxasrc_resampler_reset (GstImxASRCResampler * resampler)
{
  g_return_if_fail (resampler != NULL);

  if (resampler->samples) {
    gsize bytes;
    gint c, blocks, bpf;

    bpf = resampler->in_bps * resampler->inc;
    bytes = (resampler->n_taps / 2) * bpf;
    blocks = resampler->blocks;

    for (c = 0; c < blocks; c++)
      memset (resampler->sbuf[c], 0, bytes);
  }
  /* half of the filter is filled with 0 */
  resampler->samp_index = 0;
  resampler->samples_avail = resampler->n_taps / 2 - 1;
}

/**
 * gst_audio_resampler_update:
 * @resampler: a #GstImxASRCResampler
 * @in_rate: new input rate
 * @out_rate: new output rate
 * @options: new options or %NULL
 *
 * Update the resampler parameters for @resampler. This function should
 * not be called concurrently with any other function on @resampler.
 *
 * When @in_rate or @out_rate is 0, its value is unchanged.
 *
 * When @options is %NULL, the previously configured options are reused.
 *
 * Returns: %TRUE if the new parameters could be set
 */
gboolean
gst_imxasrc_resampler_update (GstImxASRCResampler * resampler,
    gint in_rate, gint out_rate, GstStructure * options)
{
  gint gcd, samp_phase;
  gint ret;

  g_return_val_if_fail (resampler != NULL, FALSE);

  if (in_rate <= 0)
    in_rate = resampler->in_rate;
  if (out_rate <= 0)
    out_rate = resampler->out_rate;

  if (resampler->out_rate > 0) {
    GST_INFO ("old phase %d/%d", resampler->samp_phase, resampler->out_rate);
    samp_phase =
        gst_util_uint64_scale_int (resampler->samp_phase, out_rate,
        resampler->out_rate);
  } else
    samp_phase = 0;

  gcd = gst_util_greatest_common_divisor (in_rate, out_rate);

  GST_INFO ("phase %d out_rate %d, in_rate %d, gcd %d", samp_phase, out_rate,
      in_rate, gcd);

  resampler->samp_phase = samp_phase /= gcd;
  resampler->in_rate = in_rate / gcd;
  resampler->out_rate = out_rate / gcd;

  GST_INFO ("new phase %d/%d", resampler->samp_phase, resampler->out_rate);

  resampler->samp_inc = in_rate / out_rate;
  resampler->samp_frac = in_rate % out_rate;

  GST_DEBUG ("in_rate %d, out_rate %d, %d, %d", in_rate, out_rate, resampler->in_rate, resampler->out_rate);

  ASRCAudioInfo audio_info;
  audio_info.channels = resampler->channels;
  audio_info.input_sample_rate = in_rate;
  audio_info.output_sample_rate = out_rate;

  audio_info.in_bps = resampler->in_bps;
  audio_info.out_bps = resampler->out_bps;
  audio_info.input_format = gst_to_alsa_pcm_format (resampler->in_format);
  audio_info.output_format = gst_to_alsa_pcm_format (resampler->out_format);

  ret = gst_imxasrc_resampler_configure (resampler, audio_info);
  if (ret < 0) {
    GST_ERROR ("gst_imxasrc_resampler_configure failed");
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_imxasrc_resampler_new:
 * @method: a #GstImxASRCMethod
 * @flags: #GstAudioResamplerFlags
 * @format: the #GstAudioFormat
 * @channels: the number of channels
 * @in_rate: input rate
 * @out_rate: output rate
 * @options: extra options
 *
 * Make a new resampler.
 *
 * Returns: (skip) (transfer full): The new #GstAudioResampler.
 */
GstImxASRCResampler *
gst_imxasrc_resampler_new (GstImxASRCMethod method,
    GstAudioResamplerFlags flags,
    GstAudioFormat format, gint channels,
    gint in_rate, gint out_rate, GstStructure * options)
{
  GstImxASRCResampler *resampler;
  const GstAudioFormatInfo *info;

  g_return_val_if_fail (
      format == GST_AUDIO_FORMAT_S16LE || format == GST_AUDIO_FORMAT_U16LE ||
      format == GST_AUDIO_FORMAT_S24_32LE || format == GST_AUDIO_FORMAT_S24LE ||
      format == GST_AUDIO_FORMAT_U24LE || format == GST_AUDIO_FORMAT_U24LE ||
      format == GST_AUDIO_FORMAT_S32LE || format == GST_AUDIO_FORMAT_U32LE ||
      format == GST_AUDIO_FORMAT_S20LE || format == GST_AUDIO_FORMAT_S20LE ||
      format == GST_AUDIO_FORMAT_F32LE, NULL);
  g_return_val_if_fail (channels > 0, NULL);
  g_return_val_if_fail (in_rate > 0, NULL);
  g_return_val_if_fail (out_rate > 0, NULL);

  resampler = g_slice_new0 (GstImxASRCResampler);

  resampler->in_format = format;
  resampler->out_format = format;
  resampler->channels = channels;

  info = gst_audio_format_get_info (format);
  resampler->in_bps = GST_AUDIO_FORMAT_INFO_WIDTH (info) / 8;
  resampler->out_bps = resampler->in_bps;
  resampler->sbuf = g_malloc0 (sizeof (gpointer) * channels);
  resampler->method = method;

  if (gst_imxasrc_resampler_init (resampler)) {
    GST_ERROR ("gst_imxasrc_resampler_init failed");
    goto fail;
  }

  if (!gst_imxasrc_resampler_update (resampler, in_rate, out_rate, NULL)) {
    GST_ERROR ("gst_imxasrc_resampler_update failed");
    goto fail;
  }

  gst_imxasrc_resampler_reset (resampler);
  return resampler;

fail:
    g_slice_free (GstImxASRCResampler, resampler);
    return NULL;
}

/**
 * gst_imxasrc_resampler_free:
 * @resampler: a #GstAudioResampler
 *
 * Free a previously allocated #GstAudioResampler @resampler.
 */
void
gst_imxasrc_resampler_free (GstImxASRCResampler * resampler)
{
  g_return_if_fail (resampler != NULL);

  if (resampler->method == GST_IMXASRC_METHOD_HW) {
    imx_asrc_hw_close (resampler->asrc_hw);
    g_slice_free (ASRCHWConfig, resampler->asrc_hw);
  } else if (resampler->method == GST_IMXASRC_METHOD_SSRC||
             resampler->method == GST_IMXASRC_METHOD_DSPC) {
    imx_asrc_sw_close (resampler->asrc_sw);
    g_slice_free (ASRCSWConfig, resampler->asrc_sw);
  }
  g_free (resampler->samples);
  g_free (resampler->sbuf);
  if (resampler->options)
    gst_structure_free (resampler->options);
  g_slice_free (GstImxASRCResampler, resampler);
}

/**
 * gst_imxasrc_resampler_resample:
 * @resampler: a #GstImxASRCResampler
 * @in: input samples
 * @in_frames: number of input frames
 * @out: output samples
 * @out_frames: number of output frames
 *
 * Perform resampling on @in_frames frames in @in and write @out_frames to @out.
 *
 * In case the samples are interleaved, @in and @out must point to an
 * array with a single element pointing to a block of interleaved samples.
 *
 * If non-interleaved samples are used, @in and @out must point to an
 * array with pointers to memory blocks, one for each channel.
 *
 * @in may be %NULL, in which case @in_frames of silence samples are pushed
 * into the resampler.
 *
 * This function always produces @out_frames of output and consumes @in_frames of
 * input. Use gst_imxasrc_resampler_get_out_frames() and
 * gst_imxasrc_resampler_get_in_frames() to make sure @in_frames and @out_frames
 * are matching and @in and @out point to enough memory.
 */
gint
gst_imxasrc_resampler_resample (GstImxASRCResampler * resampler,
    gpointer in[], gsize in_frames, gpointer out[], gsize out_frames)
{
  gint ret;

  ret = resampler->resample (resampler, in, in_frames, out, out_frames);

  return ret;
}
