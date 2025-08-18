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
/**
 * SECTION:element-gstimxasrc
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=audio8k32b2c_sle.pcm ! audio/x-raw, rate=8000,
 * format=S32LE, channels=2, layout=interleaved ! imxasrc ! audio/x-raw, rate=16000,
 * format=S32LE, channels=2, layout=interleaved  ! filesink location=audio16k32b2c_sle.pcm
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gstutils.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasetransform.h>
#include "gstimxasrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_imxasrc_debug_category);
#define GST_CAT_DEFAULT gst_imxasrc_debug_category

#define DEFAULT_IMXASRC_RESAMPLE_QUALITY GST_IMXASRC_RESAMPLER_QUALITY_0
#define GST_IMX_ASRC_PARAMS_QDATA g_quark_from_static_string("imxasrc-params")

/* prototypes */


static void gst_imxasrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_imxasrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_imxasrc_dispose (GObject * object);
static void gst_imxasrc_finalize (GObject * object);

static GstCaps *gst_imxasrc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_imxasrc_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_imxasrc_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_imxasrc_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static gboolean gst_imxasrc_start (GstBaseTransform * trans);
static gboolean gst_imxasrc_stop (GstBaseTransform * trans);
static gboolean gst_imxasrc_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_imxasrc_src_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_imxasrc_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

enum
{
  PROP_0,
  PROP_QUALITY,
};

static GstElementClass *gst_imxasrc_parent_class = NULL;

GType
gstimxasrc_get_resample_quality (GstImxASRCMethod method) {
  static GType gst_imxasrc_resample_quality = 0;

  if (!gst_imxasrc_resample_quality) {
    if (method == GST_IMXASRC_METHOD_SSRC) {
      static GEnumValue ssrc_resample_quality[] = {
        {GST_IMXASRC_RESAMPLER_QUALITY_0, "ssrc quality low", "low"},
        {GST_IMXASRC_RESAMPLER_QUALITY_1, "ssrc quality medium", "medium"},
        {GST_IMXASRC_RESAMPLER_QUALITY_2, "ssrc quality high", "high"},
        {0, NULL, NULL},
      };

      gst_imxasrc_resample_quality =
        g_enum_register_static ("GstImxASRCResampleQuality", ssrc_resample_quality);
    } else if (method == GST_IMXASRC_METHOD_DSPC) {
      static GEnumValue dspc_resample_quality[] = {
        {GST_IMXASRC_RESAMPLER_QUALITY_0, "dspc quality low", "low"},
        {GST_IMXASRC_RESAMPLER_QUALITY_1, "dspc quality high", "high"},
        {0, NULL, NULL},
      };

      gst_imxasrc_resample_quality =
        g_enum_register_static ("GstImxASRCResampleQuality", dspc_resample_quality);
    }
  }

  return gst_imxasrc_resample_quality;
}

static GstCaps*
gst_imx_asrc_caps_from_device (ImxASRCDeviceInfo *in_plugin)
{
  GstCaps *caps = NULL;
  GValue values = { 0, };
  GValue value = { 0, };
  GList *list = NULL;
  gint channels_min, channels_max, i;

  caps = gst_caps_new_empty_simple ("audio/x-raw");

  g_value_init (&values, GST_TYPE_LIST);
  g_value_init (&value, G_TYPE_STRING);

  list = in_plugin->get_supported_fmts ();
  for (i = 0; i < g_list_length (list); i++) {
    GstAudioFormat audio_format = (GstAudioFormat)g_list_nth_data(list, i);
    g_value_set_static_string (&value, (const gchar *)gst_audio_format_to_string(audio_format));
    gst_value_list_prepend_value (&values, &value);
  }

  gst_caps_set_value (caps, "format", &values);

  g_value_unset (&value);
  g_value_unset (&values);

  g_value_init (&values, GST_TYPE_LIST);
  g_value_init (&value, G_TYPE_INT);

  list = in_plugin->get_supported_rates ();
  for (i = 0; i < g_list_length (list); i++) {
    g_value_set_int (&value, GPOINTER_TO_INT(g_list_nth_data(list, i)));
    gst_value_list_prepend_value (&values, &value);
  }

  gst_caps_set_value (caps, "rate", &values);

  g_value_unset (&value);
  g_value_unset (&values);

  g_list_free(list);

  channels_min = in_plugin->get_min_channels ();
  channels_max = in_plugin->get_max_channels ();
  if (channels_min == channels_max)
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, channels_min, NULL);
  else
    gst_caps_set_simple (caps, "channels", GST_TYPE_INT_RANGE, channels_min,
        channels_max, NULL);

  gst_caps_set_simple(caps, "layout", G_TYPE_STRING, "interleaved", NULL);

  return caps;
}

static void
gst_imxasrc_class_init (GstImxASRCClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCaps *caps;

  ImxASRCDeviceInfo *in_plugin = (ImxASRCDeviceInfo *)
      g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass), GST_IMX_ASRC_PARAMS_QDATA);

  gchar longname[64] = {0};
  snprintf(longname, 64, "i.MX ASRC with %s", in_plugin->name);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      longname, "Filter/Converter/Audio", "Resamples audio",
      "Chancel Liu <chancel.liu@nxp.com>");

  caps = gst_imx_asrc_caps_from_device(in_plugin);
  if (!caps) {
    GST_ERROR ("Couldn't create caps for device '%s'", in_plugin->name);
    caps = gst_caps_new_empty_simple ("audio/x-raw");
  }

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
    gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
    gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));

  gst_imxasrc_parent_class = g_type_class_peek_parent (klass);
  klass->method = in_plugin->method;
  gobject_class->set_property = gst_imxasrc_set_property;
  gobject_class->get_property = gst_imxasrc_get_property;

  if (in_plugin->method == GST_IMXASRC_METHOD_SSRC ||
      in_plugin->method == GST_IMXASRC_METHOD_DSPC) {
    g_object_class_install_property (gobject_class, PROP_QUALITY,
        g_param_spec_enum ("quality", "Resample quality",
            "What quality for resample to use",
            gstimxasrc_get_resample_quality (in_plugin->method),
            DEFAULT_IMXASRC_RESAMPLE_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  gobject_class->dispose = gst_imxasrc_dispose;
  gobject_class->finalize = gst_imxasrc_finalize;
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_imxasrc_transform_caps);
  base_transform_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_imxasrc_fixate_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_imxasrc_set_caps);
  base_transform_class->transform_size = GST_DEBUG_FUNCPTR (gst_imxasrc_transform_size);
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_imxasrc_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_imxasrc_stop);
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR (gst_imxasrc_sink_event);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_imxasrc_src_event);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_imxasrc_transform);
  base_transform_class->passthrough_on_same_caps = TRUE;
}

static void
gst_imxasrc_init (GstImxASRC *imxasrc)
{
  GstImxASRCClass *klass = (GstImxASRCClass *)G_OBJECT_GET_CLASS (imxasrc);
  imxasrc->method = klass->method;
}

void
gst_imxasrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImxASRC *imxasrc = GST_IMXASRC (object);

  GST_DEBUG_OBJECT (imxasrc, "set_property");

  switch (property_id) {
    case PROP_QUALITY:
      imxasrc->quality = g_value_get_enum (value);
      GST_DEBUG_OBJECT (imxasrc, "imxasrc quality %d", imxasrc->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_imxasrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstImxASRC *imxasrc = GST_IMXASRC (object);

  GST_DEBUG_OBJECT (imxasrc, "get_property");

  switch (property_id) {
    case PROP_QUALITY:
      g_value_set_enum (value, imxasrc->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_imxasrc_dispose (GObject * object)
{
  GstImxASRC *imxasrc = GST_IMXASRC (object);

  GST_DEBUG_OBJECT (imxasrc, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_imxasrc_parent_class)->dispose (object);
}

void
gst_imxasrc_finalize (GObject * object)
{
  GstImxASRC *imxasrc = GST_IMXASRC (object);

  GST_DEBUG_OBJECT (imxasrc, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_imxasrc_parent_class)->finalize (object);
}

static GstCaps *
gst_imxasrc_transform_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);
  const GValue *val;
  GstStructure *s;
  GstCaps *res;
  gint i, n;

  GST_DEBUG_OBJECT (imxasrc, "transform_caps");

  /* transform single caps into input_caps + input_caps with the rate
   * field set to our supported range. This ensures that upstream knows
   * about downstream's preferred rate(s) and can negotiate accordingly. */
  res = gst_caps_new_empty ();
  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    s = gst_caps_get_structure (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure (res, s))
      continue;

    /* first, however, check if the caps contain a range for the rate field, in
     * which case that side isn't going to care much about the exact sample rate
     * chosen and we should just assume things will get fixated to something sane
     * and we may just as well offer our full range instead of the range in the
     * caps. If the rate is not an int range value, it's likely to express a
     * real preference or limitation and we should maintain that structure as
     * preference by putting it first into the transformed caps, and only add
     * our full rate range as second option  */
    s = gst_structure_copy (s);
    val = gst_structure_get_value (s, "rate");
    if (val == NULL || GST_VALUE_HOLDS_INT_RANGE (val)) {
      /* overwrite existing range, or add field if it doesn't exist yet */
      gst_structure_set (s, "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    } else {
      /* append caps with full range to existing caps with non-range rate field */
      gst_caps_append_structure (res, gst_structure_copy (s));
      gst_structure_set (s, "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    }
    gst_caps_append_structure (res, s);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, res, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (res);
    res = intersection;
  }

  return res;
}

static GstCaps *
gst_imxasrc_fixate_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);
  GstStructure *s;
  gint rate;

  GST_DEBUG_OBJECT (imxasrc, "fixate_caps");

  s = gst_caps_get_structure (caps, 0);
  if (G_UNLIKELY (!gst_structure_get_int (s, "rate", &rate)))
    return othercaps;

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);
  s = gst_caps_get_structure (othercaps, 0);
  gst_structure_fixate_field_nearest_int (s, "rate", rate);

  return gst_caps_fixate (othercaps);
}

static void
gst_imxasrc_reset_state (GstImxASRC * imxasrc)
{
  if (imxasrc->converter)
    gst_imxasrc_converter_reset (imxasrc->converter);
}

static gboolean
gst_imxasrc_update_state (GstImxASRC * imxasrc, GstAudioInfo * in,
    GstAudioInfo * out)
{
  GstStructure *options = NULL;

  if (imxasrc->converter == NULL && in == NULL && out == NULL)
    return TRUE;

  if (in != NULL && (in->finfo != imxasrc->in.finfo ||
          in->channels != imxasrc->in.channels ||
          in->layout != imxasrc->in.layout) && imxasrc->converter) {
    gst_imxasrc_converter_free (imxasrc->converter);
    imxasrc->converter = NULL;
  }
  if (imxasrc->converter == NULL) {
    imxasrc->converter = gst_imxasrc_converter_new (imxasrc->method,
                                                    in, out, options);
    if (imxasrc->converter == NULL)
      goto resampler_failed;

    if (imxasrc->method == GST_IMXASRC_METHOD_SSRC ||
        imxasrc->method == GST_IMXASRC_METHOD_DSPC)
      gst_imxasrc_converter_set_quality (imxasrc->converter, imxasrc->quality);
  } else if (in && out) {
    gboolean ret;

    ret =
      gst_imxasrc_converter_update_config (imxasrc->converter, in->rate,
                                           out->rate, options);
    if (!ret)
      goto update_failed;
  } else {
    gst_structure_free (options);
  }

  return TRUE;

  /* ERRORS */
resampler_failed:
  {
    GST_ERROR_OBJECT (imxasrc, "failed to create resampler");
    return FALSE;
  }
update_failed:
  {
    GST_ERROR_OBJECT (imxasrc, "failed to update resampler");
    return FALSE;
  }
}

static gboolean
gst_imxasrc_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);
  GstAudioInfo in, out;

  GST_LOG ("incaps %" GST_PTR_FORMAT ", outcaps %"
      GST_PTR_FORMAT, incaps, outcaps);

  if (!gst_audio_info_from_caps (&in, incaps))
    goto invalid_incaps;
  if (!gst_audio_info_from_caps (&out, outcaps))
    goto invalid_outcaps;

  /* Reset timestamp tracking and drain the resampler if the audio format is
   * changing. Especially when changing the sample rate our timestamp tracking
   * will be completely off, but even otherwise we would usually lose the last
   * few samples if we don't drain here */
  if (!gst_audio_info_is_equal (&in, &imxasrc->in) ||
      !gst_audio_info_is_equal (&out, &imxasrc->out)) {

    gst_imxasrc_reset_state (imxasrc);
    imxasrc->num_gap_samples = 0;
    imxasrc->num_nongap_samples = 0;
    imxasrc->t0 = GST_CLOCK_TIME_NONE;
    imxasrc->in_offset0 = GST_BUFFER_OFFSET_NONE;
    imxasrc->out_offset0 = GST_BUFFER_OFFSET_NONE;
    imxasrc->samples_in = 0;
    imxasrc->samples_out = 0;
    imxasrc->need_discont = TRUE;
  }

  if (!gst_imxasrc_update_state (imxasrc, &in, &out))
    goto invalid_configs;

  imxasrc->in = in;
  imxasrc->out = out;

  return TRUE;

  /* ERROR */
invalid_incaps:
  {
    GST_ERROR_OBJECT (trans, "invalid incaps");
    return FALSE;
  }
invalid_outcaps:
  {
    GST_ERROR_OBJECT (trans, "invalid outcaps");
    return FALSE;
  }
invalid_configs:
  {
    GST_ERROR_OBJECT (trans, "invalid configs");
    return FALSE;
  }
}

/* transform size */
static gboolean
gst_imxasrc_transform_size (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, gsize size, GstCaps * othercaps, gsize * othersize)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);
  gboolean ret = TRUE;
  gint bpf;

  GST_LOG_OBJECT (trans, "asked to transform size %" G_GSIZE_FORMAT
      " in direction %s", size, direction == GST_PAD_SINK ? "SINK" : "SRC");

  /* Number of samples in either buffer is size / (width*channels) ->
   * calculate the factor */
  bpf = GST_AUDIO_INFO_BPF (&imxasrc->in);

  /* Convert source buffer size to samples */
  size /= bpf;

  if (direction == GST_PAD_SINK) {
    /* asked to convert size of an incoming buffer */
    *othersize = gst_imxasrc_converter_get_out_frames (imxasrc->converter, size);
    *othersize *= bpf;
  } else {
    /* asked to convert size of an outgoing buffer */
    *othersize = 0;
    *othersize *= bpf;
  }

  GST_LOG_OBJECT (trans,
      "transformed size %" G_GSIZE_FORMAT " to %" G_GSIZE_FORMAT,
      size * bpf, *othersize);

  return ret;
}

/* states */
static gboolean
gst_imxasrc_start (GstBaseTransform * trans)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);

  GST_DEBUG_OBJECT (imxasrc, "start");

  return TRUE;
}

static gboolean
gst_imxasrc_stop (GstBaseTransform * trans)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);

  GST_DEBUG_OBJECT (imxasrc, "stop");

  return TRUE;
}

/* sink and src pad event handlers */
static gboolean
gst_imxasrc_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);

  GST_DEBUG_OBJECT (imxasrc, "sink_event");

  return GST_BASE_TRANSFORM_CLASS (gst_imxasrc_parent_class)->sink_event (
      trans, event);
}

static gboolean
gst_imxasrc_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);

  GST_DEBUG_OBJECT (imxasrc, "src_event");

  return GST_BASE_TRANSFORM_CLASS (gst_imxasrc_parent_class)->src_event (
      trans, event);
}

static gboolean
gst_imxasrc_check_discont (GstImxASRC * resample, GstBuffer * buf)
{
  guint64 offset;
  guint64 delta;

  /* is the incoming buffer a discontinuity? */
  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (buf)))
    return TRUE;

  /* no valid timestamps or offsets to compare --> no discontinuity */
  if (G_UNLIKELY (!(GST_BUFFER_TIMESTAMP_IS_VALID (buf) &&
              GST_CLOCK_TIME_IS_VALID (resample->t0))))
    return FALSE;

  /* convert the inbound timestamp to an offset. */
  offset =
      gst_util_uint64_scale_int_round (GST_BUFFER_TIMESTAMP (buf) -
      resample->t0, resample->in.rate, GST_SECOND);

  /* many elements generate imperfect streams due to rounding errors, so we
   * permit a small error (up to one sample) without triggering a filter
   * flush/restart (if triggered incorrectly, this will be audible) */
  /* allow even up to more samples, since sink is not so strict anyway,
   * so give that one a chance to handle this as configured */
  delta = ABS ((gint64) (offset - resample->samples_in));
  if (delta <= (resample->in.rate >> 5))
    return FALSE;

  GST_WARNING_OBJECT (resample,
      "encountered timestamp discontinuity of %" G_GUINT64_FORMAT " samples = %"
      GST_TIME_FORMAT, delta,
      GST_TIME_ARGS (gst_util_uint64_scale_int_round (delta, GST_SECOND,
              resample->in.rate)));
  return TRUE;
}

static GstFlowReturn
gst_imxasrc_process (GstImxASRC * resample, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstAudioBuffer srcabuf, dstabuf;
  gsize outsize;
  gsize in_len;
  gsize out_len;
  gboolean inbuf_writable;
  gboolean ret;

  inbuf_writable = gst_buffer_is_writable (inbuf)
      && gst_buffer_n_memory (inbuf) == 1
      && gst_memory_is_writable (gst_buffer_peek_memory (inbuf, 0));

  gst_audio_buffer_map (&srcabuf, &resample->in, inbuf,
      inbuf_writable ? GST_MAP_READWRITE : GST_MAP_READ);

  in_len = srcabuf.n_samples;
  out_len = gst_imxasrc_converter_get_out_frames (resample->converter, in_len);

  GST_DEBUG_OBJECT (resample, "in %" G_GSIZE_FORMAT " frames, out %"
      G_GSIZE_FORMAT " frames", in_len, out_len);

  /* ensure that the output buffer is not bigger than what we need */
  gst_buffer_set_size (outbuf, out_len * resample->in.bpf);

  if (GST_AUDIO_INFO_LAYOUT (&resample->out) ==
      GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    gst_buffer_add_audio_meta (outbuf, &resample->out, out_len, NULL);
  }

  gst_audio_buffer_map (&dstabuf, &resample->out, outbuf, GST_MAP_WRITE);

  ret = gst_imxasrc_converter_samples (resample->converter, 0, srcabuf.planes,
    in_len, dstabuf.planes, out_len);

  /* time */
  if (GST_CLOCK_TIME_IS_VALID (resample->t0)) {
    GST_BUFFER_TIMESTAMP (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out, GST_SECOND,
        resample->out.rate);
    GST_BUFFER_DURATION (outbuf) = resample->t0 +
        gst_util_uint64_scale_int_round (resample->samples_out + out_len,
        GST_SECOND, resample->out.rate) - GST_BUFFER_TIMESTAMP (outbuf);
  } else {
    GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  }
  /* offset */
  if (resample->out_offset0 != GST_BUFFER_OFFSET_NONE) {
    GST_BUFFER_OFFSET (outbuf) = resample->out_offset0 + resample->samples_out;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET (outbuf) + out_len;
  } else {
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_NONE;
  }
  /* move along */
  resample->samples_out += out_len;
  resample->samples_in += in_len;

  gst_audio_buffer_unmap (&srcabuf);
  gst_audio_buffer_unmap (&dstabuf);

  outsize = out_len * resample->in.bpf;

  GST_LOG_OBJECT (resample,
      "Converted to buffer of %" G_GSIZE_FORMAT
      " samples (%" G_GSIZE_FORMAT " bytes) with timestamp %" GST_TIME_FORMAT
      ", duration %" GST_TIME_FORMAT ", offset %" G_GUINT64_FORMAT
      ", offset_end %" G_GUINT64_FORMAT, out_len, outsize,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
      GST_BUFFER_OFFSET (outbuf), GST_BUFFER_OFFSET_END (outbuf));

  if (!ret)
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  else
    return GST_FLOW_OK;
}

/* transform */
static GstFlowReturn
gst_imxasrc_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstImxASRC *imxasrc = GST_IMXASRC (trans);
  GstFlowReturn ret;

  GST_LOG_OBJECT (imxasrc, "transforming buffer of %" G_GSIZE_FORMAT " bytes,"
      " ts %" GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT ", offset %"
      G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      gst_buffer_get_size (inbuf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (inbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (inbuf)),
      GST_BUFFER_OFFSET (inbuf), GST_BUFFER_OFFSET_END (inbuf));

  /* check for timestamp discontinuities;  flush/reset if needed, and set
   * flag to resync timestamp and offset counters and send event
   * downstream */
  if (G_UNLIKELY (gst_imxasrc_check_discont (imxasrc, inbuf))) {
    gst_imxasrc_reset_state (imxasrc);
    imxasrc->need_discont = TRUE;
  }

  /* handle discontinuity */
  if (G_UNLIKELY (imxasrc->need_discont)) {
    imxasrc->num_gap_samples = 0;
    imxasrc->num_nongap_samples = 0;
    /* reset */
    imxasrc->samples_in = 0;
    imxasrc->samples_out = 0;
    GST_DEBUG_OBJECT (imxasrc, "found discontinuity; resyncing");
    /* resync the timestamp and offset counters if possible */
    if (GST_BUFFER_TIMESTAMP_IS_VALID (inbuf)) {
      imxasrc->t0 = GST_BUFFER_TIMESTAMP (inbuf);
    } else {
      GST_DEBUG_OBJECT (imxasrc, "... but new timestamp is invalid");
      imxasrc->t0 = GST_CLOCK_TIME_NONE;
    }
    if (GST_BUFFER_OFFSET_IS_VALID (inbuf)) {
      imxasrc->in_offset0 = GST_BUFFER_OFFSET (inbuf);
      imxasrc->out_offset0 =
          gst_util_uint64_scale_int_round (imxasrc->in_offset0,
          imxasrc->out.rate, imxasrc->in.rate);
    } else {
      GST_DEBUG_OBJECT (imxasrc, "... but new offset is invalid");
      imxasrc->in_offset0 = GST_BUFFER_OFFSET_NONE;
      imxasrc->out_offset0 = GST_BUFFER_OFFSET_NONE;
    }
    /* set DISCONT flag on output buffer */
    GST_DEBUG_OBJECT (imxasrc, "marking this buffer with the DISCONT flag");
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    imxasrc->need_discont = FALSE;
  }

  ret = gst_imxasrc_process (imxasrc, inbuf, outbuf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    return ret;

  GST_DEBUG_OBJECT (imxasrc, "input = samples [%" G_GUINT64_FORMAT ", %"
      G_GUINT64_FORMAT ") = [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT
      ") ns;  output = samples [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT
      ") = [%" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ") ns",
      GST_BUFFER_OFFSET (inbuf), GST_BUFFER_OFFSET_END (inbuf),
      GST_BUFFER_TIMESTAMP (inbuf), GST_BUFFER_TIMESTAMP (inbuf) +
      GST_BUFFER_DURATION (inbuf), GST_BUFFER_OFFSET (outbuf),
      GST_BUFFER_OFFSET_END (outbuf), GST_BUFFER_TIMESTAMP (outbuf),
      GST_BUFFER_TIMESTAMP (outbuf) + GST_BUFFER_DURATION (outbuf));

  return GST_FLOW_OK;
}

static gboolean gst_imx_asrc_register (GstPlugin * plugin)
{
  GTypeInfo tinfo = {
    sizeof (GstImxASRCClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_imxasrc_class_init,
    NULL,
    NULL,
    sizeof (GstImxASRC),
    0,
    (GInstanceInitFunc) gst_imxasrc_init,
  };

  GType type;
  gchar *t_name;

  const ImxASRCDeviceInfo *in_plugin = imx_get_asrc_devices();
  while (in_plugin->name) {
    GST_LOG ("Registering %s asrc", in_plugin->name);
    if (!in_plugin->is_exist()) {
      GST_WARNING("device %s not exist", in_plugin->name);
      in_plugin++;
      continue;
    }

    t_name = g_strdup_printf ("imxasrc_%s", in_plugin->name);
    type = g_type_from_name (t_name);

    if (!type) {
      type = g_type_register_static (GST_TYPE_BASE_TRANSFORM, t_name, &tinfo, 0);
      g_type_set_qdata (type, GST_IMX_ASRC_PARAMS_QDATA, (gpointer) in_plugin);
    }

    if (!gst_element_register (plugin, t_name, IMX_GST_PLUGIN_RANK, type)) {
      GST_ERROR ("Failed to register %s", t_name);
      g_free (t_name);
      return FALSE;
    }
    g_free (t_name);

    in_plugin++;
  }

  return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_imxasrc_debug_category, "imxasrc", 0,
      "i.MX Audio Sample Rate Converter element");

  return gst_imx_asrc_register (plugin);
}

IMX_GST_PLUGIN_DEFINE (imxasrc, "i.MX Audio Sample Rate Converter Plugins", plugin_init);
