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

#include <gst/gst.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sound/compress_offload.h>
#include "imxasrc-lib-hw.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static size_t cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    size_t cat_done;

    cat_done = (size_t) _gst_debug_category_new ("imxasrc_lib_hw", 0,
        "imxasrc_lib_hw object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

int imx_asrc_hw_open (ASRCHWConfig *asrc_hw)
{
  char path[64];
  int i;

  for (i = 0; i < 10; i++) {
    memset (path, 0, 64);
    sprintf (path, "/dev/snd/comprC%uD0", i);
    if (access (path, F_OK) == 0)
      break;
  }
  if (i == 10) {
    GST_ERROR ("ASRCHW: no asrc sound card found");
    return -1;
  }

  asrc_hw->fd = open (path, O_RDWR);

  if (asrc_hw->fd < 0)
  {
    GST_ERROR ("ASRCHW: failed to open ASRC");
    return -1;
  }

  return 0;
}

void imx_asrc_hw_close (ASRCHWConfig *asrc_hw)
{
  ioctl (asrc_hw->fd, SNDRV_COMPRESS_TASK_FREE, &asrc_hw->task.seqno);
  close (asrc_hw->fd);

  ring_buffer_destroy (&asrc_hw->ring_buffer);

  return;
}

/* Request one available ASRC CONTEXT/PAIR then configure it */
int imx_asrc_hw_config (ASRCHWConfig *asrc_hw)
{
  struct snd_compr_codec_caps codec_caps = {};
  struct snd_compr_caps caps;
  struct snd_compr_params params;
  int i, j;
  int err = 0;
  int block_size;
  int num_blocks;
  uint8_t *silence;
  size_t silence_size;

  if (!asrc_hw) {
    GST_ERROR ("ASRCHW: asrc is invalid");
    return -1;
  }

  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_GET_CAPS, &caps)) < 0) {
    GST_ERROR ("ASRCHW: get caps FAILED: %d", err);
    return err;
  }

  codec_caps.codec = SND_AUDIOCODEC_PCM;
  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_GET_CODEC_CAPS, &codec_caps)) < 0) {
    GST_ERROR ("ASRCHW: get codec caps FAILED: %d", err);
    return err;
  }

  for (i = 0; i < codec_caps.num_descriptors; i++) {
    if (codec_caps.descriptor[i].formats != asrc_hw->audio_info.input_format)
      continue;

    for (j = 0; j < codec_caps.descriptor[i].num_sample_rates; j++) {
      if (codec_caps.descriptor[i].sample_rates[j] == asrc_hw->audio_info.input_sample_rate)
        break;
    }

    if (j == codec_caps.descriptor[i].num_sample_rates)
      continue;

    if (asrc_hw->audio_info.output_sample_rate >= codec_caps.descriptor[i].src.out_sample_rate_min &&
        asrc_hw->audio_info.output_sample_rate <= codec_caps.descriptor[i].src.out_sample_rate_max)
      break;
  }

  if (i == codec_caps.num_descriptors) {
    GST_ERROR ("ASRCHW: caps don't support");
    return -1;
  }

  params.buffer.fragment_size = 4096;
  params.buffer.fragments = 1;
  params.codec.id = SND_AUDIOCODEC_PCM;
  params.codec.ch_in  = asrc_hw->audio_info.channels;
  params.codec.ch_out = asrc_hw->audio_info.channels;
  params.codec.format = asrc_hw->audio_info.input_format;
  params.codec.sample_rate = asrc_hw->audio_info.input_sample_rate;
  params.codec.pcm_format = asrc_hw->audio_info.output_format;
  params.codec.options.src_d.out_sample_rate = asrc_hw->audio_info.output_sample_rate;
  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_SET_PARAMS, &params)) < 0) {
    GST_ERROR ("ASRCHW: set params FAILED");
    return err;
  }

  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_TASK_CREATE, &asrc_hw->task)) < 0) {
    GST_ERROR("ASRCHW: task create FAILED %d", err);
    return err;
  }

  asrc_hw->status.seqno = asrc_hw->task.seqno;

  asrc_hw->bufin_start = mmap (NULL,
                               512 * 1024, /* set by the driver */
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED,
                               asrc_hw->task.input_fd,
                               0);
  if (asrc_hw->bufin_start == MAP_FAILED) {
    GST_ERROR ("ASRCHW: MMAP IN err");
    return -1;
  }
  /* empty capture buffer */
  memset (asrc_hw->bufin_start, 0, 512 * 1024);
  asrc_hw->bufout_start = mmap (NULL,
                                512 * 1024, /* set by the driver */
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                asrc_hw->task.output_fd,
                                0);
  if (asrc_hw->bufout_start == MAP_FAILED) {
    GST_ERROR ("ASRCHW: MMAP OUT err");
    return -1;
  }
  /* empty capture buffer */
  memset (asrc_hw->bufout_start, 0, 512 * 1024);

  /* create ring buffer and feed silence data */
  block_size = asrc_hw->audio_info.out_bps * asrc_hw->audio_info.channels;
  num_blocks = asrc_hw->audio_info.output_sample_rate * MAX_RING_BUFFER_TIME / 1000;

  err = ring_buffer_create (&asrc_hw->ring_buffer, block_size, num_blocks);
  if (err < 0) {
    GST_ERROR ("ASRCHW: ring_buffer_create failed");
    return err;
  }

  silence_size = asrc_hw->audio_info.output_sample_rate * SILENCE_RING_BUFFER_TIME / 1000 * block_size;
  silence = malloc (silence_size);
  memset (silence, 0, silence_size);
  ring_buffer_put (&asrc_hw->ring_buffer, silence_size / block_size, silence);

  free (silence);

  return err;
}

size_t imx_asrc_hw_get_out_frames (ASRCHWConfig *asrc_hw, size_t in_frames)
{
  return ring_buffer_avail (&asrc_hw->ring_buffer);
}

int imx_asrc_hw_resample (ASRCHWConfig *asrc_hw, gpointer in[],
    size_t in_frames, gpointer out[], size_t out_frames)
{
  size_t in_chunk, out_chunk;
  uint8_t *in_p = in[0];
  uint8_t *out_p = out[0];
  int err;
  int num_blocks_in;

  out_chunk = imx_asrc_hw_get_out_frames (asrc_hw, in_frames);
  if (out_frames < out_chunk) {
    GST_ERROR ("ASRCHW: no enough frames to get");
    return -1;
  }

  ring_buffer_get (&asrc_hw->ring_buffer, out_frames, out_p);

  in_chunk = in_frames * asrc_hw->audio_info.in_bps * asrc_hw->audio_info.channels;
  memcpy (asrc_hw->bufin_start, in_p, in_chunk);
  asrc_hw->task.input_size = in_chunk;

  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_TASK_START, &asrc_hw->task)) < 0) {
      GST_ERROR ("ASRCHW: task start FAILED");
      return err;
  }

  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_TASK_STOP, &asrc_hw->task.seqno)) < 0) {
    GST_ERROR ("ASRCHW: task stop FAILED");
    return err;
  }

  if ((err = ioctl (asrc_hw->fd, SNDRV_COMPRESS_TASK_STATUS, &asrc_hw->status)) < 0) {
    GST_ERROR ("ASRCHW: task status FAILED");
    return err;
  }

  num_blocks_in = asrc_hw->status.output_size / asrc_hw->ring_buffer.block_size;
  ring_buffer_put (&asrc_hw->ring_buffer, num_blocks_in, asrc_hw->bufout_start);
  GST_DEBUG ("asrc_hw convert in %ld frames, out %d frames", in_frames, num_blocks_in);

  return err;
}

static struct snd_compr_codec_caps *codec_caps = NULL;

gboolean imx_asrc_hw_is_exist (void) {
  char path[64];
  int fd, i, err = 0;

  for (i = 0; i < 10; i++) {
    memset (path, 0, 64);
    sprintf (path, "/dev/snd/comprC%uD0", i);
    if (access (path, F_OK) == 0)
      break;
  }
  if (i == 10) {
    GST_WARNING ("ASRCHW: no asrc sound card found");
    return FALSE;
  }

  fd = open (path, O_RDWR);
  if (fd < 0)
  {
    GST_ERROR ("ASRCHW: failed to open ASRC");
    return FALSE;
  }

  codec_caps = malloc(sizeof(struct snd_compr_codec_caps));
  codec_caps->codec = SND_AUDIOCODEC_PCM;
  if ((err = ioctl (fd, SNDRV_COMPRESS_GET_CODEC_CAPS, codec_caps)) < 0) {
    GST_ERROR ("ASRCHW: get codec caps FAILED: %d", err);
    free (codec_caps);
    return FALSE;
  }

  close (fd);
  return TRUE;
}

GList *imx_asrc_hw_get_supported_fmts (void)
{
  GList* list = NULL;
  GstAudioFormat audio_fmt;
  int i;

  if (!codec_caps)
    return NULL;

  for (i = 0; i < codec_caps->num_descriptors; i++) {
    audio_fmt = alsa_pcm_to_gst_format (codec_caps->descriptor[i].formats);
    list = g_list_append (list, GINT_TO_POINTER(audio_fmt));
  }

  return list;
}

GList *imx_asrc_hw_get_supported_rates (void)
{
  GList* list = NULL;
  int rate;
  int i;

  if (!codec_caps)
    return NULL;

  i = codec_caps->descriptor[0].num_sample_rates;
  for (i = i - 1; i >= 0; i--) {
    rate = codec_caps->descriptor[0].sample_rates[i];
    list = g_list_append (list, GINT_TO_POINTER(rate));
  }

  return list;
}

gint imx_asrc_hw_get_min_channels (void)
{
  return 1;
}

gint imx_asrc_hw_get_max_channels (void)
{
  if (!codec_caps)
    return 0;

	return codec_caps->descriptor[0].max_ch;
}
