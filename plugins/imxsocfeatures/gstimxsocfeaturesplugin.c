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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* 
 * GStreamer plugin wrapper for i.MX SoC features library.
 *
 * Usage from other repos (no build dependency needed):
 *
 *   GstElement *soc = gst_element_factory_make("imxsocfeatures", NULL);
 *   if (soc) {
 *     gchar *chip;
 *     gboolean has_g2d;
 *
 *     g_object_get(soc, "chip-code", &chip, NULL);
 *     g_signal_emit_by_name(soc, "has-feature", "g2d-gpu", &has_g2d);
 *
 *     g_free(chip);
 *     gst_object_unref(soc);
 *   }
 */

#include "gstimxsocfeaturesplugin.h"
#include "gstimxsocfeatures.h"
#include "gstimxplugins.h"

GST_DEBUG_CATEGORY_STATIC(gst_imx_soc_features_plugin_debug);
#define GST_CAT_DEFAULT gst_imx_soc_features_plugin_debug

/* Properties */
enum
{
  PROP_0,
  PROP_SOC_ID,
  PROP_CHIP_CODE,
};

/* Signals */
enum
{
  SIGNAL_HAS_FEATURE,
  SIGNAL_IS_CHIP,
  SIGNAL_IN_GROUP,
  SIGNAL_GET_FEATURE_DESCRIPTION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GstImxSocFeaturesPlugin, gst_imx_soc_features_plugin, GST_TYPE_ELEMENT)

/* ========== Signal Handlers ========== */

static gboolean
gst_imx_soc_features_plugin_has_feature(GstImxSocFeaturesPlugin *self,
                                         const gchar *feature)
{
  return imx_soc_has_feature(feature);
}

static gboolean
gst_imx_soc_features_plugin_is_chip(GstImxSocFeaturesPlugin *self,
                                     const gchar *chip_code)
{
  return imx_soc_is_chip(chip_code);
}

static gboolean
gst_imx_soc_features_plugin_in_group(GstImxSocFeaturesPlugin *self,
                                      const gchar *group)
{
  return imx_soc_in_group(group);
}

static gchar*
gst_imx_soc_features_plugin_get_feature_description(GstImxSocFeaturesPlugin *self,
                                                     const gchar *feature)
{
  const gchar *desc = imx_soc_get_feature_description(feature);
  return desc ? g_strdup(desc) : NULL;
}

/* ========== GObject Methods ========== */

static void
gst_imx_soc_features_plugin_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_SOC_ID:
      g_value_set_string(value, imx_soc_get_soc_id());
      break;

    case PROP_CHIP_CODE:
      g_value_set_string(value, imx_soc_get_chip_code());
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
gst_imx_soc_features_plugin_init(GstImxSocFeaturesPlugin *self)
{
  /* Nothing to do - library handles lazy initialization */
}

static void
gst_imx_soc_features_plugin_class_init(GstImxSocFeaturesPluginClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  gobject_class->get_property = gst_imx_soc_features_plugin_get_property;

  /* Properties (read-only) */
  g_object_class_install_property(gobject_class, PROP_SOC_ID,
      g_param_spec_string("soc-id", "SoC ID",
          "SoC identifier from system (e.g., i.MX8MP)",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_CHIP_CODE,
      g_param_spec_string("chip-code", "Chip Code",
          "Chip code from config (e.g., MX8MP)",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* Action Signals */

  /**
   * GstImxSocFeaturesPlugin::has-feature:
   * @element: the #GstImxSocFeaturesPlugin
   * @feature: feature name to check
   *
   * Check if a feature is supported.
   *
   * Returns: TRUE if feature is supported
   */
  signals[SIGNAL_HAS_FEATURE] =
      g_signal_new("has-feature",
          G_TYPE_FROM_CLASS(klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          0, NULL, NULL, NULL,
          G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GstImxSocFeaturesPlugin::is-chip:
   * @element: the #GstImxSocFeaturesPlugin
   * @chip_code: chip code to check
   *
   * Check if running on a specific chip.
   *
   * Returns: TRUE if chip code matches
   */
  signals[SIGNAL_IS_CHIP] =
      g_signal_new("is-chip",
          G_TYPE_FROM_CLASS(klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          0, NULL, NULL, NULL,
          G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GstImxSocFeaturesPlugin::in-group:
   * @element: the #GstImxSocFeaturesPlugin
   * @group: group name to check
   *
   * Check if current chip belongs to a group.
   *
   * Returns: TRUE if in group
   */
  signals[SIGNAL_IN_GROUP] =
      g_signal_new("in-group",
          G_TYPE_FROM_CLASS(klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          0, NULL, NULL, NULL,
          G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GstImxSocFeaturesPlugin::get-feature-description:
   * @element: the #GstImxSocFeaturesPlugin
   * @feature: feature name
   *
   * Get description of a feature.
   *
   * Returns: (transfer full): description string or NULL
   */
  signals[SIGNAL_GET_FEATURE_DESCRIPTION] =
      g_signal_new("get-feature-description",
          G_TYPE_FROM_CLASS(klass),
          G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
          0, NULL, NULL, NULL,
          G_TYPE_STRING, 1, G_TYPE_STRING);

  /* Connect signal handlers using class closure */
  g_signal_override_class_handler("has-feature",
      G_TYPE_FROM_CLASS(klass),
      G_CALLBACK(gst_imx_soc_features_plugin_has_feature));

  g_signal_override_class_handler("is-chip",
      G_TYPE_FROM_CLASS(klass),
      G_CALLBACK(gst_imx_soc_features_plugin_is_chip));

  g_signal_override_class_handler("in-group",
      G_TYPE_FROM_CLASS(klass),
      G_CALLBACK(gst_imx_soc_features_plugin_in_group));

  g_signal_override_class_handler("get-feature-description",
      G_TYPE_FROM_CLASS(klass),
      G_CALLBACK(gst_imx_soc_features_plugin_get_feature_description));

  gst_element_class_set_static_metadata(element_class,
      "i.MX SoC Features",
      "Utils",
      "Provides i.MX SoC feature detection via properties and signals",
      IMX_GST_PLUGIN_AUTHOR);

  GST_DEBUG_CATEGORY_INIT(gst_imx_soc_features_plugin_debug,
      "imxsocfeatures", 0, "i.MX SoC Features Plugin");
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register
      (plugin, "imxsocfeatures", GST_RANK_NONE, GST_TYPE_IMX_SOC_FEATURES_PLUGIN)){
    return FALSE;
  }
  return TRUE;

}

IMX_GST_PLUGIN_DEFINE (imxsocfeaturesplugin, "imx soc features plugin", plugin_init);